/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define SD_CS_Pin GPIO_PIN_13
#define SD_CS_GPIO_Port GPIOC
#define FAN_CURRENT_Pin GPIO_PIN_0
#define FAN_CURRENT_GPIO_Port GPIOC
#define FLASH_MOSI_Pin GPIO_PIN_1
#define FLASH_MOSI_GPIO_Port GPIOC
#define FLASH_MISO_Pin GPIO_PIN_2
#define FLASH_MISO_GPIO_Port GPIOC
#define FLASH_CS_Pin GPIO_PIN_3
#define FLASH_CS_GPIO_Port GPIOC
#define T12_POWER_Pin GPIO_PIN_0
#define T12_POWER_GPIO_Port GPIOA
#define JBC_POWER_Pin GPIO_PIN_1
#define JBC_POWER_GPIO_Port GPIOA
#define T12_CURRENT_Pin GPIO_PIN_2
#define T12_CURRENT_GPIO_Port GPIOA
#define JBC_CURRENT_Pin GPIO_PIN_3
#define JBC_CURRENT_GPIO_Port GPIOA
#define T12_TEMP_Pin GPIO_PIN_4
#define T12_TEMP_GPIO_Port GPIOA
#define JBC_TEMP_Pin GPIO_PIN_5
#define JBC_TEMP_GPIO_Port GPIOA
#define GUN_TEMP_Pin GPIO_PIN_6
#define GUN_TEMP_GPIO_Port GPIOA
#define AMBIENT_Pin GPIO_PIN_7
#define AMBIENT_GPIO_Port GPIOA
#define TILT_SW_Pin GPIO_PIN_4
#define TILT_SW_GPIO_Port GPIOC
#define FLASH_SCK_Pin GPIO_PIN_10
#define FLASH_SCK_GPIO_Port GPIOB
#define JBC_CHANGE_Pin GPIO_PIN_12
#define JBC_CHANGE_GPIO_Port GPIOB
#define JBC_STBY_Pin GPIO_PIN_13
#define JBC_STBY_GPIO_Port GPIOB
#define TFT_BRIGHT_Pin GPIO_PIN_14
#define TFT_BRIGHT_GPIO_Port GPIOB
#define I_ENC_B_Pin GPIO_PIN_15
#define I_ENC_B_GPIO_Port GPIOB
#define I_ENC_L_Pin GPIO_PIN_6
#define I_ENC_L_GPIO_Port GPIOC
#define I_ENC_R_Pin GPIO_PIN_7
#define I_ENC_R_GPIO_Port GPIOC
#define REED_SW_Pin GPIO_PIN_9
#define REED_SW_GPIO_Port GPIOA
#define AC_RELAY_Pin GPIO_PIN_10
#define AC_RELAY_GPIO_Port GPIOA
#define GUN_POWER_Pin GPIO_PIN_11
#define GUN_POWER_GPIO_Port GPIOA
#define AC_ZERO_Pin GPIO_PIN_12
#define AC_ZERO_GPIO_Port GPIOA
#define TFT_RESET_Pin GPIO_PIN_15
#define TFT_RESET_GPIO_Port GPIOA
#define TFT_DC_Pin GPIO_PIN_10
#define TFT_DC_GPIO_Port GPIOC
#define TFT_CS_Pin GPIO_PIN_11
#define TFT_CS_GPIO_Port GPIOC
#define TFT_SCK_Pin GPIO_PIN_3
#define TFT_SCK_GPIO_Port GPIOB
#define G_ENC_B_Pin GPIO_PIN_4
#define G_ENC_B_GPIO_Port GPIOB
#define TFT_SDI_Pin GPIO_PIN_5
#define TFT_SDI_GPIO_Port GPIOB
#define G_ENC_L_Pin GPIO_PIN_6
#define G_ENC_L_GPIO_Port GPIOB
#define G_ENC_R_Pin GPIO_PIN_7
#define G_ENC_R_GPIO_Port GPIOB
#define BUZZER_Pin GPIO_PIN_8
#define BUZZER_GPIO_Port GPIOB
#define FAN_POWER_Pin GPIO_PIN_9
#define FAN_POWER_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#define FW_VERSION	("1.12")
//#define DEBUG_ON
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
