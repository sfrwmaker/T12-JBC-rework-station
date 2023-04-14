/*
 * ll_fsmc.c
 *
 *  Created on: 4 Nov 2022
 *      Author: Alex
 */

#include "ll_fsmc.h"

#ifdef FSMC_LCD_CMD

static uint8_t *LCD_CMD_PORT 	= (uint8_t *)FSMC_LCD_CMD;
static uint8_t *LCD_DATA_PORT	= (uint8_t *)FSMC_LCD_DATA;

// Hard reset pin management
static void TFT_FSMC_RST(GPIO_PinState state) {
	HAL_GPIO_WritePin(TFT_RESET_GPIO_Port, TFT_RESET_Pin, state);
}

// HARDWARE RESET
void TFT_FSMC_Reset(void) {
	TFT_FSMC_RST(GPIO_PIN_RESET);
	HAL_Delay(200);
	TFT_FSMC_RST(GPIO_PIN_SET);
}

// Send command (byte) to the Display followed by the command arguments
void TFT_FSMC_Command(uint8_t cmd, const uint8_t* buff, size_t buff_size) {
	*LCD_CMD_PORT = cmd;
	if (buff && buff_size > 0) {
		for (uint16_t i = 0; i <  buff_size; ++i) {
			*LCD_DATA_PORT	= buff[i];
		}
	}
}

bool TFT_FSMC_ReadData(uint8_t cmd, uint8_t *data, uint16_t size) {
	if (!data || size == 0) return 0;

	*LCD_CMD_PORT = cmd;
	for (uint16_t i = 0; i < size; ++i) {
		data[i] = *LCD_DATA_PORT;
	}
	return true;
}

void TFT_FSMC_ColorBlockSend_16bits(uint16_t color, uint32_t size) {
	uint8_t clr_hi = color >> 8;
	uint8_t clr_lo = color & 0xff;
	for (uint32_t i = 0; i < size; ++i) {
		*LCD_DATA_PORT = clr_hi;
		*LCD_DATA_PORT = clr_lo;
	}
}

void TFT_FSMC_ColorBlockSend_18bits(uint16_t color, uint32_t size) {
	// Convert to 18-bits color
	uint8_t r = (color & 0xF800) >> 8;
	uint8_t g = (color & 0x7E0)  >> 3;
	uint8_t b = (color & 0x1F)   << 3;
	for (uint32_t i = 0; i < size; ++i) {
		*LCD_DATA_PORT = r;
		*LCD_DATA_PORT = g;
		*LCD_DATA_PORT = b;
	}
}

void TFT_FSMC_DATA_MODE(void) { }
void TFT_FSMC_ColorBlockInit(void)	{ }
void TFT_FSMC_ColorBlockFlush(void) { }

#endif				// FSMC_LCD_CMD
