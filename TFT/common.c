/*
 * common.c
 *
 *  Created on: Nov 4, 2022
 *      Author: Alex
 */

#include <ll_spi.h>
#include <stdlib.h>
#include "common.h"

// Forward function declarations
static uint32_t	TFT_Read(uint8_t cmd, uint8_t length);
static void		TFT_DrawCircleHelper(uint16_t x0, uint16_t y0, uint16_t r, uint8_t cornername, uint16_t color);
static void 	TFT_DrawFilledCircleHelper(uint16_t x0, uint16_t y0, uint16_t r, uint8_t cornername, uint16_t delta, uint16_t color);
static void 	swap16(uint16_t* a, uint16_t* b);
static void 	swap32(int32_t*  a, int32_t* b);

// Width & Height of the display used to draw elements (with rotation)
static uint16_t				TFT_WIDTH		= 0;
static uint16_t				TFT_HEIGHT		= 0;
// Screen rotation attributes
static tRotation			angle			= TFT_ROTATION_0;
static uint8_t				madctl_arg[4]	= {0x40|0x08, 0x20|0x08, 0x80|0x08, 0x40|0x80|0x20|0x08};
static uint16_t				TFT_WIDTH_0		= 0;	// Generic display width  (without rotation)
static uint16_t				TFT_HEIGHT_0	= 0;	// Generic display height (without rotation)


uint16_t TFT_Width(void) {
	return TFT_WIDTH;
}

uint16_t TFT_Height(void) {
	return TFT_HEIGHT;
}

tRotation TFT_Rotation(void) {
	return angle;
}

void TFT_Setup(uint16_t generic_width, uint16_t generic_height, uint8_t madctl[4]) {
	TFT_WIDTH_0		= generic_width;
	TFT_HEIGHT_0	= generic_height;
	if (madctl) {
		for (uint8_t i = 0; i < 4; ++i)
			madctl_arg[i] = madctl[i];
	}
	TFT_SetRotation(TFT_ROTATION_0);
	// Initialize transfer infrastructure for the display SPI bus (With DMA)
	IFACE_ColorBlockInit();
}

void TFT_SetRotation(tRotation rotation)  {
	TFT_Delay(1);

	switch (rotation)  {
		case TFT_ROTATION_90:
		case TFT_ROTATION_270:
			TFT_WIDTH	= TFT_HEIGHT_0;
			TFT_HEIGHT	= TFT_WIDTH_0;
			break;
		case TFT_ROTATION_0:
		case TFT_ROTATION_180:
		default:
			TFT_WIDTH	= TFT_WIDTH_0;
			TFT_HEIGHT	= TFT_HEIGHT_0;
			break;
	}
	// Perform CMD_MADCTL_36 command
	TFT_Command(0x36, &madctl_arg[(uint8_t)rotation], 1);
	TFT_Delay(10);
	angle = rotation;
}

uint16_t TFT_WheelColor(uint8_t wheel_pos) {
	wheel_pos = 255 - wheel_pos;
	if (wheel_pos < 85) {
		return TFT_Color(255 - wheel_pos * 3, 0, wheel_pos * 3);
	}
	if (wheel_pos < 170) {
		wheel_pos -= 85;
		return TFT_Color(0, wheel_pos * 3, 255 - wheel_pos * 3);
	}
	wheel_pos -= 170;
	return TFT_Color(wheel_pos * 3, 255 - wheel_pos * 3, 0);
}

uint16_t TFT_Color(uint8_t red, uint8_t green, uint8_t blue) {
	return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3);
}

void TFT_SetAttrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
	uint8_t data[4];

	// column address set
	data[0] = (x0 >> 8) & 0xFF;
	data[1] = x0 & 0xFF;
	data[2] = (x1 >> 8) & 0xFF;
	data[3] = x1 & 0xFF;
	TFT_Command(0x2A, data, 4);

	// row address set
    data[0] = (y0 >> 8) & 0xFF;
    data[1] = y0 & 0xFF;
    data[2] = (y1 >> 8) & 0xFF;
    data[3] = y1 & 0xFF;
    TFT_Command(0x2B, data, 4);
}

// Initializes the rectangular area for new data
void TFT_StartDrawArea(uint16_t x0, uint16_t y0, uint16_t width, uint16_t height) {
	TFT_SetAttrWindow(x0, y0, x0 + width - 1, y0 + height -1);
	TFT_Command(0x2C, 0, 0);							// write to RAM
	IFACE_DataMode();
}

// Fills the rectangular area with a single color
void TFT_DrawFilledRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
	if ((x > TFT_WIDTH) || (y > TFT_HEIGHT) || (width < 1) || (height < 1)) return;
	if ((x + width  - 1) > TFT_WIDTH)  width  = TFT_WIDTH  - x;
	if ((y + height - 1) > TFT_HEIGHT) height = TFT_HEIGHT - y;
	TFT_SetAttrWindow(x, y, x + width - 1, y + height - 1);
	TFT_Command(0x2C, 0, 0);							// write to RAM
	IFACE_DataMode();
	TFT_ColorBlockSend(color, width*height);
	TFT_FinishDrawArea();
}

// Sets address (entire screen) and Sends Height*Width amount of color information to Display
void TFT_FillScreen(uint16_t color) {
	TFT_DrawFilledRect(0, 0, TFT_WIDTH, TFT_HEIGHT, color);
}

void TFT_DrawPixel(uint16_t x,  uint16_t y, uint16_t color) {
	if ((x >=TFT_WIDTH) || (y >=TFT_HEIGHT)) return;
	IFACE_DrawPixel(x, y, color);
}

void TFT_DrawHLine(uint16_t x, uint16_t y, uint16_t length, uint16_t color) {
	TFT_DrawFilledRect(x, y, length, 1, color);
}

void TFT_DrawVLine(uint16_t x, uint16_t y, uint16_t length, uint16_t color) {
	TFT_DrawFilledRect(x, y, 1, length, color);
}

// Draw a hollow rectangle between positions (x1,y1) and (x2,y2) with specified color
void TFT_DrawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
	if ((x >= TFT_WIDTH) || (y >= TFT_HEIGHT) || (x + width >= TFT_WIDTH) || (y + height >= TFT_HEIGHT)) return;

	TFT_DrawHLine(x, y, width, color);
	TFT_DrawHLine(x, y + height, width, color);
	TFT_DrawVLine(x, y, height, color);
	TFT_DrawVLine(x + width, y,  height, color);

	if ((width) || (height))  {
		TFT_DrawPixel(x + width, y + height, color);
	}
}

void TFT_DrawRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, uint16_t color) {
	// Draw edges
	TFT_DrawHLine(x + r  , y    , w - r - r, color);			// Top
	TFT_DrawHLine(x + r  , y + h - 1, w - r - r, color);		// Bottom
	TFT_DrawVLine(x    , y + r  , h - r - r, color);			// Left
	TFT_DrawVLine(x + w - 1, y + r  , h - r - r, color);		// Right
	// Draw four corners
	TFT_DrawCircleHelper(x + r,					y + r, 	r, 1, color);
	TFT_DrawCircleHelper(x + w - r - 1,			y + r, 	r, 2, color);
	TFT_DrawCircleHelper(x + w - r - 1, y + h - r - 1,	r, 4, color);
	TFT_DrawCircleHelper(x + r, 		y + h - r - 1,	r, 8, color);

}

void TFT_DrawFilledRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, uint16_t color) {
	TFT_DrawFilledRect(x + r, y, w - r, h, color);
	// Draw right vertical bar with round corners
	TFT_DrawFilledCircleHelper(x + w - 1,	y + r, r, 1, h - r - r - 1, color);
	// Draw left vertical bar with round corners
	TFT_DrawFilledCircleHelper(x + r,		y + r, r, 2, h - r - r - 1, color);
}

// Draw thin line
void TFT_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
	uint8_t steep = abs(y1 - y0) > abs(x1 - x0);
	if (steep) {
		swap16(&x0, &y0);
		swap16(&x1, &y1);
	}

	if (x0 > x1) {
		swap16(&x0, &x1);
		swap16(&y0, &y1);
	}

	int32_t dx = x1 - x0, dy = abs(y1 - y0);;
	int32_t err = dx >> 1, ystep = -1, xs = x0, dlen = 0;

	if (y0 < y1) ystep = 1;

	// Split into steep and not steep for FastH/V separation
	if (steep) {
		for (; x0 <= x1; x0++) {
			dlen++;
			err -= dy;
			if (err < 0) {
				err += dx;
				if (dlen == 1)
					TFT_DrawPixel(y0, xs, color);
				else TFT_DrawVLine(y0, xs, dlen, color);
				dlen = 0; y0 += ystep; xs = x0 + 1;
			}
		}
		if (dlen) TFT_DrawVLine(y0, xs, dlen, color);
	} else {
		for (; x0 <= x1; x0++) {
			dlen++;
			err -= dy;
			if (err < 0) {
				err += dx;
				if (dlen == 1)
					TFT_DrawPixel(xs, y0, color);
				else TFT_DrawHLine(xs, y0, dlen, color);
				dlen = 0; y0 += ystep; xs = x0 + 1;
			}
		}
		if (dlen) TFT_DrawHLine(xs, y0, dlen, color);
	}
}

// Draw circle
void TFT_DrawCircle(uint16_t x, uint16_t y, uint8_t radius, uint16_t color) {
    // Bresenham algorithm
    int x_pos = -radius;
    int y_pos = 0;
    int err = 2 - 2 * radius;
    int e2;

    do {
    	TFT_DrawPixel(x - x_pos, y + y_pos, color);
    	TFT_DrawPixel(x + x_pos, y + y_pos, color);
    	TFT_DrawPixel(x + x_pos, y - y_pos, color);
    	TFT_DrawPixel(x - x_pos, y - y_pos, color);
        e2 = err;
        if (e2 <= y_pos) {
            err += ++y_pos * 2 + 1;
            if(-x_pos == y_pos && e2 <= x_pos) {
              e2 = 0;
            }
        }
        if (e2 > x_pos) {
            err += ++x_pos * 2 + 1;
        }
    } while (x_pos <= 0);
}

void TFT_DrawFilledCircle(uint16_t x, uint16_t y, uint8_t radius, uint16_t color) {
    // Bresenham algorithm
    int x_pos = -radius;
    int y_pos = 0;
    int err = 2 - 2 * radius;
    int e2;

    do {
    	TFT_DrawPixel(x - x_pos, y + y_pos, color);
    	TFT_DrawPixel(x + x_pos, y + y_pos, color);
    	TFT_DrawPixel(x + x_pos, y - y_pos, color);
    	TFT_DrawPixel(x - x_pos, y - y_pos, color);
    	TFT_DrawHLine(x + x_pos, y + y_pos, 2 * (-x_pos) + 1, color);
    	TFT_DrawHLine(x + x_pos, y - y_pos, 2 * (-x_pos) + 1, color);
        e2 = err;
        if (e2 <= y_pos) {
            err += ++y_pos * 2 + 1;
            if(-x_pos == y_pos && e2 <= x_pos) {
                e2 = 0;
            }
        }
        if (e2 > x_pos) {
            err += ++x_pos * 2 + 1;
        }
    } while (x_pos <= 0);
}


void TFT_DrawTriangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
	TFT_DrawLine(x0, y0, x1, y1, color);
	TFT_DrawLine(x1, y1, x2, y2, color);
	TFT_DrawLine(x2, y2, x0, y0, color);
}

// Draw filled triangle
void TFT_DrawfilledTriangle (uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
	int32_t a, b, y, last;
	// Sort coordinates by Y order (y2 >= y1 >= y0)
	if (y0 > y1) {
		swap16(&y0, &y1);
		swap16(&x0, &x1);
	}
	if (y1 > y2) {
		swap16(&y2, &y1);
		swap16(&x2, &x1);
	}
	if (y0 > y1) {
		swap16(&y0, &y1);
		swap16(&x0, &x1);
	}

	if (y0 == y2) {								// Handle awkward all-on-same-line case as its own thing
		a = b = x0;
		if (x1 < a)      a = x1;
		else if (x1 > b) b = x1;
		if (x2 < a)      a = x2;
		else if (x2 > b) b = x2;
		TFT_DrawHLine(a, y0, b - a + 1, color);
		return;
	}

	int32_t
		dx01 = x1 - x0,
		dy01 = y1 - y0,
		dx02 = x2 - x0,
		dy02 = y2 - y0,
		dx12 = x2 - x1,
		dy12 = y2 - y1,
		sa   = 0,
		sb   = 0;

	// For upper part of triangle, find scanline crossings for segments
	// 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
	// is included here (and second loop will be skipped, avoiding a /0
	// error there), otherwise scanline y1 is skipped here and handled
	// in the second loop...which also avoids a /0 error here if y0=y1
	// (flat-topped triangle).
	if (y1 == y2) last = y1; 					// Include y1 scanline
	else         last = y1 - 1; 				// Skip it

	for (y = y0; y <= last; y++) {
		a   = x0 + sa / dy01;
		b   = x0 + sb / dy02;
		sa += dx01;
		sb += dx02;

		if (a > b) swap32(&a, &b);
		TFT_DrawHLine(a, y, b - a + 1, color);
	}

	// For lower part of triangle, find scanline crossings for segments
	// 0-2 and 1-2.  This loop is skipped if y1=y2.
	sa = dx12 * (y - y1);
	sb = dx02 * (y - y0);
	for (; y <= y2; y++) {
		a   = x1 + sa / dy12;
		b   = x0 + sb / dy02;
		sa += dx12;
		sb += dx02;

		if (a > b) swap32(&a, &b);
		TFT_DrawHLine(a, y, b - a + 1, color);
	}

}

void TFT_DrawEllipse(uint16_t x0, uint16_t y0, uint16_t rx, uint16_t ry, uint16_t color) {
	if (rx < 2 || ry < 2) return;
	int32_t x, y;
	int32_t rx2 = rx * rx;
	int32_t ry2 = ry * ry;
	int32_t fx2 = 4 * rx2;
	int32_t fy2 = 4 * ry2;
	int32_t s;

	for (x = 0, y = ry, s = 2*ry2+rx2*(1-2*ry); ry2*x <= rx2*y; x++) {
		// These are ordered to minimize coordinate changes in x or y
		// drawPixel can then send fewer bounding box commands
		TFT_DrawPixel(x0 + x, y0 + y, color);
		TFT_DrawPixel(x0 - x, y0 + y, color);
		TFT_DrawPixel(x0 - x, y0 - y, color);
		TFT_DrawPixel(x0 + x, y0 - y, color);
		if (s >= 0) {
			s += fx2 * (1 - y);
			y--;
		}
		s += ry2 * ((4 * x) + 6);
	}

	for (x = rx, y = 0, s = 2*rx2+ry2*(1-2*rx); rx2*y <= ry2*x; y++) {
		// These are ordered to minimize coordinate changes in x or y
		// drawPixel can then send fewer bounding box commands
		TFT_DrawPixel(x0 + x, y0 + y, color);
		TFT_DrawPixel(x0 - x, y0 + y, color);
		TFT_DrawPixel(x0 - x, y0 - y, color);
		TFT_DrawPixel(x0 + x, y0 - y, color);
		if (s >= 0) {
			s += fy2 * (1 - x);
			x--;
		}
		s += rx2 * ((4 * y) + 6);
	}
}

void TFT_DrawFilledEllipse(uint16_t x0, uint16_t y0, uint16_t rx, uint16_t ry, uint16_t color) {
	if (rx<2) return;
	if (ry<2) return;
	int32_t x, y;
	int32_t rx2 = rx * rx;
	int32_t ry2 = ry * ry;
	int32_t fx2 = 4 * rx2;
	int32_t fy2 = 4 * ry2;
	int32_t s;

	for (x = 0, y = ry, s = 2*ry2+rx2*(1-2*ry); ry2*x <= rx2*y; x++) {
		TFT_DrawHLine(x0 - x, y0 - y, x + x + 1, color);
		TFT_DrawHLine(x0 - x, y0 + y, x + x + 1, color);

		if (s >= 0) {
			s += fx2 * (1 - y);
			y--;
		}
		s += ry2 * ((4 * x) + 6);
	}

	for (x = rx, y = 0, s = 2*rx2+ry2*(1-2*rx); rx2*y <= ry2*x; y++) {
		TFT_DrawHLine(x0 - x, y0 - y, x + x + 1, color);
		TFT_DrawHLine(x0 - x, y0 + y, x + x + 1, color);

		if (s >= 0)
		{
			s += fy2 * (1 - x);
			x--;
		}
		s += rx2 * ((4 * y) + 6);
	}
}

// Draw rectangle Area. Every pixel generated by the callback function nextPixelCB
void TFT_DrawArea(uint16_t x0, uint16_t y0, uint16_t area_width, uint16_t area_height, t_NextPixel nextPixelCB) {
	if (x0 >= TFT_WIDTH || y0 >= TFT_HEIGHT || area_width < 1 || area_height < 1) return;
	if ((x0 + area_width  - 1) > TFT_WIDTH)  area_width  = TFT_WIDTH  - x0;
	if ((y0 + area_height - 1) > TFT_HEIGHT) area_height = TFT_HEIGHT - y0;

	TFT_StartDrawArea(x0, y0, area_width, area_height);
	// Write color data row by row
    for (uint16_t row = 0; row < area_height; ++row) {
    	for (uint16_t bit = 0; bit < area_width; ++bit) {
			uint16_t color 	 = (*nextPixelCB)(row, bit);
			TFT_ColorBlockSend(color, 1);
    	}
    }
    TFT_FinishDrawArea();						// Flush color block buffer
}

// Draw bitmap
void TFT_DrawBitmap(uint16_t x0, uint16_t y0, uint16_t area_width, uint16_t area_height,
		const uint8_t *bitmap, uint16_t bm_width, uint16_t bg_color, uint16_t fg_color) {

	if (x0 >= TFT_WIDTH || y0 >= TFT_HEIGHT || area_width < 1 || area_height < 1 || bm_width < 1 || !bitmap) return;
	if ((x0 + area_width  - 1) > TFT_WIDTH)  area_width  = TFT_WIDTH  - x0;
	if ((y0 + area_height - 1) > TFT_HEIGHT) area_height = TFT_HEIGHT - y0;

	TFT_StartDrawArea(x0, y0, area_width, area_height);

	uint16_t bytes_per_row = (bm_width + 7) >> 3;
	// Write color data row by row
    for (uint16_t row = 0; row < area_height; ++row) {
    	uint16_t out_bit  = 0;					// Number of bits were pushed out
    	for (uint16_t bit = 0; bit < bm_width; ++bit) {
			if (out_bit >= area_width)			// row is over
				break;
			uint16_t in_byte = row * bytes_per_row + (bit >> 3);
			uint8_t  in_mask = 0x80 >> (bit & 0x7);
			uint16_t color 	 = (in_mask & bitmap[in_byte])?fg_color:bg_color;
			TFT_ColorBlockSend(color, 1);
			++out_bit;
    	}
    	// Fill-up rest area with background color
    	if (area_width > out_bit) {
    		TFT_ColorBlockSend(bg_color, area_width - out_bit);
    	}
    }
    // Send rest data from the buffer
    TFT_FinishDrawArea();						// Flush color block buffer
}

// Draw bitmap created from the string.
void TFT_DrawScrolledBitmap(uint16_t x0, uint16_t y0, uint16_t area_width, uint16_t area_height,
		const uint8_t *bitmap, uint16_t bm_width, int16_t offset, uint8_t gap, uint16_t bg_color, uint16_t fg_color) {

	if (x0 >= TFT_WIDTH || y0 >= TFT_HEIGHT || area_width < 1 || area_height < 1 || bm_width < 1 || !bitmap) return;
	if ((x0 + area_width  - 1) > TFT_WIDTH)  area_width  = TFT_WIDTH  - x0;
	if ((y0 + area_height - 1) > TFT_HEIGHT) area_height = TFT_HEIGHT - y0;
	while (offset >= bm_width) offset -= bm_width + gap;

	TFT_StartDrawArea(x0, y0, area_width, area_height);

    uint16_t bytes_per_row = (bm_width + 7) >> 3;
    // Write color data row by row
    for (uint16_t row = 0; row < area_height; ++row) {
    	uint16_t out_bit = 0;					// Number of bits were pushed out
    	if (offset < 0) {						// Negative offset means bitmap should be shifted right
    		// Fill-up left side with background color
    		out_bit = abs(offset);
    		if (out_bit > area_width) out_bit = area_width;
    		TFT_ColorBlockSend(bg_color, out_bit);
    	}
    	// Send bitmap row
    	for (uint16_t bit = (offset > 0)?offset:0; bit < bm_width; ++bit) {
    		if (out_bit >= area_width)			// row is over
    			break;
    		uint16_t in_byte = row * bytes_per_row + (bit >> 3);
    		uint8_t  in_mask = 0x80 >> (bit & 0x7);
    		uint16_t color 	 = (in_mask & bitmap[in_byte])?fg_color:bg_color;
    		TFT_ColorBlockSend(color, 1);
    		++out_bit;
    	}
    	if (gap == 0) {							// Not looped bitmap. Fill-up rest area with background color
    		if (area_width > out_bit)
    			TFT_ColorBlockSend(bg_color, area_width - out_bit);
    	} else {								// Looped bitmap
    		uint8_t bg_bits = gap;				// Draw the gap between looped bitmap images
    		if (area_width > out_bit) {			// There is a place to draw
    		    if (bg_bits > (area_width - out_bit))
    		    	bg_bits = area_width - out_bit;
    		    TFT_ColorBlockSend(bg_color, bg_bits);
    		    out_bit += bg_bits;
    		}
    		// Start over the bitmap
    		for (uint16_t bit = 0; bit < bm_width; ++bit) {
    			if (out_bit >= area_width)		// row is over
    				break;
    			uint16_t in_byte = row * bytes_per_row + (bit >> 3);
    			uint8_t  in_mask = 0x80 >> (bit & 0x7);
    			uint16_t color = (in_mask & bitmap[in_byte])?fg_color:bg_color;
    			TFT_ColorBlockSend(color, 1);
    			++out_bit;
    		}
    	}
    }
    // Send rest data from the buffer
    TFT_FinishDrawArea();									// Flush color block buffer
}

// Draw pixmap
void TFT_DrawPixmap(uint16_t x0, uint16_t y0, uint16_t area_width, uint16_t area_height,
		const uint8_t *pixmap, uint16_t pm_width, uint8_t depth, uint16_t palette[]) {

	if (x0 >= TFT_WIDTH || y0 >= TFT_HEIGHT || area_width < 1 || area_height < 1 || pm_width < 1 || !pixmap) return;
	if ((x0 + area_width  - 1) > TFT_WIDTH)  area_width  = TFT_WIDTH  - x0;
	if ((y0 + area_height - 1) > TFT_HEIGHT) area_height = TFT_HEIGHT - y0;

	TFT_StartDrawArea(x0, y0, area_width, area_height);

	uint16_t bytes_per_row = (pm_width*depth + 7) >> 3;
	// Write color data row by row
    for (uint16_t row = 0; row < area_height; ++row) {
    	uint16_t out_pixel  = 0;							// Number of pixels were pushed out
    	uint16_t in_mask  = ((1 << depth) - 1) << (16-depth); // pixel bit mask shifted to the left position in the word
    	uint16_t in_byte  = row * bytes_per_row;
    	uint8_t	 sh_right = 16 - depth;
    	for (uint16_t bit = 0; bit < pm_width; ++bit) {
    		if (out_pixel >= area_width)					// row is over
				break;
			uint16_t code = pixmap[in_byte] & (in_mask >> 8);
			code <<= 8;
			code |= pixmap[in_byte+1] & (in_mask & 0xff);
			code >>= sh_right;
			uint16_t color = palette[code];
			TFT_ColorBlockSend(color, 1);
			in_mask >>= depth;
			if ((in_mask & 0xff00) == 0) {					// Go to the next byte
				in_mask <<= 8;								// Correct input mask
				sh_right += 8;								// Correct shift bits value
				++in_byte;
			}
			sh_right -= depth;
			++out_pixel;
    	}
    	// Fill-up rest area with background color
    	if (area_width > out_pixel) {
    		TFT_ColorBlockSend(palette[0], area_width - out_pixel);
    	}
    }
    // Send rest data from the buffer
    TFT_FinishDrawArea();									// Flush color block buffer
}

// ======================================================================
// Display specific default functions
// ======================================================================
uint32_t TFT_DEF_ReadID4(void) {
	return TFT_Read(0xD3, 3);
}

uint16_t TFT_ReadPixel(uint16_t x, uint16_t y, bool is16bit_color) {
	if ((x >=TFT_WIDTH) || (y >=TFT_HEIGHT)) return 0;

	if (is16bit_color)
		TFT_Command(0x3A, (uint8_t *)"\x66", 1);
	TFT_SetAttrWindow(x, y, x, y);
	uint16_t ret = 0;
	uint8_t data[4];
	if (TFT_ReadData(0x2E, data, 4)) {
		ret = TFT_Color(data[1], data[2], data[3]);
	}
	if (is16bit_color)
		TFT_Command(0x3A, (uint8_t *)"\x55", 1);
	return ret;
}

// ======================================================================
// Internal functions
// ======================================================================
// Read up-to 4 bytes of data from the display and returns 32-bits word
static uint32_t TFT_Read(uint8_t cmd, uint8_t length) {
	if (length == 0 || length > 4)
		return 0xffffffff;
	uint8_t data[4];
	if (TFT_ReadData(cmd, data, 4)) {
		uint32_t value = 0;
		for (uint8_t i = 0; i < length; ++i) {
			value <<= 8;
			value |= data[i];
		}
		return value;
	}
	return 0xffffffff;
}

// Draw round corner
static void TFT_DrawCircleHelper(uint16_t x0, uint16_t y0, uint16_t r, uint8_t cornername, uint16_t color) {
	int32_t f     = 1 - r;
	int32_t ddF_x = 1;
	int32_t ddF_y = -2 * r;
	int32_t x     = 0;

	while (x < r) {
		if (f >= 0) {
			r--;
			ddF_y += 2;
			f     += ddF_y;
		}
		x++;
		ddF_x += 2;
		f     += ddF_x;
		if (cornername & 0x4) {
			TFT_DrawPixel(x0 + x, y0 + r, color);
			TFT_DrawPixel(x0 + r, y0 + x, color);
		}
		if (cornername & 0x2) {
			TFT_DrawPixel(x0 + x, y0 - r, color);
			TFT_DrawPixel(x0 + r, y0 - x, color);
		}
		if (cornername & 0x8) {
			TFT_DrawPixel(x0 - r, y0 + x, color);
			TFT_DrawPixel(x0 - x, y0 + r, color);
		}
		if (cornername & 0x1) {
			TFT_DrawPixel(x0 - r, y0 - x, color);
			TFT_DrawPixel(x0 - x, y0 - r, color);
		}
	}
}

// Draw filled corner
static void TFT_DrawFilledCircleHelper(uint16_t x0, uint16_t y0, uint16_t r, uint8_t cornername, uint16_t delta, uint16_t color) {
	int32_t f     = 1 - r;
	int32_t ddF_x = 1;
	int32_t ddF_y = -r - r;
	int32_t x     = 0;

	delta++;
	while (x < r) {
		if (f >= 0) {
			r--;
			ddF_y += 2;
			f     += ddF_y;
		}
		x++;
		ddF_x += 2;
		f     += ddF_x;

		if (cornername & 0x1) {
			TFT_DrawVLine(x0 + x, y0 - r, r + r + delta, color);
			TFT_DrawVLine(x0 + r, y0 - x, x + x + delta, color);
		}
		if (cornername & 0x2) {
			TFT_DrawVLine(x0 - x, y0 - r, r + r + delta, color);
			TFT_DrawVLine(x0 - r, y0 - x, x + x + delta, color);
		}
	}
}

static void swap16(uint16_t* a, uint16_t* b) {
	uint16_t t = *a;
	*a = *b;
	*b = t;
}

static void swap32(int32_t* a, int32_t* b) {
	uint16_t t = *a;
	*a = *b;
	*b = t;
}

