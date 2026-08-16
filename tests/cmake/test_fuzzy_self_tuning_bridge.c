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
    int16_t region;

    assert(FB_FuzzySelfTuningBridge_GetControllerParameters(controller, &current));

    region = FB_FuzzyTemperatureProfile_FindRegion(&bridge->TemperatureProfile, 130.0f);
    assert(region >= 0);

    bridge->Status.Current = current;
    bridge->Status.Candidate = current;
    bridge->Status.Candidate.Ku = current.Ku * 0.97f;
    bridge->Status.Candidate.Kde = current.Kde * 1.03f;
    bridge->Status.Candidate.FullPowerErrorRatio =
        current.FullPowerErrorRatio * 0.98f;
    bridge->Status.Candidate.PrecisionErrorRatio =
        current.PrecisionErrorRatio * 1.02f;
    bridge->Status.CandidateAvailable = true;
    bridge->Status.CandidateRegion = region;
    bridge->Status.CandidateRegionConfidence = 0.0f;
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
    const FuzzyTemperatureRegion_t *region;
    uint32_t oldRollbackCount;
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
    region = FB_FuzzySelfTuningBridge_GetCandidateRegion(&bridge);
    assert(region != 0);
    oldRollbackCount = region->RollbackCount;

    assert(FB_FuzzySelfTuningBridge_ApplyCandidate(&bridge, &controller));
    assert(bridge.HasApplyBackup);

    assert(FB_FuzzySelfTuningBridge_Rollback(&bridge, &controller));
    assert(!bridge.Status.CandidateApplied);
    assert(!bridge.Status.RollbackRecommended);
    assert(!bridge.HasApplyBackup);

    region = FB_FuzzySelfTuningBridge_GetCandidateRegion(&bridge);
    assert(region != 0);
    assert(region->RollbackCount == oldRollbackCount + 1U);

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

static void test_pending_candidate_freezes_new_learning(void)
{
    FB_FuzzyController_t controller;
    FB_FuzzySelfTuningBridge_t bridge;

    FB_FuzzyController_Init(&controller);
    FB_FuzzySelfTuningBridge_Init(&bridge);
    prepare_candidate(&bridge, &controller);
    mark_candidate_as_tuner_suggestion(&bridge);

    assert(!FB_FuzzySelfTuningBridge_StartEpisode(
        &bridge, &controller, 130.0f, 25.0f, 1000.0f));

    FB_FuzzySelfTuningBridge_Run(
        &bridge, &controller, 130.0f, 25.0f, 1000.0f);

    assert(!bridge.Status.EpisodeActive);
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
    assert(bridge.Status.CandidateRegion == FUZZY_SELF_TUNING_REGION_INVALID);
    assert(!bridge.Tuner.Status.CandidatePending);
    assert(bridge.Tuner.Status.State == FUZZY_TUNER_IDLE);
    assert(bridge.Tuner.Status.RollbackCount == rollbackCount);
}

static void test_monitor_syncs_to_controller_and_selects_region(void)
{
    FB_FuzzyController_t controller;
    FB_FuzzySelfTuningBridge_t bridge;
    const FuzzyTemperatureRegion_t *region;

    FB_FuzzyController_Init(&controller);
    FB_FuzzySelfTuningBridge_Init(&bridge);

    assert(FB_FuzzyController_SetSampleTime(&controller, 50U));
    bridge.Config.SVChangeThreshold_c = 2.5f;

    assert(FB_FuzzySelfTuningBridge_StartEpisode(
        &bridge, &controller, 130.0f, 25.0f, 900.0f));

    assert(nearly_equal(bridge.Monitor.Config.Ts, 0.050f, 0.000001f));
    assert(nearly_equal(
        bridge.Monitor.Config.SvChangeThreshold_c,
        2.5f,
        0.000001f));
    assert(bridge.Status.ActiveRegion == 2);

    region = FB_FuzzySelfTuningBridge_GetActiveRegion(&bridge);
    assert(region != 0);
    assert(nearly_equal(region->MinTemperature_c, 120.0f, 0.000001f));
    assert(nearly_equal(region->MaxTemperature_c, 160.0f, 0.000001f));
}

static void test_out_of_range_sv_does_not_start_learning(void)
{
    FB_FuzzyController_t controller;
    FB_FuzzySelfTuningBridge_t bridge;

    FB_FuzzyController_Init(&controller);
    FB_FuzzySelfTuningBridge_Init(&bridge);

    assert(!FB_FuzzySelfTuningBridge_StartEpisode(
        &bridge, &controller, 300.0f, 25.0f, 1000.0f));
    assert(bridge.Status.ActiveRegion == FUZZY_SELF_TUNING_REGION_INVALID);
    assert(!bridge.Status.EpisodeActive);
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

    bridge.Tuner.Status.CandidatePending = false;
    bridge.Tuner.Status.State = FUZZY_TUNER_ROLLBACK;
    bridge.Status.CandidateAvailable = false;
    bridge.Status.RollbackRecommended = true;

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
    test_pending_candidate_freezes_new_learning();
    test_reject_candidate_is_not_rollback();
    test_monitor_syncs_to_controller_and_selects_region();
    test_out_of_range_sv_does_not_start_learning();
    test_rollback_recommendation_requires_explicit_rollback();
    return 0;
}
