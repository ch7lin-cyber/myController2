#include <assert.h>
#include <math.h>

#include "FB_FuzzyPerformanceMonitor.h"
#include "FB_FuzzyParameterGuard.h"
#include "FB_FuzzySelfTuner.h"

static int nearly_equal(float a, float b, float eps)
{
    float d = a - b;
    if (d < 0.0f) d = -d;
    return d <= eps;
}

static void test_parameter_guard(void)
{
    FB_FuzzyParameterGuard_t guard;
    FuzzyTunableParameters_t current;
    FuzzyTunableParameters_t requested;
    FuzzyTunableParameters_t candidate;
    FuzzyTunableParameters_t rollback;

    FB_FuzzyParameterGuard_Init(&guard);

    current.Ke = 0.050f;
    current.Kde = 0.020f;
    current.Ku = 1.000f;
    current.ErrorWindow = 20.0f;
    current.FullPowerErrorRatio = 0.050f;
    current.PrecisionErrorRatio = 0.030f;

    FB_FuzzyParameterGuard_Accept(&guard, &current);

    requested = current;
    requested.Ku = 0.500f;
    requested.Kde = 0.040f;

    assert(FB_FuzzyParameterGuard_MakeCandidate(
        &guard, &current, &requested, &candidate));

    assert(candidate.Ku < current.Ku);
    assert(candidate.Ku > 0.90f);
    assert(candidate.Kde > current.Kde);
    assert(candidate.FullPowerErrorRatio > candidate.PrecisionErrorRatio);

    assert(FB_FuzzyParameterGuard_Rollback(&guard, &rollback));
    assert(nearly_equal(rollback.Ke, current.Ke, 0.000001f));
    assert(nearly_equal(rollback.Ku, current.Ku, 0.000001f));
}

static void test_self_tuner_direction(void)
{
    FB_FuzzySelfTuner_t tuner;
    FuzzyPerformanceMetrics_t metrics;
    FuzzyTunableParameters_t current;
    FuzzyTunableParameters_t next;
    bool changed;

    FB_FuzzySelfTuner_Init(&tuner);

    current.Ke = 0.050f;
    current.Kde = 0.020f;
    current.Ku = 1.000f;
    current.ErrorWindow = 20.0f;
    current.FullPowerErrorRatio = 0.050f;
    current.PrecisionErrorRatio = 0.030f;

    metrics.StartSV = 25.0f;
    metrics.TargetSV = 130.0f;
    metrics.StartPV = 25.0f;
    metrics.PeakPV = 130.5f;
    metrics.ValleyPV = 25.0f;
    metrics.Overshoot_c = 0.5f;
    metrics.Undershoot_c = 0.0f;
    metrics.RiseTime_s = 8.0f;
    metrics.SettlingTime_s = 15.0f;
    metrics.SteadyStateError_c = 0.1f;
    metrics.IAE = 100.0f;
    metrics.ISE = 1000.0f;
    metrics.PWMActivity = 300.0f;
    metrics.MaxPVRate_c_per_s = 15.0f;
    metrics.SampleCount = 100U;
    metrics.ErrorZeroCrossCount = 2U;
    metrics.RiseReached = true;
    metrics.Settled = true;
    metrics.Complete = true;

    changed = FB_FuzzySelfTuner_EvaluateEpisode(
        &tuner, &metrics, &current, &next);
    assert(!changed);
    assert(tuner.Status.HasBaseline);

    metrics.Overshoot_c = 3.0f;
    metrics.RiseTime_s = 8.0f;
    metrics.SettlingTime_s = 18.0f;
    metrics.IAE = 120.0f;

    changed = FB_FuzzySelfTuner_EvaluateEpisode(
        &tuner, &metrics, &current, &next);
    assert(changed);
    assert(next.Ku < current.Ku);
    assert(next.Kde > current.Kde);
    assert(next.FullPowerErrorRatio < current.FullPowerErrorRatio);
    assert(next.PrecisionErrorRatio > current.PrecisionErrorRatio);
    assert(tuner.Status.CandidatePending);
}

static void test_performance_monitor(void)
{
    FB_FuzzyPerformanceMonitor_t monitor;
    const FuzzyPerformanceMetrics_t *m;
    unsigned i;
    float pv;

    FB_FuzzyPerformanceMonitor_Init(&monitor);
    monitor.Config.Ts = 0.1f;
    monitor.Config.MinEpisode_s = 0.5f;
    monitor.Config.MaxEpisode_s = 5.0f;
    monitor.Config.SettlingBand_c = 0.5f;
    monitor.Config.SettlingHold_s = 0.3f;

    FB_FuzzyPerformanceMonitor_StartEpisode(&monitor, 100.0f, 90.0f, 800.0f);

    for (i = 0U; i < 50U; ++i)
    {
        pv = 90.0f + (float)i * 0.25f;
        if (pv > 100.2f) pv = 100.2f;
        FB_FuzzyPerformanceMonitor_Run(&monitor, 100.0f, pv, 500.0f);
        if (FB_FuzzyPerformanceMonitor_IsComplete(&monitor)) break;
    }

    m = FB_FuzzyPerformanceMonitor_GetMetrics(&monitor);
    assert(m != 0);
    assert(m->SampleCount > 0U);
    assert(m->IAE >= 0.0f);
    assert(m->ISE >= 0.0f);
}

int main(void)
{
    test_parameter_guard();
    test_self_tuner_direction();
    test_performance_monitor();
    return 0;
}
