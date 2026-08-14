#include <assert.h>

#include "FB_FuzzyController.h"
#include "FB_FuzzySelfTuningBridge.h"

static int nearly_equal(float a, float b, float eps)
{
    float d = a - b;
    if (d < 0.0f) d = -d;
    return d <= eps;
}

static void prepare_candidate(
    FB_FuzzySelfTuningBridge_t *bridge,
    const FB_FuzzyController_t *controller)
{
    FuzzyTunableParameters_t current;

    assert(FB_FuzzySelfTuningBridge_GetControllerParameters(controller, &current));

    bridge->Status.Current = current;
    bridge->Status.Candidate = current;
    bridge->Status.Candidate.Ku = current.Ku * 0.97f;
    bridge->Status.Candidate.Kde = current.Kde * 1.03f;
    bridge->Status.Candidate.FullPowerErrorRatio =
        current.FullPowerErrorRatio * 0.98f;
    bridge->Status.Candidate.PrecisionErrorRatio =
        current.PrecisionErrorRatio * 1.02f;
    bridge->Status.CandidateAvailable = true;
}

static void mark_candidate_as_tuner_suggestion(FB_FuzzySelfTuningBridge_t *bridge)
{
    bridge->Tuner.Status.CandidatePending = true;
    bridge->Tuner.Status.State = FUZZY_TUNER_VERIFY;
    bridge->Tuner.Candidate = bridge->Status.Candidate;
    bridge->Tuner.Guard.Candidate = bridge->Status.Candidate;
    bridge->Tuner.Guard.HasCandidate = true;
}

static void test_default_shadow_mode_blocks_apply(void)
{
    FB_FuzzyController_t controller;
    FB_FuzzySelfTuningBridge_t bridge;
    float oldKuTrim;
    float oldFullRatio;

    FB_FuzzyController_Init(&controller);
    FB_FuzzySelfTuningBridge_Init(&bridge);

    assert(bridge.Config.ShadowMode);

    prepare_candidate(&bridge, &controller);

    oldKuTrim = controller.scaling.Config.SelfTuneKuTrim;
    oldFullRatio = controller.config.FullPowerErrorRatio;

    assert(!FB_FuzzySelfTuningBridge_ApplyCandidate(&bridge, &controller));
    assert(controller.scaling.Config.SelfTuneKuTrim == oldKuTrim);
    assert(controller.config.FullPowerErrorRatio == oldFullRatio);
    assert(!bridge.Status.CandidateApplied);
}

static void test_auto_scaling_uses_persistent_trim(void)
{
    FB_FuzzyController_t controller;
    FB_FuzzySelfTuningBridge_t bridge;
    float oldKuTrim;
    float oldKdeTrim;

    FB_FuzzyController_Init(&controller);
    FB_FuzzySelfTuningBridge_Init(&bridge);
    FB_FuzzySelfTuningBridge_SetShadowMode(&bridge, false);

    assert(controller.scaling.Config.AutoScalingEnable);

    prepare_candidate(&bridge, &controller);
    oldKuTrim = controller.scaling.Config.SelfTuneKuTrim;
    oldKdeTrim = controller.scaling.Config.SelfTuneKdeTrim;

    assert(FB_FuzzySelfTuningBridge_ApplyCandidate(&bridge, &controller));
    assert(!bridge.Status.ApplyBlockedByScalingMode);
    assert(bridge.Status.CandidateApplied);
    assert(controller.scaling.Config.SelfTuneKuTrim < oldKuTrim);
    assert(controller.scaling.Config.SelfTuneKdeTrim > oldKdeTrim);
    assert(nearly_equal(
        controller.config.FullPowerErrorRatio,
        bridge.Status.Candidate.FullPowerErrorRatio,
        0.000001f));
}

static void test_manual_scaling_blocks_trim_apply(void)
{
    FB_FuzzyController_t controller;
    FB_FuzzySelfTuningBridge_t bridge;
    float oldKuTrim;

    FB_FuzzyController_Init(&controller);
    FB_FuzzySelfTuningBridge_Init(&bridge);
    FB_FuzzySelfTuningBridge_SetShadowMode(&bridge, false);
    FB_FuzzyScaling_DisableAuto(&controller.scaling);

    prepare_candidate(&bridge, &controller);
    oldKuTrim = controller.scaling.Config.SelfTuneKuTrim;

    assert(!FB_FuzzySelfTuningBridge_ApplyCandidate(&bridge, &controller));
    assert(bridge.Status.ApplyBlockedByScalingMode);
    assert(controller.scaling.Config.SelfTuneKuTrim == oldKuTrim);
    assert(!bridge.Status.CandidateApplied);
}

static void test_apply_rollback_restores_exact_config(void)
{
    FB_FuzzyController_t controller;
    FB_FuzzySelfTuningBridge_t bridge;
    float oldKeTrim;
    float oldKdeTrim;
    float oldKuTrim;
    float oldWindowTrim;
    float oldFullRatio;
    float oldPrecisionRatio;

    FB_FuzzyController_Init(&controller);
    FB_FuzzySelfTuningBridge_Init(&bridge);
    FB_FuzzySelfTuningBridge_SetShadowMode(&bridge, false);

    oldKeTrim = controller.scaling.Config.SelfTuneKeTrim;
    oldKdeTrim = controller.scaling.Config.SelfTuneKdeTrim;
    oldKuTrim = controller.scaling.Config.SelfTuneKuTrim;
    oldWindowTrim = controller.scaling.Config.SelfTuneErrorWindowTrim;
    oldFullRatio = controller.config.FullPowerErrorRatio;
    oldPrecisionRatio = controller.config.PrecisionErrorRatio;

    prepare_candidate(&bridge, &controller);
    mark_candidate_as_tuner_suggestion(&bridge);
    assert(FB_FuzzySelfTuningBridge_ApplyCandidate(&bridge, &controller));
    assert(bridge.HasApplyBackup);

    assert(FB_FuzzySelfTuningBridge_Rollback(&bridge, &controller));
    assert(!bridge.Status.CandidateApplied);
    assert(!bridge.Status.RollbackRecommended);
    assert(!bridge.HasApplyBackup);

    assert(nearly_equal(controller.scaling.Config.SelfTuneKeTrim, oldKeTrim, 0.000001f));
    assert(nearly_equal(controller.scaling.Config.SelfTuneKdeTrim, oldKdeTrim, 0.000001f));
    assert(nearly_equal(controller.scaling.Config.SelfTuneKuTrim, oldKuTrim, 0.000001f));
    assert(nearly_equal(controller.scaling.Config.SelfTuneErrorWindowTrim, oldWindowTrim, 0.000001f));
    assert(nearly_equal(controller.config.FullPowerErrorRatio, oldFullRatio, 0.000001f));
    assert(nearly_equal(controller.config.PrecisionErrorRatio, oldPrecisionRatio, 0.000001f));
}

static void test_scaling_trim_persists_through_auto_calculation(void)
{
    FB_FuzzyScaling_t scaling;

    FB_FuzzyScaling_Init(&scaling);
    FB_FuzzyScaling_DisableAdaptive(&scaling);

    assert(FB_FuzzyScaling_SetSelfTuneTrim(
        &scaling,
        1.0f,
        1.0f,
        0.90f,
        1.0f));

    FB_FuzzyScaling_Run(&scaling, 100.0f, 90.0f);
    FB_FuzzyScaling_Run(&scaling, 100.0f, 90.0f);

    assert(nearly_equal(scaling.State.TargetKu, 0.90f, 0.000001f));
    assert(nearly_equal(scaling.Config.SelfTuneKuTrim, 0.90f, 0.000001f));
}

static void test_suggested_candidate_survives_episode_start(void)
{
    FB_FuzzyController_t controller;
    FB_FuzzySelfTuningBridge_t bridge;

    FB_FuzzyController_Init(&controller);
    FB_FuzzySelfTuningBridge_Init(&bridge);
    prepare_candidate(&bridge, &controller);
    mark_candidate_as_tuner_suggestion(&bridge);

    assert(FB_FuzzySelfTuningBridge_StartEpisode(
        &bridge, &controller, 130.0f, 25.0f, 1000.0f));

    assert(bridge.Status.CandidateAvailable);
    assert(!bridge.Status.CandidateApplied);
    assert(bridge.Tuner.Status.CandidatePending);
    assert(bridge.Tuner.Status.State == FUZZY_TUNER_VERIFY);
}

static void test_reject_candidate_is_not_rollback(void)
{
    FB_FuzzyController_t controller;
    FB_FuzzySelfTuningBridge_t bridge;
    uint32_t rollbackCount;

    FB_FuzzyController_Init(&controller);
    FB_FuzzySelfTuningBridge_Init(&bridge);
    prepare_candidate(&bridge, &controller);
    mark_candidate_as_tuner_suggestion(&bridge);
    rollbackCount = bridge.Tuner.Status.RollbackCount;

    assert(FB_FuzzySelfTuningBridge_RejectCandidate(&bridge));
    assert(!bridge.Status.CandidateAvailable);
    assert(!bridge.Status.CandidateApplied);
    assert(!bridge.Tuner.Status.CandidatePending);
    assert(bridge.Tuner.Status.State == FUZZY_TUNER_IDLE);
    assert(bridge.Tuner.Status.RollbackCount == rollbackCount);
}

static void test_unapplied_candidate_is_not_verified(void)
{
    FB_FuzzyController_t controller;
    FB_FuzzySelfTuningBridge_t bridge;
    FuzzyPerformanceMonitorConfig_t monitorConfig;
    uint32_t rollbackCount;

    FB_FuzzyController_Init(&controller);
    FB_FuzzySelfTuningBridge_Init(&bridge);
    prepare_candidate(&bridge, &controller);
    mark_candidate_as_tuner_suggestion(&bridge);
    rollbackCount = bridge.Tuner.Status.RollbackCount;

    monitorConfig = bridge.Monitor.Config;
    monitorConfig.Ts = 0.1f;
    monitorConfig.MinEpisode_s = 0.0f;
    monitorConfig.MaxEpisode_s = 0.3f;
    monitorConfig.SettlingBand_c = 1.0f;
    monitorConfig.SettlingHold_s = 0.0f;
    assert(FB_FuzzyPerformanceMonitor_SetConfig(&bridge.Monitor, &monitorConfig));

    assert(FB_FuzzySelfTuningBridge_StartEpisode(
        &bridge, &controller, 100.0f, 99.5f, 500.0f));

    FB_FuzzySelfTuningBridge_Run(&bridge, &controller, 100.0f, 99.6f, 500.0f);

    assert(!bridge.Status.EpisodeActive);
    assert(bridge.Status.CandidateAvailable);
    assert(!bridge.Status.CandidateApplied);
    assert(!bridge.Status.RollbackRecommended);
    assert(bridge.Tuner.Status.CandidatePending);
    assert(bridge.Tuner.Status.State == FUZZY_TUNER_VERIFY);
    assert(bridge.Tuner.Status.RollbackCount == rollbackCount);
}

static void test_rollback_recommendation_requires_explicit_rollback(void)
{
    FB_FuzzyController_t controller;
    FB_FuzzySelfTuningBridge_t bridge;
    float appliedKuTrim;

    FB_FuzzyController_Init(&controller);
    FB_FuzzySelfTuningBridge_Init(&bridge);
    FB_FuzzySelfTuningBridge_SetShadowMode(&bridge, false);
    prepare_candidate(&bridge, &controller);
    mark_candidate_as_tuner_suggestion(&bridge);

    assert(FB_FuzzySelfTuningBridge_ApplyCandidate(&bridge, &controller));
    appliedKuTrim = controller.scaling.Config.SelfTuneKuTrim;

    /* Simulate verification deciding the candidate is worse. */
    bridge.Tuner.Status.CandidatePending = false;
    bridge.Tuner.Status.State = FUZZY_TUNER_ROLLBACK;
    bridge.Status.CandidateAvailable = false;
    bridge.Status.RollbackRecommended = true;

    /* Monitoring is frozen while an operator/application decision is pending. */
    assert(!FB_FuzzySelfTuningBridge_StartEpisode(
        &bridge, &controller, 150.0f, 100.0f, 800.0f));
    assert(nearly_equal(
        controller.scaling.Config.SelfTuneKuTrim,
        appliedKuTrim,
        0.000001f));

    assert(FB_FuzzySelfTuningBridge_Rollback(&bridge, &controller));
    assert(!bridge.Status.RollbackRecommended);
    assert(!bridge.Status.CandidateApplied);
    assert(nearly_equal(
        controller.scaling.Config.SelfTuneKuTrim,
        FUZZY_SCALING_SELF_TUNE_TRIM_DEFAULT,
        0.000001f));
}

int main(void)
{
    test_default_shadow_mode_blocks_apply();
    test_auto_scaling_uses_persistent_trim();
    test_manual_scaling_blocks_trim_apply();
    test_apply_rollback_restores_exact_config();
    test_scaling_trim_persists_through_auto_calculation();
    test_suggested_candidate_survives_episode_start();
    test_reject_candidate_is_not_rollback();
    test_unapplied_candidate_is_not_verified();
    test_rollback_recommendation_requires_explicit_rollback();
    return 0;
}
