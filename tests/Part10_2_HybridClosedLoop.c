/******************************************************************************
 * Part 10-2 - identified plant + fuzzy transient correction + FF + bias trim
 *
 * Build conceptually with:
 *   FuzzyController_src/FB_FuzzyController.c
 *   FuzzyController_src/FB_FuzzyScaling.c
 *   FuzzyController_src/FB_FuzzyMembership.c
 *   FuzzyController_src/FB_FuzzyRule.c
 *   FuzzyController_src/FB_FuzzyOutputManager.c
 *   FuzzyController_src/FB_FuzzyHybridOutput.c
 *   ControllPlant/myPlant_1.c
 *
 * Link with libm because the plant uses expf/fabsf/lroundf.
 ******************************************************************************/

#include <stdio.h>
#include "../FuzzyController_src/FB_FuzzyController.h"
#include "../FuzzyController_src/FB_FuzzyHybridOutput.h"
#include "../ControllPlant/myPlant.h"

#define TEST_TS_S       (0.020f)
#define TEST_DURATION_S (180.0f)

static void LoadIdentifiedFF(FB_FuzzyHybridOutput_t *hybrid)
{
    FuzzyHybridOutputConfig_t cfg = hybrid->config;

    /*
     * Inverse static characteristic of the identified 25 degC-ambient plant.
     * Temperature -> PWM (0..1000), with MV percent = PWM/10.
     */
    cfg.ffSize = 5U;
    cfg.ffTable[0].temperature = 25.0000f;  cfg.ffTable[0].pwm =   0.0f;
    cfg.ffTable[1].temperature = 93.4000f;  cfg.ffTable[1].pwm = 200.0f;
    cfg.ffTable[2].temperature = 149.1500f; cfg.ffTable[2].pwm = 500.0f;
    cfg.ffTable[3].temperature = 160.8400f; cfg.ffTable[3].pwm = 800.0f;
    cfg.ffTable[4].temperature = 168.6333f; cfg.ffTable[4].pwm = 1000.0f;

    cfg.neutralFuzzyPWM = 100.0f;
    cfg.positiveCorrectionGain = 0.50f;
    cfg.negativeCorrectionGain = 5.00f;
    cfg.biasKi = 0.05f;
    cfg.biasMin = -200.0f;
    cfg.biasMax = 200.0f;
    cfg.slewRate = 5000.0f;
    cfg.enableFeedForward = true;
    cfg.enableBiasTrim = true;
    cfg.enableSlew = true;

    (void)FB_FuzzyHybridOutput_SetConfig(hybrid, &cfg);
}

static void RunCase(float sv)
{
    FB_FuzzyController_t controller;
    FB_FuzzyHybridOutput_t hybrid;
    ThermalPlant_t plant;
    const unsigned long cycles =
        (unsigned long)(TEST_DURATION_S / TEST_TS_S);
    unsigned long k;
    float maxPV = 25.0f;
    float hybridPWM = 0.0f;

    FB_FuzzyController_Init(&controller);
    FB_FuzzyHybridOutput_Init(&hybrid);
    LoadIdentifiedFF(&hybrid);
    ThermalPlant_Init(&plant, 25.0f, 25.0f, TEST_TS_S);

    for (k = 0UL; k < cycles; ++k)
    {
        const float pv = plant.temperature_c;
        const float unusedControllerPWM =
            FB_FuzzyController_Run(&controller, sv, pv);
        const float fuzzyRulePWM = controller.ruleEngine.Result.RuleOutput;
        const float mvPercent;

        (void)unusedControllerPWM;

        hybridPWM = FB_FuzzyHybridOutput_Run(
            &hybrid,
            sv,
            pv,
            fuzzyRulePWM,
            TEST_TS_S);

        mvPercent = hybridPWM * 0.1f;
        (void)ThermalPlant_Step(&plant, mvPercent);

        if (plant.temperature_c > maxPV)
            maxPV = plant.temperature_c;
    }

    printf("SV=%7.2f  PV=%9.4f  Err=%9.4f  MaxPV=%9.4f  Overshoot=%8.4f  PWM=%8.3f  FF=%8.3f  Bias=%8.3f\n",
           sv,
           plant.temperature_c,
           sv - plant.temperature_c,
           maxPV,
           maxPV - sv,
           hybridPWM,
           hybrid.state.feedForwardPWM,
           hybrid.state.biasPWM);
}

int main(void)
{
    RunCase(50.0f);
    RunCase(100.0f);
    RunCase(150.0f);
    RunCase(175.0f);
    return 0;
}
