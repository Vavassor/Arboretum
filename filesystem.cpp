#include "filesystem.h"

#include "array2.h"
#include "logging.h"
#include "memory.h"
#include "platform_definitions.h"
#include "string_utilities.h"
#include "string_build.h"

#if defined(OS_LINUX)
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <mntent.h>
#include <pwd.h>
#include <stdlib.h>

bool load_whole_file(const char* path, void** contents, u64* bytes, Stack* stack)
{
    int flag = O_RDONLY;
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    int file = open(path, flag, mode);
    if(file == -1)
    {
        return false;
    }

    off_t end = lseek(file, 0, SEEK_END);
    u64 file_size = end;
    off_t start = lseek(file, 0, SEEK_SET);
    if(end == static_cast<off_t>(-1) || start == static_cast<off_t>(-1))
    {
        close(file);
        return false;
    }

    u8* file_contents = STACK_ALLOCATE(stack, u8, file_size + 1);
    ssize_t bytes_read = read(file, file_contents, file_size);
    int closed = close(file);
    if(bytes_read == -1 || closed == -1)
    {
        STACK_DEALLOCATE(stack, file_contents);
        return false;
    }

    // Null-terminate the file data so it can be easily interpreted as text if
    // necessary.
    file_contents[file_size] = '\0';

    *contents = file_contents;
    *bytes = file_size;
    return true;
}

bool save_whole_file(const char* path, const void* contents, u64 bytes, Stack* stack)
{
    static_cast<void>(stack);

    int flag = O_WRONLY | O_CREAT | O_TRUNC;
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    int file = open(path, flag, mode);
    if(file == -1)
    {
        return false;
    }

    ssize_t written = write(file, contents, bytes);
    int closed = close(file);

    return closed != -1 && written == bytes;
}

struct File
{
    Heap* heap;
    int descriptor;
};

namespace
{
    const char* os_temp_path = "/tmp";
}

File* open_file(const char* path, FileOpenMode open_mode, Heap* heap)
{
    int flag;
    switch(open_mode)
    {
        case FileOpenMode::Read:
        {
            flag = O_RDONLY;
            break;
        }
        case FileOpenMode::Write_Temporary:
        {
            flag = O_TMPFILE | O_WRONLY;
            if(!path)
            {
                path = os_temp_path;
            }
            break;
        }
    }
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    int descriptor = open(path, flag, mode);
    if(descriptor == -1)
    {
        return nullptr;
    }

    File* file = HEAP_ALLOCATE(heap, File, 1);
    file->heap = heap;
    file->descriptor = descriptor;
    return file;
}

void close_file(File* file)
{
    if(!file)
    {
        return;
    }

    close(file->descriptor);
    HEAP_DEALLOCATE(file->heap, file);
}

bool make_file_permanent(File* file, const char* path)
{
    const int fd_path_max = 35;
    char fd_path[fd_path_max];
    format_string(fd_path, fd_path_max, "/proc/self/fd/%d", file->descriptor);
    int result = linkat(AT_FDCWD, fd_path, AT_FDCWD, path, AT_SYMLINK_FOLLOW);
    return result == 0;
}

bool read_file(File* file, void* data, u64 bytes, u64* bytes_got)
{
    ssize_t bytes_read = read(file->descriptor, data, bytes);
    if(bytes_read != -1)
    {
        *bytes_got = bytes_read;
        return true;
    }
    return false;
}

bool read_line(File* file, char** line, u64* bytes, Heap* heap)
{
    if(!*line)
    {
        *bytes = 128;
        *line = HEAP_ALLOCATE(heap, char, *bytes);
        if(!*line)
        {
            return false;
        }
    }

    int read_index = 0;

    char value;
    do
    {
        u64 bytes_read;
        bool char_read = read_file(file, &value, 1, &bytes_read);
        if(!char_read || bytes_read != 1)
        {
            if(read_index == 0)
            {
                return false;
            }
            else
            {
                break;
            }
        }

        if(*bytes - read_index < 2)
        {
            *bytes *= 2;
            char* buffer = *line;
            buffer = HEAP_REALLOCATE(heap, buffer, *bytes);
            *line = buffer;
            if(!buffer)
            {
                return false;
            }
        }

        (*line)[read_index] = value;
        read_index += 1;
    } while(value != '\n');

    (*line)[read_index] = '\0';

    return true;
}

bool write_file(File* file, const void* data, u64 bytes)
{
    s64 written = write(file->descriptor, data, bytes);
    return written == bytes;
}

void write_to_standard_output(const char* text, bool error)
{
    const char* path;
    if(error)
    {
        path = "/proc/self/fd/2";
    }
    else
    {
        path = "/proc/self/fd/1";
    }
    int flag = O_WRONLY;
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    int descriptor = open(path, flag, mode);
    if(descriptor != -1)
    {
        write(descriptor, text, string_size(text));
        close(descriptor);
    }
}

// Directory Listing............................................................

static DirectoryRecordType translate_directory_record_type(unsigned char type)
{
    switch(type)
    {
        default:     return DirectoryRecordType::Unknown;
        case DT_DIR: return DirectoryRecordType::Directory;
        case DT_REG: return DirectoryRecordType::File;
    }
}

static DirectoryRecordType translate_directory_record_type_from_mode(mode_t mode)
{
    switch(mode & S_IFMT)
    {
        default:      return DirectoryRecordType::Unknown;
        case S_IFDIR: return DirectoryRecordType::Directory;
        case S_IFREG: return DirectoryRecordType::File;
    }
}

static DirectoryRecordType fetch_symlink_type(const char* path, const char* name, Heap* heap)
{
    char* symlink_path = append_to_path(path, name, heap);
    struct stat info;
    int status = stat(symlink_path, &info);
    HEAP_DEALLOCATE(heap, symlink_path);
    if(status == -1)
    {
        return DirectoryRecordType::Unknown;
    }
    else
    {
        return translate_directory_record_type_from_mode(info.st_mode);
    }
}

bool list_files_in_directory(const char* path, Directory* result, Heap* heap)
{
    DIR* directory = opendir(path);
    if(!directory)
    {
        return false;
    }

    DirectoryRecord* listing = nullptr;
    int listing_count = 0;

    for(;;)
    {
        struct dirent* entry = readdir(directory);
        if(!entry)
        {
            break;
        }
        if(strings_match(entry->d_name, ".") || strings_match(entry->d_name, ".."))
        {
            continue;
        }
        DirectoryRecord record;
        if(entry->d_type == DT_LNK)
        {
            record.type = fetch_symlink_type(path, entry->d_name, heap);
        }
        else
        {
            record.type = translate_directory_record_type(entry->d_type);
        }
        record.name = copy_string_to_heap(entry->d_name, heap);
        record.hidden = string_starts_with(record.name, ".");
        ARRAY_ADD(listing, record, heap);
        listing_count += 1;
    }
    closedir(directory);

    result->records = listing;
    result->records_count = listing_count;

    return true;
}

// Volume Listing...............................................................

// These notes are copied from the section in "man proc" on
// /proc/[pid]/mountinfo.
struct MountInfoEntry
{
    int mount_id; // unique identifier of the mount (may be reused after umount)
    int parent_id; // ID of parent (or of self for the top of the mount tree)
    dev_t device_id; // value of st_dev for files on filesystem
    char* root; // root of the mount within the filesystem
    char* mountpoint; // mount point relative to the process's root
    char* mount_options; // per mount options
    char* optional_fields; // zero or more fields of the form "tag[:value]"
    char* filesystem_type; // name of filesystem of the form "type[.subtype]"
    char* mount_source; // filesystem specific information or "none"
    char* super_options; // per super block options
};

static void destroy_mount_info_entry(MountInfoEntry* entry, Heap* heap)
{
    if(entry)
    {
        SAFE_HEAP_DEALLOCATE(heap, entry->root);
        SAFE_HEAP_DEALLOCATE(heap, entry->mountpoint);
        SAFE_HEAP_DEALLOCATE(heap, entry->mount_options);
        SAFE_HEAP_DEALLOCATE(heap, entry->optional_fields);
        SAFE_HEAP_DEALLOCATE(heap, entry->filesystem_type);
        SAFE_HEAP_DEALLOCATE(heap, entry->mount_source);
        SAFE_HEAP_DEALLOCATE(heap, entry->super_options);
    }
}

static bool get_int(const char* original, const char** after, int* result)
{
    return string_to_int_extra(original, const_cast<char**>(after), 0, result);
}

static char* get_string(const char* line, const char* separator, const char** after, Heap* heap)
{
    const char* space = find_string(line, separator);
    if(space)
    {
        ptrdiff_t count = space - line;
        // Always skip past the separator even if there's nothing to return.
        *after = space + string_size(separator);
        if(count > 0)
        {
            return copy_chars_to_heap(line, count, heap);
        }
    }
    return nullptr;
}

// This gets *and* decodes a path string. The kernel encodes the following
// characters as a backslash and three octal digits: space ' ', tab '\t',
// backslash '\\', and line feed '\n'.
static char* get_path(const char* line, const char** after, Heap* heap)
{
    // Find the ending space ' ' first, so we can figure out how much room to
    // allocate for the path up-front. The only encoded part is the octal
    // sequences, which are a fixed size. So the output will always be the same
    // or shorter than the encoded path.
    int count = find_char(line, ' ');
    if(count == invalid_index)
    {
        return nullptr;
    }

    char* path = HEAP_ALLOCATE(heap, char, count + 1);
    if(path)
    {
        // Copy the characters to the path until the ending space is reached.
        // Also replace any octal sequences with the encoded character.
        for(int i = 0, j = 0; i <= count; j += 1)
        {
            switch(line[i])
            {
                case ' ':
                {
                    path[j] = '\0';
                    *after = &line[i + 1];
                    return path;
                }
                case '\\':
                {
                    // All octal sequences must be one backslash and exactly
                    // three digits.
                    if(i + 4 >= count)
                    {
                        HEAP_DEALLOCATE(heap, path);
                        return nullptr;
                    }

                    char c = (line[i + 1] - '0') << 6;
                    c |= (line[i + 2] - '0') << 3;
                    c |= (line[i + 3] - '0');
                    i += 4;
                    path[j] = c;

                    break;
                }
                default:
                {
                    path[j] = line[i];
                    i += 1;
                    break;
                }
            }
        }
        HEAP_DEALLOCATE(heap, path);
    }

    return nullptr;
}

static bool parse_entry(const char* line, MountInfoEntry* entry, Heap* heap)
{
    bool got = get_int(line, &line, &entry->mount_id);
    if(!got)
    {
        return false;
    }

    got = get_int(line, &line, &entry->parent_id);
    if(!got)
    {
        return false;
    }

    // device_id written "major:minor"
    int major;
    got = get_int(line, &line, &major);
    if(!got)
    {
        return false;
    }
    if(*line != ':')
    {
        return false;
    }
    line += 1;
    int minor;
    got = get_int(line, &line, &minor);
    if(!got)
    {
        return false;
    }
    entry->device_id = makedev(major, minor);

    // Make sure not to call get_path starting with a space.
    if(*line != ' ')
    {
        return false;
    }
    line += 1;

    entry->root = get_path(line, &line, heap);
    if(!entry->root)
    {
        return false;
    }

    entry->mountpoint = get_path(line, &line, heap);
    if(!entry->mountpoint)
    {
        return false;
    }

    entry->mount_options = get_string(line, " ", &line, heap);
    if(!entry->mount_options)
    {
        return false;
    }

    entry->optional_fields = get_string(line, "- ", &line, heap);

    entry->filesystem_type = get_string(line, " ", &line, heap);
    if(!entry->filesystem_type)
    {
        return false;
    }

    entry->mount_source = get_path(line, &line, heap);
    if(!entry->mount_source)
    {
        return false;
    }

    entry->super_options = get_string(line, "\n", &line, heap);
    if(!entry->super_options)
    {
        return false;
    }

    return true;
}

static bool get_mount_info_entry(File* file, MountInfoEntry* entry, Heap* heap)
{
    bool parsed = false;

    // Get the next line in the /proc/self/mountinfo file and pass it to
    // parse_entry. Keep the cleanup here so parse_entry can quit at any time
    // and assume it'll be taken care of here safely.
    u64 line_bytes;
    char* line = nullptr;
    bool got = read_line(file, &line, &line_bytes, heap);
    if(got)
    {
        parsed = parse_entry(line, entry, heap);
        if(!parsed)
        {
            destroy_mount_info_entry(entry, heap);
        }
        HEAP_DEALLOCATE(heap, line);
    }

    return parsed;
}

// udev disallows certain characters in its strings and encodes them by
// replacing "potentially unsafe" characters with their hexadecimal value
// preceded by \x, like \x20. Since backslash is used for this, it also has
// to be replaced by its own code \x5C.
static char* decode_label(const char* encoded, Heap* heap)
{
    // Since the encoding replaces characters with a fixed-size sequence, the
    // output has to be either the same size or smaller. So allocating space
    // the same size as the encoded version is always going to be enough.
    int end = string_size(encoded);
    char* label = HEAP_ALLOCATE(heap, char, end + 1);

    // Find any 4-byte sequences starting with \x to replace. Otherwise, copy
    // as you go.
    int j = 0;
    for(int i = 0; i < end; j += 1)
    {
        if(i <= end - 4 && encoded[i] == '\\' && encoded[i + 1] == 'x')
        {
            int value;
            bool success = string_to_int_extra(&encoded[i + 2], nullptr, 16, &value);
            if(success)
            {
                label[j] = value;
            }
            i += 4;
        }
        else
        {
            label[j] = encoded[i];
            i += 1;
        }
    }
    label[j] = '\0';

    return label;
}

static char* get_label(const char* device_path, Heap* heap)
{
    char* label = nullptr;

    char* canonical_device = realpath(device_path, nullptr);
    if(canonical_device)
    {
        // Find an entry in the directory /dev/disk/by-label that links to a device
        // file with a matching "real" path. Its filename will be the desired label.
        //
        // The /dev/disk/by-label directory is managed by udev, a device manager
        // for the kernel.
        const char* by_label = "/dev/disk/by-label";
        DIR* directory = opendir(by_label);
        if(directory)
        {
            for(;;)
            {
                struct dirent* entry = readdir(directory);
                if(!entry)
                {
                    break;
                }
                const char* name = entry->d_name;
                if(strings_match(name, ".") || strings_match(name, ".."))
                {
                    // Skip the entries for the directory itself and its parent.
                    continue;
                }
                if(entry->d_type == DT_LNK)
                {
                    char* link_path = append_to_path(by_label, name, heap);
                    char* path = realpath(link_path, nullptr);
                    bool matches = strings_match(path, canonical_device);
                    HEAP_DEALLOCATE(heap, link_path);
                    free(path);
                    if(matches)
                    {
                        // The filename is an encoded version of the label that
                        // corresponds to the udev device property
                        // ENV{ID_FS_LABEL_ENC}.
                        label = decode_label(name, heap);
                    }
                }
            }
            closedir(directory);
        }
        free(canonical_device);
    }

    return label;
}

bool list_volumes(VolumeList* list, Heap* heap)
{
    // First try to use /proc/self/mountinfo and fallback to listing from
    // /etc/mtab if it's not available.
    File* file = open_file("/proc/self/mountinfo", FileOpenMode::Read, heap);
    if(file)
    {
        for(;;)
        {
            MountInfoEntry entry = {};
            bool got = get_mount_info_entry(file, &entry, heap);
            if(!got)
            {
                break;
            }
            if(string_starts_with(entry.mount_source, "/dev/"))
            {
                Volume volume;
                volume.path = copy_string_to_heap(entry.mountpoint, heap);
                volume.label = get_label(entry.mount_source, heap);
                ARRAY_ADD(list->volumes, volume, heap);
            }
            destroy_mount_info_entry(&entry, heap);
        }
        close_file(file);
    }
    else
    {
        FILE* mtab_file = setmntent(_PATH_MOUNTED, "r");
        if(mtab_file)
        {
            for(;;)
            {
                struct mntent* entry = getmntent(mtab_file);
                if(!entry)
                {
                    break;
                }
                if(string_starts_with(entry->mnt_fsname, "/dev/"))
                {
                    Volume volume;
                    volume.path = copy_string_to_heap(entry->mnt_dir, heap);
                    volume.label = get_label(entry->mnt_fsname, heap);
                    ARRAY_ADD(list->volumes, volume, heap);
                }
            }
            endmntent(mtab_file);
        }
    }
    return true;
}

// User Directories.............................................................

static bool is_uid_root(int uid)
{
    return uid == 0;
}

static const char* get_home_folder()
{
    int uid = getuid();
    const char* home_env = getenv("HOME");
    if(!is_uid_root(uid) && home_env)
    {
        return home_env;
    }
    struct passwd* pw = getpwuid(uid);
    if(!pw || !pw->pw_dir)
    {
        return nullptr;
    }
    return pw->pw_dir;
}

static char* get_user_folder(const char* env_name, const char* default_relative_path, Heap* heap)
{
    const char* folder = getenv(env_name);
    if(folder)
    {
        return copy_string_to_heap(folder, heap);
    }

    const char* home = get_home_folder();
    if(home)
    {
        return append_to_path(home, default_relative_path, heap);
    }

    return nullptr;
}

static const char* get_env_name(UserFolder folder)
{
    switch(folder)
    {
        default:
        case UserFolder::Cache:     return "XDG_CACHE_HOME";
        case UserFolder::Config:    return "XDG_CONFIG_HOME";
        case UserFolder::Data:      return "XDG_DATA_HOME";
        case UserFolder::Desktop:   return "XDG_DESKTOP_DIR";
        case UserFolder::Documents: return "XDG_DOCUMENTS_DIR";
        case UserFolder::Downloads: return "XDG_DOWNLOAD_DIR";
        case UserFolder::Music:     return "XDG_MUSIC_DIR";
        case UserFolder::Pictures:  return "XDG_PICTURES_DIR";
        case UserFolder::Videos:    return "XDG_VIDEOS_DIR";
    }
}

static const char* get_default_relative_path(UserFolder folder)
{
    switch(folder)
    {
        default:
        case UserFolder::Cache:     return ".cache";
        case UserFolder::Config:    return ".config";
        case UserFolder::Data:      return ".local/share";
        case UserFolder::Desktop:   return "Desktop";
        case UserFolder::Documents: return "Documents";
        case UserFolder::Downloads: return "Downloads";
        case UserFolder::Music:     return "Music";
        case UserFolder::Pictures:  return "Pictures";
        case UserFolder::Videos:    return "Videos";
    }
}

char* get_user_folder(UserFolder folder, Heap* heap)
{
    const char* env_name = get_env_name(folder);
    const char* default_relative_path = get_default_relative_path(folder);
    return get_user_folder(env_name, default_relative_path, heap);
}

#elif defined(OS_WINDOWS)

#include "wide_char.h"

#include <Shlobj.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

bool load_whole_file(const char* path, void** contents, u64* bytes, Stack* stack)
{
    wchar_t* wide_path = utf8_to_wide_char(path, stack);
    HANDLE handle = CreateFileW(wide_path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    STACK_DEALLOCATE(stack, wide_path);
    if(handle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    BY_HANDLE_FILE_INFORMATION info;
    BOOL got = GetFileInformationByHandle(handle, &info);
    if(!got)
    {
        CloseHandle(handle);
        return false;
    }

    u64 size = info.nFileSizeHigh << 32 | info.nFileSizeLow;
    char* buffer = STACK_ALLOCATE(stack, char, size + 1);
    DWORD bytes_read;
    BOOL read = ReadFile(handle, buffer, size, &bytes_read, nullptr);

    CloseHandle(handle);

    if(!read || bytes_read != size)
    {
        STACK_DEALLOCATE(stack, buffer);
        return false;
    }

    // Null-terminate the file data so it can be easily interpreted as text if
    // necessary.
    buffer[size] = '\0';
    *contents = buffer;
    *bytes = size;

    return true;
}

bool save_whole_file(const char* path, const void* contents, u64 bytes, Stack* stack)
{
    wchar_t* wide_path = utf8_to_wide_char(path, stack);
    HANDLE handle = CreateFileW(wide_path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    STACK_DEALLOCATE(stack, wide_path);
    if(handle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DWORD bytes_written;
    BOOL wrote = WriteFile(handle, contents, bytes, &bytes_written, nullptr);
    CloseHandle(handle);

    return wrote && bytes_written == bytes;
}

struct File
{
    HANDLE handle;
    Heap* heap;
    wchar_t* wide_path;
};

File* open_file(const char* path, FileOpenMode open_mode, Heap* heap)
{
    wchar_t* wide_path = nullptr;

    DWORD access;
    DWORD share_mode;
    DWORD disposition;
    DWORD flags;
    switch(open_mode)
    {
        case FileOpenMode::Write_Temporary:
        {
            access = GENERIC_WRITE;
            share_mode = 0;
            disposition = CREATE_ALWAYS;
            flags = FILE_ATTRIBUTE_NORMAL;
            if(!path)
            {
                const int wide_path_cap = MAX_PATH;
                wchar_t just_path[wide_path_cap];
                DWORD count = GetTempPathW(wide_path_cap, just_path);
                if(count == 0)
                {
                    return nullptr;
                }
                wide_path = HEAP_ALLOCATE(heap, wchar_t, wide_path_cap);
                UINT unique = GetTempFileNameW(just_path, L"ARB", 0, wide_path);
                if(unique == 0)
                {
                    SAFE_HEAP_DEALLOCATE(heap, wide_path);
                    return nullptr;
                }
            }
            break;
        }
    }

    if(!wide_path)
    {
        wide_path = utf8_to_wide_char(path, heap);
    }

    HANDLE handle = CreateFileW(wide_path, access, share_mode, nullptr, disposition, flags, nullptr);
    if(handle == INVALID_HANDLE_VALUE)
    {
        SAFE_HEAP_DEALLOCATE(heap, wide_path);
        return nullptr;
    }

    File* file = HEAP_ALLOCATE(heap, File, 1);
    file->handle = handle;
    file->heap = heap;
    file->wide_path = wide_path;
    return file;
}

void close_file(File* file)
{
    if(!file)
    {
        return;
    }

    CloseHandle(file->handle);
    SAFE_HEAP_DEALLOCATE(file->heap, file->wide_path);
    HEAP_DEALLOCATE(file->heap, file);
}

bool make_file_permanent(File* file, const char* path)
{
    wchar_t* wide_path = utf8_to_wide_char(path, file->heap);
    DWORD flags = MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING;
    BOOL moved = MoveFileExW(file->wide_path, wide_path, flags);
    SAFE_HEAP_DEALLOCATE(file->heap, wide_path);
    return moved;
}

bool write_file(File* file, const void* data, u64 bytes)
{
    DWORD bytes_written;
    BOOL wrote = WriteFile(file->handle, data, bytes, &bytes_written, nullptr);
    return wrote && bytes_written == bytes;
}

void write_to_standard_output(const char* text, bool error)
{
    if(IsDebuggerPresent())
    {
        OutputDebugStringA(text);
    }
    else
    {
        HANDLE handle;
        if(error)
        {
            handle = GetStdHandle(STD_ERROR_HANDLE);
        }
        else
        {
            handle = GetStdHandle(STD_OUTPUT_HANDLE);
        }
        DWORD bytes_written;
        WriteFile(handle, text, string_size(text), &bytes_written, nullptr);
        CloseHandle(handle);
    }
}

// Directory Listing............................................................

static DirectoryRecordType translate_directory_record_type(DWORD attributes)
{
    if(attributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        return DirectoryRecordType::Directory;
    }
    else if(attributes & FILE_ATTRIBUTE_NORMAL)
    {
        return DirectoryRecordType::File;
    }
    else
    {
        return DirectoryRecordType::Unknown;
    }
}

bool list_files_in_directory(const char* path, Directory* result, Heap* heap)
{
    char* new_path = copy_string_to_heap(path, heap);
    new_path = append_to_path(new_path, "*", heap);
    wchar_t* wide_path = utf8_to_wide_char(new_path, heap);
    HEAP_DEALLOCATE(heap, new_path);
    
    WIN32_FIND_DATAW data;
    HANDLE found = FindFirstFileW(wide_path, &data);
    SAFE_HEAP_DEALLOCATE(heap, wide_path);
    if(found == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DirectoryRecord* listing = nullptr;
    int listing_count = 0;

    do
    {
        char* filename = wide_char_to_utf8(data.cFileName, heap);

        if(strings_match(filename, ".") || strings_match(filename, ".."))
        {
            HEAP_DEALLOCATE(heap, filename);
            continue;
        }

        DirectoryRecord record;
        record.type = translate_directory_record_type(data.dwFileAttributes);
        record.name = filename;
        record.hidden = data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN;

        ARRAY_ADD(listing, record, heap);
        listing_count += 1;
    } while(FindNextFileW(found, &data));
    
    DWORD error = GetLastError();
    FindClose(found);

    if(error != ERROR_NO_MORE_FILES)
    {
        for(int i = 0; i < listing_count; i += 1)
        {
            SAFE_HEAP_DEALLOCATE(heap, listing->name);
        }
        ARRAY_DESTROY(listing, heap);
        return false;
    }

    result->records = listing;
    result->records_count = listing_count;

    return true;
}

// Volume Listing...............................................................

static wchar_t* get_path_chain(const wchar_t* volume_name, Heap* heap)
{
    // Ask for the volume path names and just guess an arbitrary size for it.
    int count = 50;
    wchar_t* path_chain = HEAP_ALLOCATE(heap, wchar_t, count);
    DWORD char_count;
    BOOL got = GetVolumePathNamesForVolumeNameW(volume_name, path_chain, count, &char_count);
    if(got)
    {
        return path_chain;
    }

    // If it wasn't enough room, resize the space to the amount passed back in
    // char_count and try again.
    DWORD error = GetLastError();
    if(error == ERROR_MORE_DATA)
    {
        count = char_count;
        path_chain = HEAP_REALLOCATE(heap, path_chain, count);
        got = GetVolumePathNamesForVolumeNameW(volume_name, path_chain, count, &char_count);
        if(got)
        {
            return path_chain;
        }
    }

    // If that didn't work out.
    HEAP_DEALLOCATE(heap, path_chain);

    return nullptr;
}

static char* get_label(const wchar_t* path_chain, Heap* heap)
{
    const int label_cap = MAX_PATH + 1;
    wchar_t label[label_cap];
    BOOL got = GetVolumeInformationW(path_chain, label, label_cap, nullptr, nullptr, nullptr, nullptr, 0);
    if(got)
    {
        return wide_char_to_utf8(label, heap);
    }
    return nullptr;
}

static bool is_volume_ready(const wchar_t* name)
{
    BOOL result = GetVolumeInformationW(name, nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0);
    return result && GetLastError() != ERROR_NOT_READY;
}

bool list_volumes(VolumeList* list, Heap* heap)
{
    const int volume_name_cap = 50;
    wchar_t volume_name[volume_name_cap];
    HANDLE handle = FindFirstVolumeW(volume_name, volume_name_cap);
    if(handle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    BOOL found;
    do
    {
        if(is_volume_ready(volume_name))
        {
            wchar_t* path_chain = get_path_chain(volume_name, heap);
            if(!path_chain)
            {
                destroy_volume_list(list, heap);
                return false;
            }

            Volume volume;
            volume.path = wide_char_to_utf8(path_chain, heap);
            volume.label = get_label(path_chain, heap);
            if(!volume.label)
            {
                volume.label = copy_string_to_heap(volume.path, heap);
            }
            ARRAY_ADD(list->volumes, volume, heap);

            HEAP_DEALLOCATE(heap, path_chain);
        }
        found = FindNextVolumeW(handle, volume_name, volume_name_cap);
    } while(!found);

    DWORD error = GetLastError();
    FindVolumeClose(handle);

    if(error != ERROR_NO_MORE_FILES)
    {
        destroy_volume_list(list, heap);
        return false;
    }

    return true;
}

// User Directories.............................................................

KNOWNFOLDERID translate_to_known_folder_id(UserFolder folder)
{
    switch(folder)
    {
        case UserFolder::Cache:     return FOLDERID_LocalAppData;
        case UserFolder::Config:    return FOLDERID_RoamingAppData;
        case UserFolder::Data:      return FOLDERID_LocalAppData;
        case UserFolder::Desktop:   return FOLDERID_Desktop;
        case UserFolder::Documents: return FOLDERID_Documents;
        case UserFolder::Music:     return FOLDERID_Music;
        case UserFolder::Pictures:  return FOLDERID_Pictures;
        case UserFolder::Videos:    return FOLDERID_Videos;
    }
}

char* get_user_folder(UserFolder folder, Heap* heap)
{
    KNOWNFOLDERID folder_id = translate_to_known_folder_id(folder);
    PWSTR path = nullptr;
    HRESULT result = SHGetKnownFolderPath(folder_id, 0, nullptr, &path);
    if(FAILED(result))
    {
        CoTaskMemFree(path);
        return nullptr;
    }

    char* finished = wide_char_to_utf8(path, heap);
    replace_chars(finished, '\\', '/');

    CoTaskMemFree(path);

    return finished;
}

#endif // defined(OS_WINDOWS)

// Directory Listing............................................................

void destroy_directory(Directory* directory, Heap* heap)
{
    for(int i = 0; i < directory->records_count; i += 1)
    {
        HEAP_DEALLOCATE(heap, directory->records[i].name);
    }
    directory->records_count = 0;

    ARRAY_DESTROY(directory->records, heap);
}

// Volume Listing...............................................................

void destroy_volume_list(VolumeList* list, Heap* heap)
{
    if(list)
    {
        FOR_ALL(list->volumes)
        {
            HEAP_DEALLOCATE(heap, it->label);
            HEAP_DEALLOCATE(heap, it->path);
        }
        ARRAY_DESTROY(list->volumes, heap);
    }
}
