/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "key.h"
#include "oled.h"
#include "hc05.h"
#include "bh1750.h"
#include "bmp280.h"
#include "rgbled.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* 静态颜色数组 */
static const uint8_t static_colors[4][3] = {
    {255, 0, 0},    // 红
    {0, 255, 0},    // 绿
    {0, 0, 255},    // 蓝
    {255, 255, 255} // 白
};

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

#define MODE_STATIC   0
#define MODE_RAINBOW  1
#define MODE_BREATH   2
#define MODE_TEMP     3
#define MODE_MAX      3

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

static uint8_t sys_mode    = MODE_TEMP;
static uint8_t static_r = 0, static_g = 188, static_b = 199;
static uint8_t color_idx   = 0;
static uint8_t led_enable  = 1;

static int16_t  bmp_temp10  = 0;
static uint16_t bmp_press10 = 0;
static uint16_t bh1750_lux10 = 0;

static uint32_t last_sensor_display = 0;
static uint32_t last_led_update     = 0;
static uint32_t last_bt_send        = 0;

/* 刷新节流 */
static int16_t  last_temp10  = -300;   // 不可能的值
static uint16_t last_press10 = 0;
static uint16_t last_lux10   = 65535;
static int last_bt_conn = -1;   // -1 表示未知，0=未连接，1=已连接

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

static void ProcessBtCommand(char *cmd);
static void UpdateModeDisplay(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C2_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  
  Key_Init();
  OLED_Init();
  HC05_Init();

  BH1750_Init();
  if (!BMP280_Init()) {
      OLED_ShowString(1, 1, "BMP280 Fail");
      OLED_RefreshPage(0); OLED_RefreshPage(1);
      while (1);
  }
  OLED_Clear();

  RGB_Init();
  UpdateModeDisplay();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  
	  /* 按键处理 */
      uint8_t e1 = Key_GetEvent(KEY1);
      uint8_t e2 = Key_GetEvent(KEY2);

      if (e1 == KEY_EVENT_SINGLE_CLICK) {
          color_idx = (color_idx + 1) % 4;
          static_r = static_colors[color_idx][0];
          static_g = static_colors[color_idx][1];
          static_b = static_colors[color_idx][2];
          sys_mode = MODE_STATIC;
          led_enable = 1;
          RGB_SetColor(static_r, static_g, static_b);
          UpdateModeDisplay();
      } else if (e1 == KEY_EVENT_LONG_PRESS) {
          led_enable = 0;
          RGB_SetColor(0, 0, 0);
      }

      if (e2 == KEY_EVENT_SINGLE_CLICK) {
          if (++sys_mode > MODE_MAX) sys_mode = 0;
          led_enable = 1;
          if (sys_mode == MODE_STATIC) {
              RGB_SetColor(static_r, static_g, static_b);
          }
          UpdateModeDisplay();
      } else if (e2 == KEY_EVENT_LONG_PRESS) {
          led_enable = 0;
          RGB_SetColor(0, 0, 0);
      }

      /* 蓝牙接收 */
      if (bt_rx_ready) {
          bt_rx_ready = 0;
          char cmd[64] = {0};
          uint16_t len = (bt_rx_len < 63) ? bt_rx_len : 63;
          memcpy(cmd, bt_rx_buf, len);
          cmd[len] = '\0';
          ProcessBtCommand(cmd);
      }

      BH1750_Task();
      BMP280_Task();

      /* 定时显示传感器数据 (200ms) */
      if (HAL_GetTick() - last_sensor_display >= 200) {
          last_sensor_display = HAL_GetTick();

          int bt_conn = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8) == GPIO_PIN_SET) ? 1 : 0;

          int16_t  temp10;
          uint16_t press10;
          if (BMP280_GetData(&temp10, &press10)) {
              bmp_temp10  = temp10;
              bmp_press10 = press10;

              if (temp10 != last_temp10 || bt_conn != last_bt_conn) {
                  last_temp10 = temp10;
                  char buf[17];
                  snprintf(buf, sizeof(buf), "T:%d.%d C BT:%s ",
                           temp10/10, abs(temp10)%10,
                           bt_conn ? "OK" : "NO");
                  OLED_ShowString(1, 1, buf);
                  OLED_RefreshPage(0); OLED_RefreshPage(1);
              }

              if (press10 != last_press10) {
                  last_press10 = press10;
                  char buf[17];
                  snprintf(buf, sizeof(buf), "P:%d.%d hPa", press10/10, press10%10);
                  OLED_ShowString(2, 1, buf);
                  OLED_RefreshPage(2); OLED_RefreshPage(3);
              }
          } else {
              if (bt_conn != last_bt_conn) {
                  char buf[17];
                  snprintf(buf, sizeof(buf), "T:--.- C BT:%s ", bt_conn ? "OK" : "NO");
                  OLED_ShowString(1, 1, buf);
                  OLED_RefreshPage(0); OLED_RefreshPage(1);
                  last_temp10 = -300;
              }
          }

          last_bt_conn = bt_conn;

          uint16_t lux10;
          if (BH1750_GetLux(&lux10)) {
              bh1750_lux10 = lux10;
              if (lux10 != last_lux10) {
                  last_lux10 = lux10;
                  char buf[17];
                  snprintf(buf, sizeof(buf), "L:%d.%d lx     ", lux10/10, lux10%10);
                  OLED_ShowString(3, 1, buf);
                  OLED_RefreshPage(4); OLED_RefreshPage(5);
              }
          }
      }

      /* 定时蓝牙上报 (5s) */
      if (HAL_GetTick() - last_bt_send >= 5000) {
          last_bt_send = HAL_GetTick();
          char msg[64];
          snprintf(msg, sizeof(msg), "#DATA:T=%d.%d,P=%d.%d,L=%d.%d\r\n",
                   bmp_temp10/10, abs(bmp_temp10)%10,
                   bmp_press10/10, bmp_press10%10,
                   bh1750_lux10/10, bh1750_lux10%10);
          HC05_SendString(msg);
      }

      /* 动态灯光效果 (20ms) */
      if (HAL_GetTick() - last_led_update >= 20) {
          last_led_update = HAL_GetTick();
          if (led_enable) {
              switch (sys_mode) {
                  case MODE_RAINBOW:
                      RGB_RainbowTask();
                      break;
                  case MODE_BREATH:
                      RGB_BreathTask();
                      break;
                  case MODE_TEMP:
                      {
                          int16_t temp = bmp_temp10;
                          if (temp < 150) temp = 150;
                          if (temp > 350) temp = 350;
                          uint16_t hue = (uint16_t)(240 - ((temp - 150) * 12) / 10);
                          RGB_SetHue(hue);
                      }
                      break;
                  default:
                      break;
              }
          }
      }
	  
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* 蓝牙指令解析 */
static void ProcessBtCommand(char *cmd)
{
    size_t len = strlen(cmd);
    while (len > 0 && (cmd[len-1] == '\r' || cmd[len-1] == '\n')) {
        cmd[--len] = '\0';
    }

    char *p = strchr(cmd, '#');
    if (p == NULL) return;
    p++;

    if (strncmp(p, "LED:", 4) == 0) {
        p += 4;
        char *tokR = strtok(p, " ,\t");
        char *tokG = strtok(NULL, " ,\t");
        char *tokB = strtok(NULL, " ,\t");
        if (tokR && tokG && tokB) {
            int r = atoi(tokR);
            int g = atoi(tokG);
            int b = atoi(tokB);
            if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
                static_r = r; static_g = g; static_b = b;
                sys_mode = MODE_STATIC;
                led_enable = 1;
                RGB_SetColor(static_r, static_g, static_b);
                UpdateModeDisplay();
            }
        }
    } else if (strncmp(p, "MODE:", 5) == 0) {
        int mode = atoi(p + 5);
        if (mode >= 0 && mode <= MODE_MAX) {
            sys_mode = mode;
            led_enable = 1;
            if (mode == MODE_STATIC) {
                RGB_SetColor(static_r, static_g, static_b);
            }
            UpdateModeDisplay();
        }
    } else if (strncmp(p, "QUERY", 5) == 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "#DATA:T=%d.%d,P=%d.%d,L=%d.%d\r\n",
                 bmp_temp10/10, abs(bmp_temp10)%10,
                 bmp_press10/10, bmp_press10%10,
                 bh1750_lux10/10, bh1750_lux10%10);
        HC05_SendString(msg);
    }
}

/* 更新第四行模式显示 */
static void UpdateModeDisplay(void)
{
    const char *mode_names[] = {"STATIC", "RAINBOW", "BREATH", "TEMP"};
    char buf[17];
    snprintf(buf, sizeof(buf), "MODE:%-10s ", mode_names[sys_mode]);
    OLED_ShowString(4, 1, buf);
    OLED_RefreshPage(6);
    OLED_RefreshPage(7);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
