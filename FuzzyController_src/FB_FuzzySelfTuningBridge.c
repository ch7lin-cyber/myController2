/******************************************************************************
 * File    : FB_FuzzySelfTuningBridge.c
 * Brief   : Non-intrusive fuzzy self-tuning bridge implementation.
 ******************************************************************************/
#include "FB_FuzzySelfTuningBridge.h"

static float absf_local(float value)
{
    return (value >= 0.0f) ? value : -value;
}

static bool apply_parameters(
    FB_FuzzyController_t *controller,
    const FuzzyTunableParameters_t *parameters)
{
    bool ok;

    if ((controller == (FB_FuzzyController_t *)0) ||
        (parameters == (const FuzzyTunableParameters_t *)0))
    {
        return false;
    }

    ok = FB_FuzzyScaling_SetKe(&controller->scaling, parameters->Ke);
    ok = FB_FuzzyScaling_SetKde(&controller->scaling, parameters->Kde) && ok;
    ok = FB_FuzzyScaling_SetKu(&controller->scaling, parameters->Ku) && ok;
    ok = FB_FuzzyScaling_SetErrorWindow(&controller->scaling, parameters->ErrorWindow) && ok;

    if (!FB_FuzzyController_SetApproachConfig(
            controller,
            controller->config.EnablePercentApproach,
            parameters->FullPowerErrorRatio,
            parameters->PrecisionErrorRatio,
            controller->config.FullPowerErrorMin_c,
            controller->config.PrecisionErrorMin_c,
            controller->config.ApproachDownSlewRate_pwm_per_s))
    {
        ok = false;
    }

    return ok;
}

void FB_FuzzySelfTuningBridge_Init(FB_FuzzySelfTuningBridge_t *fb)
{
    if (fb == (FB_FuzzySelfTuningBridge_t *)0)
    {
        return;
    }

    fb->Config.Enable = true;
    fb->Config.ShadowMode = true;
    fb->Config.AutoStartOnSVChange = true;
    fb->Config.SVChangeThreshold_c = 1.0f;

    fb->Status.EpisodeActive = false;
    fb->Status.CandidateAvailable = false;
    fb->Status.CandidateApplied = false;
    fb->Status.EpisodeCount = 0U;

    fb->Status.Current.Ke = 0.0f;
    fb->Status.Current.Kde = 0.0f;
    fb->Status.Current.Ku = 0.0f;
    fb->Status.Current.ErrorWindow = 0.0f;
    fb->Status.Current.FullPowerErrorRatio = 0.0f;
    fb->Status.Current.PrecisionErrorRatio = 0.0f;
    fb->Status.Candidate = fb->Status.Current;

    FB_FuzzyPerformanceMonitor_Init(&fb->Monitor);
    FB_FuzzySelfTuner_Init(&fb->Tuner);

    fb->PreviousSV = 0.0f;
    fb->Initialized = true;
}

void FB_FuzzySelfTuningBridge_Reset(FB_FuzzySelfTuningBridge_t *fb)
{
    FuzzySelfTuningBridgeConfig_t config;

    if (fb == (FB_FuzzySelfTuningBridge_t *)0)
    {
        return;
    }

    config = fb->Config;
    FB_FuzzySelfTuningBridge_Init(fb);
    fb->Config = config;
}

void FB_FuzzySelfTuningBridge_SetShadowMode(
    FB_FuzzySelfTuningBridge_t *fb,
    bool shadowMode)
{
    if (fb == (FB_FuzzySelfTuningBridge_t *)0)
    {
        return;
    }

    fb->Config.ShadowMode = shadowMode;
}

bool FB_FuzzySelfTuningBridge_GetControllerParameters(
    const FB_FuzzyController_t *controller,
    FuzzyTunableParameters_t *parameters)
{
    if ((controller == (const FB_FuzzyController_t *)0) ||
        (parameters == (FuzzyTunableParameters_t *)0))
    {
        return false;
    }

    parameters->Ke = controller->scaling.State.Ke;
    parameters->Kde = controller->scaling.State.Kde;
    parameters->Ku = controller->scaling.State.Ku;
    parameters->ErrorWindow = controller->scaling.State.ErrorWindow;
    parameters->FullPowerErrorRatio = controller->config.FullPowerErrorRatio;
    parameters->PrecisionErrorRatio = controller->config.PrecisionErrorRatio;
    return true;
}

bool FB_FuzzySelfTuningBridge_StartEpisode(
    FB_FuzzySelfTuningBridge_t *fb,
    const FB_FuzzyController_t *controller,
    float sv,
    float pv,
    float pwm)
{
    if ((fb == (FB_FuzzySelfTuningBridge_t *)0) ||
        (controller == (const FB_FuzzyController_t *)0))
    {
        return false;
    }

    if (!fb->Initialized)
    {
        FB_FuzzySelfTuningBridge_Init(fb);
    }

    if (!fb->Config.Enable)
    {
        return false;
    }

    if (!FB_FuzzySelfTuningBridge_GetControllerParameters(controller, &fb->Status.Current))
    {
        return false;
    }

    FB_FuzzyPerformanceMonitor_StartEpisode(&fb->Monitor, sv, pv, pwm);
    fb->Status.EpisodeActive = true;
    fb->Status.CandidateAvailable = false;
    fb->Status.CandidateApplied = false;
    fb->PreviousSV = sv;
    return true;
}

void FB_FuzzySelfTuningBridge_Run(
    FB_FuzzySelfTuningBridge_t *fb,
    const FB_FuzzyController_t *controller,
    float sv,
    float pv,
    float pwm)
{
    FuzzyTunableParameters_t current;
    FuzzyTunableParameters_t candidate;
    bool changed;

    if ((fb == (FB_FuzzySelfTuningBridge_t *)0) ||
        (controller == (const FB_FuzzyController_t *)0))
    {
        return;
    }

    if (!fb->Initialized)
    {
        FB_FuzzySelfTuningBridge_Init(fb);
        fb->PreviousSV = sv;
    }

    if (!fb->Config.Enable)
    {
        return;
    }

    if ((!fb->Status.EpisodeActive) &&
        fb->Config.AutoStartOnSVChange &&
        (absf_local(sv - fb->PreviousSV) >= fb->Config.SVChangeThreshold_c))
    {
        (void)FB_FuzzySelfTuningBridge_StartEpisode(fb, controller, sv, pv, pwm);
    }

    fb->PreviousSV = sv;

    if (!fb->Status.EpisodeActive)
    {
        return;
    }

    FB_FuzzyPerformanceMonitor_Run(&fb->Monitor, sv, pv, pwm);

    if (!FB_FuzzyPerformanceMonitor_IsComplete(&fb->Monitor))
    {
        return;
    }

    fb->Status.EpisodeActive = false;
    fb->Status.EpisodeCount++;

    if (!FB_FuzzySelfTuningBridge_GetControllerParameters(controller, &current))
    {
        return;
    }

    fb->Status.Current = current;
    candidate = current;

    changed = FB_FuzzySelfTuner_EvaluateEpisode(
        &fb->Tuner,
        FB_FuzzyPerformanceMonitor_GetMetrics(&fb->Monitor),
        &current,
        &candidate);

    if (changed)
    {
        fb->Status.Candidate = candidate;
        fb->Status.CandidateAvailable = true;
        fb->Status.CandidateApplied = false;
    }

    /* Intentionally no automatic parameter write here. */
}

bool FB_FuzzySelfTuningBridge_GetCandidate(
    const FB_FuzzySelfTuningBridge_t *fb,
    FuzzyTunableParameters_t *candidate)
{
    if ((fb == (const FB_FuzzySelfTuningBridge_t *)0) ||
        (candidate == (FuzzyTunableParameters_t *)0) ||
        !fb->Status.CandidateAvailable)
    {
        return false;
    }

    *candidate = fb->Status.Candidate;
    return true;
}

const FuzzyPerformanceMetrics_t *FB_FuzzySelfTuningBridge_GetMetrics(
    const FB_FuzzySelfTuningBridge_t *fb)
{
    if (fb == (const FB_FuzzySelfTuningBridge_t *)0)
    {
        return (const FuzzyPerformanceMetrics_t *)0;
    }

    return FB_FuzzyPerformanceMonitor_GetMetrics(&fb->Monitor);
}

const FuzzySelfTunerStatus_t *FB_FuzzySelfTuningBridge_GetTunerStatus(
    const FB_FuzzySelfTuningBridge_t *fb)
{
    if (fb == (const FB_FuzzySelfTuningBridge_t *)0)
    {
        return (const FuzzySelfTunerStatus_t *)0;
    }

    return &fb->Tuner.Status;
}

bool FB_FuzzySelfTuningBridge_ApplyCandidate(
    FB_FuzzySelfTuningBridge_t *fb,
    FB_FuzzyController_t *controller)
{
    if ((fb == (FB_FuzzySelfTuningBridge_t *)0) ||
        (controller == (FB_FuzzyController_t *)0) ||
        !fb->Status.CandidateAvailable)
    {
        return false;
    }

    /* Shadow mode is a hard safety gate. */
    if (fb->Config.ShadowMode)
    {
        return false;
    }

    if (!apply_parameters(controller, &fb->Status.Candidate))
    {
        return false;
    }

    fb->Status.CandidateApplied = true;
    return true;
}

bool FB_FuzzySelfTuningBridge_Rollback(
    FB_FuzzySelfTuningBridge_t *fb,
    FB_FuzzyController_t *controller)
{
    FuzzyTunableParameters_t rollback;

    if ((fb == (FB_FuzzySelfTuningBridge_t *)0) ||
        (controller == (FB_FuzzyController_t *)0))
    {
        return false;
    }

    if (!FB_FuzzySelfTuner_Rollback(&fb->Tuner, &rollback))
    {
        return false;
    }

    if (!apply_parameters(controller, &rollback))
    {
        return false;
    }

    fb->Status.Current = rollback;
    fb->Status.CandidateApplied = false;
    return true;
}
