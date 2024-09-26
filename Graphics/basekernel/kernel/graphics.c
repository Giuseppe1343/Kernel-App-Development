/*
Copyright (C) 2016-2019 The University of Notre Dame
This software is distributed under the GNU General Public License.
See the file LICENSE for details.
*/

#include "graphics.h"
#include "kernel/types.h"
#include "kernel/error.h"
#include "ioports.h"
#include "font.h"
#include "string.h"
#include "kmalloc.h"
#include "bitmap.h"
#include "string.h"
#include "process.h"

#define FACTOR 256

struct graphics_clip {
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
};

struct graphics {
	struct bitmap *bitmap;
	struct graphics_color fgcolor;
	struct graphics_color bgcolor;
	struct graphics_clip clip;
	struct graphics *parent;
	int refcount;
};

static struct graphics_color color_black = { 0, 0, 0, 0 };
static struct graphics_color color_white = { 255, 255, 255, 0 };

struct graphics graphics_root;

struct graphics *graphics_create_root()
{
	struct graphics *g = &graphics_root;
	g->bitmap = bitmap_create_root();
	g->fgcolor = color_white;
	g->bgcolor = color_black;
	g->clip.x = 0;
	g->clip.y = 0;
	g->clip.w = g->bitmap->width;
	g->clip.h = g->bitmap->height;
	g->parent = 0;
	g->refcount = 1;
	return g;
}

struct graphics *graphics_create(struct graphics *parent )
{
	struct graphics *g = kmalloc(sizeof(*g));
	if(!g) return 0;

	memcpy(g, parent, sizeof(*g));

	g->parent = graphics_addref(parent);
	g->refcount = 1;

	return g;
}

struct graphics *graphics_addref( struct graphics *g )
{
	g->refcount++;
	return g;
}

void graphics_delete( struct graphics *g )
{
	if(!g) return;

	/* Cannot delete the statically allocated root graphics */
	if(g==&graphics_root) return;

	g->refcount--;
	if(g->refcount==0) {
		graphics_delete(g->parent);
		kfree(g);
	}
}

#define ADVANCE(n) { cmd+=n; length-=n; }

int graphics_write(struct graphics *g, int *cmd, int length )
{
	struct graphics_color c;

	while(length>0) {
		switch (*cmd) {
		case GRAPHICS_FGCOLOR:
			c.r = cmd[1];
			c.g = cmd[2];
			c.b = cmd[3];
			c.a = 0;
			graphics_fgcolor(g, c);
			ADVANCE(4)
			break;
		case GRAPHICS_BGCOLOR:
			c.r = cmd[1];
			c.g = cmd[2];
			c.b = cmd[3];
			c.a = 0;
			graphics_bgcolor(g, c);
			ADVANCE(4)
			break;
		case GRAPHICS_RECT:
			graphics_rect(g, cmd[1], cmd[2], cmd[3], cmd[4] , c);
			ADVANCE(5)
			break;
		case GRAPHICS_CLEAR:
			graphics_clear(g, cmd[1], cmd[2], cmd[3], cmd[4]);
			ADVANCE(5)
			break;
		case GRAPHICS_LINE:
			graphics_line(g, cmd[1], cmd[2], cmd[3], cmd[4]);
			ADVANCE(5)
			break;
		case GRAPHICS_TEXT: {
			int x = cmd[1];
			int y = cmd[2];
			int strlength = cmd[3];
			int i;
			for(i = 0; i<strlength; i++) {
				graphics_char(g,x+i*FONT_WIDTH,y,cmd[4+i]);
			}
			ADVANCE(4+strlength)
			break;
		}
		default:
			return KERROR_INVALID_REQUEST;
			break;
		}
	}
	return 0;
}

uint32_t graphics_width(struct graphics * g)
{
	return g->clip.w;
}

uint32_t graphics_height(struct graphics * g)
{
	return g->clip.h;
}

void graphics_fgcolor(struct graphics *g, struct graphics_color c)
{
	g->fgcolor = c;
}

void graphics_bgcolor(struct graphics *g, struct graphics_color c)
{
	g->bgcolor = c;
}

int graphics_clip(struct graphics *g, int x, int y, int w, int h)
{
	// Clip values may not be negative
	if(x<0 || y<0 || w<0 || h<0) return 0;

	// Child origin is relative to parent's clip origin.
	x += g->clip.x;
	y += g->clip.y;

	// Child origin must fall within parent clip
	if(x>=g->bitmap->width || y>=g->bitmap->width) return 0;

	// Child width must fall within parent size
	if((x + w) >= g->bitmap->width || (y + h) >= g->bitmap->height) return 0;

	// Apply the clip
	g->clip.x = x;
	g->clip.y = y;
	g->clip.w = w;
	g->clip.h = h;
	return 1;
}

static inline void plot_pixel(struct bitmap *b, int x, int y, struct graphics_color c)
{
	uint8_t *v = b->data + (b->width * y + x) * 3;
	if(c.a == 0) {
		v[2] = c.r;
		v[1] = c.g;
		v[0] = c.b;
	} else {
		uint16_t a = c.a;
		uint16_t b = 256 - a;
		v[0] = (c.r * b + v[0] * a) >> 8;
		v[1] = (c.g * b + v[1] * a) >> 8;
		v[2] = (c.b * b + v[2] * a) >> 8;
	}
}

static void graphics_rect_internal(struct graphics *g, int x, int y, int w, int h, struct graphics_color c )
{
	int i, j;
	
	if(x<0) { w+=x; x=0; }
	if(y<0) { h+=y; y=0; }

	if(x>g->clip.w || y>g->clip.h) return;

	w = MIN(g->clip.w - x, w);
	h = MIN(g->clip.h - y, h);

	x += g->clip.x;
	y += g->clip.y;

	for(j = 0; j < h; j++) {
		for(i = 0; i < w; i++) {
			plot_pixel(g->bitmap, x + i, y + j,c);
		}
	}
}

#define ABS(n) (((n) < 0) ? -(n) : (n)) /* Absolute function */

float sqrt2(float x)
{
    float y = 1.0;
    while (ABS(x/y - y) > 0.001 /*Tolerance*/)
    {
        y=(y+x/y)/2.0;
    }
    return y;
}

static void graphics_circ_internal(struct graphics *g, int x, int y, int r, struct graphics_color c )
{
	int i, j;

	if(x<0) { r+=x; x=0; }
	if(y<0) { r+=y; y=0; }

	if(x>g->clip.w || y>g->clip.h) return;

	r = MIN(g->clip.w - x, r);
	r = MIN(g->clip.h - y, r);

	x += g->clip.x;
	y += g->clip.y;

    float sinus = 0.70710678118;
    //This is the distance on the axis from sin(90) to sin(45). 
    int range = r/(2*sinus);
    for(int i = r ; i >= range ; --i)
    {
        int j = sqrt2(r*r - i*i);
        for(int k = -j ; k <= j ; k++)
        {
            //We draw all the 4 sides at the same time.
            plot_pixel(g->bitmap, x-k,y+i, c);
            plot_pixel(g->bitmap, x-k,y-i, c);
            plot_pixel(g->bitmap, x+i,y+k, c);
            plot_pixel(g->bitmap, x-i,y-k, c);
        }
    }
    //To fill the circle we draw the circumscribed square.
    range = r*sinus;
    for(int i = x - range + 1 ; i < x + range ; i++)
    {
        for(int j = y - range + 1 ; j < y + range ; j++)
        {
            plot_pixel(g->bitmap, i, j, c);
        }
    }
}

// Swap two bytes
#define SWAP(x,y) do { (x)=(x)^(y); (y)=(x)^(y); (x)=(x)^(y); } while(0)

// Horizontal line
void lcd_hline(struct graphics *g, int x1, int x2, int y, struct graphics_color color) {
	if(x1>=x2) SWAP(x1,x2);
	for(;x1<=x2;x1++) plot_pixel(g->bitmap, x1, y, color);
}

// Fill a triangle - slope method
void graphics_tri_internal(struct graphics *g, int x0, int y0,int x1, int y1, int x2, int y2, struct graphics_color color) {
 	int a, b, y, last;
  	// Sort coordinates by Y order (y2 >= y1 >= y0)
  	if (y0 > y1) { SWAP(y0, y1); SWAP(x0, x1); }
  	if (y1 > y2) { SWAP(y2, y1); SWAP(x2, x1); }
  	if (y0 > y1) { SWAP(y0, y1); SWAP(x0, x1); }
  
  	if(y0 == y2) { // All on same line case
    	a = b = x0;
    	if(x1 < a)      a = x1;
    	else if(x1 > b) b = x1;
    	if(x2 < a)      a = x2;
    	else if(x2 > b) b = x2;
        lcd_hline(g, a, b, y0, color);
        return;
    }

    double
        dx01 = x1 - x0,
        dy01 = y1 - y0,
        dx02 = x2 - x0,
        dy02 = y2 - y0,
        dx12 = x2 - x1,
        dy12 = y2 - y1;
    double sa = 0, sb = 0;

    // For upper part of triangle, find scanline crossings for segment
    // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y
    // is included here (and second loop will be skipped, avoiding a /
    // error there), otherwise scanline y1 is skipped here and handle
    // in the second loop...which also avoids a /0 error here if y0=y
    // (flat-topped triangle)
    if(y1 == y2) last = y1;   // Include y1 scanline
    else         last = y1-1; // Skip it

    for(y=y0; y<=last; y++) {
        a   = x0 + sa / dy01;
        b   = x0 + sb / dy02;
        sa += dx01;
        sb += dx02;
        // longhand a = x0 + (x1 - x0) * (y - y0) / (y1 - y0)
        //          b = x0 + (x2 - x0) * (y - y0) / (y2 - y0)
        lcd_hline(g, a, b, y, color);
    }

    // For lower part of triangle, find scanline crossings for segment
    // 0-2 and 1-2.  This loop is skipped if y1=y2
    sa = dx12 * (y - y1);
    sb = dx02 * (y - y0);
    for(; y<=y2; y++) {
        a   = x1 + sa / dy12;
        b   = x0 + sb / dy02;
        sa += dx12;
        sb += dx02;
        // longhand a = x1 + (x2 - x1) * (y - y1) / (y2 - y1)
        //          b = x0 + (x2 - x0) * (y - y0) / (y2 - y0)
        lcd_hline(g, a, b, y, color);
    }
}

struct graphics_color get_pixel_color(struct graphics *g, int x, int y)
{
	struct graphics_color c;
	uint8_t *v = g->bitmap->data + (g->bitmap->width * y + x) * 3;
	c.r = v[2];
	c.g = v[1];
	c.b = v[0];
	return c;
}

void graphics_tri(struct graphics *g, int x0, int y0,int x1, int y1, int x2, int y2, struct graphics_color c)
{
	graphics_tri_internal(g,x0,y0,x1,y1,x2,y2,c);
}

void graphics_circ(struct graphics *g, int x, int y, int r, struct graphics_color c)
{
	graphics_circ_internal(g,x,y,r,c);
}

void graphics_rect(struct graphics *g, int x, int y, int w, int h, struct graphics_color c)
{
	graphics_rect_internal(g,x,y,w,h,c);
}

void graphics_clear(struct graphics *g, int x, int y, int w, int h)
{
	graphics_rect_internal(g,x,y,w,h,g->bgcolor);
}

static inline void graphics_line_vert(struct graphics *g, int x, int y, int w, int h)
{
	do {
		plot_pixel(g->bitmap, x, y, g->fgcolor);
		y++;
		h--;
	} while(h > 0);
}

static inline void graphics_line_q1(struct graphics *g, int x, int y, int w, int h)
{
	int slope = FACTOR * w / h;
	int counter = 0;

	do {
		plot_pixel(g->bitmap, x, y, g->fgcolor);
		y++;
		h--;
		counter += slope;
		if(counter > FACTOR) {
			counter = counter - FACTOR;
			x++;
			w--;
		}
	} while(h > 0);
}

static inline void graphics_line_q2(struct graphics *g, int x, int y, int w, int h)
{
	int slope = FACTOR * h / w;
	int counter = 0;

	do {
		plot_pixel(g->bitmap, x, y, g->fgcolor);
		x++;
		w--;
		counter += slope;
		if(counter > FACTOR) {
			counter = counter - FACTOR;
			y++;
			h--;
		}
	} while(w > 0);
}

/* h<0, w>0, abs(h) < w */

static inline void graphics_line_q3(struct graphics *g, int x, int y, int w, int h)
{
	int slope = -FACTOR * h / w;
	int counter = 0;

	do {
		plot_pixel(g->bitmap, x, y, g->fgcolor);
		x++;
		w--;
		counter += slope;
		if(counter > FACTOR) {
			counter = counter - FACTOR;
			y--;
			h--;
		}
	} while(w>0);
}

/* h<0, w>0, abs(h) > w */

static inline void graphics_line_q4(struct graphics *g, int x, int y, int w, int h)
{
	int slope = -FACTOR * w / h;
	int counter = 0;

	do {
		plot_pixel(g->bitmap, x, y, g->fgcolor);
		y--;
		h++;
		counter += slope;
		if(counter > FACTOR) {
			counter = counter - FACTOR;
			x++;
			w--;
		}
	} while(h<0);
}

static inline void graphics_line_hozo(struct graphics *g, int x, int y, int w, int h)
{
	do {
		plot_pixel(g->bitmap, x, y, g->fgcolor);
		x++;
		w--;
	} while(w > 0);
}

void graphics_line(struct graphics *g, int x, int y, int w, int h)
{
	// If width is negative, reverse direction to simplify.
	if(w < 0) {
		x = x + w;
		y = y + h;
		w = -w;
		h = -h;
	}

	// If line falls outside of clip region, bail out.
	if(x<0 || y<0 || x>g->clip.w || y>g->clip.h) return;
	if((x+w)>=g->clip.w || (y+h)>=g->clip.h || (y+h)<0 ) return;

	// Adjust origin to clip region.
	x += g->clip.x;
	y += g->clip.y;
	
	if(h>0) {
		if(w==0) {
			graphics_line_vert(g, x, y, w, h);
		} else if(h > w) {
			graphics_line_q1(g, x, y, w, h);
		} else {
			graphics_line_q2(g, x, y, w, h);
		}
	} else if(h<0) {
		if(w==0) {
			graphics_line_vert(g, x, y+h, w, -h);
		} else if(-h < w) {
			graphics_line_q3(g, x, y, w, h);
		} else {
			graphics_line_q4(g, x, y, w, h);
		}
	} else { //h==0
		graphics_line_hozo(g, x, y, w, h);
	}
}

void graphics_bitmap(struct graphics *g, int x, int y, int width, int height, uint8_t * data)
{
	int i, j, b;
	int value;

	width = MIN(g->clip.w - x, width);
	height = MIN(g->clip.h - y, height);
	x += g->clip.x;
	y += g->clip.y;

	b = 0;

	for(j = 0; j < height; j++) {
		for(i = 0; i < width; i++) {
			value = ((*data) << b) & 0x80;
			if(value) {
				plot_pixel(g->bitmap, x + i, y + j, g->fgcolor);
			} else {
				plot_pixel(g->bitmap, x + i, y + j, g->bgcolor);
			}
			b++;
			if(b == 8) {
				data++;
				b = 0;
			}
		}
	}
}

void graphics_char(struct graphics *g, int x, int y, unsigned char c)
{
	uint32_t u = ((uint32_t) c) * FONT_WIDTH * FONT_HEIGHT / 8;
	return graphics_bitmap(g, x, y, FONT_WIDTH, FONT_HEIGHT, &fontdata[u]);
}

void graphics_scrollup(struct graphics *g, int x, int y, int w, int h, int dy)
{
	int j;

	w = MIN(g->clip.w - x, w);
	h = MIN(g->clip.h - y, h);
	x += g->clip.x;
	y += g->clip.y;

	if(dy > h)
		dy = h;

	for(j = 0; j < (h - dy); j++) {
		memcpy(&g->bitmap->data[((y + j) * g->bitmap->width + x) * 3], &g->bitmap->data[((y + j + dy) * g->bitmap->width + x) * 3], w * 3);
	}

	graphics_clear(g, x, y + h - dy, w, dy);
}
