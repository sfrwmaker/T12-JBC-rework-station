/*
 * pictute.c
 *
 *  Created on: May 27 2020
 *      Author: Alex
 */

#include "common.h"

#ifdef TFT_BMP_JPEG_ENABLE

#ifndef __cplusplus
#include <stdbool.h>
#endif

#include <string.h>
#include <stdlib.h>
#include "ff.h"
#include "tjpgd.h"

// Forward function declaration
static uint16_t readJpeg(JDEC* jd, uint8_t* buff, uint16_t nbytes);
static uint16_t drawJpegBuffer(JDEC* jd, void *bitmap, JRECT* rect);
static uint16_t clipJpegBuffer(JDEC* jd, void *bitmap, JRECT* rect);
static uint16_t u16max(uint16_t a, uint16_t b);
static uint16_t u16min(uint16_t a, uint16_t b);
static uint16_t read16(uint8_t *ptr);
static uint32_t read32(uint8_t *ptr);
static uint16_t readPixel(uint8_t *ptr, uint8_t bpp);

/*
 * The JPEG drawing routines based on TJpgDec - Tiny JPEG Decompressor
 * created by ChaN (http://elm-chan.org/fsw/tjpgd/00index.html)
 *
 * Usually we have to create the IODEV structure to be used for output callback
 * like this one:
 * typedef struct {
 *	  FILE		*fp;		// .jpeg File descriptor
 *	  uint8_t	*fbuf;		// Pointer to the frame buffer for output function
 *    uint16_t 	wbuff;		// Width of the frame buffer
 * } IODEV;
 * and then put the pointer to this structure to JDEC.device, but
 * in case of ILI9341 driver we do need file descriptor only,
 * so JDEC.device is a file descriptor in our case
 */

static const uint16_t work_buff_size = 3100;	// Working area buffer size
static void		*work;							// Working area buffer for TJPEG routines, should be allocated by jpegAllocate()
static JRECT	area;							// Clip area used by TFT_ClipJpeg()

 // Initialize jpeg structures. Should be called once
bool TFT_jpegAllocate(void) {
	work = malloc(work_buff_size);				// Allocate buffer for jpeg decompression
	return (work != 0);
}

void TFT_jpegDeallocate(void) {					// Free jpeg decompression buffer, no more jpeg images can be showed
	if (work)
		free(work);
}

bool TFT_DrawJPEG(const char *filename, int16_t x, int16_t y) {
	JDEC	jdec;								// Decompression object
	FIL		jpeg_file;							// .jpeg file descriptor

	if ((x >= TFT_Width()) || (y >= TFT_Height())) return false;
	// Open requested file
	if (FR_OK != f_open(&jpeg_file, filename, FA_READ))
		return false;

	bool	free_work_area = false;				// Flag to free work area after JPEG drawing
	if (work == 0) {							// work buffer not allocated, try to allocate now
		work = malloc(work_buff_size);
		if (work != 0)
			free_work_area = true;				// work buffer was allocated in this procedure, free it later
	}
	// Prepare to JPEG decompression
	JRESULT res = jd_prepare(&jdec, readJpeg, work, work_buff_size, &jpeg_file);
	if (res == JDR_OK) {						// Ready to draw jpeg file
		res = jd_decomp(&jdec, drawJpegBuffer, 0);
	}
	f_close(&jpeg_file);
	if (free_work_area)							// If work area was allocated during this function call
		free(work);
	return (res == JDR_OK);
}

// Draw clipped JPEG file image. Put Upper-Left corner of JPEG image to (x, y)
// and draw only a rectangular area with coordinates (area_x, area_y) and size of area_width, area_height
// As slow as a drawJPEG() function because need to read whole jpeg file
bool TFT_ClipJPEG(const char *filename, int16_t x, int16_t y, uint16_t area_x, uint16_t area_y, uint16_t area_width, uint16_t area_height) {
	JDEC	jdec;								// Decompression object
	FIL		jpeg_file;							// .jpeg file descriptor

	uint16_t scr_width 	= TFT_Width();
	uint16_t scr_height	= TFT_Height();
	if (x >= scr_width || y >= scr_height ||
		area_x > scr_width || area_height > scr_width ||
		(area_x + area_width) > scr_width || (area_y + area_height) > scr_height)
		return false;

	// Open requested file
	if (FR_OK != f_open(&jpeg_file, filename, FA_READ))
		return false;

	bool	free_work_area = false;				// Flag to free work area after JPEG drawing
	if (work == 0) {							// work buffer not allocated, try to allocate now
		work = malloc(work_buff_size);
		if (work != 0)
			free_work_area = true;				// work buffer was allocated in this procedure, free it later
	}

	// Setup clip area
	area.left	= area_x;
	area.right	= area_x + area_width;
	area.top	= area_y;
	area.bottom	= area_y + area_height;

	// Prepare to JPEG decompression
	JRESULT res = jd_prepare(&jdec, readJpeg, work, work_buff_size, &jpeg_file);
	if (res == JDR_OK) {						// Ready to draw jpeg file
		res = jd_decomp(&jdec, clipJpegBuffer, 0);
	}
	f_close(&jpeg_file);
	if (free_work_area)							// If work area was allocated during this function call
		free(work);
	return (res == JDR_OK);
}


// Input data callback for TJPEG. Read more data from the .jpeg file
// Reads nbytes bytes from opened .jpeg file described by jd to buff
// Return number of bytes read
static uint16_t readJpeg(JDEC* jd, uint8_t* buff, uint16_t nbytes) {
	UINT read = 0;								// Bytes read from file
	FIL *fp = (FIL *)jd->device;				// jd->device is a file descriptor
	if (buff) {
		f_read(fp, buff, nbytes, &read);
	} else {									// If the buffer not defined, skip bytes from file
		FSIZE_t file_pos = fp->fptr;
		if (FR_OK == f_lseek(fp, file_pos + nbytes)) {
			read = nbytes;
		}
	}
	return read;
}

// Output data callback for TJPEG. Draw the rectangular data buffer, bitmap,
// described by JRECT coordinates (left, right, top, bottom)
static uint16_t drawJpegBuffer(JDEC* jd, void *bitmap, JRECT* rect) {
	uint16_t x = rect->left;					// Top-left corner coordinates
	uint16_t y = rect->top;
	uint16_t w = rect->right -  x + 1;			// rectangular area width
	uint16_t h = rect->bottom - y + 1;			// rectangular area height
	TFT_StartDrawArea(x, y, w, h);				// Set TFT address window to clipped image bounds
	uint16_t *colors = (uint16_t *)bitmap;
	for (uint32_t i = 0; i < h*w; ++i) {		// Send every pixel to the display
		TFT_ColorBlockSend(*colors++, 1);
	}
	TFT_FinishDrawArea();
	return 1;
}

// Output data callback for TJPEG. Draw the rectangular data buffer, bitmap,
// described by JRECT coordinates (left, right, top, bottom)
// clipped by area
static uint16_t clipJpegBuffer(JDEC* jd, void *bitmap, JRECT* rect) {
	// Calculate the intersection of two rectangles: rect and area
	JRECT is;
	is.top 		= u16max(area.top,		rect->top);
	is.bottom	= u16min(area.bottom,	rect->bottom);
	is.left		= u16max(area.left,		rect->left);
	is.right	= u16min(area.right,	rect->right);
	// Check the intersection is valid
	if (is.top >= is.bottom || is.left >= is.right)
		return 1;								// Nothing to draw

	uint16_t x = is.left;						// Top-left corner coordinates
	uint16_t y = is.top;
	uint16_t w = is.right -  x + 1;				// rectangular area width
	uint16_t h = is.bottom - y + 1;				// rectangular area height

	uint16_t s_left	 = is.left - rect->left;	// Left area to be skipped in the bitmap
	uint16_t s_right = rect->right - is.right;	// Right area to be skipped in the bitmap
	uint16_t bitmap_width = rect->right - rect->left + 1;

	TFT_StartDrawArea(x, y, w, h);				// Set TFT address window to clipped image bounds
	uint16_t *colors = (uint16_t *)bitmap;
	// Skip top area
	colors += bitmap_width * (is.top - rect->top);
	for (uint16_t row = 0; row < h; ++row) {	// Send pixels row by row
		colors += s_left;						// Skip left area of the bitmap
		for (uint16_t p = 0; p < w; ++p)		// Send bitmap data into clipped area
			TFT_ColorBlockSend(*colors++, 1);
		colors += s_right;						// Skip right area of the bitmap
	}
	TFT_FinishDrawArea();
	return 1;
}

static uint16_t u16max(uint16_t a, uint16_t b) {
	return (a>b)?a:b;
}

static uint16_t u16min(uint16_t a, uint16_t b) {
	return (a<b)?a:b;
}


// BMP staff

typedef struct {
	uint32_t	offset;							// Start of image data
	int			width;							// BMP picture width
	int			height;							// BMP picture height. Can be less than zero if flipped
	uint8_t		bpp;							// Bytes per pixel
} BMP_INFO;

static bool BMP_info(FIL *bmp_file, BMP_INFO *bi) {
	// Parse BMP header
	uint8_t bmp_header[34];
	UINT bytes_read = 0;
	if (FR_OK != f_read (bmp_file, bmp_header, 34, &bytes_read)) {
		return false;
	}

	if (read16(bmp_header) == 0x4D42) { 		// BMP signature
		bi->offset = read32(&bmp_header[10]); 	// Start of image data
		// Read DIB header
		bi->width  = read32(&bmp_header[18]);
		bi->height = read32(&bmp_header[22]);
		if (read16(&bmp_header[26]) == 1) {		// Number of planes -- must be '1'
			uint8_t	bmp_depth = read16(&bmp_header[28]); // bits per pixel
			uint32_t compress = read32(&bmp_header[30]); // compression code
			// Check for supported BMP format: 24-bit RGB or 16-bit R5G6R5
			if ((bmp_depth == 24 && compress == 0) || (bmp_depth == 16 && compress == 3)) {
				bi->bpp = bmp_depth >> 3;		// Bytes per pixel
				return true;
			}
		}
	}
	return false;
}

// Draw content of BMP file in rectangular area (x, y) having width = w, height = h
// Upper-Left point of BMP file is (ulx, uly)
static bool drawBMPContent(FIL *bmp_file, BMP_INFO *bi, uint16_t x, uint16_t y, uint16_t w, uint16_t h, int16_t ULx, int16_t ULy) {

	uint8_t *scanline = malloc(w*bi->bpp);	// Buffer to read whole scanline
	bool flip = true;						// BMP is stored bottom-to-top
	int bmp_height = bi->height;
	if (bmp_height < 0) {					// If bmpHeight is negative, image is in top-down order.
		bmp_height = -bmp_height;
		flip      = false;
	}
	uint8_t bpp = bi->bpp;
	// BMP rows are padded (if needed) to 4-byte boundary
	uint32_t row_size = (bi->width * bpp + 3) & ~3;

	// Set TFT address window to clipped image bounds
	TFT_StartDrawArea(x, y, w, h);

	for (uint32_t row = 0; row < h; ++row) { // For each scanline...
		// Seek to start of scan line.  It might seem labor-intensive to be doing this on every line, but this
		// method covers a lot of gritty details like cropping and scanline padding.  Also, the seek only takes
		// place if the file position actually needs to change (avoids a lot of cluster math in SD library).
		uint32_t pos = 0;
		if (flip)							// Bitmap is stored bottom-to-top order (normal BMP)
			pos = bi->offset + (bmp_height - 1 - (row + ULy)) * row_size;
		else								// Bitmap is stored top-to-bottom
			pos = bi->offset + (row + ULy) * row_size;
		pos += ULx * bpp;					// Factor in starting column (bx1)
		if (bmp_file->fptr != pos) {		// Need seek?
			f_lseek(bmp_file, pos);
		}

		// Read whole scanline
		UINT bytes_read = 0;
		if (scanline) {
			if (FR_OK != f_read(bmp_file, scanline, w*bpp, &bytes_read)) {
				free(scanline);
				TFT_FinishDrawArea();
				return false;
			}
			for (uint32_t col = 0; col < w*bpp; col += bpp) { // For each pixel...
				uint16_t color = readPixel(&scanline[col], bpp);
				TFT_ColorBlockSend(color, 1);
			}								// end pixel
		} else {							// Failed to allocate memory for scanline, read data by one pixel
			for (uint32_t col = 0; col < w; ++col) { // For each pixel...
				uint8_t clr[3];
				if (FR_OK != f_read(bmp_file, clr, bpp, &bytes_read)) {
					TFT_FinishDrawArea();
					return false;
				}
				uint16_t color = readPixel(clr, bpp);
				TFT_ColorBlockSend(color, 1);
			}								// end pixel
		}
	}										// end scanline
	TFT_FinishDrawArea(); 					// End last TFT transaction
	if (scanline) free(scanline);
	return true;
}

static bool scrollOnBMP(FIL *bmp_file, BMP_INFO *bi, uint16_t x, uint16_t y, uint16_t w, uint16_t h, int16_t ULx, int16_t ULy,
		const uint8_t *bitmap, uint16_t bm_width, int16_t offset, uint8_t gap, uint16_t txt_color) {

	uint8_t *scanline = malloc(w*bi->bpp);	// Buffer to read whole scanline
	if (!scanline) return false;

	bool flip = true;						// BMP is stored bottom-to-top
	int bmp_height = bi->height;
	if (bmp_height < 0) {					// If bmpHeight is negative, image is in top-down order.
		bmp_height = -bmp_height;
		flip      = false;
	}
	uint8_t bpp = bi->bpp;
	// BMP rows are padded (if needed) to 4-byte boundary
	uint32_t row_size = (bi->width * bpp + 3) & ~3;

	// Set TFT address window to clipped image bounds
	TFT_StartDrawArea(x, y, w, h);

    uint16_t bytes_per_row = (bm_width + 7) >> 3;

    // Write color data row by row
    for (uint16_t row = 0; row < h; ++row) {
    	uint16_t out_bit = 0;					// Number of bits were pushed out

    	uint32_t pos = 0;
		if (flip)							// Bitmap is stored bottom-to-top order (normal BMP)
			pos = bi->offset + (bmp_height - 1 - (row + ULy)) * row_size;
		else								// Bitmap is stored top-to-bottom
			pos = bi->offset + (row + ULy) * row_size;
		pos += ULx * bpp;					// Factor in starting column (bx1)
		if (bmp_file->fptr != pos) {		// Need seek?
			f_lseek(bmp_file, pos);
		}

		// Read whole BMP scanline for background colors
		UINT bytes_read = 0;
		if (FR_OK != f_read(bmp_file, scanline, w*bpp, &bytes_read)) {
			free(scanline);
			TFT_FinishDrawArea();
			return false;
		}

    	if (offset < 0) {					// Negative offset means bitmap should be shifted right
    		// Fill-up left side with background color
    		out_bit = abs(offset);
    		if (out_bit > w) out_bit = w;
			for (uint32_t col = 0; col < out_bit*bpp; col += bpp) { // For each pixel...
				uint16_t color = readPixel(&scanline[col], bpp);
				TFT_ColorBlockSend(color, 1);
			}
    	}
    	// Send bitmap row
    	for (uint16_t bit = (offset > 0)?offset:0; bit < bm_width; ++bit) {
    		if (out_bit >= w)				// row is over
    			break;
    		uint16_t in_byte = row * bytes_per_row + (bit >> 3);
    		uint8_t  in_mask = 0x80 >> (bit & 0x7);
    		uint16_t color 	 = (in_mask & bitmap[in_byte])?txt_color:readPixel(&scanline[out_bit*bpp], bpp);
    		TFT_ColorBlockSend(color, 1);
    		++out_bit;
    	}
    	if (gap == 0) {						// Not looped bitmap. Fill-up rest area with background image
    		if (w > out_bit) {
    			for (uint32_t col = out_bit*bpp; col < w*bpp; col += bpp) { // For each pixel...
    				uint16_t color = readPixel(&scanline[col], bpp);
    				TFT_ColorBlockSend(color, 1);
    			}
    		}
    		break;							// row is over
    	} else {							// Looped bitmap
    		uint8_t bg_bits = gap;			// Draw the gap between looped bitmap images
    		if (w > out_bit) {				// There is a place to draw
    		    if (bg_bits > (w - out_bit))
    		    	bg_bits = w - out_bit;
    		    for (uint16_t i = 0; i < bg_bits; ++i) {
    		    	uint16_t color = readPixel(&scanline[out_bit*bpp], bpp);
    		    	TFT_ColorBlockSend(color, 1);
    		    	++out_bit;
    		    }
    		}
    		// Start over the bitmap
    		for (uint16_t bit = 0; bit < bm_width; ++bit) {
    			if (out_bit >= w)			// row is over
    				break;
    			uint16_t in_byte = row * bytes_per_row + (bit >> 3);
    			uint8_t  in_mask = 0x80 >> (bit & 0x7);
    			uint16_t color = (in_mask & bitmap[in_byte])?txt_color:readPixel(&scanline[out_bit*bpp], bpp);
    			TFT_ColorBlockSend(color, 1);
    			++out_bit;
    		}
    	}
    }
    // Send rest data from the buffer
    TFT_FinishDrawArea();						// Flush color block buffer
	free(scanline);
	return true;
}

// Draws BMP file image at the given coordinates.
// Both coordinates can be less than zero. In this case, top-left area will be clipped
// Only two BMP formats are supported: 24-bit per pixel, not-compressed and 16-bit per pixel R5-G6-B5
// For best performance, save BMP file as 16-bits R6-G5-B6 format and flipped row order
bool TFT_DrawBMP(const char *filename, int16_t x, int16_t y) {
	FIL			bmp_file;
	if ((x >= TFT_Width()) || (y >= TFT_Height())) return false;

	// Open requested file
	if (FR_OK != f_open(&bmp_file, filename, FA_READ))
		return false;
	bool ret = false;

	// Parse BMP header
	BMP_INFO bi;
	if (BMP_info(&bmp_file, &bi)) {
		int bmp_height = bi.height;
		if (bmp_height < 0) {					// If bmpHeight is negative, image is in top-down order.
			bmp_height = -bmp_height;
		}

		// Crop area to be loaded
		int x2 = x + bi.width  - 1;				// Lower-right corner
		int y2 = y + bmp_height - 1;
		if ((x2 >= 0) && (y2 >= 0)) {			// On screen?
			int w = bi.width;					// Width/height of section to load/display
			int h = bmp_height;
			int ulx = 0;						// Upper-Left coordinate in BMP file
			int uly = 0;
			if (x < 0) {						// Clip left
				ulx = -x;
				x   = 0;
				w   = x2 + 1;
			}
			if (y < 0) {						// Clip top
				uly = -y;
				y   = 0;
				h   = y2 + 1;
			}
			if (x2 >= TFT_Width())  w = TFT_Width()  - x; // Clip right
			if (y2 >= TFT_Height()) h = TFT_Height() - y; // Clip bottom

			ret = drawBMPContent(&bmp_file, &bi, x, y, w, h, ulx, uly);
		}
	}
	f_close(&bmp_file);
	return ret;
}

// Draw clipped BMP file image. Put Upper-Left corner of BMP image to (x, y)
// and draw only a rectangular area with coordinates (area_x, area_y) and size of area_width, area_height
// Only two BMP formats are supported: 24-bit per pixel, not-compressed and 16-bit per pixel R5-G6-B5
bool TFT_ClipBMP(const char *filename, int16_t x, int16_t y, uint16_t area_x, uint16_t area_y, uint16_t area_width, uint16_t area_height) {
	FIL			bmp_file;
	uint16_t scr_width 	= TFT_Width();
	uint16_t scr_height	= TFT_Height();
	if (x >= scr_width || y >= scr_height ||
		area_x > scr_width || area_height > scr_width ||
		(area_x + area_width) > scr_width || (area_y + area_height) > scr_height ||
		area_width < 1 || area_height < 1)
		return false;

	// Open requested file
	if (FR_OK != f_open(&bmp_file, filename, FA_READ))
		return false;
	bool ret = false;

	// Parse BMP header
	BMP_INFO bi;
	if (BMP_info(&bmp_file, &bi)) {
		int bmp_height = bi.height;
		if (bmp_height < 0) {					// If bmpHeight is negative, image is in top-down order.
			bmp_height = -bmp_height;
		}
		// Check that BMP file can cover up the drawing area completely
		if (x <= area_x && y <= area_y && bi.width >= (area_x - x + area_width) && bmp_height >= (area_y - y + area_height)) {
			ret = drawBMPContent(&bmp_file, &bi, area_x, area_y, area_width, area_height, area_x - x, area_y - y);
		}
	}
	f_close(&bmp_file);
	return ret;
}

// Draw scrolled bitmap on BMP background
bool TFT_ScrollBitmapOverBMP(const char *filename, int16_t x, int16_t y, uint16_t area_x, uint16_t area_y, uint16_t area_width, uint16_t area_height,
		const uint8_t *bitmap, uint16_t bm_width, int16_t offset, uint8_t gap, uint16_t txt_color) {

	FIL			bmp_file;
	uint16_t scr_width 	= TFT_Width();
	uint16_t scr_height	= TFT_Height();
	if (x >= scr_width || y >= scr_height || area_x > scr_width || area_height > scr_width ||
		(area_x + area_width) > scr_width || (area_y + area_height) > scr_height ||
		area_width < 1 || area_height < 1 || bm_width < 1 || !bitmap)
		return false;
	while (offset >= bm_width) offset -= bm_width + gap;

	// Open requested file
	if (FR_OK != f_open(&bmp_file, filename, FA_READ))
		return false;
	bool ret = false;

	// Parse BMP header
	BMP_INFO bi;
	if (BMP_info(&bmp_file, &bi)) {
		int bmp_height = bi.height;
		if (bmp_height < 0) {					// If bmpHeight is negative, image is in top-down order.
			bmp_height = -bmp_height;
		}
		// Check that BMP file can cover up the drawing area completely
		if (x <= area_x && y <= area_y && bi.width >= (area_x - x + area_width) && bmp_height >= (area_y - y + area_height)) {
			ret = scrollOnBMP(&bmp_file, &bi, area_x, area_y, area_width, area_height, area_x - x, area_y - y, bitmap, bm_width, offset, gap, txt_color);
		}
	}
	f_close(&bmp_file);
	return ret;
}


static uint16_t read16(uint8_t *ptr) {
	return ptr[0] | (ptr[1] << 8);
}

static uint32_t read32(uint8_t *ptr) {
	uint32_t result	= *ptr++;
	result |= *ptr++ << 8;
	result |= *ptr++ << 16;
	result |= *ptr	 << 24;
	return result;
}

static uint16_t readPixel(uint8_t *ptr, uint8_t bpp) {
	if (bpp == 3) {								// 24-bit color
		return ((ptr[2] & 0xF8) << 8) | ((ptr[1] & 0xFC) << 3) | (ptr[0] >> 3);
	}
	// 16-bit color R5-G6-B5
	return ptr[1] << 8 | ptr[0];
}

#endif /* TFT_BMP_JPEG_ENABLE */
