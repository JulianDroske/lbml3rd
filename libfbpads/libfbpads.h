#include "stdarg.h"
#include "fbpad.h"
#include "draw.h"

int libfbpads_init(char* fbdev);
int libfbpads_destroy();

void libfbpads_print(char* str);
void libfbpads_vprintf(char* fmt, va_list args);
void libfbpads_printf(char* fmt, ...);
void libfbpads_puts(char* str);
