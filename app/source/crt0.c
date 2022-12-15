#include <pspkernel.h>
#include <stdio.h>

#include "utils.h"
#include "vlf.h"

extern unsigned char fs_driver_prx_start[], intraFont_prx_start[], iop_prx_start[], vlf_prx_start[];
extern unsigned int fs_driver_prx_size, intraFont_prx_size, iop_prx_size, vlf_prx_size;

extern int app_main(int argc, char *argv[]);

int SetupCallbacks(void) {
    int CallbackThread(SceSize args, void *argp) {
        int exit_callback(int arg1, int arg2, void *common) {
            sceKernelExitGame();
            return 0;
        }
        
        int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
        sceKernelRegisterExitCallback(cbid);
        sceKernelSleepThreadCB();
        return 0;
    }
    
    SceUID thread = 0;
    if (R_SUCCEEDED(thread = sceKernelCreateThread("vlffm_callback_thread", CallbackThread, 0x11, 0xFA0, 0, 0))) {
        sceKernelStartThread(thread, 0, 0);
    }
        
    return thread;
}

void LoadStartModuleBuffer(const char *path, const void *buf, int size, SceSize args, void *argp) {
    SceUID mod, out;
    
    sceIoRemove(path);
    out = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT, 0777);
    sceIoWrite(out, buf, size);
    sceIoClose(out);
    
    mod = sceKernelLoadModule(path, 0, NULL);
    mod = sceKernelStartModule(mod, args, argp, NULL, NULL);
    sceIoRemove(path);
}

int start_thread(SceSize args, void *argp) {
    char *path = (char *)argp;
    int last_trail = -1;
    
    for(int i = 0; path[i]; i++) {
        if (path[i] == '/') {
            last_trail = i;
        }
    }
    
    if (last_trail >= 0) {
        path[last_trail] = 0;
    }
    
    sceIoChdir(path);
    path[last_trail] = '/';

    LoadStartModuleBuffer("fs_driver.prx", fs_driver_prx_start, fs_driver_prx_size, args, argp);
    LoadStartModuleBuffer("intraFont.prx", intraFont_prx_start, intraFont_prx_size, args, argp);
    LoadStartModuleBuffer("iop.prx", iop_prx_start, iop_prx_size, args, argp);
    LoadStartModuleBuffer("vlf.prx", vlf_prx_start, vlf_prx_size, args, argp);
    
    vlfGuiInit(20480, app_main);
    return sceKernelExitDeleteThread(0);
}

int module_start(SceSize args, void *argp) {
    SetupCallbacks();

    SceUID thread = 0;
    if (R_FAILED(thread = sceKernelCreateThread("vlffm_start_thread", start_thread, 0x10, 0x4000, 0, NULL))) {
        return thread;
    }
        
    sceKernelStartThread(thread, args, argp);
    return 0;
}
