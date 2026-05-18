#include "dht11.h"
#include "tim.h"
// 핀 출력 모드
static void Set_Pin_Output(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(DHT11_PORT,
                  &GPIO_InitStruct);
}

// 핀 입력 모드
static void Set_Pin_Input(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;

    HAL_GPIO_Init(DHT11_PORT,
                  &GPIO_InitStruct);
}

// us 단위 딜레이
static void Delay_us(uint16_t us)
{
    uint32_t start =
        __HAL_TIM_GET_COUNTER(&htim2);

    while((__HAL_TIM_GET_COUNTER(&htim2)
          - start) < us);
}

void DHT11_Init(void)
{
}

static uint8_t DHT11_Start(void)
{
    uint8_t response=0;

    Set_Pin_Output();

    HAL_GPIO_WritePin(
        DHT11_PORT,
        DHT11_PIN,
        GPIO_PIN_RESET);

    HAL_Delay(20);

    HAL_GPIO_WritePin(
        DHT11_PORT,
        DHT11_PIN,
        GPIO_PIN_SET);

    Delay_us(30);

    Set_Pin_Input();

    Delay_us(40);

    if(!(HAL_GPIO_ReadPin(
         DHT11_PORT,
         DHT11_PIN)))
    {
        Delay_us(80);

        if(HAL_GPIO_ReadPin(
           DHT11_PORT,
           DHT11_PIN))
        {
            response=1;
        }
    }

    while(HAL_GPIO_ReadPin(
          DHT11_PORT,
          DHT11_PIN));

    return response;
}

static uint8_t DHT11_ReadByte(void)
{
    uint8_t i,j;

    for(j=0;j<8;j++)
    {
        while(!HAL_GPIO_ReadPin(
              DHT11_PORT,
              DHT11_PIN));

        Delay_us(40);

        if(!HAL_GPIO_ReadPin(
             DHT11_PORT,
             DHT11_PIN))
        {
            i &= ~(1<<(7-j));
        }
        else
        {
            i |= (1<<(7-j));
        }

        while(HAL_GPIO_ReadPin(
             DHT11_PORT,
             DHT11_PIN));
    }

    return i;
}

uint8_t DHT11_Read(float *temperature,
                   float *humidity)
{
    uint8_t Rh_byte1;
    uint8_t Rh_byte2;
    uint8_t Temp_byte1;
    uint8_t Temp_byte2;
    uint8_t checksum;

    if(!DHT11_Start())
        return DHT11_ERROR;

    Rh_byte1 = DHT11_ReadByte();
    Rh_byte2 = DHT11_ReadByte();

    Temp_byte1 = DHT11_ReadByte();
    Temp_byte2 = DHT11_ReadByte();

    checksum = DHT11_ReadByte();

    if(checksum ==
       (Rh_byte1
       +Rh_byte2
       +Temp_byte1
       +Temp_byte2))
    {
        *humidity =
            Rh_byte1;

        *temperature =
            Temp_byte1;

        return DHT11_OK;
    }

    return DHT11_ERROR;
}
