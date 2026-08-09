#include "FB_FuzzyController.h"

#include <stddef.h>

#define FUZZY_CONTROLLER_EPSILON    (0.000001f)
#define FUZZY_CONTROLLER_FLOAT_MAX  (3.402823466e+38F)

static float FuzzyController_Clamp(float value, float minValue, float maxValue)
{
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static bool FuzzyController_IsFinite(float value)
{
    return (value == value) &&
           (value < FUZZY_CONTROLLER_FLOAT_MAX) &&
           (value > -FUZZY_CONTROLLER_FLOAT_MAX);
}

void FB_FuzzyController_Init(FB_FuzzyController_t *fb)
{
    if (fb == NULL) return;

    fb->config.Ts = FUZZY_CONTROLLER_TS;
    fb->config.Enable = true;
    fb->config.OutputMin = 0.0f;
    fb->config.OutputMax = 1000.0f;

    fb->state.SV = 0.0f;
    fb->state.PV = 0.0f;
    fb->state.Error = 0.0f;
    fb->state.dError = 0.0f;
    fb->state.PWM = 0.0f;
    fb->state.Centroid = 0.0f;
    fb->state.initialized = false;
    fb->state.firstRun = true;

    FB_FuzzyScaling_Init(&fb->scaling);
    FB_FuzzyMembership_Init(&fb->membership);
    FB_FuzzyRule_Init(&fb->ruleEngine);
    FB_FuzzyOutput_Init(&fb->output);

    FB_FuzzyController_LoadDefaultRule(fb);
    fb->state.initialized = true;
}

float FB_FuzzyController_Run(FB_FuzzyController_t *fb, float SV, float PV)
{
    float rulePWM;

    if (fb == NULL) return 0.0f;

    if (!fb->state.initialized)
        FB_FuzzyController_Init(fb);

    /* Fail-safe: invalid process data must never produce heater output. */
    if (!FuzzyController_IsFinite(SV) || !FuzzyController_IsFinite(PV))
    {
        fb->state.SV = SV;
        fb->state.PV = PV;
        fb->state.Error = 0.0f;
        fb->state.dError = 0.0f;
        fb->state.PWM = fb->config.OutputMin;
        fb->output.state.pwmFF = 0.0f;
        fb->output.state.fuzzyCorrection = 0.0f;
        fb->output.state.targetPWM = fb->config.OutputMin;
        fb->output.state.outputPWM = fb->config.OutputMin;
        fb->output.state.previousPWM = fb->config.OutputMin;
        fb->state.firstRun = true;
        return fb->state.PWM;
    }

    if (!fb->config.Enable)
    {
        fb->state.PWM = fb->config.OutputMin;
        return fb->state.PWM;
    }

    fb->state.SV = SV;
    fb->state.PV = PV;
    fb->state.Error = SV - PV;

    /* Avoid an artificial derivative spike on the first cycle. */
    if (fb->state.firstRun)
    {
        fb->scaling.State.PreviousError = fb->state.Error;
        fb->scaling.State.PreviousPV = PV;
        fb->state.firstRun = false;
    }

    FB_FuzzyScaling_Run(&fb->scaling, SV, PV);

    fb->state.Error = fb->scaling.State.Error;
    fb->state.dError = fb->scaling.State.dError;

    FB_FuzzyMembership_Run(
        &fb->membership,
        fb->scaling.State.NormalizedError,
        fb->scaling.State.NormalizedDError
    );

    FB_FuzzyRule_Run(&fb->ruleEngine, &fb->membership);

    /* Rule output is an absolute PWM singleton, 0..1000. */
    rulePWM = FuzzyController_Clamp(
        fb->ruleEngine.Result.RuleOutput,
        fb->config.OutputMin,
        fb->config.OutputMax
    );

    /* Output Manager handles FF policy, slew limiting and final clamp. */
    fb->state.PWM = FB_FuzzyOutput_RunAbsolute(
        &fb->output,
        SV,
        rulePWM,
        fb->config.Ts);

    /* Diagnostic normalized equivalent only; it is not used for control. */
    if (fb->config.OutputMax > fb->config.OutputMin + FUZZY_CONTROLLER_EPSILON)
    {
        fb->state.Centroid =
            ((rulePWM - fb->config.OutputMin) /
             (fb->config.OutputMax - fb->config.OutputMin)) * 2.0f - 1.0f;
    }
    else
    {
        fb->state.Centroid = 0.0f;
    }

    return fb->state.PWM;
}

void FB_FuzzyController_Reset(FB_FuzzyController_t *fb)
{
    if (fb == NULL) return;

    FB_FuzzyScaling_Reset(&fb->scaling);
    FB_FuzzyMembership_Reset(&fb->membership);
    FB_FuzzyRule_Reset(&fb->ruleEngine);

    fb->output.state.pwmFF = 0.0f;
    fb->output.state.fuzzyCorrection = 0.0f;
    fb->output.state.targetPWM = 0.0f;
    fb->output.state.outputPWM = 0.0f;
    fb->output.state.previousPWM = 0.0f;

    fb->state.SV = 0.0f;
    fb->state.PV = 0.0f;
    fb->state.Error = 0.0f;
    fb->state.dError = 0.0f;
    fb->state.PWM = fb->config.OutputMin;
    fb->state.Centroid = 0.0f;
    fb->state.firstRun = true;
}

void FB_FuzzyController_LoadDefaultRule(FB_FuzzyController_t *fb)
{
    if (fb == NULL) return;
    FB_FuzzyRule_LoadDefault(&fb->ruleEngine);
}

bool FB_FuzzyController_SetRule(
    FB_FuzzyController_t *fb,
    uint8_t errorIndex,
    uint8_t dErrorIndex,
    int16_t outputPWM)
{
    if (fb == NULL) return false;

    return FB_FuzzyRule_SetRule(
        &fb->ruleEngine,
        errorIndex,
        dErrorIndex,
        outputPWM
    );
}
