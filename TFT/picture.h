/*
 * picture.h
 *
 *  Created on: May 27 2020
 *      Author: Alex
 */

#ifdef TFT_BMP_JPEG_ENABLE

#ifndef _PICTURE_H_
#define _PICTURE_H_

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

bool	TFT_jpegAllocate(void);
void	TFT_jpegDeallocate(void);
bool	TFT_DrawJPEG(const char *filename, int16_t x, int16_t y);

// Draw clipped JPEG file image. Put Upper-Left corner of JPEG image to (x, y)
// and draw only a rectangular area with coordinates (area_x, area_y) and size of area_width, area_height
// As slow as a drawJPEG() function because need to read whole jpeg file
bool	TFT_ClipJPEG(const char *filename, int16_t x, int16_t y, uint16_t area_x, uint16_t area_y, uint16_t area_width, uint16_t area_height);

// Draws BMP file image at the given coordinates.
// Both coordinates can be less than zero. In this case, top-left area will be clipped
// Only two BMP formats are supported: 24-bit per pixel, not-compressed and 16-bit per pixel R5-G6-B5
// For best performance, save BMP file as 16-bits R6-G5-B6 format and flipped row order
bool	TFT_DrawBMP(const char *filename, int16_t x, int16_t y);

// Draw clipped BMP file image. Put Upper-Left corner of BMP image to (x, y)
// and draw only a rectangular area with coordinates (area_x, area_y) and size of area_width, area_height
// Only two BMP formats are supported: 24-bit per pixel, not-compressed and 16-bit per pixel R5-G6-B5
bool 	TFT_ClipBMP(const char *filename, int16_t x, int16_t y, uint16_t area_x, uint16_t area_y, uint16_t area_width, uint16_t area_height);

bool 	TFT_ScrollBitmapOverBMP(const char *filename, int16_t x, int16_t y, uint16_t area_x, uint16_t area_y, uint16_t area_width, uint16_t area_height,
			const uint8_t *bitmap, uint16_t bm_width, int16_t offset, uint8_t gap, uint16_t txt_color);
#ifdef __cplusplus
}
#endif

#endif

#endif
