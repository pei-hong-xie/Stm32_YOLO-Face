/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "dcmi.h"
#include "memorymap.h"
#include "quadspi.h"
#include "rtc.h"
#include "sdmmc.h"
#include "spi.h"
#include "usart.h"
#include "usb_otg.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "network_1725793088346.h"
#include "network_1725793088346_data.h"
#include "lcd_model.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define SRC_WIDTH 240
#define SRC_HEIGHT 280
#define DST_WIDTH 56
#define DST_HEIGHT 56

uint16_t scaled_buffer[56 * 56];  // 缩放后的图像缓冲区

// 缩放函数
void bilinear_scale(uint16_t* src, uint16_t* dst, int src_width, int src_height, int dst_width, int dst_height)
{
        float x_ratio = (float)src_width / dst_width;
    float y_ratio = (float)src_height / dst_height;
    for (int y = 0; y < dst_height; y++) {
        for (int x = 0; x < dst_width; x++) {
            int src_x = (int)(x * x_ratio);
            int src_y = (int)(y * y_ratio);
            dst[y * dst_width + x] = src[src_y * src_width + src_x];
        }
    }
}

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
static void MPU_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* Global handle to reference the instantiated C-model */
static ai_handle network = AI_HANDLE_NULL;

/* Global c-array to handle the activations buffer */
AI_ALIGNED(32)
static ai_i8 activations[AI_NETWORK_1725793088346_DATA_ACTIVATIONS_SIZE];

/* Array to store the data of the input tensor */
AI_ALIGNED(32)
static ai_i8 in_data[AI_NETWORK_1725793088346_IN_1_SIZE];
/* or static ai_u8 in_data[AI_NETWORK_IN_1_SIZE_BYTES]; */

/* c-array to store the data of the output tensor */
AI_ALIGNED(32)
static ai_i8 out_data[AI_NETWORK_1725793088346_OUT_1_SIZE];
/* static ai_u8 out_data[AI_NETWORK_OUT_1_SIZE_BYTES]; */

/* Array of pointer to manage the model's input/output tensors */
static ai_buffer *ai_input;
static ai_buffer *ai_output;


/* 
 * Bootstrap
 */
int aiInit(void) {
  ai_error err;
  
  /* Create and initialize the c-model */
  const ai_handle acts[] = { activations };
  err = ai_network_1725793088346_create_and_init(&network, acts, NULL);
  if (err.type != AI_ERROR_NONE) { 
	printf("E: AI ai_network_create error - type=%d code=%d\r\n", err.type, err.code);
    return -1;
  };

  /* Reteive pointers to the model's input/output tensors */
  ai_input = ai_network_1725793088346_inputs_get(network, NULL);
  ai_output = ai_network_1725793088346_outputs_get(network, NULL);

  return 0;
}

/* 
 * Run inference
 */
int aiRun(const void *in_data, void *out_data) {
  ai_i32 n_batch;
  ai_error err;
  
  /* 1 - Update IO handlers with the data payload */
  ai_input[0].data = AI_HANDLE_PTR(in_data);
  ai_output[0].data = AI_HANDLE_PTR(out_data);

  /* 2 - Perform the inference */
  n_batch = ai_network_1725793088346_run(network, &ai_input[0], &ai_output[0]);
  if (n_batch != 1) {
      err = ai_network_1725793088346_get_error(network);
      printf("E: AI ai_network_init error - type=%d code=%d\r\n", err.type, err.code);
      return -1;
  };
  
  return 0;
}

#define Camera_Buffer	0x24000000    // 摄像头图像缓冲区


uint8_t anchors[3][2] = {{9, 14}, {12, 17}, {22, 21}};
//uint16_t img_data[56*56];
//uint16_t image_data[56*56] = {0};

uint16_t get_pixel_value(uint16_t *pData, int row, int col)
{
	return ((uint16_t *)pData)[col + row * 56];
}


// 网络预处理函数，任务是把图像传入网络
// 网络量化之后会给出对应的输入和输出的缩放比例和偏移量，可以使用netron查看yoloface_int8.tflite得到
// 网络训练的时候需要把图像归一化，即从0~255缩放到0~1，然后再根据量化得到的缩放比例和偏移量计算
// 最后量化的网络输入图像范围是-128~127，正好是RGB分别减去128即可
void prepare_yolo_data()
{
    // 将 240x280 缩放到 56x56
    bilinear_scale((uint16_t*)Camera_Buffer, scaled_buffer, SRC_WIDTH, SRC_HEIGHT, DST_WIDTH, DST_HEIGHT);

    // 将缩放后的图像转换为网络输入格式
    for (int i = 0; i < DST_HEIGHT; i++)
    {
        for (int j = 0; j < DST_WIDTH; j++)
        {
            uint16_t color = scaled_buffer[j + i * DST_WIDTH];
            in_data[(j + i * DST_WIDTH) * 3] = (int8_t)((color & 0xF800) >> 9) - 128;
            in_data[(j + i * DST_WIDTH) * 3 + 1] = (int8_t)((color & 0x07E0) >> 3) - 128;
            in_data[(j + i * DST_WIDTH) * 3 + 2] = (int8_t)((color & 0x001F) << 3) - 128;
        }
    }
}

float sigmod(float x)
{
	float y = 1/(1+expf(x));
	return y;
}

 void post_process(int input_width, int input_height, int display_width, int display_height)
{
    int grid_x, grid_y, x1, y1, x2, y2;
    float x, y, w, h;
    float scale_x = (float)display_width / input_width;
    float scale_y = (float)display_height / input_height;
    
    for(int i = 0; i < 49; i++)
    {
        for(int j = 0; j < 3; j++)
        {
            int8_t conf = out_data[i * 18 + j * 6 + 4];
            if(conf > -9)
            {
                grid_x = i % 7;
                grid_y = (i - grid_x) / 7;
                
                x = ((float)out_data[i * 18 + j * 6] + 15) * 0.14218327403068542f;
                y = ((float)out_data[i * 18 + j * 6 + 1] + 15) * 0.14218327403068542f;
                w = ((float)out_data[i * 18 + j * 6 + 2] + 15) * 0.14218327403068542f;
                h = ((float)out_data[i * 18 + j * 6 + 3] + 15) * 0.14218327403068542f;
                
                x = (sigmod(x) + grid_y) * input_width / 7;
                y = (sigmod(y) + grid_x) * input_height / 7;
                w = expf(w) * anchors[j][1];
                h = expf(h) * anchors[j][0];
                
                x1 = (x - w / 2) * scale_x;
                x2 = (x + w / 2) * scale_x;
                y1 = (y - h / 2) * scale_y;
                y2 = (y + h / 2) * scale_y;
                
                if(x1 < 0) x1 = 0;
                if(y1 < 0) y1 = 0;
                if(x2 > display_width - 1) x2 = display_width - 1;
                if(y2 > display_height - 1) y2 = display_height - 1;
                
                LCD_DrawRect(x1, y1, x2 - x1, y2 - y1);
            }
        }
    }
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

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

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
  MX_USART1_UART_Init();
//  MX_DCMI_Init();
////  MX_QUADSPI_Init();
//  MX_RTC_Init();
////  MX_SDMMC1_SD_Init();
//  MX_SPI4_Init();
//  MX_USB_OTG_FS_PCD_Init();
  /* USER CODE BEGIN 2 */
	aiInit();
	SPI_LCD_Init();
	DCMI_OV2640_Init();
	OV2640_DMA_Transmit_Continuous(Camera_Buffer, OV2640_BufferSize);	
	OV2640_Set_Pixformat(Pixformat_RGB565);
	//LCD_SetBackColor(LCD_BLACK);
	
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
		
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  	if (DCMI_FrameState == 1)	// 采集到了一帧图像
		{		
  			DCMI_FrameState = 0;		// 清零标志位

			LCD_CopyBuffer(0,0,Display_Width,Display_Height, (uint16_t *)Camera_Buffer);	// 将图像数据复制到屏幕
			
			prepare_yolo_data();
			aiRun(in_data, out_data);

			post_process(56,56,240,280);

		}	

	  
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
	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSE
                              |RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
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
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
	
	PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART1|RCC_PERIPHCLK_SDMMC
															|RCC_PERIPHCLK_CKPER;
  
  PeriphClkInitStruct.Usart16ClockSelection = RCC_USART16CLKSOURCE_D2PCLK2;
  PeriphClkInitStruct.SdmmcClockSelection = RCC_SDMMCCLKSOURCE_PLL;  
  
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
	MPU_Region_InitTypeDef MPU_InitStruct;

	HAL_MPU_Disable();		// 先禁止MPU

	MPU_InitStruct.Enable 				= MPU_REGION_ENABLE;
	MPU_InitStruct.BaseAddress 		= 0x24000000;
	MPU_InitStruct.Size 					= MPU_REGION_SIZE_512KB;
	MPU_InitStruct.AccessPermission 	= MPU_REGION_FULL_ACCESS;
	MPU_InitStruct.IsBufferable 		= MPU_ACCESS_BUFFERABLE;
	MPU_InitStruct.IsCacheable 		= MPU_ACCESS_CACHEABLE;
	MPU_InitStruct.IsShareable 		= MPU_ACCESS_SHAREABLE;
	MPU_InitStruct.Number 				= MPU_REGION_NUMBER0;
	MPU_InitStruct.TypeExtField 		= MPU_TEX_LEVEL0;
	MPU_InitStruct.SubRegionDisable 	= 0x00;
	MPU_InitStruct.DisableExec 		= MPU_INSTRUCTION_ACCESS_ENABLE;

	HAL_MPU_ConfigRegion(&MPU_InitStruct);	

	HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);	// 使能MPU

}

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

#ifdef  USE_FULL_ASSERT
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
