#include "bmp280.h"
#include "i2c.h"

/* 8 位 I2C 地址：0x76 左移一位 */
#define BMP280_I2C_ADDR     0xEC

#define BMP280_REG_ID          0xD0
#define BMP280_REG_STATUS      0xF3
#define BMP280_REG_CTRL_MEAS   0xF4
#define BMP280_REG_CONFIG      0xF5
#define BMP280_REG_PRESS_MSB   0xF7
#define BMP280_REG_CALIB       0x88

typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
} bmp280_calib_t;

typedef enum {
    BMP280_IDLE,
    BMP280_WAITING,
    BMP280_DONE,
    BMP280_ERROR
} BMP280_State;

static bmp280_calib_t bmp_cal;
static BMP280_State   state = BMP280_IDLE;
static uint32_t       wait_tick = 0;
static uint32_t       error_tick = 0;
static BMP280_Data    data_buffer = {0};

BMP280_Data bmp280_data = {0};

static uint8_t BMP280_GetCalib(void);
static int32_t BMP280_Compensate_T(int32_t adc_T, int32_t *t_fine);
static uint32_t BMP280_Compensate_P(int32_t adc_P, int32_t t_fine);

#define BMP280_TIMEOUT_MS   200

extern I2C_HandleTypeDef hi2c1;

static void I2C1_Recover(void) {
    HAL_I2C_DeInit(&hi2c1);
    HAL_I2C_Init(&hi2c1);
}

static uint8_t BMP280_WriteReg(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    if (HAL_I2C_Master_Transmit(&hi2c1, BMP280_I2C_ADDR, buf, 2, 100) == HAL_OK)
        return 1;
    else
        return 0;
}

static uint8_t BMP280_ReadReg(uint8_t reg, uint8_t *value) {
    if (HAL_I2C_Master_Transmit(&hi2c1, BMP280_I2C_ADDR, &reg, 1, 100) != HAL_OK)
        return 0;
    if (HAL_I2C_Master_Receive(&hi2c1, BMP280_I2C_ADDR, value, 1, 100) != HAL_OK)
        return 0;
    return 1;
}

static uint8_t BMP280_ReadRegs(uint8_t reg, uint8_t *buf, uint8_t len) {
    if (HAL_I2C_Master_Transmit(&hi2c1, BMP280_I2C_ADDR, &reg, 1, 100) != HAL_OK)
        return 0;
    if (HAL_I2C_Master_Receive(&hi2c1, BMP280_I2C_ADDR, buf, len, 100) != HAL_OK)
        return 0;
    return 1;
}

uint8_t BMP280_Init(void) {
    uint8_t id, retry = 5;
    do {
        if (BMP280_ReadReg(BMP280_REG_ID, &id) && id == 0x58) break;
        HAL_Delay(10);
    } while (--retry);
    if (retry == 0) {
        I2C1_Recover();
        return 0;
    }

    if (!BMP280_GetCalib()) {
        I2C1_Recover();
        return 0;
    }

    if (!BMP280_WriteReg(BMP280_REG_CONFIG, 0xA4)) return 0;
    if (!BMP280_WriteReg(BMP280_REG_CTRL_MEAS, 0x25)) return 0;

    wait_tick = HAL_GetTick();
    state = BMP280_WAITING;
    return 1;
}

void BMP280_StartMeasurement(void) {
    if (state == BMP280_IDLE || state == BMP280_ERROR) {
        if (!BMP280_WriteReg(BMP280_REG_CTRL_MEAS, 0x25)) {
            I2C1_Recover();
            state = BMP280_ERROR;
            error_tick = HAL_GetTick();
            return;
        }
        wait_tick = HAL_GetTick();
        state = BMP280_WAITING;
    }
}

void BMP280_Task(void) {
    uint8_t status;
    uint8_t data[6];
    int32_t adc_T, adc_P, t_fine;
    uint32_t now = HAL_GetTick();

    switch (state) {
        case BMP280_IDLE:
            if (BMP280_WriteReg(BMP280_REG_CTRL_MEAS, 0x25)) {
                wait_tick = now;
                state = BMP280_WAITING;
            } else {
                I2C1_Recover();
                state = BMP280_ERROR;
                error_tick = now;
            }
            break;

        case BMP280_WAITING:
            if (now - wait_tick > BMP280_TIMEOUT_MS) {
                I2C1_Recover();
                state = BMP280_ERROR;
                error_tick = now;
                break;
            }
            if (!BMP280_ReadReg(BMP280_REG_STATUS, &status)) {
                I2C1_Recover();
                state = BMP280_ERROR;
                error_tick = now;
                break;
            }
            if ((status & 0x08) == 0) {
                if (!BMP280_ReadRegs(BMP280_REG_PRESS_MSB, data, 6)) {
                    I2C1_Recover();
                    state = BMP280_ERROR;
                    error_tick = now;
                    break;
                }
                adc_P = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) | (data[2] >> 4);
                adc_T = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) | (data[5] >> 4);

                int32_t temp_x100 = BMP280_Compensate_T(adc_T, &t_fine);
                uint32_t press_pa = BMP280_Compensate_P(adc_P, t_fine);

                if (press_pa != 0) {
                    data_buffer.temperature = (int16_t)(temp_x100 / 10);
                    data_buffer.pressure = (uint16_t)(press_pa / 10);
                    data_buffer.ready = 1;
                    bmp280_data = data_buffer;
                }
                state = BMP280_DONE;
            }
            break;

        case BMP280_DONE:
            if (!data_buffer.ready) {
                state = BMP280_IDLE;
            }
            break;

        case BMP280_ERROR:
            if (now - error_tick > 1000) {
                if (BMP280_Init()) {
                } else {
                    error_tick = now;
                }
            }
            break;
    }
}

uint8_t BMP280_GetData(int16_t *temp, uint16_t *press) {
    if (!data_buffer.ready) return 0;
    *temp = data_buffer.temperature;
    *press = data_buffer.pressure;
    data_buffer.ready = 0;
    return 1;
}

static uint8_t BMP280_GetCalib(void) {
    uint8_t calib[24];
    if (!BMP280_ReadRegs(BMP280_REG_CALIB, calib, 24)) return 0;

    bmp_cal.dig_T1 = (uint16_t)(calib[0]  | (calib[1] << 8));
    bmp_cal.dig_T2 = (int16_t) (calib[2]  | (calib[3] << 8));
    bmp_cal.dig_T3 = (int16_t) (calib[4]  | (calib[5] << 8));
    bmp_cal.dig_P1 = (uint16_t)(calib[6]  | (calib[7] << 8));
    bmp_cal.dig_P2 = (int16_t) (calib[8]  | (calib[9] << 8));
    bmp_cal.dig_P3 = (int16_t) (calib[10] | (calib[11] << 8));
    bmp_cal.dig_P4 = (int16_t) (calib[12] | (calib[13] << 8));
    bmp_cal.dig_P5 = (int16_t) (calib[14] | (calib[15] << 8));
    bmp_cal.dig_P6 = (int16_t) (calib[16] | (calib[17] << 8));
    bmp_cal.dig_P7 = (int16_t) (calib[18] | (calib[19] << 8));
    bmp_cal.dig_P8 = (int16_t) (calib[20] | (calib[21] << 8));
    bmp_cal.dig_P9 = (int16_t) (calib[22] | (calib[23] << 8));
    return 1;
}

static int32_t BMP280_Compensate_T(int32_t adc_T, int32_t *t_fine) {
    int32_t var1, var2, T;
    var1 = ((((adc_T >> 3) - ((int32_t)bmp_cal.dig_T1 << 1))) *
            ((int32_t)bmp_cal.dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)bmp_cal.dig_T1)) *
              ((adc_T >> 4) - ((int32_t)bmp_cal.dig_T1))) >> 12) *
            ((int32_t)bmp_cal.dig_T3)) >> 14;
    *t_fine = var1 + var2;
    T = (*t_fine * 5 + 128) >> 8;   // 0.01 °C
    return T;
}

static uint32_t BMP280_Compensate_P(int32_t adc_P, int32_t t_fine) {
    int64_t var1, var2, p;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)bmp_cal.dig_P6;
    var2 = var2 + ((var1 * (int64_t)bmp_cal.dig_P5) << 17);
    var2 = var2 + (((int64_t)bmp_cal.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)bmp_cal.dig_P3) >> 8) +
           ((var1 * (int64_t)bmp_cal.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)bmp_cal.dig_P1) >> 33;
    if (var1 == 0) return 0;
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)bmp_cal.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)bmp_cal.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)bmp_cal.dig_P7) << 4);
    return (uint32_t)(p / 256);   // 返回 Pa
}
