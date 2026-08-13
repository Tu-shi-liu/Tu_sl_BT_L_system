#include "hc05.h"
#include "usart.h"
#include <string.h>

static uint8_t dma_rx_buf[BT_BUF_SIZE];
volatile uint8_t bt_rx_ready = 0;
uint8_t  bt_rx_buf[BT_BUF_SIZE];
uint16_t bt_rx_len = 0;
static char tx_dma_buf[128]; 

void HC05_SendString(char *str)
{
    if (str == NULL) return;

    if (huart1.hdmatx->State != HAL_DMA_STATE_READY) {
        return;
    }

    size_t len = strlen(str);
    if (len >= sizeof(tx_dma_buf)) {
        len = sizeof(tx_dma_buf) - 1;  
    }
    memcpy(tx_dma_buf, str, len);
    tx_dma_buf[len] = '\0';

    HAL_UART_Transmit_DMA(&huart1, (uint8_t *)tx_dma_buf, len);
}

void HC05_IRQHandler(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        if (__HAL_UART_GET_FLAG(huart, UART_FLAG_IDLE)) {
            __HAL_UART_CLEAR_IDLEFLAG(huart);
            HAL_UART_DMAStop(huart);
            uint16_t recv_len = BT_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart->hdmarx);
            if (recv_len > 0 && recv_len <= BT_BUF_SIZE) {
                memcpy(bt_rx_buf, dma_rx_buf, recv_len);
                bt_rx_len = recv_len;
                bt_rx_ready = 1;
            }
            HAL_UART_Receive_DMA(huart, dma_rx_buf, BT_BUF_SIZE);
        }
        if (__HAL_UART_GET_FLAG(huart, UART_FLAG_ORE) ||
            __HAL_UART_GET_FLAG(huart, UART_FLAG_NE) ||
            __HAL_UART_GET_FLAG(huart, UART_FLAG_FE)) {
            __HAL_UART_CLEAR_FLAG(huart, UART_FLAG_ORE | UART_FLAG_NE | UART_FLAG_FE);
        }
    }
}

void HC05_Init(void)
{
    HAL_UART_Receive_DMA(&huart1, dma_rx_buf, BT_BUF_SIZE);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
}
