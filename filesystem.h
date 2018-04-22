#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

#include "sized_types.h"

struct Stack;
struct Heap;

bool load_whole_file(const char* path, void** contents, u64* bytes, Stack* stack);
bool save_whole_file(const char* path, const void* contents, u64 bytes, Stack* stack);

enum class FileOpenMode
{
    Write_Temporary,
};

struct File;
File* open_file(const char* path, FileOpenMode open_mode, Heap* heap);
void close_file(File* file);
bool make_file_permanent(File* file, const char* path);
bool write_file(File* file, const void* data, u64 bytes);

void write_to_standard_output(const char* text, bool error);

// Directory Listing............................................................

enum class DirectoryRecordType
{
    Unknown,
    File,
    Directory,
};

struct DirectoryRecord
{
    char* name;
    DirectoryRecordType type;
    bool hidden;
};

struct Directory
{
    DirectoryRecord* records;
    int records_count;
};

void destroy_directory(Directory* directory, Heap* heap);
bool list_files_in_directory(const char* path, Directory* result, Heap* heap);

// Volume Listing...............................................................

struct Volume
{
    char* label;
    char* path;
};

struct VolumeList
{
    Volume* volumes;
};

bool list_volumes(VolumeList* list, Heap* heap);
void destroy_volume_list(VolumeList* list, Heap* heap);

// User Directories.............................................................

enum UserFolder
{
    Cache,
    Config,
    Data,
    Desktop,
    Documents,
    Downloads,
    Music,
    Pictures,
    Videos,
};

char* get_user_folder(UserFolder folder, Heap* heap);

#endif // FILESYSTEM_H_
