#include <pspctrl.h>
#include <pspiofilemgr.h>
#include <pspkerneltypes.h>
#include <pspumd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "fs.h"
#include "gui.h"
#include "utils.h"
#include "vlf.h"

#define NUM_GUI_CONTEXT_MENU_ITEMS 4
#define NUM_GUI_MAIN_MENU_ITEMS    6

enum GuiContextMenuItems {
    GUI_CONTEXT_MENU_PROPERTIES,
    GUI_CONTEXT_MENU_COPY,
    GUI_CONTEXT_MENU_MOVE,
    GUI_CONTEXT_MENU_DELETE
};

enum GuiCopyActionFlags {
    GUI_COPY_ACTION_NONE,
    GUI_COPY_ACTION_COPY_PATH,
    GUI_COPY_ACTION_MOVE_PATH
};

enum GuiMainMenuList {
    GUI_DEVICE_MS0,
    GUI_DEVICE_FLASH0,
    GUI_DEVICE_FLASH1,
    GUI_DEVICE_FLASH2,
    GUI_DEVICE_FLASH3,
    GUI_DEVICE_DISC0,
};

typedef struct {
    bool enabled;
    u8 copy_flag;
    char filename[FS_MAX_PATH];
    char path[FS_MAX_PATH];
} GuiContextMenu;

typedef struct {
    int start;
    int selected;
    GuiContextMenu context_menu;
    VlfText text[11];
    VlfText cwd;
    VlfScrollBar scrollbar;
    VlfProgressBar progressbar;
    FsFileListEntry *entry;
} GuiFileList;

extern unsigned char backgrounds_bmp_start[];

static const int start_y = 50, max_entries = 11, scrollbar_height = 222, max_background_number = 35;
static GuiFileList gui = { 0 };
static bool file_op_flag = false;

static void guiDisplayMainMenu(void);
static void guiClearEventHandlers(void);

static void guiSetTitle(char *fmt, ...) {
    char text[32] = { 0 };
    va_list list;
    VlfText title_text = NULL;
    VlfPicture title_pic = NULL;
    
    va_start(list, fmt);
    vsnprintf(text, 32, fmt, list);
    va_end(list);
    
    if (title_text != NULL) {
        vlfGuiRemoveText(title_text);
    }
        
    if (title_pic != NULL) {
        vlfGuiRemovePicture(title_pic);
    }
        
    title_text = vlfGuiAddText(0, 0, text);
    title_pic = vlfGuiAddPictureResource("dd_helper.rco", "tex_folder", 4, -1);
    vlfGuiSetTitleBarEx(title_text, NULL, 1, 0, config.background_number);
}

static void guiSetBackground(void) {
    if (config.background_number < 0) {
        config.background_number = max_background_number;
    }
    else if (config.background_number > max_background_number) {
        config.background_number = 0;
    }
    
    vlfGuiSetBackgroundFileBuffer(backgrounds_bmp_start + config.background_number * 6176, 6176, 1);
}

int guiIncrementBackgroundNumber(void *param) {
    config.background_number++;
    guiSetBackground();
    configSave();
    return VLF_EV_RET_NOTHING;
}

int guiDecrementBackgroundNumber(void *param) {
    config.background_number--;
    guiSetBackground();
    configSave();
    return VLF_EV_RET_NOTHING;
}

static void guiSetScrollbar(void) {
    if (gui.scrollbar == NULL) {
        gui.scrollbar = vlfGuiAddScrollBar(475, start_y, scrollbar_height, ((((float)max_entries/(float)g_file_list.length) * (float)scrollbar_height)));
    }
}

static void guiSetSecondaryTitle(const char *title) {
    if (gui.cwd != NULL) {
        vlfGuiRemoveText(gui.cwd);
    }
    
    gui.cwd = vlfGuiAddTextF(240, 25, strlen(title) > 45? "%.45s..." : "%s", title);
    vlfGuiSetTextAlignment(gui.cwd, VLF_ALIGNMENT_CENTER);
}

static void guiDisplayErrorDialog(void) {
    vlfGuiMessageDialog(g_err_string, VLF_MD_BUTTONS_NONE);
    memset(g_err_string, 0, 1024);
}

static void guiDisplayFileBrowser(bool init) {
    const int sel_dist = 20, height = 20;

    if (init) {
        gui.entry = g_file_list.head;
    }

    guiSetSecondaryTitle(g_cwd);
    const int max = g_file_list.length > max_entries? max_entries : g_file_list.length;

    for (int i = gui.start, counter = 0; i < max || counter < max; i++, counter++) {
        gui.text[counter] = vlfGuiAddTextF(15, start_y + ((sel_dist - height) / 2) + (i - gui.start) * sel_dist,
            strlen(gui.entry->name) > 45? "%.45s..." : "%s%s", gui.entry->name, gui.entry->is_folder? "/" : "");
        gui.entry = gui.entry->next;
    }

    if (init) {
        vlfGuiSetTextFocus(gui.text[gui.selected]);

        if (g_file_list.length > max_entries) {
            guiSetScrollbar();
        }
    }
}

static void guiClearFileList(void) {
    for (int i = 0; i < max_entries; i++) {
        if (gui.text[i] != NULL) {
            vlfGuiRemoveText(gui.text[i]);
            gui.text[i] = NULL;
        }
    }
}

static void guiClearScrollbar(void) {
    if (gui.scrollbar != NULL) {
        vlfGuiRemoveScrollBar(gui.scrollbar);
        gui.scrollbar = NULL;
    }
}

static void guiClearSelection(void) {
    guiClearScrollbar();
    gui.start = 0;
    gui.selected = 0;
}

static void guiGetEntry(void) {
    gui.entry = g_file_list.head;
    
    for (int i = 0; i < gui.selected; i++) {
        gui.entry = gui.entry->next;
    }
}

static void guiRefreshFileList(bool init) {
    guiClearFileList();
    gui.entry = g_file_list.head;
    
    for (int i = 0; i < gui.start; i++) {
        gui.entry = gui.entry->next;
    }

    if (gui.scrollbar != NULL) {
        vlfGuiMoveScrollBarSlider(gui.scrollbar, (((float)gui.start/(float)g_file_list.length) * (float)scrollbar_height));
    }

    guiDisplayFileBrowser(init);
    vlfGuiDrawFrame();
}

static void guiDisplayProperties(void) {
    char message[512] = { 0 };
    char size[16] = { 0 };

    utilsGetSizeString(size, gui.entry->size);
    snprintf(message, 512, "Properties:\n\nName: %s\nSize: %s\nModified: %d/%d/%d %02d:%02d\n",
        gui.entry->name,
        gui.entry->is_folder? "-" : size,
        gui.entry->time.year, gui.entry->time.month, gui.entry->time.day, gui.entry->time.hour, gui.entry->time.minute
    );

    vlfGuiMessageDialog(message, VLF_MD_BUTTONS_NONE);
}

static void guiFileProcessSetProgress(int value, int max) {
    if (gui.progressbar == NULL) {
        gui.progressbar = vlfGuiAddProgressBar(136);
    }

    if (gui.progressbar != NULL) {
        vlfGuiProgressBarSetProgress(gui.progressbar, (float)value/(float)max * 100.f);
        vlfGuiDrawFrame();
    }
}

static void guiSetCopyFlag(u8 value) {
    if (value > 0) {
        gui.context_menu.copy_flag = value;
    }

    snprintf(gui.context_menu.filename, FS_MAX_PATH, gui.entry->name);
    snprintf(gui.context_menu.path, FS_MAX_PATH, fsSetPath(gui.entry, NULL));
    
    gui.context_menu.enabled = false;
    vlfGuiCancelLateralMenu();
}

static void guiResetContextMenu(u8 *flag) {
    if (flag) {
        *flag = GUI_COPY_ACTION_NONE;
    }

    file_op_flag = false;
    memset(gui.context_menu.filename, 0, FS_MAX_PATH);
    memset(gui.context_menu.path, 0, FS_MAX_PATH);
    
    if (gui.progressbar != NULL) {
        vlfGuiRemoveProgressBar(gui.progressbar);
        gui.progressbar = NULL;
    }

    guiGetEntry();
    fsFreeDirectoryEntries(&g_file_list);
    fsGetDirectoryEntries(&g_file_list, g_cwd);
    guiClearSelection();
    guiRefreshFileList(true);
}

static int guiControlContextMenuSelection(int selection) {
    switch (selection) {
        case GUI_CONTEXT_MENU_PROPERTIES:
            guiDisplayProperties();
            break;

        case GUI_CONTEXT_MENU_COPY:
            if (gui.context_menu.copy_flag == GUI_COPY_ACTION_NONE) {
                guiSetCopyFlag(GUI_COPY_ACTION_COPY_PATH);
            }
            else {
                file_op_flag = true;
                FileProcessParam file_process_param = { 0 };
                file_process_param.setProgress = guiFileProcessSetProgress;

                gui.context_menu.enabled = false;
                vlfGuiRemoveTextFocus(gui.text[gui.selected - gui.start], 1);
                vlfGuiCancelLateralMenu();
                vlfGuiSetRectangleVisibility(0, 50, 480, 222, 0);
                
                char progress_message[512];
                snprintf(progress_message, 512, "Copying: %s", gui.context_menu.filename);
                VlfText progress_text = vlfGuiAddText(240, 110, progress_message);
                vlfGuiSetTextAlignment(progress_text, VLF_ALIGNMENT_CENTER);
                
                if (R_FAILED(fsCopyPath(gui.context_menu.path, fsSetPath(gui.entry, gui.context_menu.filename), &file_process_param))) {
                    guiDisplayErrorDialog();
                }

                if (progress_text != NULL) {
                    vlfGuiRemoveText(progress_text);
                    progress_text = NULL;
                }

                vlfGuiSetRectangleVisibility(0, 50, 480, 222, 1);
                guiResetContextMenu(&gui.context_menu.copy_flag);
            }
            break;

        case GUI_CONTEXT_MENU_MOVE:
            if (gui.context_menu.copy_flag == GUI_COPY_ACTION_NONE) {
                guiSetCopyFlag(GUI_COPY_ACTION_MOVE_PATH);
            }
            else {
                gui.context_menu.enabled = false;
                vlfGuiRemoveTextFocus(gui.text[gui.selected - gui.start], 1);
                vlfGuiCancelLateralMenu();
                
                if (R_FAILED(fsMovePath(gui.context_menu.path, fsSetPath(gui.entry, gui.context_menu.filename)))) {
                    guiDisplayErrorDialog();
                }

                guiResetContextMenu(&gui.context_menu.copy_flag);
            }
            break;

        case GUI_CONTEXT_MENU_DELETE:
            char message[512] = { 0 };
            snprintf(message, 512, "This action cannot be undone. Do you wish to delete %s?", gui.entry->name);
            int button_res = vlfGuiMessageDialog(message, VLF_MD_BUTTONS_YESNO | VLF_MD_INITIAL_CURSOR_NO);

            if (button_res == VLF_MD_YES) {
                file_op_flag = true;
                if (R_FAILED(fsDelete(fsSetPath(gui.entry, NULL), NULL))) {
                    guiDisplayErrorDialog();
                }
                
                gui.context_menu.enabled = false;
                vlfGuiRemoveTextFocus(gui.text[gui.selected - gui.start], 1);
                guiResetContextMenu(NULL);
                vlfGuiCancelLateralMenu();
            }
            break;
    }

    return VLF_EV_RET_NOTHING;
}

static void guiDisplayContextMenu(void) {
    const char *item_labels[NUM_GUI_CONTEXT_MENU_ITEMS] = { 
        "Properties",
        gui.context_menu.copy_flag == 1? "Paste" : "Copy",
        gui.context_menu.copy_flag == 2? "Paste" : "Move",
        "Delete"
    };

    vlfGuiLateralMenuEx(NUM_GUI_CONTEXT_MENU_ITEMS, item_labels, 0, guiControlContextMenuSelection, 120, config.background_number);
    vlfGuiDrawFrame();
}

static int guiControlFileBrowserUp(void *param) {
    if (file_op_flag) {
        return VLF_EV_RET_NOTHING;
    }

    vlfGuiRemoveTextFocus(gui.text[gui.selected - gui.start], 1);
    gui.selected--;
    
    if (gui.selected < 0) {
        gui.selected = (g_file_list.length - 1);
    }
    
    if ((g_file_list.length - 1) < max_entries) {
        gui.start = 0;
        guiRefreshFileList(false);
    }
    else if (gui.start > gui.selected) {
        gui.start--;
        guiRefreshFileList(false);
    }
    else if ((gui.selected == (g_file_list.length - 1)) && ((g_file_list.length - 1) > (max_entries - 1))) {
        gui.start = (g_file_list.length - 1) - (max_entries - 1);
        guiRefreshFileList(false);
    }

    vlfGuiSetTextFocus(gui.text[gui.selected - gui.start]);
    return VLF_EV_RET_NOTHING;
}

static int guiControlFileBrowserDown(void *param) {
    if (file_op_flag) {
        return VLF_EV_RET_NOTHING;
    }

    vlfGuiRemoveTextFocus(gui.text[gui.selected - gui.start], 1);
    gui.selected++;
    
    if (gui.selected > (g_file_list.length - 1)) {
        gui.selected = 0;
    }
    
    if ((gui.selected > (gui.start + (max_entries - 1))) && ((gui.start + (max_entries - 1)) < (g_file_list.length - 1))) {
        gui.start++;
        guiRefreshFileList(false);
    }
    
    if (gui.selected == 0) {
        gui.start = 0;
        guiRefreshFileList(false);
    }

    vlfGuiSetTextFocus(gui.text[gui.selected - gui.start]);
    return VLF_EV_RET_NOTHING;
}

static int guiControlContextMenu(void *param) {
    if (file_op_flag) {
        return VLF_EV_RET_NOTHING;
    }

    gui.context_menu.enabled = !gui.context_menu.enabled;

    if (gui.context_menu.enabled) {
        guiGetEntry();
        guiClearScrollbar();
        guiDisplayContextMenu();
    }
    else {
        vlfGuiCancelLateralMenu();
        guiSetScrollbar();
    }

    return VLF_EV_RET_NOTHING;
}

static int guiControlFileBrowserEnter(void *param) {
    if (file_op_flag) {
        return VLF_EV_RET_NOTHING;
    }

    guiGetEntry();

    if (!gui.entry->is_folder) {
        return VLF_EV_RET_NOTHING;
    }

    vlfGuiRemoveTextFocus(gui.text[gui.selected - gui.start], 1);

    if (strncasecmp(gui.entry->name, "..", 2) == 0) {
        if (R_FAILED(fsParentDir(&g_file_list))) {
            guiDisplayErrorDialog();
        }
    }
    else if (gui.entry->is_folder) {
        if (R_FAILED(fsAppendDir(&g_file_list, gui.entry->name))) {
            guiDisplayErrorDialog();
        }
    }
    
    guiClearSelection();
    guiRefreshFileList(true);
    return VLF_EV_RET_NOTHING;
}

static int guiControlFileBrowserCancel(void *param) {
    if (file_op_flag) {
        return VLF_EV_RET_NOTHING;
    }

    if (gui.context_menu.enabled) {
        gui.context_menu.enabled = false;
        vlfGuiCancelLateralMenu();
        guiSetScrollbar();
        return VLF_EV_RET_NOTHING;
    }

    vlfGuiRemoveTextFocus(gui.text[gui.selected - gui.start], 1);
    guiGetEntry();
    int ret = 0;
    
    if (R_FAILED(ret = fsParentDir(&g_file_list))) {
        guiDisplayErrorDialog();
    }

    guiClearSelection();

    // Go back to device selection
    if (ret == 1) {
        guiClearFileList();
        guiClearEventHandlers();
        guiDisplayMainMenu();
        return VLF_EV_RET_NOTHING;
    }
    
    guiRefreshFileList(true);
    return VLF_EV_RET_NOTHING;
}

static void guiClearEventHandlers(void) {
    vlfGuiRemoveEventHandler(guiControlFileBrowserUp);
    vlfGuiRemoveEventHandler(guiControlFileBrowserDown);
    vlfGuiRemoveEventHandler(guiControlContextMenu);
    vlfGuiRemoveEventHandler(guiControlFileBrowserEnter);
    vlfGuiRemoveEventHandler(guiControlFileBrowserCancel);
}

static void guiControlFileBrowser(void) {
    vlfGuiAddEventHandler(PSP_CTRL_UP, -1, guiControlFileBrowserUp, NULL);
    vlfGuiAddEventHandler(PSP_CTRL_DOWN, -1, guiControlFileBrowserDown, NULL);
    vlfGuiAddEventHandler(PSP_CTRL_TRIANGLE, -1, guiControlContextMenu, NULL);
    vlfGuiAddEventHandler(PSP_KEY_ENTER, -1, guiControlFileBrowserEnter, NULL);
    vlfGuiAddEventHandler(PSP_KEY_CANCEL, -1, guiControlFileBrowserCancel, NULL);
}

static int guiControlMainMenu(int selection) {
    int ret = 0;
    fsFreeDirectoryEntries(&g_file_list);

    switch (selection) {
        case GUI_DEVICE_MS0:
            strncpy(g_cwd, "ms0:/", 6);
            break;

        case GUI_DEVICE_FLASH0:
            ret = utilsSetDevice("flash0:", "lflash0:0,0", "flashfat0:", g_cwd);
            break;
        
        case GUI_DEVICE_FLASH1:
            ret = utilsSetDevice("flash1:", "lflash0:0,1", "flashfat1:", g_cwd);
            break;
        
        case GUI_DEVICE_FLASH2:
            ret = utilsSetDevice("flash2:", "lflash0:0,2", "flashfat2:", g_cwd);
            break;
        
        case GUI_DEVICE_FLASH3:
            ret = utilsSetDevice("flash3:", "lflash0:0,3", "flashfat3:", g_cwd);
            break;

        case GUI_DEVICE_DISC0:
            ret = -1;

            if (sceUmdCheckMedium() != 0) {
                if (R_FAILED(ret = sceUmdActivate(1, "disc0:"))) {
                    utilsLogError("sceUmdActivate(disc0) failed: 0x%x\n", ret);
                }
                
                if (R_FAILED(ret = sceUmdWaitDriveStat(PSP_UMD_READY))) {
                    utilsLogError("sceUmdWaitDriveStat() failed: 0x%x\n", ret);
                }

                strncpy(g_cwd, "disc0:/", 8);
            }
            else {
                utilsLogError("UMD not present.\nPlease insert a UMD into the disc drive.");
            }

            break;

    }

    if (R_SUCCEEDED(ret)) {
        vlfGuiCancelCentralMenu();
        memset(&g_file_list, 0, sizeof(FsFileList));
        fsGetDirectoryEntries(&g_file_list, g_cwd);
        guiDisplayFileBrowser(true);
        guiControlFileBrowser();
    }
    else {
        guiDisplayErrorDialog();
        ret = 0;
    }
    
    return 0;
}

static void guiDisplayMainMenu(void) {
    const char *item_labels[NUM_GUI_MAIN_MENU_ITEMS] = { "ms0:/", "flash0:/", "flash1:/", "flash2:/", "flash3:/", "disc0:/" };
    
    guiSetSecondaryTitle("Devices");
    vlfGuiCentralMenu(NUM_GUI_MAIN_MENU_ITEMS, item_labels, 0, guiControlMainMenu, 0, 0);
}

void guiInit(void) {
    guiSetTitle("VLFFM v%d.%d", VERSION_MAJOR, VERSION_MINOR);
    vlfGuiAddEventHandler(PSP_CTRL_RTRIGGER, -1, guiIncrementBackgroundNumber, NULL);
    vlfGuiAddEventHandler(PSP_CTRL_LTRIGGER, -1, guiDecrementBackgroundNumber, NULL);
    guiSetBackground();
    guiDisplayMainMenu();
}
