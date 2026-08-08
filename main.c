/******************************************************************************
 * File    : main.c
 * Brief   : Fuzzy heater controller application entry point
 *
 * Hardware-specific functions such as Temperature_GetC() and
 * PWM_SetDuty() are intentionally kept as application interfaces.
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include "FB_FuzzyController.h"

/* Hardware/application interfaces supplied by the target platform. */
extern float Temperature_GetC(void);
extern void PWM_SetDuty(uint16_t duty);
extern void delay_ms(uint32_t ms);

#define HEATER_SV_C              (175.0f)
#define CONTROLLER_PERIOD_MS     (20U)

static FB_FuzzyController_t HeaterFuzzy;

int main(void)
{
    float temperature;
    float pwm;

    /* Target-specific hardware initialization shall be performed here. */
    /* PWM_Init(); */
    /* TemperatureSensor_Init(); */

    FB_FuzzyController_Init(&HeaterFuzzy);

    PWM_SetDuty(0U);

    while (1)
    {
        /* Read the actual process variable immediately before control. */
        temperature = Temperature_GetC();

        /* Execute one 20 ms fuzzy-control cycle. */
        pwm = FB_FuzzyController_Run(
            &HeaterFuzzy,
            HEATER_SV_C,
            temperature);

        /* Controller output is absolute PWM: 0..1000 = 0.0..100.0%. */
        if (pwm < 0.0f)
        {
            pwm = 0.0f;
        }
        else if (pwm > 1000.0f)
        {
            pwm = 1000.0f;
        }

        PWM_SetDuty((uint16_t)(pwm + 0.5f));

        /*
         * Prototype timing only.
         * For the final MCU application, call the controller from a
         * 20 ms periodic timer/RTOS task so execution jitter is bounded.
         */
        delay_ms(CONTROLLER_PERIOD_MS);
    }
}
