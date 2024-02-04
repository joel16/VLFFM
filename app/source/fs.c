#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs.h"
#include "kernel.h"
#include "utils.h"

// Following FS code using linked list is from VitaShell by TheOfficialFloW with slight
// modifications to use pspsdk instead of vitasdk and without VitaShell specific changes
// https://github.com/TheOfficialFloW/VitaShell/blob/master/file.c

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

char g_cwd[FS_MAX_PATH] = { 0 };
FsFileList g_file_list = { 0 };

static const int buf_size = 0x10000;

static bool fsHasEndSlash(const char *path) {
    return path[strlen(path) - 1] == '/';
}

static void fsAddEntry(FsFileList *list, FsFileListEntry *entry) {
    if ((!list) || (!entry)) {
        return;
    }
    
    entry->next = NULL;
    entry->previous = NULL;
    
    if (list->head == NULL) {
        list->head = entry;
        list->tail = entry;
    }
    else {
        FsFileListEntry *p = list->head;
        FsFileListEntry *previous = NULL;
        
        char entry_name[FS_MAX_PATH];
        strcpy(entry_name, entry->name);
        
        while (p) {
            char p_name[FS_MAX_PATH];
            strcpy(p_name, p->name);
            
            // '..' is always at first
            if (strcmp(entry_name, "..") == 0) {
                break;
            }
            
            if (strcmp(p_name, "..") == 0) {
                previous = p;
                p = p->next;
                continue;
            }
            
            // First folders then files
            if (entry->is_folder > p->is_folder) {
                break;
            }
            
            // Sort by name within the same type
            if (entry->is_folder == p->is_folder) {
                if (strcasecmp(entry_name, p_name) < 0) {
                    break;
                }
            }
            
            previous = p;
            p = p->next;
        }
        
        if (previous == NULL) { // Order: entry (new head) -> p (old head)
            entry->next = p;
            p->previous = entry;
            list->head = entry;
        }
        else if (previous->next == NULL) { // Order: p (old tail) -> entry (new tail)
            FsFileListEntry *tail = list->tail;
            tail->next = entry;
            entry->previous = tail;
            list->tail = entry;
        }
        else { // Order: previous -> entry -> p
            previous->next = entry;
            entry->previous = previous;
            entry->next = p;
            p->previous = entry;
        }
    }
    
    list->length++;
}

int fsGetDirectoryEntries(FsFileList *list, const char *path) {
    if (!list) {
        return -1;
    }
    
    SceUID dfd = 0;
    if (R_FAILED(dfd = pspIoOpenDir(path))) {
        utilsLogError("%s: pspIoOpenDir(%s) failed: 0x%x\n", __func__, path, dfd);
        return dfd;
    }
    
    int res = 0;
    
    do {
        SceIoDirent dir;
        memset(&dir, 0, sizeof(SceIoDirent));
        
        res = pspIoReadDir(dfd, &dir);
        if (res > 0) {
            if (strcmp(dir.d_name, ".") == 0) {
                continue;
            }

            FsFileListEntry *entry = malloc(sizeof(FsFileListEntry));

            if (entry) {
                entry->is_folder = FIO_S_ISDIR(dir.d_stat.st_mode);
                
                if (entry->is_folder) {
                    entry->name_length = strlen(dir.d_name) + 1;
                    entry->name = malloc(entry->name_length + 1);
                    strcpy(entry->name, dir.d_name);
                    list->folders++;
                }
                else {
                    entry->name_length = strlen(dir.d_name);
                    entry->name = malloc(entry->name_length + 1);
                    strcpy(entry->name, dir.d_name);
                    list->files++;
                }
                
                entry->size = dir.d_stat.st_size;
                memcpy(&entry->time, &dir.d_stat.sce_st_mtime, sizeof(ScePspDateTime));
                fsAddEntry(list, entry);
            }
        }
    } while (res > 0);
    
    pspIoCloseDir(dfd);
    return 0;
}

void fsFreeDirectoryEntries(FsFileList *list) {
    if (!list) {
        return;
    }
    
    FsFileListEntry *entry = list->head;
    
    while (entry) {
        FsFileListEntry *next = entry->next;
        free(entry->name);
        free(entry);
        entry = next;
    }
    
    list->head = NULL;
    list->tail = NULL;
    list->length = 0;
    list->files = 0;
    list->folders = 0;
}

int fsAppendDir(FsFileList *list, const char *path) {
    char cwd[FS_MAX_PATH] = { 0 };

    if ((snprintf(cwd, FS_MAX_PATH, "%s%s%s", g_cwd, fsHasEndSlash(g_cwd) ? "" : "/", path)) > 0) {
        strncpy(g_cwd, cwd, FS_MAX_PATH);
        fsFreeDirectoryEntries(list);

        int ret = 0;
        if (R_FAILED(ret = fsGetDirectoryEntries(list, g_cwd))) {
            return ret;
        }
    }

    return 0;
}

int fsParentDir(FsFileList *list) {
    int cwd_len = strlen(g_cwd);

    if (g_cwd[(cwd_len - 1)] == '/') {
        return 1;
    }

    bool copy = false;
    char cwd[FS_MAX_PATH] = { 0 };
    int len = 0;
    
    for (int i = cwd_len; i >= 0; i--) {
        if (g_cwd[i] == '/') {
            copy = true;
        }
        if (copy) {
            cwd[i] = g_cwd[i];
            len++;
        }
    }
    
    if (len > 1 && cwd[len - 1] == '/') {
        len--;
    }
    
    cwd[len] = '\0';
    
    if (cwd[len - 1] == ':') {
        snprintf(g_cwd, 257, "%s/", cwd);
    }
    else {
        strncpy(g_cwd, cwd, FS_MAX_PATH);
    }
    
    fsFreeDirectoryEntries(list);

    int ret = 0;
    if (R_FAILED(ret = fsGetDirectoryEntries(list, g_cwd))) {
        return ret;
    }

    return 0;
}

const char *fsSetPath(FsFileListEntry *entry, const char *path) {
    static char new_path[FS_MAX_PATH] = { 0 };
    if (R_FAILED(snprintf(new_path, FS_MAX_PATH, "%s%s%s", g_cwd, fsHasEndSlash(g_cwd) ? "" : "/", path == NULL? entry->name : path))) {
        return NULL;
    }

    return new_path;
}

static int fsCopyFile(const char *src_path, const char *dest_path, FileProcessParam *param) {
    // The source and destination paths are identical
    if (strcasecmp(src_path, dest_path) == 0) {
        return -1;
    }

    // The destination is a subfolder of the source folder
    int len = strlen(src_path);
    if (strncasecmp(src_path, dest_path, len) == 0 && (dest_path[len] == '/' || dest_path[len - 1] == '/')) {
        return -2;
    }
    
    SceUID src_fd = 0;
    if (R_FAILED(src_fd = pspIoOpenFile(src_path, PSP_O_RDONLY, 0))) {
        utilsLogError("%s: pspIoOpenFile(%s) failed: 0x%x\n", __func__, src_path, src_fd);
        return src_fd;
    }
    
    u64 size = pspIoLseek(src_fd, 0, PSP_SEEK_END);
    pspIoLseek(src_fd, 0, PSP_SEEK_SET);

    SceUID dest_fd = 0;
    if (R_FAILED(dest_fd = pspIoOpenFile(dest_path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777))) {
        utilsLogError("%s: pspIoOpenFile(%s) failed: 0x%x\n", __func__, dest_path, dest_fd);
        pspIoCloseFile(src_fd);
        return dest_fd;
    }

    u8 *buf = malloc(buf_size);
    if (buf == NULL) {
        return -3;
    }
    
    memset(buf, 0, buf_size);
    u64 offset = 0;

    do {
        int ret = 0, read = 0;
        if (R_FAILED(ret = read = pspIoReadFile(src_fd, buf, buf_size))) {
            utilsLogError("%s: pspIoReadFile(%s) failed: 0x%08x\n", __func__, src_path, ret);
            pspIoCloseFile(dest_fd);
            pspIoCloseFile(src_fd);
            free(buf);
            return ret;
        }

        offset += read;

        if (R_FAILED(ret = pspIoWriteFile(dest_fd, buf, read))) {
            utilsLogError("%s: pspIoWriteFile(%s) failed: 0x%08x\n", __func__, dest_path, ret);
            pspIoCloseFile(dest_fd);
            pspIoCloseFile(src_fd);
            free(buf);
            return ret;
        }

        if (param) {
            param->max = size;
            param->value = offset;

            if (param->setProgress) {
                param->setProgress(param->value ? param->value : 0, param->max);
            }

            if (param->cancelHandler && param->cancelHandler()) {
                pspIoCloseFile(dest_fd);
                pspIoCloseFile(src_fd);
                free(buf);
                return 0;
            }
        }
    } while (offset < size);

    pspIoCloseFile(dest_fd);
    pspIoCloseFile(src_fd);
    free(buf);
    return 1;
}

int fsCopyPath(const char *src_path, const char *dest_path, FileProcessParam *param) {
    // The source and destination paths are identical
    if (strcasecmp(src_path, dest_path) == 0) {
        return -1;
    }

    // The destination is a subfolder of the source folder
    size_t len = strlen(src_path);
    if (strncasecmp(src_path, dest_path, len) == 0 && (dest_path[len] == '/' || dest_path[len - 1] == '/')) {
        return -2;
    }

    SceUID dfd = pspIoOpenDir(src_path);
    if (dfd >= 0) {
        int ret = 0;
        if (R_FAILED(ret = pspIoMakeDir(dest_path, 0777))) {
            utilsLogError("%s: pspIoMakeDir(%s) failed: 0x%08x\n", __func__, dest_path, ret);
            pspIoCloseDir(dfd);
            return ret;
        }

        if (param) {
            // if (param->value) {
            //     (param->value)++;
            // }

            // if (param->setProgress) {
            //     param->setProgress(param->value ? param->value : 0, param->max);
            // }

            if (param->cancelHandler && param->cancelHandler()) {
                pspIoCloseDir(dfd);
                return 0;
            }
        }

        int res = 0;

        do {
            SceIoDirent dir;
            memset(&dir, 0, sizeof(SceIoDirent));

            res = pspIoReadDir(dfd, &dir);
            if (res > 0) {
                if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) {
                    continue;
                }

                len = strlen(src_path) + strlen(dir.d_name) + 2;
                char new_src_path[len];
                memset(new_src_path, 0, len);
                snprintf(new_src_path, len, "%s%s%s", src_path, fsHasEndSlash(src_path) ? "" : "/", dir.d_name);

                len = strlen(dest_path) + strlen(dir.d_name) + 2;
                char new_dest_path[len];
                memset(new_dest_path, 0, len);
                snprintf(new_dest_path, len, "%s%s%s", dest_path, fsHasEndSlash(dest_path) ? "" : "/", dir.d_name);

                if (FIO_S_ISDIR(dir.d_stat.st_mode)) {
                    ret = fsCopyPath(new_src_path, new_dest_path, param);
                }
                else {
                    ret = fsCopyFile(new_src_path, new_dest_path, param);
                }

                if (ret <= 0) {
                    pspIoCloseDir(dfd);
                    return ret;
                }
            }
        } while (res > 0);

        pspIoCloseDir(dfd);
    }
    else {
        return fsCopyFile(src_path, dest_path, param);
    }

    return 1;
}

int fsMovePath(const char *src_path, const char *dest_path) {
    int ret = 0;
    size_t i = 0;
    char strage[32] = { 0 };
    char *p1 = NULL, *p2 = NULL;

    p1 = strchr(src_path, ':');
    if (p1 == NULL) {
        return -1;
    }
    
    p2 = strchr(dest_path, ':');
    if (p2 == NULL) {
        return -2;
    }
    
    if ((p1 - src_path) != (p2 - dest_path)) {
        return -3;
    }
    
    for (i = 0; (src_path + i) <= p1; i++) {
        if ((i+1) >= sizeof(strage)) {
            return -4;
        }
        
        if (src_path[i] != dest_path[i]) {
            return -5;
        }
        
        strage[i] = src_path[i];
    }
    
    strage[i] = '\0';
    
    u32 data[2] = { 0 };
    data[0] = (u32)(p1 + 1);
    data[1] = (u32)(p2 + 1);
    
    if (R_FAILED(ret = pspIoDevctl(strage, 0x02415830, &data, sizeof(data), NULL, 0))) {
        utilsLogError("%s: pspIoDevctl(%s, %s) failed: 0x%08x\n", __func__, src_path, dest_path, ret);
        return ret;
    }

    return 0;
}

int fsDeletePath(const char *path, FileProcessParam *param) {
    SceUID dfd = pspIoOpenDir(path);
    if (dfd >= 0) {
        int res = 0;
        
        do {
            SceIoDirent dir;
            memset(&dir, 0, sizeof(SceIoDirent));
            
            res = pspIoReadDir(dfd, &dir);
            if (res > 0) {
                if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) {
                    continue;
                }
                
                char new_path[512] = { 0 };
                snprintf(new_path, 512, "%s%s%s", path, fsHasEndSlash(path) ? "" : "/", dir.d_name);
                
                if (FIO_S_ISDIR(dir.d_stat.st_mode)) {
                    int ret = fsDeletePath(new_path, param);
                    if (ret <= 0) {
                        utilsLogError("%s: fsDeletePath(%s) failed: 0x%08x\n", __func__, new_path, ret);
                        pspIoCloseDir(dfd);
                        return ret;
                    }
                } else {
                    int ret = pspIoRemoveFile(new_path);
                    if (ret < 0) {
                        utilsLogError("%s: pspIoRemoveFile(%s) failed: 0x%08x\n", __func__, new_path, ret);
                        pspIoCloseDir(dfd);
                        return ret;
                    }
                    
                    if (param) {
                        if (param->value) {
                            (param->value)++;
                        }

                        if (param->setProgress) {
                            param->setProgress(param->value ? param->value : 0, param->max);
                        }
                        
                        if (param->cancelHandler && param->cancelHandler()) {
                            pspIoCloseDir(dfd);
                            return 0;
                        }
                    }
                }
            }
        } while (res > 0);
        
        pspIoCloseDir(dfd);
        
        int ret = pspIoRemoveDir(path);
        if (ret < 0) {
            utilsLogError("%s: pspIoRemoveDir(%s) failed: 0x%08x\n", __func__, path, ret);
            return ret;
        }
        
        if (param) {
            if (param->value) {
                (param->value)++;
            }
            
            if (param->setProgress) {
                param->setProgress(param->value ? param->value : 0, param->max);
            }
            
            if (param->cancelHandler && param->cancelHandler()) {
                return 0;
            }
        }
    } else {
        int ret = pspIoRemoveFile(path);
        if (ret < 0) {
            utilsLogError("%s: pspIoRemoveFile(%s) failed: 0x%08x\n", __func__, path, ret);
            return ret;
        }
        
        if (param) {
            if (param->value) {
                (param->value)++;
            }

            if (param->setProgress) {
                param->setProgress(param->value ? param->value : 0, param->max);
            }
            
            if (param->cancelHandler && param->cancelHandler()) {
                return 0;
            }
        }
    }
    
    return 1;
}

bool fsFileExists(const char *path) {
    SceIoStat stat;
    return sceIoGetstat(path, &stat) >= 0;
}

SceSize fsGetFileSize(const char *path) {
    int ret = 0;
    
    SceIoStat stat;
    memset(&stat, 0, sizeof(stat));
    
    if (R_FAILED(ret = sceIoGetstat(path, &stat))) {
        return ret;
    }
    
    return stat.st_size;
}

int fsReadFile(const char *path, void *buf, SceSize size) {
    SceUID file = 0;
    
    if (R_FAILED(file = sceIoOpen(path, PSP_O_RDONLY, 0))) {
        return file;
    }
    
    int bytesRead = sceIoRead(file, buf, size);
    sceIoClose(file);
    return bytesRead;
}
    

int fsWriteFile(const char *path, void *buf, SceSize size) {	
    SceUID file = 0;

    if (R_FAILED(file = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777))) {
        return file;
    }
    
    int bytesWritten = sceIoWrite(file, buf, size);
    sceIoClose(file);
    return bytesWritten;
}
