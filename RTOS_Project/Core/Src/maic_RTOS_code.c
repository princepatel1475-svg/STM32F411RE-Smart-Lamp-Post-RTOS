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
#include "cmsis_os.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "dht11.h"
#include <stdio.h>
#include <string.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "dht11.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart1;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for DHTTask */
osThreadId_t DHTTaskHandle;
const osThreadAttr_t DHTTask_attributes = {
  .name = "DHTTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for ADCTask */
osThreadId_t ADCTaskHandle;
const osThreadAttr_t ADCTask_attributes = {
  .name = "ADCTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for OLEDTask */
osThreadId_t OLEDTaskHandle;
const osThreadAttr_t OLEDTask_attributes = {
  .name = "OLEDTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* USER CODE BEGIN PV */
uint16_t mq135_val = 0;
uint16_t rain_val  = 0;
uint8_t  ldr_dark  = 0;

uint8_t dht_temp = 25;
uint8_t dht_hum  = 50;
uint8_t dht_ok   = 0;

RTC_TimeTypeDef sTime;
RTC_DateTypeDef sDate;

uint8_t  screen      = 0;
uint32_t lastSwitch  = 0;
uint32_t lastDHT     = 0;
uint32_t lastSensor  = 0;

char buf[32];
/* USER CODE END PV */

// Mutex handle
osMutexId_t oledMutexHandle;
const osMutexAttr_t oledMutex_attributes = {
  .name = "oledMutex"
};

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_RTC_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART1_UART_Init(void);
void StartDefaultTask(void *argument);
void StartDHT11Task(void *argument);
void StartADCTask(void *argument);
void StartOLEDTask(void *argument);

/* USER CODE BEGIN 0 */
uint16_t ADC_Read(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel      = channel;
    sConfig.Rank         = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 100);
    uint16_t val = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return val;
}
/* USER CODE END 0 */

int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_RTC_Init();
  MX_I2C1_Init();
  MX_TIM1_Init();
  MX_USART1_UART_Init();

  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start(&htim1);

  // Init OLED once before tasks start
  ssd1306_Init();
  ssd1306_Fill(Black);
  ssd1306_UpdateScreen();

  // RTC set only on very first flash using backup register
  // 0xBEEF in BKP_DR0 = "already set" — survives power cycle
  // No need to ever comment this out — runs only once automatically
     // ← just the call, nothing else
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* creation of oledMutex */
  oledMutexHandle = osMutexNew(&oledMutex_attributes);

  /* Create the thread(s) */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
  DHTTaskHandle     = osThreadNew(StartDHT11Task,   NULL, &DHTTask_attributes);
  ADCTaskHandle     = osThreadNew(StartADCTask,     NULL, &ADCTask_attributes);
  OLEDTaskHandle    = osThreadNew(StartOLEDTask,    NULL, &OLEDTask_attributes);

  /* Start scheduler */
  osKernelStart();

  while (1) {}
}

/* USER CODE BEGIN 4 */

// ============ TASK 1: DHT11 — reads every 2 seconds ============
void StartDHT11Task(void *argument)
{
    uint8_t t = 0, h = 0;
    for(;;)
    {
        if (DHT11_Read(&t, &h) && t > 0 && t < 60 && h > 0 && h <= 100)
        {
            dht_temp = t;
            dht_hum  = h;
            dht_ok   = 1;
        }
        else
        {
            dht_ok = 0;
        }
        osDelay(2000);
    }
}

// ============ TASK 2: ADC — reads MQ135, Rain, LDR every 1s ============
void StartADCTask(void *argument)
{
    for(;;)
    {
        mq135_val = ADC_Read(ADC_CHANNEL_0);
        rain_val  = ADC_Read(ADC_CHANNEL_8);

        ldr_dark = !HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5,
                          ldr_dark ? GPIO_PIN_SET : GPIO_PIN_RESET);

        osDelay(1000);
    }
}

// ============ TASK 3: OLED — updates display every 150ms ============
void StartOLEDTask(void *argument)
{
    ssd1306_Init();
    ssd1306_Fill(Black);
    ssd1306_UpdateScreen();

    for(;;)
    {
        osMutexAcquire(oledMutexHandle, osWaitForever);

        ssd1306_Fill(Black);

        HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
        HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

        // ===== SCREEN 0: NIRMA =====
        if (screen == 0)
        {
            ssd1306_FillRectangle(0, 0, 127, 14, White);
            ssd1306_SetCursor(8, 3);
            ssd1306_WriteString("NIRMA UNIVERSITY", Font_7x10, Black);
            ssd1306_DrawRectangle(0, 0, 127, 63, White);
            ssd1306_SetCursor(10, 18);
            // FIX 2: changed from "Smart Env Monitor"
            ssd1306_WriteString("Smart Lamp Post ", Font_7x10, White);
            ssd1306_SetCursor(8, 32);
            // FIX 3: removed "IoT Lab", kept only ECE Dept
            ssd1306_WriteString("ECE Dept        ", Font_7x10, White);
            ssd1306_SetCursor(22, 46);
            ssd1306_WriteString("STM32F411RE", Font_7x10, White);
            ssd1306_Line(2, 58, 125, 58, White);
        }

        // ===== SCREEN 1: SENSORS =====
        else if (screen == 1)
        {
            uint8_t airPct  = (mq135_val * 100) / 4095;
            // FIX 1: Rain inverted — YL-83 dry=high ADC (~3800), wet=low ADC (~300)
            uint8_t rainPct = 100 - ((rain_val * 100) / 4095);

            ssd1306_SetCursor(0, 0);
            if (dht_ok)
                sprintf(buf, "T:%2dC  H:%2d%%", dht_temp, dht_hum);
            else
                sprintf(buf, "T: --   H: --  ");
            ssd1306_WriteString(buf, Font_7x10, White);

            ssd1306_SetCursor(0, 13);
            sprintf(buf, "Air:%3d%%", airPct);
            ssd1306_WriteString(buf, Font_7x10, White);
            ssd1306_DrawRectangle(55, 13, 127, 22, White);
            ssd1306_FillRectangle(56, 14, 56 + (airPct * 70 / 100), 21, White);

            ssd1306_SetCursor(0, 26);
            ssd1306_WriteString(ldr_dark ?
                "Light: DARK     " :
                "Light: BRIGHT   ", Font_7x10, White);

            ssd1306_SetCursor(0, 39);
            sprintf(buf, "Rain:%3d%%", rainPct);
            ssd1306_WriteString(buf, Font_7x10, White);
            ssd1306_DrawRectangle(55, 39, 127, 48, White);
            ssd1306_FillRectangle(56, 40, 56 + (rainPct * 70 / 100), 47, White);

            ssd1306_SetCursor(0, 53);
            ssd1306_WriteString(dht_ok ?
                "DHT11: OK " :
                "DHT11: FAIL", Font_7x10, White);
        }

        // ===== SCREEN 2: DATE & TIME =====
        else if (screen == 2)
        {
            ssd1306_FillRectangle(0, 0, 127, 13, White);
            ssd1306_SetCursor(25, 2);
            ssd1306_WriteString("DATE & TIME", Font_7x10, Black);

            ssd1306_SetCursor(10, 20);
            sprintf(buf, "%02d:%02d:%02d",
                    sTime.Hours, sTime.Minutes, sTime.Seconds);
            ssd1306_WriteString(buf, Font_11x18, White);

            const char* months[] = {
                "","JAN","FEB","MAR","APR","MAY",
                "JUN","JUL","AUG","SEP","OCT","NOV","DEC"
            };
            ssd1306_SetCursor(5, 46);
            sprintf(buf, "%02d-%s-20%02d",
                    sDate.Date, months[sDate.Month], sDate.Year);
            ssd1306_WriteString(buf, Font_7x10, White);
        }

        ssd1306_UpdateScreen();
        osMutexRelease(oledMutexHandle);
        osDelay(150);
    }
}

// ============ TASK 4: DEFAULT — switches screen every 4s ============
void StartDefaultTask(void *argument)
{
    for(;;)
    {
        screen = (screen + 1) % 3;

        char msg[40];
        sprintf(msg, "Screen: %d | T:%dC H:%d%%\r\n",
                screen, dht_temp, dht_hum);
        HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);

        osDelay(4000);
    }
}

/* USER CODE END 4 */

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 96;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK) Error_Handler();
}

static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) Error_Handler();
}

static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
}

static void MX_RTC_Init(void)
{
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK) Error_Handler();

  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) != 0xCAFE)
  {
      RTC_TimeTypeDef sTime = {0};
      sTime.Hours   = 9;
      sTime.Minutes = 17;
      sTime.Seconds = 0;
      sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
      sTime.StoreOperation = RTC_STOREOPERATION_RESET;
      if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK) Error_Handler();

      RTC_DateTypeDef sDate = {0};
      sDate.WeekDay = RTC_WEEKDAY_SATURDAY;
      sDate.Month   = RTC_MONTH_MARCH;
      sDate.Date    = 13;
      sDate.Year    = 26;
      if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK) Error_Handler();

      HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, 0xCAFE);
  }
}

static void MX_TIM1_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 95;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 0xFFFF;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK) Error_Handler();
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK) Error_Handler();
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK) Error_Handler();
}

static void MX_USART1_UART_Init(void)
{
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(DHT11_PIN_GPIO_Port, DHT11_PIN_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = DHT11_PIN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(DHT11_PIN_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = USART_TX_Pin|USART_RX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {}
#endif
