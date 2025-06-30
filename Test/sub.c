#include "main.h"
#include "ssd1306.h"
#include "fonts.h"
#include "stdio.h"
#include "string.h"

// UART, I2C, RTC, ADC, TIM, GPIO 핸들 선언
UART_HandleTypeDef huart1;
I2C_HandleTypeDef hi2c1;
RTC_HandleTypeDef hrtc;
ADC_HandleTypeDef hadc1;
TIM_HandleTypeDef htim3;

char uart_buf[100];
uint32_t adc_val = 0;
volatile uint8_t button_flag = 0;
RTC_TimeTypeDef sTime;
RTC_DateTypeDef sDate;

// --- 초기화 함수들 선언 ---
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_RTC_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);

// --- 메인 함수 ---
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  MX_RTC_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();

  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  SSD1306_Init();

  // 디스플레이 초기 메시지
  SSD1306_GotoXY(10, 10);
  SSD1306_Puts("System Start", &Font_11x18, 1);
  SSD1306_UpdateScreen();

  HAL_UART_Transmit(&huart1, (uint8_t*)"System Initialized\r\n", 21, HAL_MAX_DELAY);

  while (1)
  {
    // --- ADC 값 읽기 ---
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY) == HAL_OK)
    {
      adc_val = HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);

    // --- PWM 제어 (서보모터 제어 예시: 0~180도) ---
    uint32_t pulse = 500 + (adc_val * 2000 / 4095); // 0.5ms ~ 2.5ms
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pulse);

    // --- 시간 읽기 ---
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    // --- OLED 디스플레이 출력 ---
    char line1[32], line2[32];
    sprintf(line1, "ADC: %4lu", adc_val);
    sprintf(line2, "Time: %02d:%02d:%02d", sTime.Hours, sTime.Minutes, sTime.Seconds);

    SSD1306_Fill(SSD1306_COLOR_BLACK);
    SSD1306_GotoXY(0, 0);
    SSD1306_Puts(line1, &Font_7x10, 1);
    SSD1306_GotoXY(0, 12);
    SSD1306_Puts(line2, &Font_7x10, 1);
    SSD1306_UpdateScreen();

    // --- UART로 값 출력 ---
    sprintf(uart_buf, "[%02d:%02d:%02d] ADC: %lu\r\n", sTime.Hours, sTime.Minutes, sTime.Seconds, adc_val);
    HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), HAL_MAX_DELAY);

    // --- 버튼 인터럽트 이벤트 처리 ---
    if (button_flag)
    {
      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13); // LED Toggle
      HAL_UART_Transmit(&huart1, (uint8_t*)"Button Pressed!\r\n", 17, HAL_MAX_DELAY);
      button_flag = 0;
    }

    HAL_Delay(500);
  }
}

// --- 외부 인터럽트 콜백 ---
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_0)
  {
    button_flag = 1;
  }
}

// --- Peripheral Initialization Functions ---

static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};
  __HAL_RCC_ADC1_CLK_ENABLE();

  hadc1.Instance = ADC1;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  HAL_ADC_Init(&hadc1);

  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
  HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

static void MX_TIM3_Init(void)
{
  TIM_OC_InitTypeDef sConfigOC = {0};
  __HAL_RCC_TIM3_CLK_ENABLE();

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 72 - 1;  // 1MHz
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 20000 - 1; // 20ms for servo
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  HAL_TIM_PWM_Init(&htim3);

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 1500;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1);
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

static void MX_I2C1_Init(void)
{
  __HAL_RCC_I2C1_CLK_ENABLE();
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  HAL_I2C_Init(&hi2c1);
}

static void MX_RTC_Init(void)
{
  __HAL_RCC_RTC_ENABLE();
  hrtc.Instance = RTC;
  hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;
  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_NONE;
  HAL_RTC_Init(&hrtc);

  // RTC 기본 시간 설정 (필요시)
  sTime.Hours = 12;
  sTime.Minutes = 0;
  sTime.Seconds = 0;
  HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);

  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_JUNE;
  sDate.Date = 30;
  sDate.Year = 25;
  HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  // PC13: LED 출력
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  // PA0: 버튼 입력 (인터럽트)
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  HAL_NVIC_SetPriority(EXTI0_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}
