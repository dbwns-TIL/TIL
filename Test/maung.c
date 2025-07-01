#include "main.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>

typedef enum {
    EVENT_SENSOR_READ,
    EVENT_DISPLAY_UPDATE,
    EVENT_ERROR
} EventType;

typedef struct {
    EventType type;
    uint16_t value;
} Event;

ADC_HandleTypeDef hadc1;
UART_HandleTypeDef huart1;
osThreadId sensorTaskHandle;
osThreadId logicTaskHandle;
osThreadId displayTaskHandle;
osMessageQId eventQueueHandle;

osMessageQDef(eventQueue, 16, Event);

void SendEvent(EventType type, uint16_t value) {
    Event evt = { .type = type, .value = value };
    osMessagePut(eventQueueHandle, *(uint32_t*)&evt, 0);
}

// SensorTask: ADC Read -> 이벤트 전달
void SensorTask(void const *arg) {
    uint16_t adcVal;
    while (1) {
        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK) {
            adcVal = HAL_ADC_GetValue(&hadc1);
            SendEvent(EVENT_SENSOR_READ, adcVal);
        }
        HAL_ADC_Stop(&hadc1);
        osDelay(500);
    }
}

// LogicTask: 이벤트 처리 -> LED 제어, 디스플레이 요청
void LogicTask(void const *arg) {
    osEvent evt;
    while (1) {
        evt = osMessageGet(eventQueueHandle, osWaitForever);
        if (evt.status == osEventMessage) {
            Event e = *(Event*)&evt.value.v;
            switch (e.type) {
                case EVENT_SENSOR_READ:
                    if (e.value > 2000)
                        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);  // LED ON
                    else
                        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);    // LED OFF
                    SendEvent(EVENT_DISPLAY_UPDATE, e.value);
                    break;

                default:
                    break;
            }
        }
    }
}

// DisplayTask: UART로 센서 값 출력
void DisplayTask(void const *arg) {
    osEvent evt;
    char msg[32];
    while (1) {
        evt = osMessageGet(eventQueueHandle, osWaitForever);
        if (evt.status == osEventMessage) {
            Event e = *(Event*)&evt.value.v;
            if (e.type == EVENT_DISPLAY_UPDATE) {
                sprintf(msg, "ADC: %u\r\n", e.value);
                HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
            }
        }
    }
}

// 메인
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_USART1_UART_Init();

    eventQueueHandle = osMessageCreate(osMessageQ(eventQueue), NULL);

    osThreadDef(sensorTask, SensorTask, osPriorityNormal, 0, 128);
    osThreadDef(logicTask, LogicTask, osPriorityAboveNormal, 0, 128);
    osThreadDef(displayTask, DisplayTask, osPriorityBelowNormal, 0, 128);
    sensorTaskHandle  = osThreadCreate(osThread(sensorTask), NULL);
    logicTaskHandle   = osThreadCreate(osThread(logicTask), NULL);
    displayTaskHandle = osThreadCreate(osThread(displayTask), NULL);

    osKernelStart();

    while (1) {}
}

// 주변장치 초기화
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

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
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // LED PC13
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}
