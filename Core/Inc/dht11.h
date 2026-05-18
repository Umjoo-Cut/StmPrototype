#ifndef __DHT11_H
#define __DHT11_H

#include "main.h"

#define DHT11_PORT GPIOB
#define DHT11_PIN GPIO_PIN_5

#define DHT11_OK 1
#define DHT11_ERROR 0

void DHT11_Init(void);
uint8_t DHT11_Read(float *temperature,
                   float *humidity);

#endif
