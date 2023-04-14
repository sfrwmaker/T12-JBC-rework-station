/*
 * config.h
 *
 *  Created on: 4 Nov 2022
 *      Author: Alex
 */

#ifndef _TFT_CONFIG_H_
#define _TFT__CONFIG_H_

#include "main.h"

// Un-comment the following line to include code for BMP and JPEG drawings, external FAT drive required
//#define TFT_BMP_JPEG_ENABLE


// ========================SPI Interface definitions====================
#define TFT_SPI_PORT		hspi1
extern SPI_HandleTypeDef 	TFT_SPI_PORT;

// To enable DMA support, Un-comment the next line
#define TFT_USE_DMA			1
// =========================End of SPI Interface definitions============

/*
// ========================FSMC Interface definitions===================
#define FSMC_LCD_CMD   	0x60000000
// LCD DATA Bit Mask, FSMC_A16 = 1 0000 0000 0000 0000
#define FSMC_LCD_DATA 	0x60010000
// ========================End of FSMC Interface definitions============
*/

#define		TFT_Delay(a)	HAL_Delay(a);

#endif				// _TFT_CONFIG_H
