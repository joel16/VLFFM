#pragma once

#include <psptypes.h>

/// Checks whether a result code indicates success.
#define R_SUCCEEDED(res) ((res) >= 0)
/// Checks whether a result code indicates failure.
#define R_FAILED(res)    ((res) < 0)

extern char g_err_string[1024];

void utilsLogError(const char *error, ...);
void utilsGetSizeString(char *string, int size);
int utilsSetDevice(const char *dev, const char *dev2, const char *dev3, char *dst);
