#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "fbpad.h"

struct font {
	int rows, cols;	/* glyph bitmap rows and columns */
	int n;		/* number of font glyphs */
	int *glyphs;	/* glyph unicode character codes */
	char *data;	/* glyph bitmaps */
};

/*
 * This tinyfont header is followed by:
 *
 * glyphs[n]	unicode character codes (int)
 * bitmaps[n]	character bitmaps (char[rows * cols])
 */
struct tinyfont {
	char sig[8];	/* tinyfont signature; "tinyfont" */
	int ver;	/* version; 0 */
	int n;		/* number of glyphs */
	int rows, cols;	/* glyph dimensions */
};

struct font *font_open_static(char *data)
{
	struct font *font;
	struct tinyfont head;
	head = *(struct tinyfont*) data;
	int offset = sizeof(head);
	font = malloc(sizeof(*font));
	font->n = head.n;
	font->rows = head.rows;
	font->cols = head.cols;
	font->glyphs = (int*)(data+offset);
	offset += font->n * sizeof(int);
	font->data = data+offset;
	offset += font->n * font->rows * font->cols;
	if (!font->glyphs || !font->data) {
		// font_free(font);
		return NULL;
	}
	return font;
}

static int find_glyph(struct font *font, int c)
{
	int l = 0;
	int h = font->n;
	while (l < h) {
		int m = (l + h) / 2;
		if (font->glyphs[m] == c)
			return m;
		if (c < font->glyphs[m])
			h = m;
		else
			l = m + 1;
	}
	return -1;
}

int font_bitmap(struct font *font, void *dst, int c)
{
	int i = find_glyph(font, c);
	int len = font->rows * font->cols;
	if (i < 0)
		return 1;
	memcpy(dst, font->data + i * len, len);
	return 0;
}

int font_rows(struct font *font)
{
	return font->rows;
}

int font_cols(struct font *font)
{
	return font->cols;
}
