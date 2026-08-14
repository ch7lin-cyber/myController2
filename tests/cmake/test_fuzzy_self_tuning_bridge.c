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
    assert(FB_FuzzySelfTuningBridge_ApplyCandidate(&bridge, &controller));
    assert(bridge.HasApplyBackup);

    assert(FB_FuzzySelfTuningBridge_Rollback(&bridge, &controller));
    assert(!bridge.Status.CandidateApplied);
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

int main(void)
{
    test_default_shadow_mode_blocks_apply();
    test_auto_scaling_uses_persistent_trim();
    test_manual_scaling_blocks_trim_apply();
    test_apply_rollback_restores_exact_config();
    test_scaling_trim_persists_through_auto_calculation();
    return 0;
}
