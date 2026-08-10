#ifndef __RGBLED_H
#define __RGBLED_H

#include <stdint.h>

void RGB_Init(void);
void RGB_SetColor(uint8_t r, uint8_t g, uint8_t b);
void RGB_SetHue(uint16_t hue); 
void RGB_RainbowTask(void);
void RGB_BreathTask(void);

#endif
