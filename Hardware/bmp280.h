#ifndef __BMP280_H
#define __BMP280_H

#include "stm32f1xx_hal.h"

typedef struct {
    int16_t  temperature;       /* 温度 × 10 （0.1 °C） */
    uint16_t pressure;          /* 气压 × 10 （0.1 hPa） */
    uint8_t  ready;
} BMP280_Data;

extern BMP280_Data bmp280_data;

uint8_t BMP280_Init(void);
void    BMP280_StartMeasurement(void);
void    BMP280_Task(void);
uint8_t BMP280_GetData(int16_t *temp, uint16_t *press);

#endif
