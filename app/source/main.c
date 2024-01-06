#include <pspkernel.h>
#include <stdbool.h>
#include <string.h>

#include "config.h"
#include "gui.h"
#include "utils.h"
#include "vlf.h"

PSP_MODULE_INFO("VLFFM", PSP_MODULE_USER, VERSION_MAJOR, VERSION_MINOR);
PSP_MAIN_THREAD_ATTR(0);

int app_main(int argc, char *argv[]) {
    configLoad();
    vlfGuiSystemSetup(1, 1, 1);
    guiInit();
    
    while (true) {
        vlfGuiDrawFrame();
    }

    sceKernelExitGame();
    return 0;
}
