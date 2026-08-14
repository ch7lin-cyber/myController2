#include <assert.h>

#include "FB_FuzzyController.h"
#include "FB_FuzzySelfTuningBridge.h"

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
    float oldTargetKu;
    float oldFullRatio;

    FB_FuzzyController_Init(&controller);
    FB_FuzzySelfTuningBridge_Init(&bridge);

    assert(bridge.Config.ShadowMode);

    prepare_candidate(&bridge, &controller);

    oldTargetKu = controller.scaling.State.TargetKu;
    oldFullRatio = controller.config.FullPowerErrorRatio;

    assert(!FB_FuzzySelfTuningBridge_ApplyCandidate(&bridge, &controller));
    assert(controller.scaling.State.TargetKu == oldTargetKu);
    assert(controller.config.FullPowerErrorRatio == oldFullRatio);
    assert(!bridge.Status.CandidateApplied);
}

static void test_auto_scaling_blocks_explicit_apply(void)
{
    FB_FuzzyController_t controller;
    FB_FuzzySelfTuningBridge_t bridge;
    float oldTargetKu;

    FB_FuzzyController_Init(&controller);
    FB_FuzzySelfTuningBridge_Init(&bridge);
    FB_FuzzySelfTuningBridge_SetShadowMode(&bridge, false);

    assert(controller.scaling.Config.AutoScalingEnable);

    prepare_candidate(&bridge, &controller);
    oldTargetKu = controller.scaling.State.TargetKu;

    assert(!FB_FuzzySelfTuningBridge_ApplyCandidate(&bridge, &controller));
    assert(bridge.Status.ApplyBlockedByAutoScaling);
    assert(controller.scaling.State.TargetKu == oldTargetKu);
    assert(!bridge.Status.CandidateApplied);
}

static void test_manual_scaling_allows_explicit_apply(void)
{
    FB_FuzzyController_t controller;
    FB_FuzzySelfTuningBridge_t bridge;
    float requestedKu;

    FB_FuzzyController_Init(&controller);
    FB_FuzzySelfTuningBridge_Init(&bridge);
    FB_FuzzySelfTuningBridge_SetShadowMode(&bridge, false);
    FB_FuzzyScaling_DisableAuto(&controller.scaling);

    prepare_candidate(&bridge, &controller);
    requestedKu = bridge.Status.Candidate.Ku;

    assert(FB_FuzzySelfTuningBridge_ApplyCandidate(&bridge, &controller));
    assert(!bridge.Status.ApplyBlockedByAutoScaling);
    assert(bridge.Status.CandidateApplied);
    assert(controller.scaling.State.TargetKu == requestedKu);
    assert(controller.config.FullPowerErrorRatio ==
           bridge.Status.Candidate.FullPowerErrorRatio);
}

int main(void)
{
    test_default_shadow_mode_blocks_apply();
    test_auto_scaling_blocks_explicit_apply();
    test_manual_scaling_allows_explicit_apply();
    return 0;
}
