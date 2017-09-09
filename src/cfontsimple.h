#define FONTDATA_WIDTH  192
#define FONTDATA_HEIGHT 84
#define GLYPH_WIDTH     10
#define GLYPH_HEIGHT    12
#define MISSILE_CHAR    127
#define MISSILE_WIDTH   6
#define MISSILE_HEIGHT  12
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG 
#include "stb_image.h"

//    int x,y,n;
//    unsigned char *data = stbi_load(filename, &x, &y, &n, 0);
//    // ... process data if not NULL ...
//    // ... x = width, y = height, n = # 8-bit components per pixel ...
//    // ... replace '0' with '1'..'4' to force that many components per pixel
//    // ... but 'n' will always be the number that it would have been if you said 0
//    stbi_image_free(data)

unsigned char *fontdata;
int fontdata_x;
int fontdata_y;
int fontdata_n;

BOOL font_initialized = FALSE;

BOOL init_font(const char* filename)
{
    fontdata = stbi_load(filename,&fontdata_x,&fontdata_y,&fontdata_n,1);

    if(fontdata == NULL)
        return FALSE;

    if(fontdata_x != FONTDATA_WIDTH || fontdata_y != FONTDATA_HEIGHT)
        return FALSE;

    font_initialized = TRUE;

	return TRUE;
}

void free_font()
{
	stbi_image_free(fontdata);
}

void draw_char(const char c, int x, int y, char color)
{
    if (!font_initialized)
        return;

    unsigned char* dst = back_buffer;
    dst = dst + (buffer_width*y) + x;

    int relative_char_position = c - 32;

    int src_x = (relative_char_position % 16)*(GLYPH_WIDTH + 2);
    int src_y = (relative_char_position / 16)*(GLYPH_HEIGHT + 2);

	unsigned char* src = fontdata;
	src = src + (FONTDATA_WIDTH*src_y) + src_x;

    for(int i = 0; i < GLYPH_HEIGHT; ++i)
	{
		for (int j = 0; j < GLYPH_WIDTH; ++j)
		{
			if (dst < back_buffer || dst >(unsigned char*)back_buffer + (buffer_width*buffer_height))
				return;

			if(*src > 0)
				*dst = color;
			++dst; ++src;
		}

        src+=(FONTDATA_WIDTH - GLYPH_WIDTH);
		dst+=(buffer_width - GLYPH_WIDTH);
    }
}

static void draw_string(char *s, int x, int y, char color)
{
	char* p = s;

	while (*p)
	{
		draw_char(*p++, x, y, color);
		x += GLYPH_WIDTH + 1;
	}
}
