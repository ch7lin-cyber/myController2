/******************************************************************************
 * File    : FB_FuzzySelfTuner.c
 * Brief   : Supervisory self tuner for fuzzy controller parameters.
 ******************************************************************************/

#include "FB_FuzzySelfTuner.h"

#define SELF_TUNER_EPSILON (0.000001f)

static float absf_local(float value)
{
    return (value >= 0.0f) ? value : -value;
}

static float maxf_local(float a, float b)
{
    return (a > b) ? a : b;
}

static float positive_excess(float value, float target)
{
    return (value > target) ? (value - target) : 0.0f;
}

static float episode_step_magnitude(const FuzzyPerformanceMetrics_t *metrics)
{
    if (metrics == (const FuzzyPerformanceMetrics_t *)0)
    {
        return 0.0f;
    }

    return absf_local(metrics->TargetSV - metrics->StartPV);
}

void FB_FuzzySelfTuner_Init(FB_FuzzySelfTuner_t *fb)
{
    if (fb == (FB_FuzzySelfTuner_t *)0)
    {
        return;
    }

    fb->Config.Enable = true;
    fb->Config.TargetOvershoot_c = 1.0f;
    fb->Config.TargetRiseTime_s = 10.0f;
    fb->Config.TargetSettlingTime_s = 20.0f;
    fb->Config.TargetSteadyStateError_c = 0.2f;
    fb->Config.MaxZeroCrossCount = 4U;

    /* Cost terms are normalized for different step amplitudes/durations. */
    fb->Config.WeightOvershoot = 4.0f;
    fb->Config.WeightRiseTime = 1.0f;
    fb->Config.WeightSettlingTime = 2.0f;
    fb->Config.WeightIAE = 1.0f;
    fb->Config.WeightSteadyStateError = 3.0f;
    fb->Config.WeightPWMActivity = 0.1f;

    fb->Config.GainStepUp = 0.02f;
    fb->Config.GainStepDown = 0.03f;
    fb->Config.DampingStepUp = 0.03f;
    fb->Config.ApproachStep = 0.02f;
    fb->Config.MinimumImprovement = 0.02f;

    fb->Config.VerificationTargetTolerance_c = 10.0f;
    fb->Config.VerificationStepRatioTolerance = 0.25f;
    fb->Config.MinimumStepMagnitude_c = 2.0f;

    fb->Status.State = FUZZY_TUNER_IDLE;
    fb->Status.BaselineCost = 0.0f;
    fb->Status.CandidateCost = 0.0f;
    fb->Status.HasBaseline = false;
    fb->Status.CandidatePending = false;
    fb->Status.VerificationDeferred = false;
    fb->Status.AcceptedCount = 0U;
    fb->Status.RollbackCount = 0U;

    FB_FuzzyParameterGuard_Init(&fb->Guard);

    fb->Baseline.Ke = 0.0f;
    fb->Baseline.Kde = 0.0f;
    fb->Baseline.Ku = 0.0f;
    fb->Baseline.ErrorWindow = 0.0f;
    fb->Baseline.FullPowerErrorRatio = 0.0f;
    fb->Baseline.PrecisionErrorRatio = 0.0f;
    fb->Candidate = fb->Baseline;

    fb->BaselineTargetSV_c = 0.0f;
    fb->BaselineStepMagnitude_c = 0.0f;
}

void FB_FuzzySelfTuner_Reset(FB_FuzzySelfTuner_t *fb)
{
    FuzzySelfTunerConfig_t config;

    if (fb == (FB_FuzzySelfTuner_t *)0)
    {
        return;
    }

    config = fb->Config;
    FB_FuzzySelfTuner_Init(fb);
    fb->Config = config;
}

bool FB_FuzzySelfTuner_SetConfig(
    FB_FuzzySelfTuner_t *fb,
    const FuzzySelfTunerConfig_t *config)
{
    if ((fb == (FB_FuzzySelfTuner_t *)0) ||
        (config == (const FuzzySelfTunerConfig_t *)0))
    {
        return false;
    }

    if ((config->TargetOvershoot_c < 0.0f) ||
        (config->TargetRiseTime_s <= 0.0f) ||
        (config->TargetSettlingTime_s <= 0.0f) ||
        (config->TargetSteadyStateError_c < 0.0f) ||
        (config->GainStepUp <= 0.0f) ||
        (config->GainStepDown <= 0.0f) ||
        (config->DampingStepUp <= 0.0f) ||
        (config->ApproachStep <= 0.0f) ||
        (config->MinimumImprovement < 0.0f) ||
        (config->VerificationTargetTolerance_c < 0.0f) ||
        (config->VerificationStepRatioTolerance < 0.0f) ||
        (config->VerificationStepRatioTolerance > 1.0f) ||
        (config->MinimumStepMagnitude_c <= 0.0f))
    {
        return false;
    }

    fb->Config = *config;
    fb->Status.State = config->Enable ? FUZZY_TUNER_IDLE : FUZZY_TUNER_DISABLED;
    return true;
}

float FB_FuzzySelfTuner_CalculateCost(
    const FB_FuzzySelfTuner_t *fb,
    const FuzzyPerformanceMetrics_t *metrics)
{
    float cost;
    float stepMagnitude;
    float normalizedIAE;
    float normalizedPWMActivity;

    if ((fb == (const FB_FuzzySelfTuner_t *)0) ||
        (metrics == (const FuzzyPerformanceMetrics_t *)0))
    {
        return 0.0f;
    }

    stepMagnitude = maxf_local(
        episode_step_magnitude(metrics),
        fb->Config.MinimumStepMagnitude_c);

    normalizedIAE = metrics->IAE / stepMagnitude;
    normalizedPWMActivity = (metrics->SampleCount > 0U)
        ? (metrics->PWMActivity / (float)metrics->SampleCount)
        : 0.0f;

    cost = 0.0f;
    cost += fb->Config.WeightOvershoot *
            positive_excess(metrics->Overshoot_c, fb->Config.TargetOvershoot_c);
    cost += fb->Config.WeightRiseTime *
            positive_excess(metrics->RiseTime_s, fb->Config.TargetRiseTime_s);
    cost += fb->Config.WeightSettlingTime *
            positive_excess(metrics->SettlingTime_s, fb->Config.TargetSettlingTime_s);
    cost += fb->Config.WeightIAE * normalizedIAE;
    cost += fb->Config.WeightSteadyStateError *
            positive_excess(absf_local(metrics->SteadyStateError_c),
                            fb->Config.TargetSteadyStateError_c);
    cost += fb->Config.WeightPWMActivity * normalizedPWMActivity;

    if (!metrics->Settled)
    {
        cost += fb->Config.WeightSettlingTime * fb->Config.TargetSettlingTime_s;
    }

    if (metrics->ErrorZeroCrossCount > fb->Config.MaxZeroCrossCount)
    {
        cost += (float)(metrics->ErrorZeroCrossCount - fb->Config.MaxZeroCrossCount);
    }

    return cost;
}

bool FB_FuzzySelfTuner_IsComparableEpisode(
    const FB_FuzzySelfTuner_t *fb,
    const FuzzyPerformanceMetrics_t *metrics)
{
    float stepMagnitude;
    float stepDifferenceRatio;
    float referenceStep;

    if ((fb == (const FB_FuzzySelfTuner_t *)0) ||
        (metrics == (const FuzzyPerformanceMetrics_t *)0) ||
        !fb->Status.HasBaseline)
    {
        return false;
    }

    stepMagnitude = episode_step_magnitude(metrics);
    if ((stepMagnitude < fb->Config.MinimumStepMagnitude_c) ||
        (fb->BaselineStepMagnitude_c < fb->Config.MinimumStepMagnitude_c))
    {
        return false;
    }

    if (absf_local(metrics->TargetSV - fb->BaselineTargetSV_c) >
        fb->Config.VerificationTargetTolerance_c)
    {
        return false;
    }

    referenceStep = maxf_local(
        fb->BaselineStepMagnitude_c,
        fb->Config.MinimumStepMagnitude_c);

    if (referenceStep <= SELF_TUNER_EPSILON)
    {
        return false;
    }

    stepDifferenceRatio = absf_local(stepMagnitude - fb->BaselineStepMagnitude_c) /
                          referenceStep;

    return stepDifferenceRatio <= fb->Config.VerificationStepRatioTolerance;
}

void FB_FuzzySelfTuner_AcceptCurrent(
    FB_FuzzySelfTuner_t *fb,
    const FuzzyTunableParameters_t *parameters,
    float cost)
{
    if ((fb == (FB_FuzzySelfTuner_t *)0) ||
        (parameters == (const FuzzyTunableParameters_t *)0))
    {
        return;
    }

    fb->Baseline = *parameters;
    fb->Status.BaselineCost = cost;
    fb->Status.HasBaseline = true;
    fb->Status.CandidatePending = false;
    fb->Status.VerificationDeferred = false;
    fb->Status.State = FUZZY_TUNER_ACCEPT;
    fb->Status.AcceptedCount++;
    FB_FuzzyParameterGuard_Accept(&fb->Guard, parameters);
}

bool FB_FuzzySelfTuner_Rollback(
    FB_FuzzySelfTuner_t *fb,
    FuzzyTunableParameters_t *parameters)
{
    if ((fb == (FB_FuzzySelfTuner_t *)0) ||
        (parameters == (FuzzyTunableParameters_t *)0))
    {
        return false;
    }

    if (!FB_FuzzyParameterGuard_Rollback(&fb->Guard, parameters))
    {
        return false;
    }

    fb->Status.State = FUZZY_TUNER_ROLLBACK;
    fb->Status.CandidatePending = false;
    fb->Status.VerificationDeferred = false;
    fb->Status.RollbackCount++;
    return true;
}

void FB_FuzzySelfTuner_CancelCandidate(FB_FuzzySelfTuner_t *fb)
{
    if (fb == (FB_FuzzySelfTuner_t *)0)
    {
        return;
    }

    fb->Status.CandidatePending = false;
    fb->Status.CandidateCost = 0.0f;
    fb->Status.VerificationDeferred = false;
    fb->Candidate = fb->Baseline;
    fb->Guard.HasCandidate = false;
    fb->Status.State = fb->Config.Enable ? FUZZY_TUNER_IDLE : FUZZY_TUNER_DISABLED;
}

bool FB_FuzzySelfTuner_EvaluateEpisode(
    FB_FuzzySelfTuner_t *fb,
    const FuzzyPerformanceMetrics_t *metrics,
    const FuzzyTunableParameters_t *current,
    FuzzyTunableParameters_t *nextParameters)
{
    float cost;
    float improvementRequired;
    float stepMagnitude;
    FuzzyTunableParameters_t requested;

    if ((fb == (FB_FuzzySelfTuner_t *)0) ||
        (metrics == (const FuzzyPerformanceMetrics_t *)0) ||
        (current == (const FuzzyTunableParameters_t *)0) ||
        (nextParameters == (FuzzyTunableParameters_t *)0) ||
        !metrics->Complete)
    {
        return false;
    }

    if (!fb->Config.Enable)
    {
        fb->Status.State = FUZZY_TUNER_DISABLED;
        return false;
    }

    stepMagnitude = episode_step_magnitude(metrics);
    if (stepMagnitude < fb->Config.MinimumStepMagnitude_c)
    {
        fb->Status.State = fb->Status.CandidatePending
            ? FUZZY_TUNER_VERIFY
            : FUZZY_TUNER_IDLE;
        fb->Status.VerificationDeferred = fb->Status.CandidatePending;
        return false;
    }

    cost = FB_FuzzySelfTuner_CalculateCost(fb, metrics);
    fb->Status.State = FUZZY_TUNER_EVALUATE;
    fb->Status.VerificationDeferred = false;

    if (!fb->Status.HasBaseline)
    {
        FB_FuzzySelfTuner_AcceptCurrent(fb, current, cost);
        fb->BaselineTargetSV_c = metrics->TargetSV;
        fb->BaselineStepMagnitude_c = stepMagnitude;
        *nextParameters = *current;
        return false;
    }

    if (fb->Status.CandidatePending)
    {
        if (!FB_FuzzySelfTuner_IsComparableEpisode(fb, metrics))
        {
            fb->Status.State = FUZZY_TUNER_VERIFY;
            fb->Status.VerificationDeferred = true;
            *nextParameters = *current;
            return false;
        }

        fb->Status.CandidateCost = cost;
        improvementRequired = fb->Status.BaselineCost * fb->Config.MinimumImprovement;

        if ((fb->Status.BaselineCost - cost) >= improvementRequired)
        {
            FB_FuzzySelfTuner_AcceptCurrent(fb, current, cost);
            fb->BaselineTargetSV_c = metrics->TargetSV;
            fb->BaselineStepMagnitude_c = stepMagnitude;
            *nextParameters = *current;
            return false;
        }

        if (FB_FuzzySelfTuner_Rollback(fb, nextParameters))
        {
            return true;
        }

        return false;
    }

    requested = *current;
    fb->Status.State = FUZZY_TUNER_ADJUST;

    if (metrics->Overshoot_c > fb->Config.TargetOvershoot_c)
    {
        requested.Ku *= (1.0f - fb->Config.GainStepDown);
        requested.Kde *= (1.0f + fb->Config.DampingStepUp);
        requested.FullPowerErrorRatio *= (1.0f - fb->Config.ApproachStep);
        requested.PrecisionErrorRatio *= (1.0f + fb->Config.ApproachStep);
    }
    else if ((metrics->RiseTime_s > fb->Config.TargetRiseTime_s) &&
             (metrics->Overshoot_c <= fb->Config.TargetOvershoot_c))
    {
        requested.Ke *= (1.0f + fb->Config.GainStepUp);
        requested.Ku *= (1.0f + fb->Config.GainStepUp);
        requested.FullPowerErrorRatio *= (1.0f + fb->Config.ApproachStep);
    }

    if (metrics->ErrorZeroCrossCount > fb->Config.MaxZeroCrossCount)
    {
        requested.Ke *= (1.0f - fb->Config.GainStepDown);
        requested.Ku *= (1.0f - fb->Config.GainStepDown);
        requested.Kde *= (1.0f + fb->Config.DampingStepUp);
    }

    if ((!metrics->Settled) &&
        (metrics->Overshoot_c <= fb->Config.TargetOvershoot_c))
    {
        requested.ErrorWindow *= (1.0f + fb->Config.GainStepUp);
    }

    if (!FB_FuzzyParameterGuard_MakeCandidate(
            &fb->Guard, current, &requested, &fb->Candidate))
    {
        return false;
    }

    fb->Status.CandidatePending = true;
    fb->Status.State = FUZZY_TUNER_VERIFY;
    *nextParameters = fb->Candidate;
    return true;
}
