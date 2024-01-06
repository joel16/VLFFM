#include <pspkernel.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"

char g_err_string[1024];

void utilsLogError(const char *error, ...) {
    va_list list;
    va_start(list, error);
    vsprintf(g_err_string, error, list);
    va_end(list);
    printf(g_err_string);
}

void utilsGetSizeString(char *string, int size) {
    int i = 0;
    const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
    
    while (size >= 1024.f) {
        size /= 1024.f;
        i++;
    }
    
    sprintf(string, "%.*f %s", (i == 0) ? 0 : 2, (double)size, units[i]);
}

int utilsSetDevice(const char *dev, const char *dev2, const char *dev3, char *dst) {
    int ret = 0;

    if ((R_FAILED(ret = sceIoUnassign(dev))) && (ret != SCE_KERNEL_ERROR_NODEV)) {
        utilsLogError("sceIoUnassign(%s) failed: 0x%x\n", dev, ret);
    }
    
    if (R_FAILED(ret = sceIoAssign(dev, dev2, dev3, IOASSIGN_RDWR, NULL, 0))) {
        utilsLogError("sceIoAssign(%s) failed: 0x%x\n", dev, ret);
    }
    
    snprintf(dst, 10, "%s/", dev);
    return ret;
}
