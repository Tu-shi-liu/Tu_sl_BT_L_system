#ifndef __KEY_H
#define __KEY_H

#include <stdint.h>

#define KEY1 0
#define KEY2 1
#define KEY3 2

#define KEY_EVENT_NONE          0
#define KEY_EVENT_SINGLE_CLICK  1
#define KEY_EVENT_DOUBLE_CLICK  2
#define KEY_EVENT_LONG_PRESS    3

void Key_Init(void);
void Key_Scan(void);
uint8_t Key_GetEvent(uint8_t key_id);

#endif
