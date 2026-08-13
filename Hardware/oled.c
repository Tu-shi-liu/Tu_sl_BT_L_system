#include "oled.h"
#include "OLED_Font.h"
#include <string.h>

extern I2C_HandleTypeDef hi2c2;

#define OLED_ADDR       0x78
#define OLED_CMD        0x00
#define OLED_DATA       0x40
#define OLED_PAGES      8
#define OLED_WIDTH      128
#define OLED_I2C_TIMEOUT 20

static uint8_t OLED_GRAM[OLED_PAGES][OLED_WIDTH];
static uint8_t oled_error_count = 0;
static uint32_t last_recover_tick = 0;

static void OLED_WriteCmd(uint8_t cmd);
static void OLED_WriteData(uint8_t *data, uint16_t size);
static void OLED_SetCursor(uint8_t page, uint8_t col);
static void OLED_I2C_Recover(void);

static void OLED_I2C_Recover(void)
{
    if (HAL_GetTick() - last_recover_tick < 2000) {
        return;
    }
    last_recover_tick = HAL_GetTick();

    HAL_I2C_DeInit(&hi2c2);
    HAL_Delay(1);
    HAL_I2C_Init(&hi2c2);

    OLED_WriteCmd(0xAE);
    OLED_WriteCmd(0xD5); OLED_WriteCmd(0x80);
    OLED_WriteCmd(0xA8); OLED_WriteCmd(0x3F);
    OLED_WriteCmd(0xD3); OLED_WriteCmd(0x00);
    OLED_WriteCmd(0x40);
    OLED_WriteCmd(0xA1);
    OLED_WriteCmd(0xC8);
    OLED_WriteCmd(0xDA); OLED_WriteCmd(0x12);
    OLED_WriteCmd(0x81); OLED_WriteCmd(0xCF);
    OLED_WriteCmd(0xD9); OLED_WriteCmd(0xF1);
    OLED_WriteCmd(0xDB); OLED_WriteCmd(0x30);
    OLED_WriteCmd(0xA4);
    OLED_WriteCmd(0xA6);
    OLED_WriteCmd(0x8D); OLED_WriteCmd(0x14);
    OLED_WriteCmd(0xAF);
	
    OLED_Clear();
    oled_error_count = 0;
}

static void OLED_WriteCmd(uint8_t cmd)
{
    uint8_t buf[2] = {OLED_CMD, cmd};
    if (HAL_I2C_Master_Transmit(&hi2c2, OLED_ADDR, buf, 2, OLED_I2C_TIMEOUT) != HAL_OK)
    {
        if (++oled_error_count >= 3) {
            OLED_I2C_Recover();
        }
    }
    else
    {
        oled_error_count = 0;
    }
}

static void OLED_WriteData(uint8_t *data, uint16_t size)
{
    if (data == NULL || size > 128) return; 
	static uint8_t buf[129];
    buf[0] = OLED_DATA;
    memcpy(buf + 1, data, size);

    if (HAL_I2C_Master_Transmit(&hi2c2, OLED_ADDR, buf, size + 1, OLED_I2C_TIMEOUT) != HAL_OK)
    {
        if (++oled_error_count >= 3) {
            OLED_I2C_Recover();
        }
    }
    else
    {
        oled_error_count = 0;
    }
}

static void OLED_SetCursor(uint8_t page, uint8_t col)
{
    OLED_WriteCmd(0xB0 + page);
    OLED_WriteCmd(0x00 + (col & 0x0F));
    OLED_WriteCmd(0x10 + ((col >> 4) & 0x0F));
}

void OLED_Init(void)
{
    HAL_Delay(200);
    if (HAL_I2C_IsDeviceReady(&hi2c2, OLED_ADDR, 3, 100) != HAL_OK) {
        while (1);
    }

    OLED_WriteCmd(0xAE);
    OLED_WriteCmd(0xD5); OLED_WriteCmd(0x80);
    OLED_WriteCmd(0xA8); OLED_WriteCmd(0x3F);
    OLED_WriteCmd(0xD3); OLED_WriteCmd(0x00);
    OLED_WriteCmd(0x40);
    OLED_WriteCmd(0xA1);
    OLED_WriteCmd(0xC8);
    OLED_WriteCmd(0xDA); OLED_WriteCmd(0x12);
    OLED_WriteCmd(0x81); OLED_WriteCmd(0xCF);
    OLED_WriteCmd(0xD9); OLED_WriteCmd(0xF1);
    OLED_WriteCmd(0xDB); OLED_WriteCmd(0x30);
    OLED_WriteCmd(0xA4);
    OLED_WriteCmd(0xA6);
    OLED_WriteCmd(0x8D); OLED_WriteCmd(0x14);
    OLED_WriteCmd(0xAF);

    OLED_Clear();
    oled_error_count = 0;
}

void OLED_Clear(void)
{
    memset(OLED_GRAM, 0x00, sizeof(OLED_GRAM));
    OLED_Refresh();
}

void OLED_ShowChar(uint8_t line, uint8_t col, char ch)
{
    if (line < 1 || line > 4 || col < 1 || col > 16) return;
    uint8_t *p0 = &OLED_GRAM[(line - 1) * 2][(col - 1) * 8];
    uint8_t *p1 = &OLED_GRAM[(line - 1) * 2 + 1][(col - 1) * 8];
    const uint8_t *font = OLED_F8x16[ch - ' '];
    for (int i = 0; i < 8; i++) {
        p0[i] = font[i];
        p1[i] = font[i + 8];
    }
}

void OLED_ShowString(uint8_t line, uint8_t col, const char *str)
{
    if (str == NULL) return;  
	while (*str && col <= 16) {
        OLED_ShowChar(line, col++, *str++);
    }
}

void OLED_Refresh(void)
{
    for (uint8_t page = 0; page < OLED_PAGES; page++) {
        OLED_SetCursor(page, 0);
        OLED_WriteData(OLED_GRAM[page], OLED_WIDTH);
    }
}

void OLED_RefreshPage(uint8_t page)
{
    if (page >= OLED_PAGES) return;
    OLED_SetCursor(page, 0);
    OLED_WriteData(OLED_GRAM[page], OLED_WIDTH);
}
