// main.c
#include "stm32f4xx.h"
#include "stdio.h"

void SystemClock_Config(void);
void GPIO_Init(void);
void USART2_Init(void);
void TIM2_Init(void);
void PWM_Init(void);
void ADC1_Init(void);
void I2C1_Init(void);
void SPI1_Init(void);
void OLED_Display(char* str);

void delay_ms(uint32_t ms);

volatile uint32_t adc_value = 0;
volatile uint8_t button_pressed = 0;

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    GPIO_Init();
    USART2_Init();
    TIM2_Init();
    PWM_Init();
    ADC1_Init();
    I2C1_Init();
    SPI1_Init();

    HAL_TIM_Base_Start_IT(&htim2);
    HAL_ADC_Start(&hadc1);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);

    char msg[64];

    while (1)
    {
        // ADC Read
        HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
        adc_value = HAL_ADC_GetValue(&hadc1);

        // PWM 조절 (ADC 값 기반)
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, adc_value / 16);

        // 버튼 눌림 처리
        if (button_pressed)
        {
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
            button_pressed = 0;

            snprintf(msg, sizeof(msg), "LED TOGGLE, ADC: %lu\r\n", adc_value);
            HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
        }

        // OLED 출력
        snprintf(msg, sizeof(msg), "Light: %lu", adc_value);
        OLED_Display(msg);

        delay_ms(500);
    }
}

// GPIO 설정 (LED, 버튼)
void GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // LED - PA5
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // 버튼 - PA0 (인터럽트)
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI0_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

// USART2 (PA2: TX, PA3: RX)
void USART2_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    huart2.Instance = USART2;
    huart2.Init.BaudRate = 9600;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    HAL_UART_Init(&huart2);
}

// TIM2 - 주기 인터럽트용
void TIM2_Init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 16000 - 1;
    htim2.Init.Period = 1000 - 1;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    HAL_TIM_Base_Init(&htim2);

    HAL_NVIC_SetPriority(TIM2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
}

// PWM (TIM3, CH1, PA6)
void PWM_Init(void)
{
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 160 - 1;
    htim3.Init.Period = 1000 -
