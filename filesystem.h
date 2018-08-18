#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

#include "memory.h"

#include <stdbool.h>

bool load_whole_file(const char* path, void** contents, uint64_t* bytes, Stack* stack);
bool save_whole_file(const char* path, const void* contents, uint64_t bytes, Stack* stack);

typedef enum FileOpenMode
{
    FILE_OPEN_MODE_READ,
    FILE_OPEN_MODE_WRITE_TEMPORARY,
} FileOpenMode;

typedef struct File File;

File* open_file(const char* path, FileOpenMode open_mode, Heap* heap);
void close_file(File* file);
bool make_file_permanent(File* file, const char* path);
bool read_file(File* file, void* data, uint64_t bytes, uint64_t* bytes_got);
bool write_file(File* file, const void* data, uint64_t bytes);

void write_to_standard_output(const char* text, bool error);

// Directory Listing............................................................

typedef enum DirectoryRecordType
{
    DIRECTORY_RECORD_TYPE_UNKNOWN,
    DIRECTORY_RECORD_TYPE_FILE,
    DIRECTORY_RECORD_TYPE_DIRECTORY,
} DirectoryRecordType;

typedef struct DirectoryRecord
{
    char* name;
    DirectoryRecordType type;
    bool hidden;
} DirectoryRecord;

typedef struct Directory
{
    DirectoryRecord* records;
    int records_count;
} Directory;

void destroy_directory(Directory* directory, Heap* heap);
bool list_files_in_directory(const char* path, Directory* result, Heap* heap);

// Volume Listing...............................................................

typedef struct Volume
{
    char* label;
    char* path;
} Volume;

typedef struct VolumeList
{
    Volume* volumes;
} VolumeList;

bool list_volumes(VolumeList* list, Heap* heap);
void destroy_volume_list(VolumeList* list, Heap* heap);

// User Directories.............................................................

typedef enum UserFolder
{
    USER_FOLDER_CACHE,
    USER_FOLDER_CONFIG,
    USER_FOLDER_DATA,
    USER_FOLDER_DESKTOP,
    USER_FOLDER_DOCUMENTS,
    USER_FOLDER_DOWNLOADS,
    USER_FOLDER_MUSIC,
    USER_FOLDER_PICTURES,
    USER_FOLDER_VIDEOS,
} UserFolder;

char* get_user_folder(UserFolder folder, Heap* heap);

// Executable Directory.........................................................

char* get_executable_folder(Heap* heap);

#endif // FILESYSTEM_H_
