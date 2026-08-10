#include "key.h"
#include "gpio.h"

#define DEBOUNCE_MS      20
#define DOUBLE_CLICK_MS  500
#define LONG_PRESS_MS    1000

typedef enum {
    KS_RELEASED,
    KS_PRESS_DEB,
    KS_PRESSED,
    KS_WAIT_DOUBLE,
    KS_LONG,
    KS_IGNORE
} KeyState;

typedef struct {
    uint8_t id;
    GPIO_TypeDef *port;
    uint16_t pin;
    KeyState state;
    uint16_t timer;
    uint8_t event_flag;
    uint8_t event;
    uint8_t pending_click;
} Key_Type;

static Key_Type keys[3] = {
    {KEY1, GPIOB, GPIO_PIN_12, KS_RELEASED, 0, 0, KEY_EVENT_NONE, 0},
    {KEY2, GPIOB, GPIO_PIN_13, KS_RELEASED, 0, 0, KEY_EVENT_NONE, 0},
    {KEY3, GPIOB, GPIO_PIN_14, KS_RELEASED, 0, 0, KEY_EVENT_NONE, 0}
};

void Key_Init(void) {}

void Key_Scan(void) {
    for (uint8_t i = 0; i < 3; i++) {
        uint8_t cur = (HAL_GPIO_ReadPin(keys[i].port, keys[i].pin) == GPIO_PIN_RESET) ? 1 : 0;
        switch (keys[i].state) {
            case KS_RELEASED:
                if (cur) { keys[i].state = KS_PRESS_DEB; keys[i].timer = 0; }
                break;
            case KS_PRESS_DEB:
                if (cur) {
                    if (++keys[i].timer >= DEBOUNCE_MS) { keys[i].state = KS_PRESSED; keys[i].timer = 0; }
                } else keys[i].state = KS_RELEASED;
                break;
            case KS_PRESSED:
                if (cur) {
                    if (++keys[i].timer >= LONG_PRESS_MS && !keys[i].event_flag) {
                        keys[i].event = KEY_EVENT_LONG_PRESS; keys[i].event_flag = 1;
                        keys[i].pending_click = 0;
                        keys[i].state = KS_LONG;
                    }
                } else {
                    if (keys[i].timer < LONG_PRESS_MS) {
                        keys[i].state = KS_WAIT_DOUBLE; keys[i].timer = 0; keys[i].pending_click = 1;
                    } else keys[i].state = KS_RELEASED;
                }
                break;
            case KS_WAIT_DOUBLE:
                if (cur) {
                    if (keys[i].timer <= DOUBLE_CLICK_MS && keys[i].pending_click) {
                        keys[i].event = KEY_EVENT_DOUBLE_CLICK; keys[i].event_flag = 1;
                        keys[i].pending_click = 0;
                        keys[i].state = KS_IGNORE;
                    } else {
                        keys[i].state = KS_PRESS_DEB; keys[i].timer = 0; keys[i].pending_click = 0;
                    }
                } else {
                    if (++keys[i].timer > DOUBLE_CLICK_MS) {
                        if (keys[i].pending_click) {
                            keys[i].event = KEY_EVENT_SINGLE_CLICK; keys[i].event_flag = 1;
                        }
                        keys[i].state = KS_RELEASED;
                    }
                }
                break;
            case KS_LONG:
                if (!cur) keys[i].state = KS_RELEASED;
                break;
            case KS_IGNORE:
                if (!cur) keys[i].state = KS_RELEASED;
                break;
        }
    }
}

uint8_t Key_GetEvent(uint8_t key_id) {
    if (keys[key_id].event_flag) {
        keys[key_id].event_flag = 0;
        uint8_t e = keys[key_id].event;
        keys[key_id].event = KEY_EVENT_NONE;
        return e;
    }
    return KEY_EVENT_NONE;
}
