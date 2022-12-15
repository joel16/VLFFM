#pragma once

#include <pspctrl.h>
#include <psptypes.h>

/// Checks whether a result code indicates success.
#define R_SUCCEEDED(res) ((res) >= 0)
/// Checks whether a result code indicates failure.
#define R_FAILED(res)    ((res) < 0)

extern int PSP_KEY_ENTER, PSP_KEY_CANCEL;
extern char g_err_string[1024];

void utilsLogError(const char *error, ...);
int utilsGetEnterButton(void);
int utilsGetCancelButton(void);
void utilsGetSizeString(char *string, int size);
