/******************************************************************************
 * File    : FB_FuzzyDefuzzifier.c
 * Version : V2.0
 *
 * Brief   : Mamdani Fuzzy Defuzzification Engine
 ******************************************************************************/

#include "FB_FuzzyDefuzzifier.h"

#include <stddef.h>

/* ============================================================================
 * Internal Functions
 * ========================================================================== */

static float FuzzyDefuzz_Abs(
    float x
)
{
    return (x >= 0.0f) ? x : -x;
}

/* -------------------------------------------------------------------------- */

static float FuzzyDefuzz_Min(
    float a,
    float b
)
{
    return (a < b) ? a : b;
}

/* -------------------------------------------------------------------------- */

static float FuzzyDefuzz_Max(
    float a,
    float b
)
{
    return (a > b) ? a : b;
}

/* -------------------------------------------------------------------------- */

static float FuzzyDefuzz_Clamp(
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

/* ============================================================================
 * Triangular Membership
 * ========================================================================== */

static float FuzzyDefuzz_Triangle(
    float x,
    const FuzzyOutputMF_t *mf
)
{
    if (mf == NULL)
    {
        return 0.0f;
    }

    /*
     * Outside range.
     */
    if ((x <= mf->Left) ||
        (x >= mf->Right))
    {
        /*
         * Handle peak at boundary.
         */
        if ((x == mf->Center) &&
            (mf->Center == mf->Left ||
             mf->Center == mf->Right))
        {
            return mf->Peak;
        }

        return 0.0f;
    }

    /*
     * Peak.
     */
    if (x == mf->Center)
    {
        return mf->Peak;
    }

    /*
     * Rising edge.
     */
    if (x < mf->Center)
    {
        float width =
            mf->Center - mf->Left;

        if (width <= 0.000001f)
        {
            return mf->Peak;
        }

        return
            (
                (x - mf->Left) /
                width
            )
            *
            mf->Peak;
    }

    /*
     * Falling edge.
     */
    else
    {
        float width =
            mf->Right - mf->Center;

        if (width <= 0.000001f)
        {
            return mf->Peak;
        }

        return
            (
                (mf->Right - x) /
                width
            )
            *
            mf->Peak;
    }
}

/* ============================================================================
 * Default Membership Configuration
 * ========================================================================== */

static void FuzzyDefuzz_SetDefaultMF(
    FuzzyDefuzzifierConfig_t *config
)
{
    /*
     * Seven output linguistic variables.
     *
     * NB NM NS ZE PS PM PB
     *
     * Normalized range:
     *
     * -1.0 ~ +1.0
     */

    config->MF[0].Left   = -1.00f;
    config->MF[0].Center = -1.00f;
    config->MF[0].Right  = -0.666f;
    config->MF[0].Peak   = 1.00f;

    config->MF[1].Left   = -1.00f;
    config->MF[1].Center = -0.666f;
    config->MF[1].Right  = -0.333f;
    config->MF[1].Peak   = 1.00f;

    config->MF[2].Left   = -0.666f;
    config->MF[2].Center = -0.333f;
    config->MF[2].Right  = 0.000f;
    config->MF[2].Peak   = 1.00f;

    config->MF[3].Left   = -0.333f;
    config->MF[3].Center = 0.000f;
    config->MF[3].Right  = 0.333f;
    config->MF[3].Peak   = 1.00f;

    config->MF[4].Left   = 0.000f;
    config->MF[4].Center = 0.333f;
    config->MF[4].Right  = 0.666f;
    config->MF[4].Peak   = 1.00f;

    config->MF[5].Left   = 0.333f;
    config->MF[5].Center = 0.666f;
    config->MF[5].Right  = 1.000f;
    config->MF[5].Peak   = 1.00f;

    config->MF[6].Left   = 0.666f;
    config->MF[6].Center = 1.000f;
    config->MF[6].Right  = 1.000f;
    config->MF[6].Peak   = 1.00f;
}

/* ============================================================================
 * Initialization
 * ========================================================================== */

void FB_FuzzyDefuzzifier_Init(
    FB_FuzzyDefuzzifier_t *fb
)
{
    uint16_t i;

    if (fb == NULL)
    {
        return;
    }

    /*
     * Configuration.
     */
    fb->Config.OutputMin =
        FUZZY_DEFUZZ_DEFAULT_OUTPUT_MIN;

    fb->Config.OutputMax =
        FUZZY_DEFUZZ_DEFAULT_OUTPUT_MAX;

    fb->Config.CentroidPoints =
        FUZZY_DEFUZZ_DEFAULT_CENTROID_POINTS;

    fb->Config.OutputSlewRate =
        FUZZY_DEFUZZ_DEFAULT_SLEW_RATE;

    fb->Config.OutputScalingEnable =
        true;

    fb->Config.OutputSlewEnable =
        true;

    FuzzyDefuzz_SetDefaultMF(
        &fb->Config
    );

    /*
     * Runtime.
     */
    for (i = 0U;
         i < FUZZY_DEFUZZ_OUTPUT_MF_COUNT;
         i++)
    {
        fb->State.Activation[i] =
            0.0f;
    }

    for (i = 0U;
         i < FUZZY_DEFUZZ_DEFAULT_CENTROID_POINTS;
         i++)
    {
        fb->State.Aggregated[i] =
            0.0f;
    }

    fb->State.Centroid =
        0.0f;

    fb->State.RawOutput =
        0.0f;

    fb->State.ScaledOutput =
        0.0f;

    fb->State.Output =
        0.0f;

    fb->State.Ku =
        1.0f;

    fb->State.TargetOutput =
        0.0f;

    fb->State.PreviousOutput =
        0.0f;

    fb->State.Numerator =
        0.0f;

    fb->State.Denominator =
        0.0f;
}

/* ============================================================================
 * Reset
 * ========================================================================== */

void FB_FuzzyDefuzzifier_Reset(
    FB_FuzzyDefuzzifier_t *fb
)
{
    uint16_t i;

    if (fb == NULL)
    {
        return;
    }

    for (i = 0U;
         i < FUZZY_DEFUZZ_OUTPUT_MF_COUNT;
         i++)
    {
        fb->State.Activation[i] =
            0.0f;
    }

    for (i = 0U;
         i < FUZZY_DEFUZZ_DEFAULT_CENTROID_POINTS;
         i++)
    {
        fb->State.Aggregated[i] =
            0.0f;
    }

    fb->State.Centroid =
        0.0f;

    fb->State.RawOutput =
        0.0f;

    fb->State.ScaledOutput =
        0.0f;

    fb->State.Output =
        0.0f;

    fb->State.TargetOutput =
        0.0f;

    fb->State.PreviousOutput =
        0.0f;

    fb->State.Numerator =
        0.0f;

    fb->State.Denominator =
        0.0f;
}

/* ============================================================================
 * Clear Rule Activation
 * ========================================================================== */

void FB_FuzzyDefuzzifier_Clear(
    FB_FuzzyDefuzzifier_t *fb
)
{
    uint8_t i;

    if (fb == NULL)
    {
        return;
    }

    for (i = 0U;
         i < FUZZY_DEFUZZ_OUTPUT_MF_COUNT;
         i++)
    {
        fb->State.Activation[i] =
            0.0f;
    }
}

/* ============================================================================
 * Apply One Rule
 * ========================================================================== */

void FB_FuzzyDefuzzifier_ApplyRule(
    FB_FuzzyDefuzzifier_t *fb,
    uint8_t outputMF,
    float strength
)
{
    if (fb == NULL)
    {
        return;
    }

    if (outputMF >=
        FUZZY_DEFUZZ_OUTPUT_MF_COUNT)
    {
        return;
    }

    strength =
        FuzzyDefuzz_Clamp(
            strength,
            0.0f,
            1.0f
        );

    /*
     * Mamdani aggregation:
     *
     * max(existing, current rule)
     */
    fb->State.Activation[outputMF] =
        FuzzyDefuzz_Max(
            fb->State.Activation[outputMF],
            strength
        );
}

/* ============================================================================
 * Apply 49 Rules
 * ========================================================================== */

void FB_FuzzyDefuzzifier_ApplyRules(
    FB_FuzzyDefuzzifier_t *fb,
    const float ruleStrength[7][7],
    const uint8_t ruleTable[7][7]
)
{
    uint8_t e;
    uint8_t de;

    if ((fb == NULL) ||
        (ruleStrength == NULL) ||
        (ruleTable == NULL))
    {
        return;
    }

    /*
     * Clear previous cycle.
     */
    FB_FuzzyDefuzzifier_Clear(fb);

    /*
     * 7 x 7 = 49 rules.
     */
    for (e = 0U;
         e < 7U;
         e++)
    {
        for (de = 0U;
             de < 7U;
             de++)
        {
            uint8_t outputMF;
            float strength;

            outputMF =
                ruleTable[e][de];

            strength =
                ruleStrength[e][de];

            FB_FuzzyDefuzzifier_ApplyRule(
                fb,
                outputMF,
                strength
            );
        }
    }
}

/* ============================================================================
 * Calculate Centroid
 * ========================================================================== */

float FB_FuzzyDefuzzifier_CalculateCentroid(
    FB_FuzzyDefuzzifier_t *fb
)
{
    uint16_t i;

    float numerator;
    float denominator;

    float x;
    float dx;

    uint16_t points;

    if (fb == NULL)
    {
        return 0.0f;
    }

    points =
        fb->Config.CentroidPoints;

    /*
     * Safety.
     */
    if (points < 2U)
    {
        points = 2U;
    }

    if (points >
        FUZZY_DEFUZZ_DEFAULT_CENTROID_POINTS)
    {
        points =
            FUZZY_DEFUZZ_DEFAULT_CENTROID_POINTS;
    }

    /*
     * Normalized output range.
     */
    dx =
        (
            FUZZY_DEFUZZ_NORMALIZED_MAX -
            FUZZY_DEFUZZ_NORMALIZED_MIN
        )
        /
        (float)(points - 1U);

    numerator = 0.0f;

    denominator = 0.0f;

    /*
     * ---------------------------------------------------------
     * Aggregate all seven output MFs.
     * ---------------------------------------------------------
     */
    for (i = 0U;
         i < points;
         i++)
    {
        uint8_t mfIndex;

        float aggregate =
            0.0f;

        x =
            FUZZY_DEFUZZ_NORMALIZED_MIN +
            ((float)i * dx);

        /*
         * Max aggregation.
         */
        for (mfIndex = 0U;
             mfIndex <
             FUZZY_DEFUZZ_OUTPUT_MF_COUNT;
             mfIndex++)
        {
            float mfValue;
            float clipped;

            mfValue =
                FuzzyDefuzz_Triangle(
                    x,
                    &fb->Config.MF[mfIndex]
                );

            /*
             * Mamdani implication:
             *
             * min(MF(x), RuleStrength)
             */
            clipped =
                FuzzyDefuzz_Min(
                    mfValue,
                    fb->State.Activation[mfIndex]
                );

            /*
             * Aggregate:
             *
             * max()
             */
            aggregate =
                FuzzyDefuzz_Max(
                    aggregate,
                    clipped
                );
        }

        fb->State.Aggregated[i] =
            aggregate;

        /*
         * Centroid integration.
         */
        numerator +=
            x *
            aggregate;

        denominator +=
            aggregate;
    }

    fb->State.Numerator =
        numerator;

    fb->State.Denominator =
        denominator;

    /*
     * No active rule.
     *
     * Return zero.
     */
    if (denominator <= 0.000001f)
    {
        fb->State.Centroid =
            0.0f;

        return 0.0f;
    }

    fb->State.Centroid =
        numerator /
        denominator;

    /*
     * Final safety clamp.
     */
    fb->State.Centroid =
        FuzzyDefuzz_Clamp(
            fb->State.Centroid,
            -1.0f,
            1.0f
        );

    return fb->State.Centroid;
}

/* ============================================================================
 * Normalized -> Physical Output
 * ========================================================================== */

float FB_FuzzyDefuzzifier_NormalizedToOutput(
    FB_FuzzyDefuzzifier_t *fb,
    float normalized
)
{
    float ratio;
    float output;

    if (fb == NULL)
    {
        return 0.0f;
    }

    normalized =
        FuzzyDefuzz_Clamp(
            normalized,
            -1.0f,
            1.0f
        );

    /*
     * ---------------------------------------------------------
     * IMPORTANT
     *
     * A temperature heater usually requires:
     *
     * Fuzzy output:
     *
     *      -1 ~ +1
     *
     * mapped into:
     *
     *      0 ~ 1000
     *
     * Therefore:
     *
     *      -1 -> 0
     *       0 -> 500
     *      +1 -> 1000
     *
     * ---------------------------------------------------------
     */

    ratio =
        (
            normalized + 1.0f
        )
        *
        0.5f;

    output =
        fb->Config.OutputMin +
        (
            ratio *
            (
                fb->Config.OutputMax -
                fb->Config.OutputMin
            )
        );

    output =
        FuzzyDefuzz_Clamp(
            output,
            fb->Config.OutputMin,
            fb->Config.OutputMax
        );

    return output;
}

/* ============================================================================
 * Output Slew
 * ========================================================================== */

float FB_FuzzyDefuzzifier_Slew(
    float current,
    float target,
    float rate,
    float Ts
)
{
    float delta;
    float maxDelta;

    if (Ts <= 0.0f)
    {
        return target;
    }

    if (rate <= 0.0f)
    {
        return target;
    }

    delta =
        target -
        current;

    maxDelta =
        rate *
        Ts;

    if (delta > maxDelta)
    {
        delta = maxDelta;
    }
    else if (delta < -maxDelta)
    {
        delta = -maxDelta;
    }

    return current + delta;
}

/* ============================================================================
 * Main Run
 * ========================================================================== */

float FB_FuzzyDefuzzifier_Run(
    FB_FuzzyDefuzzifier_t *fb,
    float Ku
)
{
    float centroid;
    float rawOutput;
    float scaledOutput;
    float output;

    /*
     * Part 4 itself does not know the actual
     * controller Ts.
     *
     * Since your system is 20 ms:
     *
     * Ts = 0.020 sec
     */
    const float Ts =
        0.020f;

    if (fb == NULL)
    {
        return 0.0f;
    }

    /*
     * ---------------------------------------------------------
     * 1. Centroid
     * ---------------------------------------------------------
     */

    centroid =
        FB_FuzzyDefuzzifier_CalculateCentroid(
            fb
        );

    /*
     * ---------------------------------------------------------
     * 2. Normalized -> PWM
     * ---------------------------------------------------------
     */

    rawOutput =
        FB_FuzzyDefuzzifier_NormalizedToOutput(
            fb,
            centroid
        );

    fb->State.RawOutput =
        rawOutput;

    /*
     * ---------------------------------------------------------
     * 3. Output Scaling
     * ---------------------------------------------------------
     */

    if (fb->Config.OutputScalingEnable)
    {
        /*
         * Ku = 1.0
         *
         * output remains unchanged.
         *
         * Ku < 1:
         *
         * reduce output.
         */
        scaledOutput =
            rawOutput *
            Ku;
    }
    else
    {
        scaledOutput =
            rawOutput;
    }

    scaledOutput =
        FuzzyDefuzz_Clamp(
            scaledOutput,
            fb->Config.OutputMin,
            fb->Config.OutputMax
        );

    fb->State.Ku =
        Ku;

    fb->State.ScaledOutput =
        scaledOutput;

    /*
     * ---------------------------------------------------------
     * 4. Output Slew
     * ---------------------------------------------------------
     */

    if (fb->Config.OutputSlewEnable)
    {
        output =
            FB_FuzzyDefuzzifier_Slew(
                fb->State.Output,
                scaledOutput,
                fb->Config.OutputSlewRate,
                Ts
            );
    }
    else
    {
        output =
            scaledOutput;
    }

    /*
     * Final clamp.
     */
    output =
        FuzzyDefuzz_Clamp(
            output,
            fb->Config.OutputMin,
            fb->Config.OutputMax
        );

    /*
     * ---------------------------------------------------------
     * Store
     * ---------------------------------------------------------
     */

    fb->State.PreviousOutput =
        fb->State.Output;

    fb->State.TargetOutput =
        scaledOutput;

    fb->State.Output =
        output;

    return output;
}

/* ============================================================================
 * Set Configuration
 * ========================================================================== */

bool FB_FuzzyDefuzzifier_SetConfig(
    FB_FuzzyDefuzzifier_t *fb,
    const FuzzyDefuzzifierConfig_t *config
)
{
    uint8_t i;

    if ((fb == NULL) ||
        (config == NULL))
    {
        return false;
    }

    /*
     * Output range validation.
     */
    if (config->OutputMax <=
        config->OutputMin)
    {
        return false;
    }

    /*
     * Centroid points.
     */
    if (config->CentroidPoints < 2U)
    {
        return false;
    }

    if (config->CentroidPoints >
        FUZZY_DEFUZZ_DEFAULT_CENTROID_POINTS)
    {
        return false;
    }

    /*
     * Output slew.
     */
    if (config->OutputSlewRate < 0.0f)
    {
        return false;
    }

    /*
     * Membership validation.
     */
    for (i = 0U;
         i < FUZZY_DEFUZZ_OUTPUT_MF_COUNT;
         i++)
    {
        if (config->MF[i].Peak < 0.0f)
        {
            return false;
        }

        if (config->MF[i].Peak > 1.0f)
        {
            return false;
        }

        if (config->MF[i].Left >
            config->MF[i].Center)
        {
            return false;
        }

        if (config->MF[i].Center >
            config->MF[i].Right)
        {
            return false;
        }
    }

    fb->Config =
        *config;

    return true;
}

/* ============================================================================
 * Get Configuration
 * ========================================================================== */

bool FB_FuzzyDefuzzifier_GetConfig(
    const FB_FuzzyDefuzzifier_t *fb,
    FuzzyDefuzzifierConfig_t *config
)
{
    if ((fb == NULL) ||
        (config == NULL))
    {
        return false;
    }

    *config =
        fb->Config;

    return true;
}

/* ============================================================================
 * Set Ku
 * ========================================================================== */

bool FB_FuzzyDefuzzifier_SetKu(
    FB_FuzzyDefuzzifier_t *fb,
    float ku
)
{
    if (fb == NULL)
    {
        return false;
    }

    if (ku < 0.0f)
    {
        return false;
    }

    fb->State.Ku =
        ku;

    return true;
}

/* ============================================================================
 * Set Output Range
 * ========================================================================== */

bool FB_FuzzyDefuzzifier_SetOutputRange(
    FB_FuzzyDefuzzifier_t *fb,
    float minOutput,
    float maxOutput
)
{
    if (fb == NULL)
    {
        return false;
    }

    if (maxOutput <= minOutput)
    {
        return false;
    }

    fb->Config.OutputMin =
        minOutput;

    fb->Config.OutputMax =
        maxOutput;

    /*
     * Clamp current output.
     */
    fb->State.Output =
        FuzzyDefuzz_Clamp(
            fb->State.Output,
            minOutput,
            maxOutput
        );

    return true;
}