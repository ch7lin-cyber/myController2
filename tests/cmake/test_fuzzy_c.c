#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "FB_FuzzyController.h"
#include "FB_FuzzyScaling.h"
#include "FB_FuzzyMembership.h"
#include "FB_FuzzyRule.h"
#include "FB_FuzzyDefuzzifier.h"
#include "FB_FuzzyOutputManager.h"
#include "FB_FuzzyHybridOutput.h"
#include "FB_FuzzyConfigManager.h"

static int nearly_equal(float a, float b)
{
    return fabsf(a - b) < 0.000001f;
}

int main(void)
{
    FB_FuzzyController_t controller;
    FB_FuzzyConfigManager_t config;
    float pwm;

    FB_FuzzyController_Init(&controller);
    FB_FuzzyConfig_Init(&config);

    assert(FB_FuzzyConfig_Check(&config));

    assert(FB_FuzzyController_GetSampleTime(&controller) == 20U);
    assert(nearly_equal(controller.config.Ts, 0.020f));

    assert(!FB_FuzzyController_SetSampleTime(&controller, 0U));
    assert(!FB_FuzzyController_SetSampleTime(&controller, 6001U));

    assert(FB_FuzzyController_SetSampleTime(&controller, 1U));
    assert(FB_FuzzyController_GetSampleTime(&controller) == 1U);
    assert(nearly_equal(controller.config.Ts, 0.001f));
    assert(nearly_equal(controller.scaling.Config.Ts, 0.001f));

    assert(FB_FuzzyController_SetSampleTime(&controller, 6000U));
    assert(FB_FuzzyController_GetSampleTime(&controller) == 6000U);
    assert(nearly_equal(controller.config.Ts, 6.0f));
    assert(nearly_equal(controller.scaling.Config.Ts, 6.0f));

    assert(FB_FuzzyController_SetSampleTime(&controller, 100U));

    pwm = FB_FuzzyController_Run(&controller, 130.0f, 25.0f);

    assert(isfinite(pwm));
    assert(pwm >= controller.config.OutputMin);
    assert(pwm <= controller.config.OutputMax);

    FB_FuzzyController_Reset(&controller);
    assert(controller.state.firstRun == true);
    assert(FB_FuzzyController_GetSampleTime(&controller) == 100U);
    assert(nearly_equal(controller.config.Ts, 0.100f));
    assert(nearly_equal(controller.scaling.Config.Ts, 0.100f));

    printf("C all-header/API + sample-time smoke test: PASS (PWM=%.3f)\n", pwm);
    return 0;
}
