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

static FuzzyTunableParameters_t default_parameters(void)
{
    FuzzyTunableParameters_t p;
    p.Ke = 0.050f;
    p.Kde = 0.020f;
    p.Ku = 1.000f;
    p.ErrorWindow = 20.0f;
    p.FullPowerErrorRatio = 0.050f;
    p.PrecisionErrorRatio = 0.030f;
    return p;
}

static FuzzyPerformanceMetrics_t good_metrics(void)
{
    FuzzyPerformanceMetrics_t m = {0};
    m.StartSV = 25.0f;
    m.TargetSV = 130.0f;
    m.StartPV = 25.0f;
    m.PeakPV = 130.5f;
    m.ValleyPV = 25.0f;
    m.Overshoot_c = 0.5f;
    m.RiseTime_s = 8.0f;
    m.SettlingTime_s = 15.0f;
    m.SteadyStateError_c = 0.1f;
    m.IAE = 100.0f;
    m.ISE = 1000.0f;
    m.PWMActivity = 300.0f;
    m.MaxPVRate_c_per_s = 15.0f;
    m.SampleCount = 100U;
    m.ErrorZeroCrossCount = 2U;
    m.RiseReached = true;
    m.Settled = true;
    m.Complete = true;
    return m;
}

static void test_parameter_guard(void)
{
    FB_FuzzyParameterGuard_t guard;
    FuzzyTunableParameters_t current = default_parameters();
    FuzzyTunableParameters_t requested = current;
    FuzzyTunableParameters_t candidate;
    FuzzyTunableParameters_t rollback;

    FB_FuzzyParameterGuard_Init(&guard);
    FB_FuzzyParameterGuard_Accept(&guard, &current);

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

static void test_shadow_candidate_generation(void)
{
    FB_FuzzySelfTuner_t tuner;
    FuzzyPerformanceMetrics_t metrics = good_metrics();
    FuzzyTunableParameters_t current = default_parameters();
    FuzzyTunableParameters_t next;
    bool changed;

    FB_FuzzySelfTuner_Init(&tuner);

    /* Episode 1 establishes the accepted baseline only. */
    changed = FB_FuzzySelfTuner_EvaluateEpisode(
        &tuner, &metrics, &current, &next);
    assert(!changed);
    assert(tuner.Status.HasBaseline);
    assert(!tuner.Status.CandidatePending);

    /* Episode 2 is deliberately poor. Shadow mode only inspects next. */
    metrics.Overshoot_c = 3.0f;
    metrics.SettlingTime_s = 18.0f;
    metrics.IAE = 120.0f;

    changed = FB_FuzzySelfTuner_EvaluateEpisode(
        &tuner, &metrics, &current, &next);
    assert(changed);
    assert(tuner.Status.CandidatePending);
    assert(next.Ku < current.Ku);
    assert(next.Kde > current.Kde);
    assert(next.FullPowerErrorRatio < current.FullPowerErrorRatio);
    assert(next.PrecisionErrorRatio > current.PrecisionErrorRatio);

    /* Critical Shadow-Mode property: active parameters remain untouched. */
    assert(nearly_equal(current.Ke, 0.050f, 0.000001f));
    assert(nearly_equal(current.Kde, 0.020f, 0.000001f));
    assert(nearly_equal(current.Ku, 1.000f, 0.000001f));
}

static void test_candidate_rollback(void)
{
    FB_FuzzySelfTuner_t tuner;
    FuzzyPerformanceMetrics_t metrics = good_metrics();
    FuzzyTunableParameters_t current = default_parameters();
    FuzzyTunableParameters_t candidate;
    FuzzyTunableParameters_t rollback;

    FB_FuzzySelfTuner_Init(&tuner);
    assert(!FB_FuzzySelfTuner_EvaluateEpisode(&tuner, &metrics, &current, &candidate));

    metrics.Overshoot_c = 3.0f;
    metrics.IAE = 120.0f;
    assert(FB_FuzzySelfTuner_EvaluateEpisode(&tuner, &metrics, &current, &candidate));

    /* Simulate applying candidate for verification, then getting a worse result. */
    metrics.Overshoot_c = 5.0f;
    metrics.SettlingTime_s = 25.0f;
    metrics.IAE = 180.0f;
    assert(FB_FuzzySelfTuner_EvaluateEpisode(&tuner, &metrics, &candidate, &rollback));
    assert(tuner.Status.State == FUZZY_TUNER_ROLLBACK);
    assert(nearly_equal(rollback.Ke, current.Ke, 0.000001f));
    assert(nearly_equal(rollback.Ku, current.Ku, 0.000001f));
    assert(tuner.Status.RollbackCount == 1U);
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

static void test_sv_change_restarts_active_episode(void)
{
    FB_FuzzyPerformanceMonitor_t monitor;

    FB_FuzzyPerformanceMonitor_Init(&monitor);
    monitor.Config.Ts = 0.1f;
    monitor.Config.SvChangeThreshold_c = 1.0f;

    FB_FuzzyPerformanceMonitor_StartEpisode(&monitor, 100.0f, 90.0f, 700.0f);
    FB_FuzzyPerformanceMonitor_Run(&monitor, 100.0f, 92.0f, 650.0f);

    assert(monitor.Metrics.SampleCount == 1U);
    assert(nearly_equal(monitor.Metrics.TargetSV, 100.0f, 0.000001f));

    /* New setpoint invalidates the previous response and starts a fresh episode. */
    FB_FuzzyPerformanceMonitor_Run(&monitor, 120.0f, 92.5f, 800.0f);

    assert(monitor.EpisodeActive);
    assert(!monitor.Metrics.Complete);
    assert(monitor.Metrics.SampleCount == 0U);
    assert(nearly_equal(monitor.Metrics.TargetSV, 120.0f, 0.000001f));
    assert(nearly_equal(monitor.Metrics.StartPV, 92.5f, 0.000001f));
}

int main(void)
{
    test_parameter_guard();
    test_shadow_candidate_generation();
    test_candidate_rollback();
    test_performance_monitor();
    test_sv_change_restarts_active_episode();
    return 0;
}
