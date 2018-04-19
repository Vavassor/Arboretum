#include "filesystem.h"

#include "array2.h"
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

char* get_documents_folder(Heap* heap)
{
    return get_user_folder("XDG_DOCUMENTS_DIR", "Documents", heap);
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

// User Directories.............................................................

char* get_documents_folder(Heap* heap)
{
	PWSTR path = nullptr;
	HRESULT result = SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &path);
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
