/******************************************************************************
 * File    : FB_FuzzyPerformanceMonitor.c
 * Brief   : Episode-based performance monitor for fuzzy self tuning.
 ******************************************************************************/

#include "FB_FuzzyPerformanceMonitor.h"

static float absf_local(float value)
{
    return (value >= 0.0f) ? value : -value;
}

static void clear_metrics(FuzzyPerformanceMetrics_t *metrics)
{
    if (metrics == (FuzzyPerformanceMetrics_t *)0)
    {
        return;
    }

    metrics->StartSV = 0.0f;
    metrics->TargetSV = 0.0f;
    metrics->StartPV = 0.0f;
    metrics->PeakPV = 0.0f;
    metrics->ValleyPV = 0.0f;
    metrics->Overshoot_c = 0.0f;
    metrics->Undershoot_c = 0.0f;
    metrics->RiseTime_s = 0.0f;
    metrics->SettlingTime_s = 0.0f;
    metrics->SteadyStateError_c = 0.0f;
    metrics->IAE = 0.0f;
    metrics->ISE = 0.0f;
    metrics->PWMActivity = 0.0f;
    metrics->MaxPVRate_c_per_s = 0.0f;
    metrics->SampleCount = 0U;
    metrics->ErrorZeroCrossCount = 0U;
    metrics->RiseReached = false;
    metrics->Settled = false;
    metrics->Complete = false;
}

void FB_FuzzyPerformanceMonitor_Init(FB_FuzzyPerformanceMonitor_t *fb)
{
    if (fb == (FB_FuzzyPerformanceMonitor_t *)0)
    {
        return;
    }

    fb->Config.Ts = 0.020f;
    fb->Config.SvChangeThreshold_c = 1.0f;
    fb->Config.RiseBandRatio = 0.90f;
    fb->Config.SettlingBand_c = 0.5f;
    fb->Config.SettlingHold_s = 2.0f;
    fb->Config.MinEpisode_s = 2.0f;
    fb->Config.MaxEpisode_s = 120.0f;

    clear_metrics(&fb->Metrics);
    fb->PreviousSV = 0.0f;
    fb->PreviousPV = 0.0f;
    fb->PreviousPWM = 0.0f;
    fb->PreviousError = 0.0f;
    fb->EpisodeTime_s = 0.0f;
    fb->SettlingHoldTime_s = 0.0f;
    fb->Initialized = false;
    fb->EpisodeActive = false;
}

void FB_FuzzyPerformanceMonitor_Reset(FB_FuzzyPerformanceMonitor_t *fb)
{
    if (fb == (FB_FuzzyPerformanceMonitor_t *)0)
    {
        return;
    }

    clear_metrics(&fb->Metrics);
    fb->PreviousSV = 0.0f;
    fb->PreviousPV = 0.0f;
    fb->PreviousPWM = 0.0f;
    fb->PreviousError = 0.0f;
    fb->EpisodeTime_s = 0.0f;
    fb->SettlingHoldTime_s = 0.0f;
    fb->Initialized = false;
    fb->EpisodeActive = false;
}

bool FB_FuzzyPerformanceMonitor_SetConfig(
    FB_FuzzyPerformanceMonitor_t *fb,
    const FuzzyPerformanceMonitorConfig_t *config)
{
    if ((fb == (FB_FuzzyPerformanceMonitor_t *)0) ||
        (config == (const FuzzyPerformanceMonitorConfig_t *)0))
    {
        return false;
    }

    if ((config->Ts <= 0.0f) ||
        (config->SvChangeThreshold_c < 0.0f) ||
        (config->RiseBandRatio <= 0.0f) ||
        (config->RiseBandRatio > 1.0f) ||
        (config->SettlingBand_c <= 0.0f) ||
        (config->SettlingHold_s < 0.0f) ||
        (config->MinEpisode_s < 0.0f) ||
        (config->MaxEpisode_s <= config->MinEpisode_s))
    {
        return false;
    }

    fb->Config = *config;
    return true;
}

void FB_FuzzyPerformanceMonitor_StartEpisode(
    FB_FuzzyPerformanceMonitor_t *fb,
    float sv,
    float pv,
    float pwm)
{
    if (fb == (FB_FuzzyPerformanceMonitor_t *)0)
    {
        return;
    }

    clear_metrics(&fb->Metrics);
    fb->Metrics.StartSV = fb->Initialized ? fb->PreviousSV : sv;
    fb->Metrics.TargetSV = sv;
    fb->Metrics.StartPV = pv;
    fb->Metrics.PeakPV = pv;
    fb->Metrics.ValleyPV = pv;

    fb->PreviousSV = sv;
    fb->PreviousPV = pv;
    fb->PreviousPWM = pwm;
    fb->PreviousError = sv - pv;
    fb->EpisodeTime_s = 0.0f;
    fb->SettlingHoldTime_s = 0.0f;
    fb->Initialized = true;
    fb->EpisodeActive = true;
}

void FB_FuzzyPerformanceMonitor_Run(
    FB_FuzzyPerformanceMonitor_t *fb,
    float sv,
    float pv,
    float pwm)
{
    float error;
    float pvRate;
    float targetSpan;
    float riseThreshold;
    bool riseReachedNow;

    if (fb == (FB_FuzzyPerformanceMonitor_t *)0)
    {
        return;
    }

    if (!fb->Initialized)
    {
        fb->PreviousSV = sv;
        fb->PreviousPV = pv;
        fb->PreviousPWM = pwm;
        fb->PreviousError = sv - pv;
        fb->Initialized = true;
        return;
    }

    if ((!fb->EpisodeActive) &&
        (absf_local(sv - fb->PreviousSV) >= fb->Config.SvChangeThreshold_c))
    {
        FB_FuzzyPerformanceMonitor_StartEpisode(fb, sv, pv, pwm);
        return;
    }

    if (!fb->EpisodeActive)
    {
        fb->PreviousSV = sv;
        fb->PreviousPV = pv;
        fb->PreviousPWM = pwm;
        fb->PreviousError = sv - pv;
        return;
    }

    /*
     * A second SV change invalidates the current response episode. Restart from
     * the new operating point instead of mixing two setpoint responses into one
     * set of metrics.
     */
    if (absf_local(sv - fb->Metrics.TargetSV) >= fb->Config.SvChangeThreshold_c)
    {
        FB_FuzzyPerformanceMonitor_StartEpisode(fb, sv, pv, pwm);
        return;
    }

    error = sv - pv;
    fb->EpisodeTime_s += fb->Config.Ts;
    fb->Metrics.SampleCount++;
    fb->Metrics.IAE += absf_local(error) * fb->Config.Ts;
    fb->Metrics.ISE += error * error * fb->Config.Ts;
    fb->Metrics.PWMActivity += absf_local(pwm - fb->PreviousPWM);

    if (pv > fb->Metrics.PeakPV)
    {
        fb->Metrics.PeakPV = pv;
    }
    if (pv < fb->Metrics.ValleyPV)
    {
        fb->Metrics.ValleyPV = pv;
    }

    pvRate = (pv - fb->PreviousPV) / fb->Config.Ts;
    if (absf_local(pvRate) > fb->Metrics.MaxPVRate_c_per_s)
    {
        fb->Metrics.MaxPVRate_c_per_s = absf_local(pvRate);
    }

    if (((fb->PreviousError > 0.0f) && (error < 0.0f)) ||
        ((fb->PreviousError < 0.0f) && (error > 0.0f)))
    {
        if (fb->Metrics.ErrorZeroCrossCount < 65535U)
        {
            fb->Metrics.ErrorZeroCrossCount++;
        }
    }

    targetSpan = fb->Metrics.TargetSV - fb->Metrics.StartPV;
    riseReachedNow = false;
    if (targetSpan >= 0.0f)
    {
        riseThreshold = fb->Metrics.StartPV + targetSpan * fb->Config.RiseBandRatio;
        riseReachedNow = (pv >= riseThreshold);
    }
    else
    {
        riseThreshold = fb->Metrics.StartPV + targetSpan * fb->Config.RiseBandRatio;
        riseReachedNow = (pv <= riseThreshold);
    }

    if (!fb->Metrics.RiseReached && riseReachedNow)
    {
        fb->Metrics.RiseReached = true;
        fb->Metrics.RiseTime_s = fb->EpisodeTime_s;
    }

    if (absf_local(error) <= fb->Config.SettlingBand_c)
    {
        fb->SettlingHoldTime_s += fb->Config.Ts;
        if ((!fb->Metrics.Settled) &&
            (fb->SettlingHoldTime_s >= fb->Config.SettlingHold_s))
        {
            fb->Metrics.Settled = true;
            fb->Metrics.SettlingTime_s = fb->EpisodeTime_s - fb->Config.SettlingHold_s;
        }
    }
    else
    {
        fb->SettlingHoldTime_s = 0.0f;
        fb->Metrics.Settled = false;
    }

    if (fb->Metrics.TargetSV >= fb->Metrics.StartPV)
    {
        fb->Metrics.Overshoot_c = fb->Metrics.PeakPV - fb->Metrics.TargetSV;
        if (fb->Metrics.Overshoot_c < 0.0f)
        {
            fb->Metrics.Overshoot_c = 0.0f;
        }
        fb->Metrics.Undershoot_c = fb->Metrics.TargetSV - fb->Metrics.ValleyPV;
    }
    else
    {
        fb->Metrics.Undershoot_c = fb->Metrics.TargetSV - fb->Metrics.ValleyPV;
        if (fb->Metrics.Undershoot_c < 0.0f)
        {
            fb->Metrics.Undershoot_c = 0.0f;
        }
        fb->Metrics.Overshoot_c = fb->Metrics.PeakPV - fb->Metrics.TargetSV;
    }

    fb->Metrics.SteadyStateError_c = error;

    if (((fb->EpisodeTime_s >= fb->Config.MinEpisode_s) && fb->Metrics.Settled) ||
        (fb->EpisodeTime_s >= fb->Config.MaxEpisode_s))
    {
        fb->Metrics.Complete = true;
        fb->EpisodeActive = false;
    }

    fb->PreviousSV = sv;
    fb->PreviousPV = pv;
    fb->PreviousPWM = pwm;
    fb->PreviousError = error;
}

bool FB_FuzzyPerformanceMonitor_IsComplete(
    const FB_FuzzyPerformanceMonitor_t *fb)
{
    if (fb == (const FB_FuzzyPerformanceMonitor_t *)0)
    {
        return false;
    }

    return fb->Metrics.Complete;
}

const FuzzyPerformanceMetrics_t *FB_FuzzyPerformanceMonitor_GetMetrics(
    const FB_FuzzyPerformanceMonitor_t *fb)
{
    if (fb == (const FB_FuzzyPerformanceMonitor_t *)0)
    {
        return (const FuzzyPerformanceMetrics_t *)0;
    }

    return &fb->Metrics;
}
