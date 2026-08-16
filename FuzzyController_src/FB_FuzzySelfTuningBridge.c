/******************************************************************************
 * File    : FB_FuzzySelfTuningBridge.c
 * Brief   : Non-intrusive fuzzy self-tuning bridge implementation.
 ******************************************************************************/
#include "FB_FuzzySelfTuningBridge.h"

#define BRIDGE_EPSILON (0.000001f)

static float absf_local(float value)
{
    return (value >= 0.0f) ? value : -value;
}

static float clampf_local(float value, float minimum, float maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static float safe_ratio(float desired, float current)
{
    if (absf_local(current) <= BRIDGE_EPSILON)
    {
        return 1.0f;
    }

    return desired / current;
}

static float get_region_confidence(
    const FB_FuzzySelfTuningBridge_t *fb,
    int16_t regionIndex)
{
    const FuzzyTemperatureRegion_t *region;

    if ((fb == (const FB_FuzzySelfTuningBridge_t *)0) ||
        (regionIndex < 0))
    {
        return 0.0f;
    }

    region = FB_FuzzyTemperatureProfile_GetRegion(
        &fb->TemperatureProfile,
        (uint8_t)regionIndex);

    return (region != (const FuzzyTemperatureRegion_t *)0) ?
           region->Confidence : 0.0f;
}

static bool restore_apply_backup(
    FB_FuzzySelfTuningBridge_t *fb,
    FB_FuzzyController_t *controller)
{
    bool ok;

    if ((fb == (FB_FuzzySelfTuningBridge_t *)0) ||
        (controller == (FB_FuzzyController_t *)0) ||
        !fb->HasApplyBackup)
    {
        return false;
    }

    ok = FB_FuzzyScaling_SetConfig(
        &controller->scaling,
        &fb->AppliedScalingConfigBackup);

    if (!FB_FuzzyController_SetApproachConfig(
            controller,
            controller->config.EnablePercentApproach,
            fb->AppliedFullPowerErrorRatioBackup,
            fb->AppliedPrecisionErrorRatioBackup,
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
    fb->Status.RollbackRecommended = false;
    fb->Status.ApplyBlockedByScalingMode = false;
    fb->Status.EpisodeCount = 0U;
    fb->Status.ActiveRegion = FUZZY_SELF_TUNING_REGION_INVALID;
    fb->Status.CandidateRegion = FUZZY_SELF_TUNING_REGION_INVALID;
    fb->Status.ActiveRegionConfidence = 0.0f;
    fb->Status.CandidateRegionConfidence = 0.0f;

    fb->Status.Current.Ke = 0.0f;
    fb->Status.Current.Kde = 0.0f;
    fb->Status.Current.Ku = 0.0f;
    fb->Status.Current.ErrorWindow = 0.0f;
    fb->Status.Current.FullPowerErrorRatio = 0.0f;
    fb->Status.Current.PrecisionErrorRatio = 0.0f;
    fb->Status.Candidate = fb->Status.Current;

    FB_FuzzyPerformanceMonitor_Init(&fb->Monitor);
    FB_FuzzySelfTuner_Init(&fb->Tuner);
    FB_FuzzyTemperatureProfile_Init(&fb->TemperatureProfile);

    fb->AppliedFullPowerErrorRatioBackup = 0.0f;
    fb->AppliedPrecisionErrorRatioBackup = 0.0f;
    fb->HasApplyBackup = false;

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
    int16_t regionIndex;

    if ((fb == (FB_FuzzySelfTuningBridge_t *)0) ||
        (controller == (const FB_FuzzyController_t *)0))
    {
        return false;
    }

    if (!fb->Initialized)
    {
        FB_FuzzySelfTuningBridge_Init(fb);
    }

    if (!fb->Config.Enable ||
        fb->Status.RollbackRecommended ||
        (fb->Status.CandidateAvailable && !fb->Status.CandidateApplied))
    {
        return false;
    }

    regionIndex = FB_FuzzyTemperatureProfile_FindRegion(
        &fb->TemperatureProfile,
        sv);

    if (regionIndex < 0)
    {
        fb->Status.ActiveRegion = FUZZY_SELF_TUNING_REGION_INVALID;
        fb->Status.ActiveRegionConfidence = 0.0f;
        return false;
    }

    if (!FB_FuzzySelfTuningBridge_GetControllerParameters(controller, &fb->Status.Current))
    {
        return false;
    }

    fb->Monitor.Config.Ts = controller->config.Ts;
    fb->Monitor.Config.SvChangeThreshold_c = fb->Config.SVChangeThreshold_c;

    FB_FuzzyPerformanceMonitor_StartEpisode(&fb->Monitor, sv, pv, pwm);
    fb->Status.EpisodeActive = true;
    fb->Status.ActiveRegion = regionIndex;
    fb->Status.ActiveRegionConfidence = get_region_confidence(fb, regionIndex);
    fb->Status.ApplyBlockedByScalingMode = false;
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
    FuzzyTunableParameters_t nextParameters;
    bool changed;
    bool wasCandidatePending;
    int16_t completedRegion;

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

    if (!fb->Config.Enable || fb->Status.RollbackRecommended)
    {
        return;
    }

    if (fb->Status.CandidateAvailable && !fb->Status.CandidateApplied)
    {
        fb->PreviousSV = sv;
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
    completedRegion = fb->Status.ActiveRegion;

    if (completedRegion >= 0)
    {
        (void)FB_FuzzyTemperatureProfile_RecordObservation(
            &fb->TemperatureProfile,
            (uint8_t)completedRegion);
        fb->Status.ActiveRegionConfidence = get_region_confidence(
            fb,
            completedRegion);
    }

    if (!FB_FuzzySelfTuningBridge_GetControllerParameters(controller, &current))
    {
        return;
    }

    fb->Status.Current = current;
    nextParameters = current;
    wasCandidatePending = fb->Tuner.Status.CandidatePending;

    changed = FB_FuzzySelfTuner_EvaluateEpisode(
        &fb->Tuner,
        FB_FuzzyPerformanceMonitor_GetMetrics(&fb->Monitor),
        &current,
        &nextParameters);

    if (!wasCandidatePending)
    {
        if (changed)
        {
            fb->Status.Candidate = nextParameters;
            fb->Status.CandidateAvailable = true;
            fb->Status.CandidateApplied = false;
            fb->Status.RollbackRecommended = false;
            fb->Status.CandidateRegion = completedRegion;
            fb->Status.CandidateRegionConfidence = get_region_confidence(
                fb,
                completedRegion);
        }
        return;
    }

    if (!fb->Status.CandidateApplied)
    {
        return;
    }

    if (fb->Tuner.Status.State == FUZZY_TUNER_ACCEPT)
    {
        if (fb->Status.CandidateRegion >= 0)
        {
            (void)FB_FuzzyTemperatureProfile_RecordAccepted(
                &fb->TemperatureProfile,
                (uint8_t)fb->Status.CandidateRegion,
                &fb->Status.Candidate);
            fb->Status.CandidateRegionConfidence = get_region_confidence(
                fb,
                fb->Status.CandidateRegion);
        }

        fb->Status.CandidateAvailable = false;
        fb->Status.CandidateApplied = false;
        fb->Status.RollbackRecommended = false;
        fb->HasApplyBackup = false;
        return;
    }

    if (fb->Tuner.Status.State == FUZZY_TUNER_ROLLBACK)
    {
        fb->Status.CandidateAvailable = false;
        fb->Status.RollbackRecommended = true;
    }
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

const FuzzyTemperatureRegion_t *FB_FuzzySelfTuningBridge_GetActiveRegion(
    const FB_FuzzySelfTuningBridge_t *fb)
{
    if ((fb == (const FB_FuzzySelfTuningBridge_t *)0) ||
        (fb->Status.ActiveRegion < 0))
    {
        return (const FuzzyTemperatureRegion_t *)0;
    }

    return FB_FuzzyTemperatureProfile_GetRegion(
        &fb->TemperatureProfile,
        (uint8_t)fb->Status.ActiveRegion);
}

const FuzzyTemperatureRegion_t *FB_FuzzySelfTuningBridge_GetCandidateRegion(
    const FB_FuzzySelfTuningBridge_t *fb)
{
    if ((fb == (const FB_FuzzySelfTuningBridge_t *)0) ||
        (fb->Status.CandidateRegion < 0))
    {
        return (const FuzzyTemperatureRegion_t *)0;
    }

    return FB_FuzzyTemperatureProfile_GetRegion(
        &fb->TemperatureProfile,
        (uint8_t)fb->Status.CandidateRegion);
}

bool FB_FuzzySelfTuningBridge_RejectCandidate(FB_FuzzySelfTuningBridge_t *fb)
{
    if ((fb == (FB_FuzzySelfTuningBridge_t *)0) ||
        !fb->Status.CandidateAvailable ||
        fb->Status.CandidateApplied)
    {
        return false;
    }

    FB_FuzzySelfTuner_CancelCandidate(&fb->Tuner);
    fb->Status.CandidateAvailable = false;
    fb->Status.RollbackRecommended = false;
    fb->Status.CandidateRegion = FUZZY_SELF_TUNING_REGION_INVALID;
    fb->Status.CandidateRegionConfidence = 0.0f;
    return true;
}

bool FB_FuzzySelfTuningBridge_ApplyCandidate(
    FB_FuzzySelfTuningBridge_t *fb,
    FB_FuzzyController_t *controller)
{
    float errorWindowRatio;
    float keRatio;
    float kdeRatio;
    float kuRatio;
    float keTrim;
    float kdeTrim;
    float kuTrim;
    float errorWindowTrim;

    if ((fb == (FB_FuzzySelfTuningBridge_t *)0) ||
        (controller == (FB_FuzzyController_t *)0) ||
        !fb->Status.CandidateAvailable ||
        fb->Status.CandidateApplied ||
        fb->Status.RollbackRecommended ||
        (fb->Status.CandidateRegion < 0))
    {
        return false;
    }

    fb->Status.ApplyBlockedByScalingMode = false;

    if (fb->Config.ShadowMode)
    {
        return false;
    }

    if (!controller->scaling.Config.AutoScalingEnable)
    {
        fb->Status.ApplyBlockedByScalingMode = true;
        return false;
    }

    fb->AppliedScalingConfigBackup = controller->scaling.Config;
    fb->AppliedFullPowerErrorRatioBackup = controller->config.FullPowerErrorRatio;
    fb->AppliedPrecisionErrorRatioBackup = controller->config.PrecisionErrorRatio;
    fb->HasApplyBackup = true;

    errorWindowRatio = safe_ratio(
        fb->Status.Candidate.ErrorWindow,
        fb->Status.Current.ErrorWindow);

    keRatio = safe_ratio(fb->Status.Candidate.Ke, fb->Status.Current.Ke) *
              errorWindowRatio;
    kdeRatio = safe_ratio(fb->Status.Candidate.Kde, fb->Status.Current.Kde) *
               errorWindowRatio;
    kuRatio = safe_ratio(fb->Status.Candidate.Ku, fb->Status.Current.Ku);

    keTrim = clampf_local(
        controller->scaling.Config.SelfTuneKeTrim * keRatio,
        FUZZY_SCALING_SELF_TUNE_TRIM_MIN,
        FUZZY_SCALING_SELF_TUNE_TRIM_MAX);
    kdeTrim = clampf_local(
        controller->scaling.Config.SelfTuneKdeTrim * kdeRatio,
        FUZZY_SCALING_SELF_TUNE_TRIM_MIN,
        FUZZY_SCALING_SELF_TUNE_TRIM_MAX);
    kuTrim = clampf_local(
        controller->scaling.Config.SelfTuneKuTrim * kuRatio,
        FUZZY_SCALING_SELF_TUNE_TRIM_MIN,
        FUZZY_SCALING_SELF_TUNE_TRIM_MAX);
    errorWindowTrim = clampf_local(
        controller->scaling.Config.SelfTuneErrorWindowTrim * errorWindowRatio,
        FUZZY_SCALING_SELF_TUNE_TRIM_MIN,
        FUZZY_SCALING_SELF_TUNE_TRIM_MAX);

    if (!FB_FuzzyScaling_SetSelfTuneTrim(
            &controller->scaling,
            keTrim,
            kdeTrim,
            kuTrim,
            errorWindowTrim))
    {
        fb->HasApplyBackup = false;
        return false;
    }

    if (!FB_FuzzyController_SetApproachConfig(
            controller,
            controller->config.EnablePercentApproach,
            fb->Status.Candidate.FullPowerErrorRatio,
            fb->Status.Candidate.PrecisionErrorRatio,
            controller->config.FullPowerErrorMin_c,
            controller->config.PrecisionErrorMin_c,
            controller->config.ApproachDownSlewRate_pwm_per_s))
    {
        (void)FB_FuzzyScaling_SetConfig(
            &controller->scaling,
            &fb->AppliedScalingConfigBackup);
        fb->HasApplyBackup = false;
        return false;
    }

    fb->Status.CandidateApplied = true;
    fb->Status.RollbackRecommended = false;
    return true;
}

bool FB_FuzzySelfTuningBridge_Rollback(
    FB_FuzzySelfTuningBridge_t *fb,
    FB_FuzzyController_t *controller)
{
    FuzzyTunableParameters_t tunerRollback;
    int16_t rollbackRegion;

    if ((fb == (FB_FuzzySelfTuningBridge_t *)0) ||
        (controller == (FB_FuzzyController_t *)0) ||
        !fb->Status.CandidateApplied ||
        !fb->HasApplyBackup)
    {
        return false;
    }

    rollbackRegion = fb->Status.CandidateRegion;

    if (!restore_apply_backup(fb, controller))
    {
        return false;
    }

    if (fb->Tuner.Status.CandidatePending)
    {
        (void)FB_FuzzySelfTuner_Rollback(&fb->Tuner, &tunerRollback);
    }

    if (rollbackRegion >= 0)
    {
        (void)FB_FuzzyTemperatureProfile_RecordRollback(
            &fb->TemperatureProfile,
            (uint8_t)rollbackRegion);
        fb->Status.CandidateRegionConfidence = get_region_confidence(
            fb,
            rollbackRegion);
    }

    fb->Status.CandidateAvailable = false;
    fb->Status.CandidateApplied = false;
    fb->Status.RollbackRecommended = false;
    fb->Status.ApplyBlockedByScalingMode = false;
    fb->HasApplyBackup = false;
    return true;
}
