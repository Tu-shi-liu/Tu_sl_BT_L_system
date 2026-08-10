#include "pwm.h"
#include "tim.h"

void PWM_Init(void)
{
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
}

void PWM_SetDuty(uint8_t channel, uint16_t duty)
{
    if (duty > 999) duty = 999;
    switch (channel) {
        case PWM_CH_RED:
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, duty);
            break;
        case PWM_CH_GREEN:
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, duty);
            break;
        case PWM_CH_BLUE:
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, duty);
            break;
    }
}
