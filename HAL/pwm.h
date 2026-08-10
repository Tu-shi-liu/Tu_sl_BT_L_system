#ifndef __PWM_H
#define __PWM_H

#include <stdint.h>

#define PWM_CH_RED   0
#define PWM_CH_GREEN 1
#define PWM_CH_BLUE  2

void PWM_Init(void);
void PWM_SetDuty(uint8_t channel, uint16_t duty);

#endif
