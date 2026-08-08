#include "FB_FuzzyController.h"

#include <stddef.h>

#define FUZZY_CONTROLLER_EPSILON    (0.000001f)

static float FuzzyController_Clamp(
    float value,
    float minValue,
    float maxValue
)
{
    if (value < minValue)
    {
        return minValue;
    }

    if (value > maxValue)
    {
        return maxValue;
    }

    return value;
}

/*
 * Initialize
 */
void FB_FuzzyController_Init(
    FB_FuzzyController_t *fb
)
{
    if (fb == NULL)
    {
        return;
    }

    fb->config.Ts = FUZZY_CONTROLLER_TS;
    fb->config.Enable = true;
    fb->config.OutputMin = 0.0f;
    fb->config.OutputMax = 1000.0f;

    fb->state.SV = 0.0f;
    fb->state.PV = 0.0f;
    fb->state.Error = 0.0f;
    fb->state.dError = 0.0f;
    fb->state.PWM = 0.0f;
    fb->state.Centroid = 0.0f;
    fb->state.initialized = false;
    fb->state.firstRun = true;

    FB_FuzzyScaling_Init(&fb->scaling);
    FB_FuzzyMembership_Init(&fb->membership);
    FB_FuzzyRule_Init(&fb->ruleEngine);
    FB_FuzzyDefuzzifier_Init(&fb->defuzz);
    FB_FuzzyOutput_Init(&fb->output);

    FB_FuzzyController_LoadDefaultRule(fb);

    fb->state.initialized = true;
}

/*
 * Execute
 *
 * Signal flow:
 *
 *   SV/PV
 *      |
 *      v
 *   Scaling
 *      |
 *      +--> normalized Error
 *      +--> normalized dError
 *      |
 *      v
 *   Membership
 *      |
 *      v
 *   7x7 Mamdani Rule Engine
 *      |
 *      v
 *   Weighted PWM rule output
 *      |
 *      v
 *   Output clamp
 *
 * NOTE:
 * The current FB_FuzzyRule implementation defines its rule table as
 * absolute PWM commands (0 ~ 1000), not linguistic output terms.
 * Therefore the controller intentionally uses RuleOutput directly here.
 * The Defuzzifier and OutputManager remain owned by the controller for the
 * next integration stage, but are not mixed into this absolute-PWM path.
 */
float FB_FuzzyController_Run(
    FB_FuzzyController_t *fb,
    float SV,
    float PV
)
{
    if (fb == NULL)
    {
        return 0.0f;
    }

    if (!fb->state.initialized)
    {
        FB_FuzzyController_Init(fb);
    }

    if (!fb->config.Enable)
    {
        fb->state.PWM = fb->config.OutputMin;
        return fb->state.PWM;
    }

    /* Store inputs. */
    fb->state.SV = SV;
    fb->state.PV = PV;
    fb->state.Error = SV - PV;

    /*
     * Prevent an artificial derivative spike on the first controller cycle.
     * Scaling normally calculates dError from its previous state.
     */
    if (fb->state.firstRun)
    {
        fb->scaling.State.PreviousError = fb->state.Error;
        fb->scaling.State.PreviousPV = PV;
        fb->state.firstRun = false;
    }

    /* Part 3: adaptive scaling and normalization. */
    FB_FuzzyScaling_Run(
        &fb->scaling,
        SV,
        PV
    );

    fb->state.Error = fb->scaling.State.Error;
    fb->state.dError = fb->scaling.State.dError;

    /* Part 1: membership calculation. */
    FB_FuzzyMembership_Run(
        &fb->membership,
        fb->scaling.State.NormalizedError,
        fb->scaling.State.NormalizedDError
    );

    /* Part 2: 7x7 Mamdani rule evaluation. */
    FB_FuzzyRule_Run(
        &fb->ruleEngine,
        &fb->membership
    );

    /*
     * The Rule Engine currently returns a weighted-average absolute PWM
     * command. Keep this representation intact and clamp at the controller
     * boundary.
     */
    fb->state.PWM = FuzzyController_Clamp(
        fb->ruleEngine.Result.RuleOutput,
        fb->config.OutputMin,
        fb->config.OutputMax
    );

    /*
     * Diagnostic normalized centroid equivalent for callers/telemetry.
     * It is not fed into OutputManager because RuleOutput is absolute PWM.
     */
    if (fb->config.OutputMax > fb->config.OutputMin + FUZZY_CONTROLLER_EPSILON)
    {
        fb->state.Centroid =
            ((fb->state.PWM - fb->config.OutputMin) /
             (fb->config.OutputMax - fb->config.OutputMin)) * 2.0f - 1.0f;
    }
    else
    {
        fb->state.Centroid = 0.0f;
    }

    return fb->state.PWM;
}

/*
 * Reset
 */
void FB_FuzzyController_Reset(
    FB_FuzzyController_t *fb
)
{
    if (fb == NULL)
    {
        return;
    }

    FB_FuzzyScaling_Reset(&fb->scaling);
    FB_FuzzyMembership_Reset(&fb->membership);
    FB_FuzzyRule_Reset(&fb->ruleEngine);
    FB_FuzzyDefuzzifier_Reset(&fb->defuzz);

    fb->state.SV = 0.0f;
    fb->state.PV = 0.0f;
    fb->state.Error = 0.0f;
    fb->state.dError = 0.0f;
    fb->state.PWM = fb->config.OutputMin;
    fb->state.Centroid = 0.0f;
    fb->state.firstRun = true;
}

/*
 * Default Rule
 */
void FB_FuzzyController_LoadDefaultRule(
    FB_FuzzyController_t *fb
)
{
    if (fb == NULL)
    {
        return;
    }

    FB_FuzzyRule_LoadDefault(&fb->ruleEngine);
}

/*
 * Update Rule Runtime
 */
bool FB_FuzzyController_SetRule(
    FB_FuzzyController_t *fb,
    uint8_t errorIndex,
    uint8_t dErrorIndex,
    int16_t outputPWM
)
{
    if (fb == NULL)
    {
        return false;
    }

    return FB_FuzzyRule_SetRule(
        &fb->ruleEngine,
        errorIndex,
        dErrorIndex,
        outputPWM
    );
}
