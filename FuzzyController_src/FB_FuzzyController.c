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

static float FuzzyController_Abs(float value)
{
    return (value >= 0.0f) ? value : -value;
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

static bool FuzzyController_IsDerivativeConfigValid(const FB_FuzzyController_t *fb)
{
    if (fb == NULL) return false;

    if (!FuzzyController_IsFinite(fb->config.DErrorFilterTau_s) ||
        (fb->config.DErrorFilterTau_s < 0.0f) ||
        (fb->config.DErrorFilterTau_s > FUZZY_CONTROLLER_DERROR_FILTER_TAU_MAX_S))
        return false;

    if (!FuzzyController_IsFinite(fb->config.DErrorDeadband_c_per_s) ||
        (fb->config.DErrorDeadband_c_per_s < 0.0f) ||
        (fb->config.DErrorDeadband_c_per_s > FUZZY_CONTROLLER_DERROR_DEADBAND_MAX))
        return false;

    return true;
}

static bool FuzzyController_IsConfigValid(const FB_FuzzyController_t *fb)
{
    if (fb == NULL) return false;

    if (!FuzzyController_IsSampleTimeValid(fb->config.SampleTime_ms))
        return false;

    if (!FuzzyController_IsFinite(fb->config.Ts) ||
        (fb->config.Ts <= FUZZY_CONTROLLER_EPSILON))
        return false;

    if (!FuzzyController_IsDerivativeConfigValid(fb))
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

static float FuzzyController_FilterDError(
    FB_FuzzyController_t *fb,
    float rawDError)
{
    float filtered;
    float effective;
    float alpha;
    float magnitude;

    if ((fb == NULL) || !FuzzyController_IsFinite(rawDError))
        return 0.0f;

    if (fb->config.DErrorFilterTau_s <= FUZZY_CONTROLLER_EPSILON)
    {
        filtered = rawDError;
    }
    else
    {
        alpha = fb->config.Ts /
                (fb->config.DErrorFilterTau_s + fb->config.Ts);
        alpha = FuzzyController_Clamp(alpha, 0.0f, 1.0f);
        filtered = fb->state.FilteredDError +
                   alpha * (rawDError - fb->state.FilteredDError);
    }

    if (!FuzzyController_IsFinite(filtered)) filtered = 0.0f;
    fb->state.FilteredDError = filtered;

    magnitude = FuzzyController_Abs(filtered);
    if (magnitude <= fb->config.DErrorDeadband_c_per_s)
    {
        effective = 0.0f;
    }
    else
    {
        magnitude -= fb->config.DErrorDeadband_c_per_s;
        effective = (filtered >= 0.0f) ? magnitude : -magnitude;
    }

    return FuzzyController_IsFinite(effective) ? effective : 0.0f;
}

static void FuzzyController_ClearDerivativeState(FB_FuzzyController_t *fb)
{
    if (fb == NULL) return;

    fb->state.RawDError = 0.0f;
    fb->state.FilteredDError = 0.0f;
    fb->state.dError = 0.0f;
    fb->scaling.State.dError = 0.0f;
    fb->scaling.State.PVRate = 0.0f;
    fb->scaling.State.NormalizedDError = 0.0f;
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
    fb->config.DErrorFilterTau_s = FUZZY_CONTROLLER_DERROR_FILTER_TAU_DEFAULT_S;
    fb->config.DErrorDeadband_c_per_s = FUZZY_CONTROLLER_DERROR_DEADBAND_DEFAULT;
    fb->config.Enable = true;
    fb->config.OutputMin = FUZZY_CONTROLLER_PWM_MIN;
    fb->config.OutputMax = FUZZY_CONTROLLER_PWM_MAX;

    fb->state.SV = 0.0f;
    fb->state.PV = 0.0f;
    fb->state.Error = 0.0f;
    fb->state.RawDError = 0.0f;
    fb->state.FilteredDError = 0.0f;
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

    /* Configuration-stage change: restart derivative references cleanly. */
    fb->state.firstRun = true;
    FuzzyController_ClearDerivativeState(fb);

    return true;
}

uint32_t FB_FuzzyController_GetSampleTime(const FB_FuzzyController_t *fb)
{
    if (fb == NULL) return 0U;
    return fb->config.SampleTime_ms;
}

bool FB_FuzzyController_SetDerivativeFilter(
    FB_FuzzyController_t *fb,
    float filterTau_s,
    float deadband_c_per_s)
{
    if (fb == NULL) return false;

    if (!FuzzyController_IsFinite(filterTau_s) ||
        (filterTau_s < 0.0f) ||
        (filterTau_s > FUZZY_CONTROLLER_DERROR_FILTER_TAU_MAX_S))
        return false;

    if (!FuzzyController_IsFinite(deadband_c_per_s) ||
        (deadband_c_per_s < 0.0f) ||
        (deadband_c_per_s > FUZZY_CONTROLLER_DERROR_DEADBAND_MAX))
        return false;

    fb->config.DErrorFilterTau_s = filterTau_s;
    fb->config.DErrorDeadband_c_per_s = deadband_c_per_s;
    fb->state.firstRun = true;
    FuzzyController_ClearDerivativeState(fb);
    return true;
}

float FB_FuzzyController_Run(FB_FuzzyController_t *fb, float SV, float PV)
{
    float rulePWM;
    float rawDError;

    if (fb == NULL) return 0.0f;

    if (!fb->state.initialized)
        FB_FuzzyController_Init(fb);

    /* Fail-safe: invalid controller configuration must never drive the heater. */
    if (!FuzzyController_IsConfigValid(fb))
    {
        FuzzyController_ForceOutputMin(fb);
        fb->state.Error = 0.0f;
        FuzzyController_ClearDerivativeState(fb);
        fb->state.firstRun = true;
        return fb->state.PWM;
    }

    /* Fail-safe: invalid process data must never produce heater output. */
    if (!FuzzyController_IsFinite(SV) || !FuzzyController_IsFinite(PV))
    {
        fb->state.SV = SV;
        fb->state.PV = PV;
        fb->state.Error = 0.0f;
        FuzzyController_ClearDerivativeState(fb);
        FuzzyController_ForceOutputMin(fb);
        fb->state.firstRun = true;
        return fb->state.PWM;
    }

    if (!fb->config.Enable)
    {
        /* A disabled heater controller must not retain a stale output state. */
        FuzzyController_ForceOutputMin(fb);
        FuzzyController_ClearDerivativeState(fb);
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
        fb->state.RawDError = 0.0f;
        fb->state.FilteredDError = 0.0f;
        fb->state.dError = 0.0f;
        fb->state.firstRun = false;
    }

    FB_FuzzyScaling_Run(&fb->scaling, SV, PV);

    fb->state.Error = fb->scaling.State.Error;

    /*
     * Scaling computes the mathematically raw derivative.  Preserve it for
     * diagnostics, then condition it before fuzzy inference.  This prevents a
     * 0.1 degC sensor LSB at 20 ms from appearing as an immediate +/-5 degC/s
     * full-scale fuzzy event.
     */
    rawDError = fb->scaling.State.dError;
    fb->state.RawDError = rawDError;
    fb->state.dError = FuzzyController_FilterDError(fb, rawDError);

    fb->scaling.State.dError = fb->state.dError;
    fb->scaling.State.NormalizedDError = FB_FuzzyScaling_NormalizeDError(
        fb->state.dError,
        fb->scaling.State.Kde);

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
    fb->state.RawDError = 0.0f;
    fb->state.FilteredDError = 0.0f;
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
