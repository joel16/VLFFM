#include <pspkernel.h>
#include <stdbool.h>
#include <string.h>

#include "config.h"
#include "gui.h"
#include "utils.h"
#include "vlf.h"

PSP_MODULE_INFO("VLFFM", PSP_MODULE_USER, VERSION_MAJOR, VERSION_MINOR);
PSP_MAIN_THREAD_ATTR(0);

static bool running = true;

int app_main(int argc, char *argv[]) {
    PSP_KEY_ENTER = utilsGetEnterButton();
    PSP_KEY_CANCEL = utilsGetCancelButton();

    configLoad();
    vlfGuiSystemSetup(1, 1, 1);
    guiInit();
    
    while (running) {
        vlfGuiDrawFrame();
    }

    sceKernelExitGame();
    return 0;
}
