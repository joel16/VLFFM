#include <pspkernel.h>
#include <pspreg.h>
#include <psputility_sysparam.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"

int PSP_KEY_ENTER, PSP_KEY_CANCEL;
char g_err_string[1024];

void utilsLogError(const char *error, ...) {
    va_list list;
    va_start(list, error);
    vsprintf(g_err_string, error, list);
    va_end(list);
    printf(g_err_string);
}

static int utilsGetRegistryValue(const char *dir, const char *name, unsigned int *value) {
    int ret = 0;
    struct RegParam reg_param;
    REGHANDLE reg_handle = 0, reg_handle_cat = 0, reg_handle_key = 0;
    unsigned int type = 0, size = 0;
    
    memset(&reg_param, 0, sizeof(struct RegParam));
    reg_param.regtype = 1;
    reg_param.namelen = strlen("/system");
    reg_param.unk2 = 1;
    reg_param.unk3 = 1;
    strcpy(reg_param.name, "/system");
    
    if (R_FAILED(ret = sceRegOpenRegistry(&reg_param, 2, &reg_handle))) {
        utilsLogError("sceRegOpenRegistry() failed: 0x%08x\n", ret);
        return ret;
    }
    
    if (R_FAILED(ret = sceRegOpenCategory(reg_handle, dir, 2, &reg_handle_cat))) {
        sceRegCloseRegistry(reg_handle);
        utilsLogError("sceRegOpenCategory() failed: 0x%08x\n", ret);
        return ret;
    }
    
    if (R_FAILED(ret = sceRegGetKeyInfo(reg_handle_cat, name, &reg_handle_key, &type, &size))) {
        sceRegCloseCategory(reg_handle_cat);
        sceRegCloseRegistry(reg_handle);
        utilsLogError("sceRegGetKeyInfo() failed: 0x%08x\n", ret);
        return ret;
    }
    
    if (R_FAILED(ret = sceRegGetKeyValue(reg_handle_cat, reg_handle_key, value, 4))) {
        sceRegCloseCategory(reg_handle_cat);
        sceRegCloseRegistry(reg_handle);
        utilsLogError("sceRegGetKeyValue() failed: 0x%08x\n", ret);
        return ret;
    }
    
    if (R_FAILED(ret = sceRegFlushCategory(reg_handle_cat))) {
        sceRegCloseCategory(reg_handle_cat);
        sceRegCloseRegistry(reg_handle);
        utilsLogError("sceRegFlushCategory() failed: 0x%08x\n", ret);
        return ret;
    }
    
    if (R_FAILED(ret = sceRegCloseCategory(reg_handle_cat))) {
        sceRegCloseRegistry(reg_handle);
        utilsLogError("sceRegCloseCategory() failed: 0x%08x\n", ret);
        return ret;
    }
    
    if (R_FAILED(ret = sceRegFlushRegistry(reg_handle))) {
        sceRegCloseRegistry(reg_handle);
        utilsLogError("sceRegFlushRegistry() failed: 0x%08x\n", ret);
        return ret;
    }
    
    if (R_FAILED(ret = sceRegCloseRegistry(reg_handle))) {
        utilsLogError("sceRegFlushRegistry() failed: 0x%08x\n", ret);
        return ret;
    }
    
    return 0;
}

int utilsGetEnterButton(void) {
    int ret = 0, button = -1;
    
    if (R_FAILED(ret = sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN, &button))) {
        utilsLogError("sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN) failed: 0x%08x\n", ret);
        unsigned int reg_button = -1;

        if (R_SUCCEEDED(utilsGetRegistryValue("/CONFIG/SYSTEM/XMB", "button_assign", &reg_button))) {
            if (reg_button == 0) {
                return PSP_CTRL_CIRCLE;
            }
            
            return PSP_CTRL_CROSS;
        }
    }
    
    if (button == 0) {
        return PSP_CTRL_CIRCLE;
    }
    
    return PSP_CTRL_CROSS;
}

int utilsGetCancelButton(void) {
    int ret = 0, button = -1;
    
    if (R_FAILED(ret = sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN, &button))) {
        utilsLogError("sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN) failed: 0x%08x\n", ret);
        unsigned int reg_button = -1;
        
        if (R_SUCCEEDED(utilsGetRegistryValue("/CONFIG/SYSTEM/XMB", "button_assign", &reg_button))) {
            if (reg_button == 0) {
                return PSP_CTRL_CROSS;
            }
            
            return PSP_CTRL_CIRCLE;
        }
    }
    
    if (button == 0) {
        return PSP_CTRL_CROSS;
    }
    
    return PSP_CTRL_CIRCLE;
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
