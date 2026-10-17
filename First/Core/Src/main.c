/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @author         : STMicroelectronics (generated); Codex (user integration)
  * @brief          : Main program body
  * @date           : 2026-10-14
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
#include "./main.h"
#include "./dma.h"
#include "./fdcan.h"
#include "./usart.h"
#include "./gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "./bsp/bsp_uart_log.h"
#include "./bsp/bsp_fdcan.h"
#include "./device/can_probe.h"
#include "./service/fdcan_diag.h"
#include "./bsp/bsp_uart_stream.h"
#include "./service/tuning_link.h"
#include "./device/motor.h"
#include "./app/motor_tune.h"
#include "./composition/firmware.h"
#ifndef DIAG_MODE
#define DIAG_MODE 0
#endif /* DIAG_MODE default */
#if DIAG_MODE == 7
#include "can_link_probe.h"
#endif /* read-only link diagnostic */

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

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

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

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

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
  MX_FDCAN1_Init();
  MX_UART7_Init();
  /* USER CODE BEGIN 2 */
#if DIAG_MODE != 3
#if DIAG_MODE == 1
  /* only for DEBUG: exercise both BRS choices in the internal controller. */
  hfdcan1.Init.FrameFormat=FDCAN_FRAME_FD_BRS;
#endif /* internal loopback format */
  bsp_fdcan_result_t can_result=bsp_fdcan_init(DIAG_MODE==1 ? BSP_FDCAN_LOOPBACK : BSP_FDCAN_NORMAL);
  if (can_result!=BSP_FDCAN_OK)
  {
    bsp_uart_log_write("FAIL BSP_INIT\r\n");
    Error_Handler();
  }
  #if DIAG_MODE != 0
  can_probe_init(DIAG_MODE==1);
  #endif /* legacy diagnostic images */
#if DIAG_MODE == 0
  if (!firmware_init(HAL_GetTick())) Error_Handler();
#elif DIAG_MODE == 1 || DIAG_MODE == 2
  fdcan_diag_init((fdcan_diag_mode_t)DIAG_MODE,HAL_GetTick,bsp_uart_log_write);
#elif DIAG_MODE == 7
  can_link_probe_init(HAL_GetTick());
#elif DIAG_MODE == 4 || DIAG_MODE == 5 || DIAG_MODE == 6
  if (!bsp_uart_stream_init()) Error_Handler();
  tuning_link_init(bsp_uart_stream_read,bsp_uart_stream_write);
  motor_init(HAL_GetTick());
#if DIAG_MODE == 6
  //only forDEBUG：主控PID定速试验，设备和通信接口由main统一装配。
  motor_tune_pid_demo_init(HAL_GetTick());
#elif DIAG_MODE == 5
  motor_tune_speed_demo_init(HAL_GetTick());
#else /* DIAG_MODE == 4: interactive tuning */
  motor_tune_init(HAL_GetTick());
#endif /* application variant */
#endif /* diagnostic or tuning initialization */
#endif /* DIAG_MODE != 3 */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
#if DIAG_MODE == 0
    firmware_tick(HAL_GetTick());
#elif DIAG_MODE == 3
    bsp_uart_log_test_tick();
#elif DIAG_MODE == 1 || DIAG_MODE == 2
    fdcan_diag_tick();
#elif DIAG_MODE == 7
    can_link_probe_tick(HAL_GetTick());
#elif DIAG_MODE == 4 || DIAG_MODE == 5 || DIAG_MODE == 6
    motor_tune_tick(HAL_GetTick(),bsp_uart_stream_errors());
#endif /* diagnostic or tuning main loop */
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

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = 64;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 15;
  RCC_OscInitStruct.PLL.PLLP = 1;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
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
