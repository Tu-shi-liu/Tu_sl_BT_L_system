#ifndef __HC05_H
#define __HC05_H

#include "stm32f1xx_hal.h"

#define BT_BUF_SIZE 64

extern volatile uint8_t bt_rx_ready;
extern uint8_t  bt_rx_buf[BT_BUF_SIZE];
extern uint16_t bt_rx_len;

void HC05_Init(void);
void HC05_SendString(char *str);
void HC05_IRQHandler(UART_HandleTypeDef *huart);

#endif
