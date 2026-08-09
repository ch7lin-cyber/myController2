/******************************************************************************
 * File    : FB_FuzzyScaling.c
 * Version : V2.2
 *
 * Brief   : Auto / Adaptive Scaling Engine
 *
 * Safety revision:
 *   - Reject NaN / +/-Inf configuration values.
 *   - Reject invalid SV/PV at runtime without propagating NaN/Inf.
 *   - Reject invalid public calculation inputs.
 *   - Preserve the last valid state when an input sample is invalid.
 *   - Error window adaptation is applied to the runtime state.
 ******************************************************************************/

#include "FB_FuzzyScaling.h"
#include <stddef.h>
#include <float.h>

#define FUZZY_SCALING_EPSILON             (0.000001f)
#define FUZZY_SCALING_ERROR_RATIO         (1.50f)
#define FUZZY_SCALING_MIN_ERROR_REF       (2.0f)
#define FUZZY_SCALING_KDE_RATIO           (0.10f)
#define FUZZY_SCALING_DYNAMIC_KU_GAIN     (0.50f)

/* Runtime adaptation rates. Units are per second. */
#define FUZZY_SCALING_ERROR_WINDOW_SLEW   (100.0f)
#define FUZZY_SCALING_KE_SLEW_RATE        (5.0f)
#define FUZZY_SCALING_KDE_SLEW_RATE       (5.0f)

static bool FuzzyScaling_IsFinite(float x)
{
    /* Portable embedded-C finite check; does not require C99 isfinite(). */
    return ((x == x) && (x <= FLT_MAX) && (x >= -FLT_MAX));
}

static float FuzzyScaling_Abs(float x)
{
    return (x >= 0.0f) ? x : -x;
}

static float FuzzyScaling_Max(float a, float b)
{
    return (a > b) ? a : b;
}

static float FuzzyScaling_Clamp(float value, float minValue, float maxValue)
{
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

void FB_FuzzyScaling_Init(FB_FuzzyScaling_t *fb)
{
    if (fb == NULL) return;

    fb->Config.Ts = FUZZY_SCALING_DEFAULT_TS;
    fb->Config.MinTemperature = FUZZY_SCALING_DEFAULT_MIN_TEMP;
    fb->Config.MaxTemperature = FUZZY_SCALING_DEFAULT_MAX_TEMP;
    fb->Config.BaseErrorWindow = FUZZY_SCALING_DEFAULT_ERROR_WINDOW;
    fb->Config.MinErrorWindow = FUZZY_SCALING_MIN_ERROR_WINDOW;
    fb->Config.MaxErrorWindow = FUZZY_SCALING_MAX_ERROR_WINDOW;
    fb->Config.MinKe = FUZZY_SCALING_MIN_KE;
    fb->Config.MaxKe = FUZZY_SCALING_MAX_KE;
    fb->Config.MinKde = FUZZY_SCALING_MIN_KDE;
    fb->Config.MaxKde = FUZZY_SCALING_MAX_KDE;
    fb->Config.MinKu = FUZZY_SCALING_MIN_KU;
    fb->Config.MaxKu = FUZZY_SCALING_MAX_KU;
    fb->Config.DynamicGain = FUZZY_SCALING_DEFAULT_DYNAMIC_GAIN;
    fb->Config.MaxPVRate = FUZZY_SCALING_DEFAULT_MAX_PV_RATE;
    fb->Config.KuSlewRate = FUZZY_SCALING_DEFAULT_KU_SLEW_RATE;
    fb->Config.AutoScalingEnable = true;
    fb->Config.AdaptiveEnable = true;

    fb->State.Ke = 0.05f;
    fb->State.Kde = 0.10f;
    fb->State.Ku = 1.00f;
    fb->State.TargetKe = fb->State.Ke;
    fb->State.TargetKde = fb->State.Kde;
    fb->State.TargetKu = fb->State.Ku;
    fb->State.ErrorWindow = fb->Config.BaseErrorWindow;
    fb->State.TargetErrorWindow = fb->State.ErrorWindow;
    fb->State.Error = 0.0f;
    fb->State.PreviousError = 0.0f;
    fb->State.dError = 0.0f;
    fb->State.PV = 0.0f;
    fb->State.PreviousPV = 0.0f;
    fb->State.PVRate = 0.0f;
    fb->State.DynamicFactor = 1.0f;
    fb->State.NormalizedError = 0.0f;
    fb->State.NormalizedDError = 0.0f;
    fb->State.Initialized = true;
}

void FB_FuzzyScaling_Reset(FB_FuzzyScaling_t *fb)
{
    if (fb == NULL) return;

    fb->State.Error = 0.0f;
    fb->State.PreviousError = 0.0f;
    fb->State.dError = 0.0f;
    fb->State.PV = 0.0f;
    fb->State.PreviousPV = 0.0f;
    fb->State.PVRate = 0.0f;
    fb->State.DynamicFactor = 1.0f;
    fb->State.NormalizedError = 0.0f;
    fb->State.NormalizedDError = 0.0f;
    fb->State.ErrorWindow = fb->Config.BaseErrorWindow;
    fb->State.TargetErrorWindow = fb->State.ErrorWindow;
    fb->State.Ke = 0.05f;
    fb->State.Kde = 0.10f;
    fb->State.Ku = 1.0f;
    fb->State.TargetKe = fb->State.Ke;
    fb->State.TargetKde = fb->State.Kde;
    fb->State.TargetKu = fb->State.Ku;
}

float FB_FuzzyScaling_CalculateErrorWindow(FB_FuzzyScaling_t *fb, float sv, float pv)
{
    float absoluteError;
    float window;

    if ((fb == NULL) || !FuzzyScaling_IsFinite(sv) || !FuzzyScaling_IsFinite(pv))
    {
        return FUZZY_SCALING_DEFAULT_ERROR_WINDOW;
    }

    absoluteError = FuzzyScaling_Abs(sv - pv);
    if (!FuzzyScaling_IsFinite(absoluteError))
    {
        return FUZZY_SCALING_DEFAULT_ERROR_WINDOW;
    }

    window = fb->Config.BaseErrorWindow;

    if (fb->Config.AdaptiveEnable &&
        absoluteError > FUZZY_SCALING_MIN_ERROR_REF)
    {
        window = FuzzyScaling_Max(
            absoluteError * FUZZY_SCALING_ERROR_RATIO,
            fb->Config.BaseErrorWindow);
    }

    window = FuzzyScaling_Clamp(
        window,
        fb->Config.MinErrorWindow,
        fb->Config.MaxErrorWindow);

    fb->State.TargetErrorWindow = window;
    return window;
}

float FB_FuzzyScaling_CalculateDynamicFactor(FB_FuzzyScaling_t *fb, float pvRate)
{
    float normalizedRate;
    float factor;

    if ((fb == NULL) || !FuzzyScaling_IsFinite(pvRate)) return 1.0f;

    if (fb->Config.MaxPVRate > FUZZY_SCALING_EPSILON)
    {
        normalizedRate = FuzzyScaling_Abs(pvRate) / fb->Config.MaxPVRate;
    }
    else
    {
        normalizedRate = 0.0f;
    }

    if (!FuzzyScaling_IsFinite(normalizedRate)) normalizedRate = 1.0f;
    normalizedRate = FuzzyScaling_Clamp(normalizedRate, 0.0f, 1.0f);

    factor = 1.0f -
             (normalizedRate * fb->Config.DynamicGain);

    if (normalizedRate > 0.75f)
    {
        factor -= normalizedRate * FUZZY_SCALING_DYNAMIC_KU_GAIN;
    }

    if (!FuzzyScaling_IsFinite(factor)) factor = FUZZY_SCALING_MIN_DYNAMIC_FACTOR;

    factor = FuzzyScaling_Clamp(
        factor,
        FUZZY_SCALING_MIN_DYNAMIC_FACTOR,
        FUZZY_SCALING_MAX_DYNAMIC_FACTOR);

    fb->State.DynamicFactor = factor;
    return factor;
}

float FB_FuzzyScaling_CalculateKe(FB_FuzzyScaling_t *fb)
{
    float ke;

    if (fb == NULL) return 0.05f;

    if (fb->State.ErrorWindow > FUZZY_SCALING_EPSILON &&
        FuzzyScaling_IsFinite(fb->State.ErrorWindow))
    {
        ke = 1.0f / fb->State.ErrorWindow;
    }
    else
    {
        ke = fb->Config.MaxKe;
    }

    if (fb->Config.AdaptiveEnable)
    {
        ke *= fb->State.DynamicFactor;
    }

    if (!FuzzyScaling_IsFinite(ke)) ke = fb->Config.MinKe;

    ke = FuzzyScaling_Clamp(ke, fb->Config.MinKe, fb->Config.MaxKe);
    fb->State.TargetKe = ke;
    return ke;
}

float FB_FuzzyScaling_CalculateKde(FB_FuzzyScaling_t *fb)
{
    float kde;

    if (fb == NULL) return 0.10f;

    if (fb->State.ErrorWindow > FUZZY_SCALING_EPSILON &&
        FuzzyScaling_IsFinite(fb->State.ErrorWindow))
    {
        kde = 1.0f /
              (fb->State.ErrorWindow * FUZZY_SCALING_KDE_RATIO);
    }
    else
    {
        kde = fb->Config.MaxKde;
    }

    if (fb->Config.AdaptiveEnable)
    {
        kde *= fb->State.DynamicFactor;
    }

    if (!FuzzyScaling_IsFinite(kde)) kde = fb->Config.MinKde;

    kde = FuzzyScaling_Clamp(kde, fb->Config.MinKde, fb->Config.MaxKde);
    fb->State.TargetKde = kde;
    return kde;
}

float FB_FuzzyScaling_CalculateKu(FB_FuzzyScaling_t *fb)
{
    float ku;

    if (fb == NULL) return 1.0f;

    ku = 1.0f;

    if (fb->Config.AdaptiveEnable)
    {
        ku *= fb->State.DynamicFactor;
    }

    if (!FuzzyScaling_IsFinite(ku)) ku = fb->Config.MinKu;

    ku = FuzzyScaling_Clamp(ku, fb->Config.MinKu, fb->Config.MaxKu);
    fb->State.TargetKu = ku;
    return ku;
}

float FB_FuzzyScaling_Slew(float current, float target, float rate, float Ts)
{
    float delta;
    float maxDelta;

    if (!FuzzyScaling_IsFinite(current) || !FuzzyScaling_IsFinite(target))
    {
        return FuzzyScaling_IsFinite(target) ? target : current;
    }

    if (Ts <= 0.0f || rate <= 0.0f) return target;

    delta = target - current;
    maxDelta = rate * Ts;

    if (!FuzzyScaling_IsFinite(delta) || !FuzzyScaling_IsFinite(maxDelta))
    {
        return current;
    }

    if (delta > maxDelta) delta = maxDelta;
    else if (delta < -maxDelta) delta = -maxDelta;

    return current + delta;
}

float FB_FuzzyScaling_NormalizeError(float error, float ke)
{
    float normalized;

    if (!FuzzyScaling_IsFinite(error) || !FuzzyScaling_IsFinite(ke)) return 0.0f;
    normalized = error * ke;
    if (!FuzzyScaling_IsFinite(normalized)) return 0.0f;
    return FuzzyScaling_Clamp(normalized, -1.0f, 1.0f);
}

float FB_FuzzyScaling_NormalizeDError(float dError, float kde)
{
    float normalized;

    if (!FuzzyScaling_IsFinite(dError) || !FuzzyScaling_IsFinite(kde)) return 0.0f;
    normalized = dError * kde;
    if (!FuzzyScaling_IsFinite(normalized)) return 0.0f;
    return FuzzyScaling_Clamp(normalized, -1.0f, 1.0f);
}

void FB_FuzzyScaling_Run(FB_FuzzyScaling_t *fb, float sv, float pv)
{
    float pvRate;

    if (fb == NULL) return;

    if (!fb->State.Initialized)
    {
        FB_FuzzyScaling_Init(fb);
    }

    /* Safety: never allow an invalid sensor/SV sample into the state. */
    if (!FuzzyScaling_IsFinite(sv) || !FuzzyScaling_IsFinite(pv))
    {
        fb->State.Error = 0.0f;
        fb->State.dError = 0.0f;
        fb->State.PVRate = 0.0f;
        fb->State.NormalizedError = 0.0f;
        fb->State.NormalizedDError = 0.0f;
        fb->State.DynamicFactor = 1.0f;
        return;
    }

    if (!FuzzyScaling_IsFinite(fb->Config.Ts) ||
        fb->Config.Ts <= FUZZY_SCALING_EPSILON)
    {
        fb->State.Error = sv - pv;
        fb->State.dError = 0.0f;
        fb->State.PVRate = 0.0f;
        fb->State.NormalizedError = FB_FuzzyScaling_NormalizeError(
            fb->State.Error, fb->State.Ke);
        fb->State.NormalizedDError = 0.0f;
        fb->State.PreviousError = fb->State.Error;
        fb->State.PreviousPV = pv;
        return;
    }

    fb->State.PV = pv;
    fb->State.Error = sv - pv;

    if (!FuzzyScaling_IsFinite(fb->State.Error))
    {
        fb->State.Error = 0.0f;
        fb->State.dError = 0.0f;
        fb->State.PVRate = 0.0f;
        fb->State.NormalizedError = 0.0f;
        fb->State.NormalizedDError = 0.0f;
        return;
    }

    fb->State.dError =
        (fb->State.Error - fb->State.PreviousError) /
        fb->Config.Ts;

    pvRate =
        (pv - fb->State.PreviousPV) /
        fb->Config.Ts;

    if (!FuzzyScaling_IsFinite(fb->State.dError)) fb->State.dError = 0.0f;
    if (!FuzzyScaling_IsFinite(pvRate)) pvRate = 0.0f;

    fb->State.PVRate = pvRate;

    if (fb->Config.AutoScalingEnable)
    {
        (void)FB_FuzzyScaling_CalculateErrorWindow(fb, sv, pv);

        if (fb->Config.AdaptiveEnable)
        {
            (void)FB_FuzzyScaling_CalculateDynamicFactor(fb, pvRate);
        }
        else
        {
            fb->State.DynamicFactor = 1.0f;
        }

        fb->State.ErrorWindow = FB_FuzzyScaling_Slew(
            fb->State.ErrorWindow,
            fb->State.TargetErrorWindow,
            FUZZY_SCALING_ERROR_WINDOW_SLEW,
            fb->Config.Ts);

        (void)FB_FuzzyScaling_CalculateKe(fb);
        (void)FB_FuzzyScaling_CalculateKde(fb);
        (void)FB_FuzzyScaling_CalculateKu(fb);
    }
    else
    {
        fb->State.DynamicFactor = 1.0f;
    }

    fb->State.Ke = FB_FuzzyScaling_Slew(
        fb->State.Ke,
        fb->State.TargetKe,
        FUZZY_SCALING_KE_SLEW_RATE,
        fb->Config.Ts);

    fb->State.Kde = FB_FuzzyScaling_Slew(
        fb->State.Kde,
        fb->State.TargetKde,
        FUZZY_SCALING_KDE_SLEW_RATE,
        fb->Config.Ts);

    fb->State.Ku = FB_FuzzyScaling_Slew(
        fb->State.Ku,
        fb->State.TargetKu,
        fb->Config.KuSlewRate,
        fb->Config.Ts);

    fb->State.Ke = FuzzyScaling_Clamp(fb->State.Ke,
                                      fb->Config.MinKe,
                                      fb->Config.MaxKe);
    fb->State.Kde = FuzzyScaling_Clamp(fb->State.Kde,
                                       fb->Config.MinKde,
                                       fb->Config.MaxKde);
    fb->State.Ku = FuzzyScaling_Clamp(fb->State.Ku,
                                      fb->Config.MinKu,
                                      fb->Config.MaxKu);

    fb->State.NormalizedError = FB_FuzzyScaling_NormalizeError(
        fb->State.Error,
        fb->State.Ke);

    fb->State.NormalizedDError = FB_FuzzyScaling_NormalizeDError(
        fb->State.dError,
        fb->State.Kde);

    fb->State.PreviousError = fb->State.Error;
    fb->State.PreviousPV = fb->State.PV;
}

bool FB_FuzzyScaling_SetConfig(
    FB_FuzzyScaling_t *fb,
    const FuzzyScalingConfig_t *config)
{
    if ((fb == NULL) || (config == NULL)) return false;

    if (!FuzzyScaling_IsFinite(config->Ts) || config->Ts <= 0.0f) return false;
    if (!FuzzyScaling_IsFinite(config->MinTemperature) ||
        !FuzzyScaling_IsFinite(config->MaxTemperature) ||
        config->MinTemperature >= config->MaxTemperature) return false;
    if (!FuzzyScaling_IsFinite(config->BaseErrorWindow) ||
        config->BaseErrorWindow <= 0.0f) return false;
    if (!FuzzyScaling_IsFinite(config->MinErrorWindow) ||
        config->MinErrorWindow <= 0.0f) return false;
    if (!FuzzyScaling_IsFinite(config->MaxErrorWindow) ||
        config->MaxErrorWindow < config->MinErrorWindow) return false;
    if (config->BaseErrorWindow < config->MinErrorWindow ||
        config->BaseErrorWindow > config->MaxErrorWindow) return false;

    if (!FuzzyScaling_IsFinite(config->MinKe) ||
        !FuzzyScaling_IsFinite(config->MaxKe) ||
        config->MinKe <= 0.0f || config->MaxKe < config->MinKe) return false;
    if (!FuzzyScaling_IsFinite(config->MinKde) ||
        !FuzzyScaling_IsFinite(config->MaxKde) ||
        config->MinKde <= 0.0f || config->MaxKde < config->MinKde) return false;
    if (!FuzzyScaling_IsFinite(config->MinKu) ||
        !FuzzyScaling_IsFinite(config->MaxKu) ||
        config->MinKu <= 0.0f || config->MaxKu < config->MinKu) return false;

    if (!FuzzyScaling_IsFinite(config->DynamicGain) || config->DynamicGain < 0.0f) return false;
    if (!FuzzyScaling_IsFinite(config->MaxPVRate) || config->MaxPVRate <= 0.0f) return false;
    if (!FuzzyScaling_IsFinite(config->KuSlewRate) || config->KuSlewRate <= 0.0f) return false;

    fb->Config = *config;

    fb->State.ErrorWindow = FuzzyScaling_Clamp(
        fb->State.ErrorWindow,
        fb->Config.MinErrorWindow,
        fb->Config.MaxErrorWindow);

    fb->State.TargetErrorWindow = FuzzyScaling_Clamp(
        fb->State.TargetErrorWindow,
        fb->Config.MinErrorWindow,
        fb->Config.MaxErrorWindow);

    fb->State.Ke = FuzzyScaling_Clamp(fb->State.Ke,
                                      fb->Config.MinKe,
                                      fb->Config.MaxKe);
    fb->State.Kde = FuzzyScaling_Clamp(fb->State.Kde,
                                       fb->Config.MinKde,
                                       fb->Config.MaxKde);
    fb->State.Ku = FuzzyScaling_Clamp(fb->State.Ku,
                                      fb->Config.MinKu,
                                      fb->Config.MaxKu);

    return true;
}

bool FB_FuzzyScaling_GetConfig(
    const FB_FuzzyScaling_t *fb,
    FuzzyScalingConfig_t *config)
{
    if ((fb == NULL) || (config == NULL)) return false;
    *config = fb->Config;
    return true;
}

bool FB_FuzzyScaling_SetKe(FB_FuzzyScaling_t *fb, float ke)
{
    if (fb == NULL || !FuzzyScaling_IsFinite(ke)) return false;
    if ((ke < fb->Config.MinKe) || (ke > fb->Config.MaxKe)) return false;
    fb->State.TargetKe = ke;
    return true;
}

bool FB_FuzzyScaling_SetKde(FB_FuzzyScaling_t *fb, float kde)
{
    if (fb == NULL || !FuzzyScaling_IsFinite(kde)) return false;
    if ((kde < fb->Config.MinKde) || (kde > fb->Config.MaxKde)) return false;
    fb->State.TargetKde = kde;
    return true;
}

bool FB_FuzzyScaling_SetKu(FB_FuzzyScaling_t *fb, float ku)
{
    if (fb == NULL || !FuzzyScaling_IsFinite(ku)) return false;
    if ((ku < fb->Config.MinKu) || (ku > fb->Config.MaxKu)) return false;
    fb->State.TargetKu = ku;
    return true;
}

bool FB_FuzzyScaling_SetErrorWindow(FB_FuzzyScaling_t *fb, float window)
{
    if (fb == NULL || !FuzzyScaling_IsFinite(window)) return false;
    if ((window < fb->Config.MinErrorWindow) ||
        (window > fb->Config.MaxErrorWindow)) return false;

    fb->State.TargetErrorWindow = window;
    fb->State.ErrorWindow = window;
    return true;
}

void FB_FuzzyScaling_EnableAuto(FB_FuzzyScaling_t *fb)
{
    if (fb != NULL) fb->Config.AutoScalingEnable = true;
}

void FB_FuzzyScaling_DisableAuto(FB_FuzzyScaling_t *fb)
{
    if (fb != NULL) fb->Config.AutoScalingEnable = false;
}

void FB_FuzzyScaling_EnableAdaptive(FB_FuzzyScaling_t *fb)
{
    if (fb != NULL) fb->Config.AdaptiveEnable = true;
}

void FB_FuzzyScaling_DisableAdaptive(FB_FuzzyScaling_t *fb)
{
    if (fb != NULL)
    {
        fb->Config.AdaptiveEnable = false;
        fb->State.DynamicFactor = 1.0f;
    }
}
