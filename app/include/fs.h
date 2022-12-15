#pragma once

#include <stdbool.h>

#define FS_MAX_PATH 256

typedef struct FsFileListEntry {
    struct FsFileListEntry *next;
    struct FsFileListEntry *previous;
    char *name;
    int name_length;
    bool is_folder;
    SceOff size;
    ScePspDateTime time;
} FsFileListEntry;

typedef struct {
    FsFileListEntry *head;
    FsFileListEntry *tail;
    int length;
    char path[FS_MAX_PATH];
    int files;
    int folders;
} FsFileList;

typedef struct {
    u64 value;
    u64 max;
    void (*setProgress)(int value, int max);
    bool (*cancelHandler)(void);
} FileProcessParam;

extern char g_cwd[FS_MAX_PATH];
extern FsFileList g_file_list;

int fsGetDirectoryEntries(FsFileList *list, const char *path);
void fsFreeDirectoryEntries(FsFileList *list);
int fsAppendDir(FsFileList *list, const char *path);
int fsParentDir(FsFileList *list);
const char *fsSetPath(FsFileListEntry *entry, const char *path);
int fsCopyPath(const char *src_path, const char *dest_path, FileProcessParam *param);
int fsMovePath(const char *src_path, const char *dest_path);
int fsDelete(const char *path, FileProcessParam *param);
