/******************************************************************************
 * File    : FB_FuzzyParameterGuard.c
 * Brief   : Bounds, step limits and rollback support for self-tuned parameters.
 ******************************************************************************/

#include "FB_FuzzyParameterGuard.h"
#include "FB_FuzzyScaling.h"

static float clampf_local(float value, float minimum, float maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

static float limit_relative_step(float current, float requested, float maxRelativeStep)
{
    float base;
    float maxDelta;
    float delta;

    base = (current >= 0.0f) ? current : -current;
    if (base < 0.000001f)
    {
        base = 1.0f;
    }

    maxDelta = base * maxRelativeStep;
    delta = requested - current;

    if (delta > maxDelta)
    {
        delta = maxDelta;
    }
    else if (delta < -maxDelta)
    {
        delta = -maxDelta;
    }

    return current + delta;
}

static bool config_is_valid(const FuzzyParameterGuardConfig_t *config)
{
    if (config == (const FuzzyParameterGuardConfig_t *)0)
    {
        return false;
    }

    if ((config->MinKe <= 0.0f) || (config->MinKe >= config->MaxKe))
    {
        return false;
    }
    if ((config->MinKde <= 0.0f) || (config->MinKde >= config->MaxKde))
    {
        return false;
    }
    if ((config->MinKu <= 0.0f) || (config->MinKu >= config->MaxKu))
    {
        return false;
    }
    if ((config->MinErrorWindow <= 0.0f) ||
        (config->MinErrorWindow >= config->MaxErrorWindow))
    {
        return false;
    }
    if ((config->MinFullPowerErrorRatio < 0.0f) ||
        (config->MinFullPowerErrorRatio >= config->MaxFullPowerErrorRatio))
    {
        return false;
    }
    if ((config->MinPrecisionErrorRatio < 0.0f) ||
        (config->MinPrecisionErrorRatio >= config->MaxPrecisionErrorRatio))
    {
        return false;
    }
    if ((config->MaxRelativeStep <= 0.0f) ||
        (config->MaxRelativeStep > 0.25f))
    {
        return false;
    }

    return true;
}

void FB_FuzzyParameterGuard_Init(FB_FuzzyParameterGuard_t *fb)
{
    if (fb == (FB_FuzzyParameterGuard_t *)0)
    {
        return;
    }

    fb->Config.MinKe = FUZZY_SCALING_MIN_KE;
    fb->Config.MaxKe = FUZZY_SCALING_MAX_KE;
    fb->Config.MinKde = FUZZY_SCALING_MIN_KDE;
    fb->Config.MaxKde = FUZZY_SCALING_MAX_KDE;
    fb->Config.MinKu = FUZZY_SCALING_MIN_KU;
    fb->Config.MaxKu = FUZZY_SCALING_MAX_KU;
    fb->Config.MinErrorWindow = FUZZY_SCALING_MIN_ERROR_WINDOW;
    fb->Config.MaxErrorWindow = FUZZY_SCALING_MAX_ERROR_WINDOW;
    fb->Config.MinFullPowerErrorRatio = 0.02f;
    fb->Config.MaxFullPowerErrorRatio = 0.20f;
    fb->Config.MinPrecisionErrorRatio = 0.005f;
    fb->Config.MaxPrecisionErrorRatio = 0.15f;
    fb->Config.MaxRelativeStep = 0.05f;

    fb->Accepted.Ke = 0.0f;
    fb->Accepted.Kde = 0.0f;
    fb->Accepted.Ku = 0.0f;
    fb->Accepted.ErrorWindow = 0.0f;
    fb->Accepted.FullPowerErrorRatio = 0.0f;
    fb->Accepted.PrecisionErrorRatio = 0.0f;
    fb->Candidate = fb->Accepted;
    fb->HasAccepted = false;
    fb->HasCandidate = false;
}

bool FB_FuzzyParameterGuard_SetConfig(
    FB_FuzzyParameterGuard_t *fb,
    const FuzzyParameterGuardConfig_t *config)
{
    if ((fb == (FB_FuzzyParameterGuard_t *)0) || !config_is_valid(config))
    {
        return false;
    }

    fb->Config = *config;
    return true;
}

bool FB_FuzzyParameterGuard_MakeCandidate(
    FB_FuzzyParameterGuard_t *fb,
    const FuzzyTunableParameters_t *current,
    const FuzzyTunableParameters_t *requested,
    FuzzyTunableParameters_t *candidate)
{
    FuzzyTunableParameters_t next;

    if ((fb == (FB_FuzzyParameterGuard_t *)0) ||
        (current == (const FuzzyTunableParameters_t *)0) ||
        (requested == (const FuzzyTunableParameters_t *)0) ||
        (candidate == (FuzzyTunableParameters_t *)0))
    {
        return false;
    }

    next.Ke = limit_relative_step(current->Ke, requested->Ke, fb->Config.MaxRelativeStep);
    next.Kde = limit_relative_step(current->Kde, requested->Kde, fb->Config.MaxRelativeStep);
    next.Ku = limit_relative_step(current->Ku, requested->Ku, fb->Config.MaxRelativeStep);
    next.ErrorWindow = limit_relative_step(current->ErrorWindow, requested->ErrorWindow, fb->Config.MaxRelativeStep);
    next.FullPowerErrorRatio = limit_relative_step(current->FullPowerErrorRatio,
                                                   requested->FullPowerErrorRatio,
                                                   fb->Config.MaxRelativeStep);
    next.PrecisionErrorRatio = limit_relative_step(current->PrecisionErrorRatio,
                                                   requested->PrecisionErrorRatio,
                                                   fb->Config.MaxRelativeStep);

    next.Ke = clampf_local(next.Ke, fb->Config.MinKe, fb->Config.MaxKe);
    next.Kde = clampf_local(next.Kde, fb->Config.MinKde, fb->Config.MaxKde);
    next.Ku = clampf_local(next.Ku, fb->Config.MinKu, fb->Config.MaxKu);
    next.ErrorWindow = clampf_local(next.ErrorWindow,
                                    fb->Config.MinErrorWindow,
                                    fb->Config.MaxErrorWindow);
    next.FullPowerErrorRatio = clampf_local(next.FullPowerErrorRatio,
                                            fb->Config.MinFullPowerErrorRatio,
                                            fb->Config.MaxFullPowerErrorRatio);
    next.PrecisionErrorRatio = clampf_local(next.PrecisionErrorRatio,
                                            fb->Config.MinPrecisionErrorRatio,
                                            fb->Config.MaxPrecisionErrorRatio);

    if (next.PrecisionErrorRatio >= next.FullPowerErrorRatio)
    {
        next.PrecisionErrorRatio = next.FullPowerErrorRatio * 0.75f;
        next.PrecisionErrorRatio = clampf_local(next.PrecisionErrorRatio,
                                                fb->Config.MinPrecisionErrorRatio,
                                                fb->Config.MaxPrecisionErrorRatio);
    }

    fb->Candidate = next;
    fb->HasCandidate = true;
    *candidate = next;
    return true;
}

void FB_FuzzyParameterGuard_Accept(
    FB_FuzzyParameterGuard_t *fb,
    const FuzzyTunableParameters_t *parameters)
{
    if ((fb == (FB_FuzzyParameterGuard_t *)0) ||
        (parameters == (const FuzzyTunableParameters_t *)0))
    {
        return;
    }

    fb->Accepted = *parameters;
    fb->Candidate = *parameters;
    fb->HasAccepted = true;
    fb->HasCandidate = false;
}

bool FB_FuzzyParameterGuard_Rollback(
    const FB_FuzzyParameterGuard_t *fb,
    FuzzyTunableParameters_t *parameters)
{
    if ((fb == (const FB_FuzzyParameterGuard_t *)0) ||
        (parameters == (FuzzyTunableParameters_t *)0) ||
        !fb->HasAccepted)
    {
        return false;
    }

    *parameters = fb->Accepted;
    return true;
}
