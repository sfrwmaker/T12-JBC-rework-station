/*
 * bitpam.cpp
 *
 *  Created on: May 23 2020
 *      Author: Alex
 */

#include <string.h>
#include <stdlib.h>
#include "bitmap.h"

BITMAP::BITMAP(uint16_t width, uint16_t height) {
	ds = 0;
	if (width == 0 || height == 0) return;

	uint8_t	bytes_per_row = (width+7) >> 3;
	ds = (struct data *)calloc(sizeof(struct data) + height * bytes_per_row, sizeof(uint8_t));
	if (!ds) return;
	ds->w 		= width;
	ds->h 		= height;
	ds->links 	= 1;
}

BITMAP::BITMAP(const BITMAP &bm) {
	this->ds = bm.ds;
	++ds->links;
}

BITMAP&	BITMAP::operator=(const BITMAP &bm) {
	if (this != &bm) {
		if (ds != 0 && --(this->ds->links) == 0) {
			free(ds);
		}
		this->ds = bm.ds;
		++ds->links;
	}
	return *this;
}

BITMAP::~BITMAP(void) {
	if (!ds) return;
	if (--ds->links > 0) return;						// Destroy yet another copy of the bitmap
	free(ds);
	ds = 0;
}

void BITMAP::clear(void) {
	if (!ds) return;
	uint8_t  bytes_per_row = (ds->w + 7) >> 3;
	uint32_t size = bytes_per_row * ds->h;
	for (uint32_t i = 0; i < size; ++i)
		ds->data[i] = 0;
}

void BITMAP::drawPixel(uint16_t x, uint16_t y) {
	if (!ds) return;
	uint8_t  bytes_per_row = (ds->w + 7) >> 3;
	uint16_t in_byte = y * bytes_per_row + (x >> 3);
	uint8_t  in_mask = 0x80 >> (x & 0x7);
	ds->data[in_byte] |= in_mask;
}

bool BITMAP::pixel(uint16_t x, uint16_t y) {
	if (!ds) return false;
	uint8_t  bytes_per_row = (ds->w + 7) >> 3;
	uint16_t in_byte = y * bytes_per_row + (x >> 3);
	uint8_t  in_mask = 0x80 >> (x & 0x7);
	return ((ds->data[in_byte] & in_mask) != 0);
}

void BITMAP::drawHLine(uint16_t x, uint16_t y, uint16_t length) {
	if (!ds) return;
	if (length == 0 || x > ds->w || y > ds->h) return;
	if (x + length >= ds->w) length = ds->w - x;
	uint16_t bytes_per_row	= (ds->w+7) >> 3;
	uint32_t byte_index		= y * bytes_per_row + (x >> 3);
	uint16_t start_bit		= x & 7;
	while (length > 0) {
		uint8_t mask = (0x100 >> start_bit) - 1; 			// several '1'staring from start_bit to end of byte
		uint16_t end_bit = start_bit + length;
		if (end_bit < 8) {									// Line terminates in the current byte
			uint8_t clear_mask = (0x100 >> end_bit) - 1;
			mask &= ~clear_mask;							// clear tail of the byte
			length = 0;										// Last pixel
		} else {
			length -= 8 - start_bit;
		}
		ds->data[byte_index++] |= mask;
		start_bit = 0;										// Fill up the next byte
	}
}

void BITMAP::drawVLine(uint16_t x, uint16_t y, uint16_t length) {
	if (!ds) return;
	if (length == 0 || x > ds->w || y > ds->h) return;
	if (y+length >= ds->h) length = ds->h - y;
	uint16_t bytes_per_row	= (ds->w+7) >> 3;
	uint32_t byte_index		= y * bytes_per_row + (x >> 3);
	uint16_t bit			= x & 7;
	uint8_t  mask = (0x80 >> bit);
	for (uint16_t i = 0; i < length; ++i) {
		ds->data[byte_index] |= mask;
		byte_index += bytes_per_row;
	}
}


void BITMAP::drawIcon(uint16_t x, uint16_t y, const uint8_t *icon, uint16_t ic_width, uint16_t ic_height) {
	if (!ds || !icon) return;
	if (x > ds->w || y > ds->h) return;
	uint16_t bm_bytes_per_row = (ds->w + 7) >> 3;
	uint16_t ic_bytes_per_row = (ic_width + 7) >> 3;
	uint8_t  first_bit 	= x & 7;							// The first bit in the bitmap byte
	uint16_t first_byte	= x >> 3;							// First byte of bitmap to start drawing the icon
	for (uint16_t row = 0; row < ic_height; ++ row) {
		if (row + y >= ds->h)								// Out of the bitmap border
			break;
		uint8_t lb = icon[row * ic_bytes_per_row];			// Left icon byte in the row
		lb >>= first_bit;
		uint16_t bm_byte = (row + y) * bm_bytes_per_row + first_byte; // Left bitmap byte in the row
		ds->data[bm_byte++] |= lb;							// Draw left byte in the row
		for (uint16_t ic_byte = 0; ic_byte < ic_bytes_per_row; ++ic_byte) {
			if (ic_byte + first_byte >= bm_bytes_per_row-1)	// Out of the bitmap border
				break;
			uint16_t icon_pos = ic_bytes_per_row * row + ic_byte;
			uint16_t mask = icon[icon_pos] << 8;
			if (ic_byte < ic_bytes_per_row - 1)				// If first_bit > 0, the icon can be shifted right some pixels!
				mask |= icon[icon_pos+1];
			mask >>= first_bit;
			mask &= 0xff;
			ds->data[bm_byte++] = mask;
		}
	}
}

void BITMAP::drawVGauge(uint16_t gauge, bool edged) {
	if (!ds) return;
	if (gauge > ds->h) gauge = ds->h;
	uint16_t top_line = ds->h - gauge;
	clear();

	uint16_t x_offset		= 0;
	uint16_t half_height	= ds->h >> 1;
	if (edged) {
		drawHLine(0, 0, ds->w);								// Horizontal top line
		for (uint16_t i = 1; i < top_line; ++i) {			// draw edged part of the triangle
			uint16_t next_offset = (ds->w * i + half_height) / ds->h;
			drawHLine(x_offset, i, next_offset - x_offset+1); // draw left line of the triangle
			x_offset = next_offset;
		}
		drawVLine(ds->w-1, 0, top_line);
	}
	for (uint16_t i = top_line; i < ds->h; ++i) {
		x_offset = (ds->w * i + half_height) / ds->h;
		drawHLine(x_offset, i, ds->w - x_offset);			// fill-up the triangle
	}
}
