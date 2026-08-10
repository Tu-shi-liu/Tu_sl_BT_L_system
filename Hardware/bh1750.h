#ifndef __BH1750_H
#define __BH1750_H

#include <stdint.h>

#define BH1750_MODE_CONTINUOUS  0
#define BH1750_MODE_SINGLE      1
#define BH1750_MODE             BH1750_MODE_CONTINUOUS
#define BH1750_INTERVAL_MS      200
#define BH1750_ADDR             0x23           /* 7位地址 */

typedef struct {
    uint16_t lux_raw;  
    uint8_t  ready;
} BH1750_Data;

extern BH1750_Data bh1750_data;

void    BH1750_Init(void);
void    BH1750_Task(void);
uint8_t BH1750_GetLux(uint16_t *lux); 
void    BH1750_Trigger(void);

#endif
