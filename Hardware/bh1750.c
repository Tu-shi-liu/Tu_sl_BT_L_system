#include "bh1750.h"
#include "i2c.h"

static uint32_t GetTick(void) {
    extern volatile uint32_t g_msTick;
    return g_msTick;
}

typedef enum {
    STATE_IDLE,
    STATE_INIT_POWER_ON,
    STATE_INIT_SET_MODE,
    STATE_WAIT_MEASURE,
    STATE_READ_DATA,
    STATE_DONE,
    STATE_ERROR
} BH1750_State;

static BH1750_State state = STATE_IDLE;
static uint32_t     state_timer = 0;
static uint8_t      init_step = 0;
static uint8_t      retry_count = 0;
static BH1750_Data  data = {0};

BH1750_Data bh1750_data = {0};

static void I2C1_Recover(void) {
    extern I2C_HandleTypeDef hi2c1;
    HAL_I2C_DeInit(&hi2c1);
    HAL_I2C_Init(&hi2c1);
}

static uint8_t I2C_WriteByte(uint8_t addr, uint8_t cmd) {
    if (HAL_I2C_Master_Transmit(&hi2c1, addr << 1, &cmd, 1, 100) == HAL_OK)
        return 1;
    else
        return 0;
}

void BH1750_Init(void) {
    state = STATE_INIT_POWER_ON;
    state_timer = GetTick();
    retry_count = 0;
    init_step = 0;
    data.ready = 0;
    bh1750_data.ready = 0;
}

uint8_t BH1750_GetLux(uint16_t *lux) {
    if (data.ready == 0) return 0;
    *lux = data.lux_raw;
    data.ready = 0;
    bh1750_data.ready = 0;
    return 1;
}

void BH1750_Trigger(void) {
    if (BH1750_MODE == BH1750_MODE_SINGLE) {
        state = STATE_IDLE;
    }
}

void BH1750_Task(void) {
    uint32_t now = GetTick();

    switch (state) {
        case STATE_INIT_POWER_ON:
            if (I2C_WriteByte(BH1750_ADDR, 0x01)) {
                state_timer = now;
                state = STATE_INIT_SET_MODE;
                init_step = 0;
                retry_count = 0;
            } else {
                if (++retry_count >= 3) {
                    I2C1_Recover();
                    state = STATE_ERROR;
                    state_timer = now;
                }
            }
            break;

        case STATE_INIT_SET_MODE:
            if (now - state_timer < 10) break;
            if (init_step == 0) {
                uint8_t cmd = (BH1750_MODE == BH1750_MODE_CONTINUOUS) ? 0x10 : 0x20;
                if (I2C_WriteByte(BH1750_ADDR, cmd)) {
                    init_step = 1;
                    state_timer = now;
                    retry_count = 0;
                } else {
                    if (++retry_count >= 3) {
                        I2C1_Recover();
                        state = STATE_ERROR;
                        state_timer = now;
                    }
                }
            } else {
                if (now - state_timer >= 180) {
                    state = STATE_IDLE;
                }
            }
            break;

        case STATE_IDLE:
#if BH1750_MODE == BH1750_MODE_CONTINUOUS
            state_timer = now;
            state = STATE_WAIT_MEASURE;
#else
            if (I2C_WriteByte(BH1750_ADDR, 0x20)) {
                state_timer = now;
                state = STATE_WAIT_MEASURE;
                retry_count = 0;
            } else {
                if (++retry_count >= 3) {
                    I2C1_Recover();
                    state = STATE_ERROR;
                    state_timer = now;
                }
            }
#endif
            break;

        case STATE_WAIT_MEASURE: {
            uint32_t wait = (BH1750_MODE == BH1750_MODE_CONTINUOUS) ? BH1750_INTERVAL_MS : 180;
            if (now - state_timer >= wait) {
                uint8_t buf[2];
                if (HAL_I2C_Master_Receive(&hi2c1, BH1750_ADDR << 1, buf, 2, 100) == HAL_OK) {
                    uint16_t raw = ((uint16_t)buf[0] << 8) | buf[1];
                    data.lux_raw = (uint16_t)(((uint32_t)raw * 25) / 3);
                    data.ready = 1;
                    bh1750_data = data;
                    state = STATE_DONE;
                    retry_count = 0;
                } else {
                    if (++retry_count >= 3) {
                        I2C1_Recover();
                        state = STATE_ERROR;
                        state_timer = now;
                    }
                }
            }
            break;
        }

        case STATE_DONE:
            if (!data.ready) {
                state = STATE_IDLE;
            }
            break;

        case STATE_ERROR:
            if (now - state_timer > 2000) {
                BH1750_Init();
            }
            break;

        default:
            state = STATE_IDLE;
            break;
    }
}
