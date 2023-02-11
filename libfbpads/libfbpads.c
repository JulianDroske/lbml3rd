#include "libfbpads.h"

#include "stdarg.h"
#include "string.h"
#include "conf.h"
#include "fbpad.h"
#include "draw.h"
#include "vtconsole.h"


vtconsole_t* vtc_instance = NULL;
struct term *term_instance = NULL;

void vtc_paint(vtconsole_t *vtc, vtcell_t *cell, int x, int y){
	// TODO
	// pad_put();
	term_setfgcolor(term_clrmap(cell->attr.fg));
	term_setbgcolor(term_clrmap(cell->attr.bg));
	term_movecursor(y, x);
	term_insertchar(cell->c);
	// term_drawchar(cell->c, y, x);
	term_redraw(0);
}

void vtc_cursormove(vtconsole_t *vtc, vtcursor_t *cur){
	term_movecursor(cur->y, cur->x);
	term_redraw(0);
}

void libfbpads_print(char* str){
	vtconsole_write(vtc_instance, str, strlen(str));
}

void libfbpads_vprintf(char* fmt, va_list args){
	static char buf[0xff];
	vsnprintf(buf, 0xff, fmt, args);
	libfbpads_print(buf);
}

void libfbpads_printf(char* fmt, ...){
	va_list args;
	va_start(args, fmt);
	libfbpads_vprintf(fmt, args);
	va_end(args);
}

void libfbpads_puts(char* str){
	libfbpads_printf("%s\n", str);
}

int libfbpads_init(char* fbdev){
	if (fb_init(fbdev)) {
		// fprintf(stderr, "libfbpads: failed to initialize framebuffer\n");
		return 1;
	}
	if (pad_init()) {
		// fprintf(stderr, "fbpads: cannot load fonts\n");
		return 1;
	}
	term_instance = term_make();
	term_load(term_instance, 1);
	term_reset();
	term_redraw(1);
	vtc_instance = vtconsole(pad_cols(), pad_rows(), vtc_paint, vtc_cursormove);
	return 0;
}

int libfbpads_destroy(){
	if(vtc_instance){
		vtconsole_delete(vtc_instance);
		vtc_instance = NULL;
	}
	if(term_instance){
		term_free(term_instance);
		pad_free();
		fb_free();
		term_instance = NULL;
	}
}
