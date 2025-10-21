/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>  // use the sine function
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

/* USER CODE BEGIN PV */
static char linebuf[128];  // send the CSV file to Raspberry Pi
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void send_line_sensor(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


static uint16_t adc_read_pa0(void)
{
    ADC_ChannelConfTypeDef s = {0};
    s.Channel = ADC_CHANNEL_5;                       // PA0 = IN5
    s.Rank = ADC_REGULAR_RANK_1;
    s.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;      // Change to 247.5
    HAL_ADC_ConfigChannel(&hadc1, &s);

    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);    // wait for 10ms
    uint16_t raw = (uint16_t)HAL_ADC_GetValue(&hadc1);  // change 32 bits to 16 bits. Only use 12-bit：0 ~ 4095.
    //uint32_t raw = (uint32_t)HAL_ADC_GetValue(&hadc1);

    HAL_ADC_Stop(&hadc1);
    return raw;                               // raw ∈ [0,4095]
}




// Set the Vref = VDDA = 3.3V
static float raw_to_volt(uint16_t raw)
{
    return 3.3f * ((float)raw / 4095.0f);
}




// 移動平均（10筆抗抖）
static float avg10(float x)
{
    enum { N = 10 };
    static float buf[N];
    static uint8_t idx = 0;
    static uint8_t filled = 0;
    static float acc = 0.0f;

    if (filled < N) {
        buf[idx++] = x;
        acc += x;
        filled++;
        if (idx >= N) idx = 0;
        return acc / (float)filled;
    } else {    // after input first 10 data
        float old = buf[idx];
        buf[idx] = x;
        acc += x - old;
        idx++;
        if (idx >= N) idx = 0;
        return acc / (float)N;
    }
}



// NTU mapping for 2.38V ~ 3.3V  →  0 ~ 100 NTU
#define V_MIN       2.38f
#define V_MAX       3.3f
#define NTU_SLOPE   (100.0f / (V_MAX - V_MIN))
#define NTU_OFFSET  (-NTU_SLOPE * V_MIN)         // When NTU=0, V=V_MIN



static void send_line_sensor(void)
{
    // Read PA0 (ADC1_IN5)
	uint16_t raw = adc_read_pa0();

    float v = raw_to_volt(raw);
    float v_avg = avg10(v);  // smooth
    //v = avg50(v);  // smooth

    // Linearity for 2.38V~3.3V → 0~100 NTU by using v_avg
    float ntu = NTU_SLOPE * v_avg + NTU_OFFSET;
    if (ntu < 0.0f)   ntu = 0.0f;
    if (ntu > 100.0f) ntu = 100.0f;



    uint32_t ts = HAL_GetTick();


//    int n = snprintf(linebuf, sizeof(linebuf),
//                     "%lu,%.2f,%.3f\r\n",
//                     (unsigned long)ts,ntu,v_avg);

    int n = snprintf(linebuf, sizeof(linebuf),
                        "%.2f\r\n",
                        ntu);

    // Send the data by UART
    if (n > 0)
        HAL_UART_Transmit(&huart2, (uint8_t*)linebuf, (uint16_t)n, HAL_MAX_DELAY);
}


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
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */

  HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);  // calibration once

  const char *header = "ntu\r\n";
  HAL_UART_Transmit(&huart2, (uint8_t*)header, strlen(header), HAL_MAX_DELAY);

  /* USER CODE END 2 */


  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
	  send_line_sensor();      // 每次送一行
	  HAL_Delay(500);          // 500 ms 一筆


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

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 16;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

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
