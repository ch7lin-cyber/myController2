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
    FB_FuzzyHybridOutput_t hybrid;
    float pwm;
    float ff130;
    float bias_before;

    FB_FuzzyController_Init(&controller);
    FB_FuzzyConfig_Init(&config);

    assert(FB_FuzzyConfig_Check(&config));

    assert(FB_FuzzyController_GetSampleTime(&controller) == 20U);
    assert(nearly_equal(controller.config.Ts, 0.020f));
    assert(nearly_equal(controller.config.DErrorFilterTau_s, 0.20f));
    assert(nearly_equal(controller.config.DErrorDeadband_c_per_s, 0.20f));
    assert(controller.config.EnableBoost == true);
    assert(nearly_equal(controller.config.BoostEnterError_c, 20.0f));
    assert(nearly_equal(controller.config.BoostExitError_c, 18.0f));
    assert(controller.config.UseHybridOutput == false);

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

    /* Real-data regression: 0.1 degC LSB at 20 ms => raw |dPV/dt| = 5 degC/s. */
    FB_FuzzyController_Reset(&controller);
    assert(FB_FuzzyController_SetSampleTime(&controller, 20U));
    assert(FB_FuzzyController_SetDerivativeFilter(&controller, 0.20f, 0.20f));
    controller.config.Enable = true;

    (void)FB_FuzzyController_Run(&controller, 130.0f, 120.8f);
    (void)FB_FuzzyController_Run(&controller, 130.0f, 120.9f);

    assert(controller.state.RawDError < -4.9f);
    assert(fabsf(controller.state.FilteredDError) < 1.0f);
    assert(fabsf(controller.state.dError) < 1.0f);
    assert(fabsf(controller.scaling.State.NormalizedDError) < 1.0f);

    /* Derivative-on-PV: an SV step at constant PV must not create derivative kick. */
    FB_FuzzyController_Reset(&controller);
    assert(FB_FuzzyController_SetSampleTime(&controller, 20U));
    (void)FB_FuzzyController_Run(&controller, 100.0f, 80.0f);
    (void)FB_FuzzyController_Run(&controller, 130.0f, 80.0f);
    assert(fabsf(controller.state.RawDError) < 0.0001f);
    assert(fabsf(controller.state.dError) < 0.0001f);

    assert(!FB_FuzzyController_SetDerivativeFilter(&controller, -0.1f, 0.2f));
    assert(!FB_FuzzyController_SetDerivativeFilter(&controller, 0.2f, -0.1f));

    assert(!FB_FuzzyController_SetBoostConfig(&controller, true, 18.0f, 20.0f));
    assert(FB_FuzzyController_SetBoostConfig(&controller, true, 20.0f, 18.0f));

    /* Identified FF map: 130 C should interpolate near 397 PWM. */
    ff130 = FB_FuzzyHybridOutput_CalcFF(&controller.hybridOutput, 130.0f);
    assert(ff130 > 390.0f);
    assert(ff130 < 405.0f);

    /* Bias learning is only allowed close to SV. */
    FB_FuzzyHybridOutput_Init(&hybrid);
    hybrid.config.ffSize = 0U;
    hybrid.config.enableFeedForward = false;
    hybrid.state.biasPWM = 25.0f;
    bias_before = hybrid.state.biasPWM;
    (void)FB_FuzzyHybridOutput_Run(&hybrid, 130.0f, 120.0f, 100.0f, 0.02f);
    assert(nearly_equal(hybrid.state.biasPWM, bias_before));
    (void)FB_FuzzyHybridOutput_Run(&hybrid, 130.0f, 129.0f, 100.0f, 0.02f);
    assert(hybrid.state.biasPWM > bias_before);

    /* Boost: enter above 20 C, hold through hysteresis, exit below 18 C. */
    FB_FuzzyController_Reset(&controller);
    FB_FuzzyController_EnableHybridOutput(&controller, true);
    controller.config.Enable = true;
    controller.hybridOutput.state.biasPWM = 100.0f;

    pwm = FB_FuzzyController_Run(&controller, 130.0f, 100.0f); /* error = 30 */
    assert(controller.state.BoostActive == true);
    assert(nearly_equal(controller.hybridOutput.state.biasPWM, 0.0f));
    assert(nearly_equal(pwm, 1000.0f));

    pwm = FB_FuzzyController_Run(&controller, 130.0f, 111.0f); /* error = 19 */
    assert(controller.state.BoostActive == true);
    assert(nearly_equal(pwm, 1000.0f));

    pwm = FB_FuzzyController_Run(&controller, 130.0f, 112.1f); /* error = 17.9 */
    assert(controller.state.BoostActive == false);
    assert(isfinite(pwm));
    assert(pwm >= 0.0f && pwm <= 1000.0f);

    /* Normal hybrid path still works inside the non-boost region. */
    FB_FuzzyController_Reset(&controller);
    FB_FuzzyController_EnableHybridOutput(&controller, true);
    controller.config.Enable = true;
    pwm = FB_FuzzyController_Run(&controller, 130.0f, 120.0f);

    assert(controller.config.UseHybridOutput == true);
    assert(controller.state.BoostActive == false);
    assert(controller.hybridOutput.state.feedForwardPWM > 390.0f);
    assert(controller.hybridOutput.state.targetPWM >= controller.hybridOutput.state.feedForwardPWM);
    assert(isfinite(pwm));
    assert(pwm >= 0.0f && pwm <= 1000.0f);

    printf("C branch3 regression PASS (PWM=%.3f, FF130=%.3f)\n",
           pwm, ff130);
    return 0;
}
