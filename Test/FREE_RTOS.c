#include "main.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>

// 핸들러 선언
ADC_HandleTypeDef hadc1;
UART_HandleTypeDef huart1;
osThreadId ledTaskHandle;
osThreadId uartTaskHandle;
osThreadId adcTaskHandle;

// 큐 핸들러
osMessageQId adcQueueHandle;
osMessageQDef(adcQueue, 8, uint16_t);

// 태스크 선언
void StartLedTask(void const * argument);
void StartUartTask(void const * argument);
void StartAdcTask(void const * argument);

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART1_UART_Init(void);

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_USART1_UART_Init();

  // RTOS 큐 생성
  adcQueueHandle = osMessageCreate(osMessageQ(adcQueue), NULL);

  // RTOS 태스크 생성
  osThreadDef(ledTask, StartLedTask, osPriorityLow, 0, 128);
  ledTaskHandle = osThreadCreate(osThread(ledTask), NULL);

  osThreadDef(uartTask, StartUartTask, osPriorityNormal, 0, 128);
  uartTaskHandle = osThreadCreate(osThread(uartTask), NULL);

  osThreadDef(adcTask, StartAdcTask, osPriorityAboveNormal, 0, 128);
  adcTaskHandle = osThreadCreate(osThread(adcTask), NULL);

  // RTOS 시작
  osKernelStart();

  while (1) {}
}

void StartLedTask(void const * argument)
{
  for(;;)
  {
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    osDelay(500);
  }
}

void StartUartTask(void const * argument)
{
  char msg[64];
  uint16_t adc_val;

  for(;;)
  {
    if (osMessagePeek(adcQueueHandle, &adc_val, osWaitForever) == osOK)
    {
      sprintf(msg, "ADC: %u\r\n", adc_val);
      HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
    }
    osDelay(1000);
  }
}

void StartAdcTask(void const * argument)
{
  uint16_t adc_val;

  for(;;)
  {
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY) == HAL_OK)
    {
      adc_val = HAL_ADC_GetValue(&hadc1);
      osMessagePut(adcQueueHandle, adc_val, 10);
    }
    HAL_ADC_Stop(&hadc1);
    osDelay(100);
  }
}

// --- 주변장치 초기화 ---

static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};
  __HAL_RCC_ADC1_CLK_ENABLE();

  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  HAL_ADC_Init(&hadc1);

  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
  HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

static void MX_USART1_UART_Init(void)
{
  __HAL_RCC_USART1_CLK_ENABLE();
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  HAL_UART_Init(&huart1);
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOC_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}
