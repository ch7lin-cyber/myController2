#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "FB_FuzzyController.h"
#include "FB_FuzzyConfigManager.h"

int main(void)
{
    FB_FuzzyController_t controller;
    FB_FuzzyConfigManager_t config;
    float pwm;

    FB_FuzzyController_Init(&controller);
    FB_FuzzyConfig_Init(&config);

    assert(FB_FuzzyConfig_Check(&config));

    pwm = FB_FuzzyController_Run(&controller, 130.0f, 25.0f);

    assert(isfinite(pwm));
    assert(pwm >= controller.config.OutputMin);
    assert(pwm <= controller.config.OutputMax);

    FB_FuzzyController_Reset(&controller);
    assert(controller.state.firstRun == true);

    printf("C API smoke test: PASS (PWM=%.3f)\n", pwm);
    return 0;
}
