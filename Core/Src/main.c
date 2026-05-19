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
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "dht11.h"
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
// =========================
// 타이밍 설정
// =========================

#define MQ3_SAMPLE_INTERVAL     100     // MQ3 샘플링 주기(ms)
#define MQ3_MEASURE_TIME        5000    // 전체 측정 시간(ms)

#define PRESSURE_LOST_TIMEOUT   10000   // PASS 상태에서 자리 비움 허용 시간(ms)

#define FAIL_BLINK_TIME         5000    // FAIL 상태 Engine LED 깜빡 시간(ms)
// =========================
// 상태 머신
// =========================

typedef enum {
    STATE_IDLE,
    STATE_WAIT_SEAT,
    STATE_WAIT_BLOW,
    STATE_MEASURING,
    STATE_WAIT_RESULT,
    STATE_PASS,
    STATE_FAIL
} SystemState_t;

SystemState_t current_state;
uint32_t state_enter_time;
// =========================
// 버튼 상태 저장
// =========================

uint8_t start_btn_prev = 1;
uint8_t blow_btn_prev = 1;


// =========================
// 타이머 변수
// =========================

uint32_t measure_start_time = 0;

uint32_t pressure_lost_start = 0;

uint32_t last_sample_time = 0;

// =========================
// DHT11
// =========================
uint32_t last_humidity_time = 0;

float temperature = 0;
float humidity = 0;

uint8_t humidity_count = 0;

// =========================
// UART
// =========================

char uart_rx_data[32];

uint8_t rx_char;

char tx_buf[64];
// =========================
// FAIL 재측정 횟수
// =========================
uint8_t fail_count = 0;

#define MAX_FAIL_COUNT 3

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_NVIC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// =========================
// LED 전체 OFF
// =========================
void LED_All_Off(void)
{
    HAL_GPIO_WritePin(Green_LED_GPIO_Port,
                      Green_LED_Pin,
                      GPIO_PIN_RESET);

    HAL_GPIO_WritePin(Yellow_LED_GPIO_Port,
                      Yellow_LED_Pin,
                      GPIO_PIN_RESET);

    HAL_GPIO_WritePin(Red_LED_GPIO_Port,
                      Red_LED_Pin,
                      GPIO_PIN_RESET);
}

// =========================
// Green LED ON
// =========================
void Green_LED_On(void)
{
    HAL_GPIO_WritePin(Green_LED_GPIO_Port,
                      Green_LED_Pin,
                      GPIO_PIN_SET);
}

// =========================
// Yellow LED ON
// =========================
void Yellow_LED_On(void)
{
    HAL_GPIO_WritePin(Yellow_LED_GPIO_Port,
                      Yellow_LED_Pin,
                      GPIO_PIN_SET);
}

// =========================
// Red LED ON
// =========================
void Red_LED_On(void)
{
    HAL_GPIO_WritePin(Red_LED_GPIO_Port,
                      Red_LED_Pin,
                      GPIO_PIN_SET);
}

// =========================
// Engine ON
// =========================
void Engine_ON(void)
{
    HAL_GPIO_WritePin(Engine_LED_GPIO_Port,
                      Engine_LED_Pin,
                      GPIO_PIN_SET);
}

// =========================
// Engine OFF
// =========================
void Engine_OFF(void)
{
    HAL_GPIO_WritePin(Engine_LED_GPIO_Port,
                      Engine_LED_Pin,
                      GPIO_PIN_RESET);
}

// =========================
// Seat Sensor 상태 읽기
// Pull-up 기준:
// 착석 = 0
// 비착석 = 1
// =========================
uint8_t Is_Seat_Active(void)
{
    return (HAL_GPIO_ReadPin(SEAT_SENSOR_GPIO_Port,
                             SEAT_SENSOR_Pin) == 0);
}

// =========================
// MQ3 ADC 읽기
// =========================
uint32_t Read_MQ3(void)
{
    HAL_ADC_Start(&hadc1);

    HAL_ADC_PollForConversion(&hadc1, 100);

    uint32_t value = HAL_ADC_GetValue(&hadc1);

    HAL_ADC_Stop(&hadc1);

    return value;
}

// =========================
// 부저 패턴 - 삐 삐 삐 삐 삐
// =========================
void Buzzer_Alert(void)
{
    for(int i = 0; i < 5; i++)
    {
        HAL_TIM_PWM_Start(&htim1,
                          TIM_CHANNEL_1);

        HAL_Delay(200);

        HAL_TIM_PWM_Stop(&htim1,
                         TIM_CHANNEL_1);

        HAL_Delay(100);
    }
}

// =========================
// UART 송신
// 자동 개행 포함
// =========================
void UART_Send(const char* msg)
{
    char buffer[64];

    snprintf(buffer,
             sizeof(buffer),
             "%s\r\n",
             msg);

    HAL_UART_Transmit(&huart1,
                      (uint8_t*)buffer,
                      strlen(buffer),
                      100);
}

// =========================
// UART2 TEST
// 자동 개행 포함
// =========================
//void UART_Send(const char* msg)
//{
//    char buffer[64];
//
//    snprintf(buffer,
//             sizeof(buffer),
//             "%s\r\n",
//             msg);
//
//    // Raspberry Pi 통신
//    HAL_UART_Transmit(&huart1,
//                      (uint8_t*)buffer,
//                      strlen(buffer),
//                      100);
//
//    // PC 디버그 출력
//    HAL_UART_Transmit(&huart2,
//                      (uint8_t*)buffer,
//                      strlen(buffer),
//                      100);
//}
// =========================
// 상태 전환
// =========================
void Change_State(SystemState_t new_state)
{
    current_state = new_state;

    state_enter_time = HAL_GetTick();

    LED_All_Off();
}

// =========================
// UART RX Callback
// PASS / FAIL 수신
// =========================
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    static uint8_t idx = 0;

    if(huart->Instance == USART1)
    {
        if(rx_char == '\r')
        {
            // 무시 (CR 제거)
        }
        else if(rx_char == '\n')
        {
            uart_rx_data[idx] = '\0';

            if(strcmp(uart_rx_data, "PASS") == 0)
                Change_State(STATE_PASS);

            else if(strcmp(uart_rx_data, "FAIL") == 0)
            {
            	fail_count++;
                Change_State(STATE_FAIL);
            }
            else if(strcmp(uart_rx_data, "ERROR") == 0)
                Change_State(STATE_IDLE);

            idx = 0;
        }
        else
        {
            if(idx < sizeof(uart_rx_data) - 1)
            {
                uart_rx_data[idx++] = rx_char;
            }
            else
            {
                // overflow 방지 → 리셋
                idx = 0;
            }
        }

        HAL_UART_Receive_IT(&huart1, &rx_char, 1);
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
  MX_ADC1_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();
  MX_TIM1_Init();
  // MX_USART2_UART_Init(); // uart2 test용

  /* Initialize interrupts */
  MX_NVIC_Init();
  /* USER CODE BEGIN 2 */
  // 시작: IDLE 상태, 엔진 OFF
  Change_State(STATE_IDLE);

  Engine_OFF();

  // TIM2 시작 (DHT11 us 딜레이용)
  HAL_TIM_Base_Start(&htim2);

  DHT11_Init();

  // UART RX 인터럽트 시작
  HAL_UART_Receive_IT(&huart1,
                      &rx_char,
                      1);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  uint32_t now = HAL_GetTick();

	// =========================
	// 버튼 읽기
	// =========================

	uint8_t start_btn_now =
	HAL_GPIO_ReadPin(START_BTN_GPIO_Port,
					 START_BTN_Pin);

	uint8_t blow_btn_now =
	HAL_GPIO_ReadPin(BLOW_BTN_GPIO_Port,
					 BLOW_BTN_Pin);

	// =========================
	// falling edge 검출
	// =========================

	uint8_t start_pressed =
	(start_btn_prev == 1 && start_btn_now == 0);

	uint8_t blow_pressed =
	(blow_btn_prev == 1 && blow_btn_now == 0);

	start_btn_prev = start_btn_now;
	blow_btn_prev = blow_btn_now;

	// =========================
	// 상태 머신
	// =========================

	switch(current_state)
	{

		// =========================
		// IDLE
		// =========================
		case STATE_IDLE:

			LED_All_Off();

			Engine_OFF();

			pressure_lost_start = 0;

			// START 버튼
			if(start_pressed)
			{
			    fail_count = 0;
				UART_Send("SYSTEM_START");

				Change_State(STATE_WAIT_SEAT);
			}

			break;


		// =========================
		// WAIT_SEAT
		// =========================
		case STATE_WAIT_SEAT:

			Yellow_LED_On();

			// 착석 감지
			if(Is_Seat_Active())
			{
				UART_Send("SEAT_ON");

				Change_State(STATE_WAIT_BLOW);
			}

			break;


		// =========================
		// WAIT_BLOW
		// =========================
		case STATE_WAIT_BLOW:

			// Yellow LED blink
			if((now / 500) % 2)
			{
				Yellow_LED_On();
			}
			else
			{
				LED_All_Off();
			}

			// 자리 이탈
			if(!Is_Seat_Active())
			{
				UART_Send("SEAT_OFF");

				Change_State(STATE_IDLE);
			}

			// Blow 버튼
			if(blow_pressed)
			{
				measure_start_time = HAL_GetTick();

				last_sample_time = 0;

				last_humidity_time = HAL_GetTick() - 1000;

				humidity_count = 0;
				UART_Send("MEASURE_BEGIN");
				Change_State(STATE_MEASURING);
			}

			break;


		// =========================
		// MEASURING
		// =========================
		case STATE_MEASURING:

			// 빠른 blink
			if((now / 100) % 2)
			{
				Yellow_LED_On();
			}
			else
			{
				LED_All_Off();
			}

			// 측정 중 자리 이탈
			if(!Is_Seat_Active())
			{
				UART_Send("SEAT_OFF");

				Change_State(STATE_IDLE);

				break;
			}

			// 100ms 샘플링
			if(now - last_sample_time >= MQ3_SAMPLE_INTERVAL)
			{
				last_sample_time = now;

				uint32_t mq3 = Read_MQ3();

				snprintf(tx_buf,
				         sizeof(tx_buf),
				         "MQ3:%lu",
				         mq3);

				UART_Send(tx_buf);
			}

			// =========================
			// DHT11 : 1초마다 총 5회
			// =========================

			if((now - last_humidity_time >= 1000)
			   && (humidity_count < 5))
			{
			    last_humidity_time = now;

			    if(DHT11_Read(&temperature,
			                  &humidity)
			       == DHT11_OK)
			    {
			        snprintf(tx_buf,
			                 sizeof(tx_buf),
			                 "HUM:%d",
			                 (int)humidity);

			        UART_Send(tx_buf);
			    }
			    else
			    {
			        UART_Send("DHT_ERROR");
			    }

			    // 성공/실패 관계없이 횟수 증가
			    humidity_count++;
			}
			// 측정 종료
			if(now - measure_start_time >= MQ3_MEASURE_TIME)
			{
				UART_Send("MEASURE_END");

				Change_State(STATE_WAIT_RESULT);
			}

			break;


		// =========================
		// WAIT_RESULT
		// =========================
		case STATE_WAIT_RESULT:

			// 느린 blink
			if((now / 300) % 2)
			{
				Yellow_LED_On();
			}
			else
			{
				LED_All_Off();
			}

			// 자리 이탈
			if(!Is_Seat_Active())
			{
				UART_Send("SEAT_OFF");

				Change_State(STATE_IDLE);
			}

			break;


		// =========================
		// PASS
		// =========================
		case STATE_PASS:

			LED_All_Off();

			Green_LED_On();

			Engine_ON();

			// 운전자 이탈 감지
			if(!Is_Seat_Active())
			{
				if(pressure_lost_start == 0)
				{
					pressure_lost_start = HAL_GetTick();
				}

				// 일정 시간 이상 자리 비움
				if(HAL_GetTick() - pressure_lost_start >= PRESSURE_LOST_TIMEOUT)
				{
					UART_Send("SEAT_OFF");

					Change_State(STATE_IDLE);
				}
			}
			else
			{
				pressure_lost_start = 0;
			}

			break;


		// =========================
		// FAIL
		// =========================
		case STATE_FAIL:

		    LED_All_Off();

		    Red_LED_On();

		    Engine_OFF();

		    // FAIL 진입 직후 부저
		    if(now - state_enter_time < 50)
		    {
		        Buzzer_Alert();
		    }

		    // 3회 미만이면 재측정 허용
		    if(fail_count < MAX_FAIL_COUNT)
		    {
		        // Engine LED blink
		        if((now / 200) % 2)
		        {
		            Engine_ON();
		        }
		        else
		        {
		            Engine_OFF();
		        }

		        // Blow 버튼 누르면 재측정
		        if(blow_pressed)
		        {
		            measure_start_time = HAL_GetTick();

		            last_sample_time = 0;

		            last_humidity_time =
		                HAL_GetTick() - 1000;

		            humidity_count = 0;

		            UART_Send("RETRY_MEASURE");

		            Change_State(STATE_MEASURING);
		        }
		    }

		    // 3회째 실패
		    else
		    {
		        Engine_OFF();

		        // START로 초기화
		        if(start_pressed)
		        {
		            fail_count = 0;

		            Change_State(STATE_IDLE);
		        }
		    }

		    break;
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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

/**
  * @brief NVIC Configuration.
  * @retval None
  */
static void MX_NVIC_Init(void)
{
  /* USART1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
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
