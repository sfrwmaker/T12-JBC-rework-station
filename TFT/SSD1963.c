/*
 * SSD1963.c
 *
 *  Created on: May 15, 2024
 *
 *  Based of source code of anothermist, https://github.com/anothermist/DISPLAYS
 *  UNDER GNU v3 licence
 */

#include <stdint.h>
#include "SSD1963.h"
#include "gamma.h"

// Command list starts on chapter 8 of datasheet
typedef enum {
	CMD_NOP_00			= 0x00,
	CMD_SWRESET_01		= 0x01,
	CMD_RDDID_04		= 0x04,
	CMD_RDERR_DSI_05	= 0x05,
	CMD_RDDST_09		= 0x09,
	CMD_RDDPM_0A		= 0x0A,
	CMD_RDDMADCTL_0B	= 0x0B,
	CMD_RDDCOLMOD_0C	= 0x0C,
	CMD_RDDIM_0D		= 0x0D,
	CMD_RDDSM_0E		= 0x0E,
	CMD_RDDSDR_0F		= 0x0F,
	CMD_SLPIN_10		= 0x10,
	CMD_SLPOUT_11		= 0x11,
	CMD_PTLON_12		= 0x12,
	CMD_NORON_13		= 0x13,
	CMD_DINVOFF_20		= 0x20,
	CMD_DINVON_21		= 0x21,
	CMD_DISPOFF_28		= 0x28,
	CMD_DISPON_29		= 0x29,
	CMD_CASET_2A		= 0x2A,
	CMD_PASET_2B		= 0x2B,
	CMD_RAMWR_2C		= 0x2C,
	CMD_RAMRD_2E		= 0x2E,
	CMD_PTLAR_30		= 0x30,
	CMD_VSCRDEF_33		= 0x33,
	CMD_TEOFF_34		= 0x34,
	CMD_TEON_35			= 0x35,
	CMD_MADCTL_36		= 0x36,
	CMD_VSCRSADD_37		= 0x37,
	CMD_IDMOFF_38		= 0x38,
	CMD_IDMON_39		= 0x39,
	CMD_PIXSET_3A		= 0x3A,
	CMD_RDMEMCONT_3E	= 0x3E,
	CMD_TESCAN_44		= 0x44,
	CMD_RDTESCAN_45		= 0x45,
	CMD_WRDISBV_51		= 0x51,
	CMD_RDDISBV_52		= 0x52,
	CMD_WRCTRLD_53		= 0x53,
	CMD_RDCTRLD_54		= 0x54,
	CMD_WRCABC_55		= 0x55,
	CMD_RDCABC_56		= 0x56,
	CMD_WRCABCMB_5E		= 0x5E,
	CMD_RDCABCMB_5F		= 0x5F,
	CMD_RDFCHKSUM_AA	= 0xAA,
	CMD_RDCCHKSUM_AF	= 0xAF,
	CMD_RDID1_DA		= 0xDA,
	CMD_RDID2_DB		= 0xDB,
	CMD_RDID3_DC		= 0xDC,
	CMD_IFMODE_B0		= 0xB0,
	CMD_FRMCTR1_B1		= 0xB1,
	CMD_FRMCTR2_B2		= 0xB2,
	CMD_FRMCTR3_B3		= 0xB3,
	CMD_INVTR_B4		= 0xB4,
	CMD_BPC_B5			= 0xB5,
	CMD_DFC_B6			= 0xB6,
	CMD_EM_B7			= 0xB7,
	CMD_VCL_B8			= 0xB8,
	CMD_VGH_B9			= 0xB9,
	CMD_VGLX_BA			= 0xBA,
	CMD_VGSP_BC			= 0xBC,
	CMD_VGSN_BD			= 0xBD,
	CMD_VCOM_BE			= 0xBE,
	CMD_VGH_BF			= 0xBF,
	CMD_PWCTRL1_C0		= 0xC0,
	CMD_PWCTRL2_C1		= 0xC1,
	CMD_PWCTRL3_C2		= 0xC2,
	CMD_VMCTRL1_C5		= 0xC5,
	CMD_VCMOFFS_C6		= 0xC6,
	CMD_TIM_C9			= 0xC9,
	CMD_BOE_CC			= 0xCC,
	CMD_NVMADW_D0		= 0xD0,
	CMD_GAMMA_D1		= 0xD1,
	CMD_GAMMA_D2		= 0xD2,
	CMD_GAMMA_D3		= 0xD3,
	CMD_GAMMA_D4		= 0xD4,
	CMD_GAMMA_D5		= 0xD5,
	CMD_GAMMA_D6		= 0xD6,
	CMD_PGAMCTRL_E0		= 0xE0,
	CMD_NGAMCTRL_E1		= 0xE1,
	CMD_DGAMCTRL_E2		= 0xE2,
	CMD_DGAMCTRL_E3		= 0xE3,
	CMD_LSHIFT_FREQ_E6	= 0xE6,
	CMD_DOCA_E8			= 0xE8,
	CMD_CSCON_F0		= 0xF0,
	CMD_SPIRC_FB		= 0xFB,
	CMD_TIM_FF			= 0xFF
} SSD1963_CMD;

// Initialize the Display
void SSD1963_Init(void) {
	// Initialize display interface. By default the library guess the display interface
	TFT_InterfaceSetup(TFT_16bits, 0);
	// Reset display hardware
	TFT_DEF_Reset();
	// SOFTWARE RESET
	TFT_Command(CMD_SWRESET_01,	0, 0);
	TFT_Delay(150);

	TFT_Command(CMD_DGAMCTRL_E2, (uint8_t *)"\x1d\x02\xFF", 3);			// set frequency^ presceller(M=29), multiplier(N=2), on-off multiplier and presceller
	TFT_Command(CMD_PGAMCTRL_E0, (uint8_t *)"\x01", 1);					// Turn on the PLL
	TFT_Delay(120);														// Wait for 100us to let the PLL stable and read the PLL lock status bit.
	TFT_Command(CMD_PGAMCTRL_E0, (uint8_t *)"\x03", 1);					// Switch the clock source to PLL
	TFT_Delay(120);
	TFT_Command(CMD_SWRESET_01,	0, 0);
	TFT_Delay(120);

	TFT_Command(CMD_LSHIFT_FREQ_E6, (uint8_t *)"\x03\x85\x1d", 3);		// Configure the dot clock frequency
	TFT_Command(CMD_IFMODE_B0, (uint8_t *)"\xAE\x20\x03\x1F\x01\xDF\x2D", 7); // Set the panel size to 480 x 800 and polarity
	TFT_Command(CMD_INVTR_B4, (uint8_t *)"\x03\xA0\x00\x2E\x30\x00\x0F\x00", 8); // Horizontal Display Period
	TFT_Command(CMD_DFC_B6, (uint8_t *)"\x02\x0D\x00\x10\x10\x08\x00", 7);
	TFT_Command(CMD_VCOM_BE, (uint8_t *)"\x08\xFF\x01", 3);				// PWM signal frequency = PLL clock / (256 * (PWMF[7:0] + 1)) / 256
	TFT_Command(CMD_MADCTL_36, 0, 0);									// set address_mode
	TFT_Command(CMD_CSCON_F0, (uint8_t *)"\x03", 1);					// mcu interface config, 16 bit interface (565)

#ifdef APPLY_GAMMA_PROFILE
	TFT_Gamma_NT35510();												// Gamma Setting
#endif

	// Exit sleep mode
	TFT_DEF_SleepOut();

	// Turn On Display
	TFT_Command(CMD_DISPON_29,	0, 0);
	TFT_Delay(5);
	TFT_Command(CMD_RAMWR_2C,	0, 0);

	// Setup display parameters
	uint8_t rot[4] = {0x00, 0x20|0x40, 0x80|0x40, 0x80|0x20};
	TFT_Setup(SSD1963_SCREEN_WIDTH, SSD1963_SCREEN_HEIGHT, rot);
}
