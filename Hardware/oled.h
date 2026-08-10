#ifndef __OLED_H
#define __OLED_H

#include "stm32f1xx_hal.h"

void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowChar(uint8_t line, uint8_t col, char ch);
void OLED_ShowString(uint8_t line, uint8_t col, const char *str);
void OLED_Refresh(void);
void OLED_RefreshPage(uint8_t page);

#endif
