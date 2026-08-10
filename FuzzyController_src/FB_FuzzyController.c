#include "FB_FuzzyController.h"

#include <stddef.h>

#define FUZZY_CONTROLLER_EPSILON    (0.000001f)
#define FUZZY_CONTROLLER_FLOAT_MAX  (3.402823466e+38F)
#define FUZZY_CONTROLLER_PWM_MIN    (0.0f)
#define FUZZY_CONTROLLER_PWM_MAX    (1000.0f)
#define FUZZY_MS_TO_SEC             (0.001f)

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

static bool FuzzyController_IsSampleTimeValid(uint32_t sampleTime_ms)
{
    return (sampleTime_ms >= FUZZY_CONTROLLER_SAMPLE_TIME_MIN_MS) &&
           (sampleTime_ms <= FUZZY_CONTROLLER_SAMPLE_TIME_MAX_MS);
}

static float FuzzyController_SampleTimeToSeconds(uint32_t sampleTime_ms)
{
    return (float)sampleTime_ms * FUZZY_MS_TO_SEC;
}

static float FuzzyController_GetSafeOutputMin(const FB_FuzzyController_t *fb)
{
    if ((fb != NULL) &&
        FuzzyController_IsFinite(fb->config.OutputMin) &&
        (fb->config.OutputMin >= FUZZY_CONTROLLER_PWM_MIN) &&
        (fb->config.OutputMin <= FUZZY_CONTROLLER_PWM_MAX))
    {
        return fb->config.OutputMin;
    }

    return FUZZY_CONTROLLER_PWM_MIN;
}

static bool FuzzyController_IsConfigValid(const FB_FuzzyController_t *fb)
{
    if (fb == NULL) return false;

    if (!FuzzyController_IsSampleTimeValid(fb->config.SampleTime_ms))
        return false;

    if (!FuzzyController_IsFinite(fb->config.Ts) ||
        (fb->config.Ts <= FUZZY_CONTROLLER_EPSILON))
        return false;

    if (!FuzzyController_IsFinite(fb->config.OutputMin) ||
        !FuzzyController_IsFinite(fb->config.OutputMax))
        return false;

    if ((fb->config.OutputMin < FUZZY_CONTROLLER_PWM_MIN) ||
        (fb->config.OutputMax > FUZZY_CONTROLLER_PWM_MAX) ||
        (fb->config.OutputMin >= fb->config.OutputMax))
        return false;

    return true;
}

static void FuzzyController_ForceOutputMin(FB_FuzzyController_t *fb)
{
    const float safeMin = FuzzyController_GetSafeOutputMin(fb);

    fb->state.PWM = safeMin;
    fb->output.state.pwmFF = 0.0f;
    fb->output.state.fuzzyCorrection = 0.0f;
    fb->output.state.targetPWM = safeMin;
    fb->output.state.outputPWM = safeMin;
    fb->output.state.previousPWM = safeMin;
}

void FB_FuzzyController_Init(FB_FuzzyController_t *fb)
{
    if (fb == NULL) return;

    fb->config.SampleTime_ms = FUZZY_CONTROLLER_SAMPLE_TIME_DEFAULT_MS;
    fb->config.Ts = FuzzyController_SampleTimeToSeconds(
        fb->config.SampleTime_ms);
    fb->config.Enable = true;
    fb->config.OutputMin = FUZZY_CONTROLLER_PWM_MIN;
    fb->config.OutputMax = FUZZY_CONTROLLER_PWM_MAX;

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

    /* Keep all time/output limits on the same controller configuration. */
    fb->scaling.Config.Ts = fb->config.Ts;
    fb->output.config.pwmMin = fb->config.OutputMin;
    fb->output.config.pwmMax = fb->config.OutputMax;

    FB_FuzzyController_LoadDefaultRule(fb);
    fb->state.initialized = true;
}

bool FB_FuzzyController_SetSampleTime(
    FB_FuzzyController_t *fb,
    uint32_t sampleTime_ms)
{
    float Ts;

    if (fb == NULL) return false;
    if (!FuzzyController_IsSampleTimeValid(sampleTime_ms)) return false;

    Ts = FuzzyController_SampleTimeToSeconds(sampleTime_ms);
    if (!FuzzyController_IsFinite(Ts) || (Ts <= FUZZY_CONTROLLER_EPSILON))
        return false;

    fb->config.SampleTime_ms = sampleTime_ms;
    fb->config.Ts = Ts;
    fb->scaling.Config.Ts = Ts;

    /*
     * This API is intended for configuration before cyclic execution. If it is
     * used again while stopped, restart derivative references cleanly without
     * changing the configured output limits or rule/membership data.
     */
    fb->state.firstRun = true;
    fb->state.dError = 0.0f;
    fb->scaling.State.dError = 0.0f;
    fb->scaling.State.PVRate = 0.0f;

    return true;
}

uint32_t FB_FuzzyController_GetSampleTime(const FB_FuzzyController_t *fb)
{
    if (fb == NULL) return 0U;
    return fb->config.SampleTime_ms;
}

float FB_FuzzyController_Run(FB_FuzzyController_t *fb, float SV, float PV)
{
    float rulePWM;

    if (fb == NULL) return 0.0f;

    if (!fb->state.initialized)
        FB_FuzzyController_Init(fb);

    /* Fail-safe: invalid controller configuration must never drive the heater. */
    if (!FuzzyController_IsConfigValid(fb))
    {
        FuzzyController_ForceOutputMin(fb);
        fb->state.Error = 0.0f;
        fb->state.dError = 0.0f;
        fb->state.firstRun = true;
        return fb->state.PWM;
    }

    /* Fail-safe: invalid process data must never produce heater output. */
    if (!FuzzyController_IsFinite(SV) || !FuzzyController_IsFinite(PV))
    {
        fb->state.SV = SV;
        fb->state.PV = PV;
        fb->state.Error = 0.0f;
        fb->state.dError = 0.0f;
        FuzzyController_ForceOutputMin(fb);
        fb->state.firstRun = true;
        return fb->state.PWM;
    }

    if (!fb->config.Enable)
    {
        /* A disabled heater controller must not retain a stale output state. */
        FuzzyController_ForceOutputMin(fb);
        fb->state.firstRun = true;
        return fb->state.PWM;
    }

    /* Synchronize dependent blocks with the public controller configuration. */
    fb->config.Ts = FuzzyController_SampleTimeToSeconds(
        fb->config.SampleTime_ms);
    fb->scaling.Config.Ts = fb->config.Ts;
    fb->output.config.pwmMin = fb->config.OutputMin;
    fb->output.config.pwmMax = fb->config.OutputMax;

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

    FuzzyController_ForceOutputMin(fb);

    fb->state.SV = 0.0f;
    fb->state.PV = 0.0f;
    fb->state.Error = 0.0f;
    fb->state.dError = 0.0f;
    fb->state.Centroid = 0.0f;
    fb->state.firstRun = true;

    /* Keep the externally configured execution period across Reset(). */
    fb->config.Ts = FuzzyController_SampleTimeToSeconds(
        fb->config.SampleTime_ms);
    fb->scaling.Config.Ts = fb->config.Ts;
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
