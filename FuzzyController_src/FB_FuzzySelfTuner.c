/******************************************************************************
 * File    : FB_FuzzySelfTuner.c
 * Brief   : Supervisory self tuner for fuzzy controller parameters.
 ******************************************************************************/

#include "FB_FuzzySelfTuner.h"

static float absf_local(float value)
{
    return (value >= 0.0f) ? value : -value;
}

static float positive_excess(float value, float target)
{
    return (value > target) ? (value - target) : 0.0f;
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

    fb->Config.WeightOvershoot = 4.0f;
    fb->Config.WeightRiseTime = 1.0f;
    fb->Config.WeightSettlingTime = 2.0f;
    fb->Config.WeightIAE = 0.05f;
    fb->Config.WeightSteadyStateError = 3.0f;
    fb->Config.WeightPWMActivity = 0.001f;

    fb->Config.GainStepUp = 0.02f;
    fb->Config.GainStepDown = 0.03f;
    fb->Config.DampingStepUp = 0.03f;
    fb->Config.ApproachStep = 0.02f;
    fb->Config.MinimumImprovement = 0.02f;

    fb->Status.State = FUZZY_TUNER_IDLE;
    fb->Status.BaselineCost = 0.0f;
    fb->Status.CandidateCost = 0.0f;
    fb->Status.HasBaseline = false;
    fb->Status.CandidatePending = false;
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
        (config->MinimumImprovement < 0.0f))
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

    if ((fb == (const FB_FuzzySelfTuner_t *)0) ||
        (metrics == (const FuzzyPerformanceMetrics_t *)0))
    {
        return 0.0f;
    }

    cost = 0.0f;
    cost += fb->Config.WeightOvershoot *
            positive_excess(metrics->Overshoot_c, fb->Config.TargetOvershoot_c);
    cost += fb->Config.WeightRiseTime *
            positive_excess(metrics->RiseTime_s, fb->Config.TargetRiseTime_s);
    cost += fb->Config.WeightSettlingTime *
            positive_excess(metrics->SettlingTime_s, fb->Config.TargetSettlingTime_s);
    cost += fb->Config.WeightIAE * metrics->IAE;
    cost += fb->Config.WeightSteadyStateError *
            positive_excess(absf_local(metrics->SteadyStateError_c),
                            fb->Config.TargetSteadyStateError_c);
    cost += fb->Config.WeightPWMActivity * metrics->PWMActivity;

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
    fb->Status.RollbackCount++;
    return true;
}

bool FB_FuzzySelfTuner_EvaluateEpisode(
    FB_FuzzySelfTuner_t *fb,
    const FuzzyPerformanceMetrics_t *metrics,
    const FuzzyTunableParameters_t *current,
    FuzzyTunableParameters_t *nextParameters)
{
    float cost;
    float improvementRequired;
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

    cost = FB_FuzzySelfTuner_CalculateCost(fb, metrics);
    fb->Status.State = FUZZY_TUNER_EVALUATE;

    if (!fb->Status.HasBaseline)
    {
        FB_FuzzySelfTuner_AcceptCurrent(fb, current, cost);
        *nextParameters = *current;
        return false;
    }

    if (fb->Status.CandidatePending)
    {
        fb->Status.CandidateCost = cost;
        improvementRequired = fb->Status.BaselineCost * fb->Config.MinimumImprovement;

        if ((fb->Status.BaselineCost - cost) >= improvementRequired)
        {
            FB_FuzzySelfTuner_AcceptCurrent(fb, current, cost);
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
