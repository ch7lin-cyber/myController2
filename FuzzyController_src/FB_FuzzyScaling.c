/******************************************************************************
 * File    : FB_FuzzyScaling.c
 * Version : V2.0
 *
 * Brief   : Auto / Adaptive Scaling Engine
 ******************************************************************************/

#include "FB_FuzzyScaling.h"

#include <stddef.h>

/* ============================================================================
 * Internal Constants
 * ========================================================================== */

#define FUZZY_SCALING_EPSILON       (0.000001f)

/*
 * Error Window strategy:
 *
 * Large error:
 *     Larger window
 *
 * Small error:
 *     Smaller window
 *
 * This ratio controls how fast the window
 * changes with the absolute error.
 */
#define FUZZY_SCALING_ERROR_RATIO    (1.50f)

/*
 * Minimum practical error used by adaptive scaling.
 */
#define FUZZY_SCALING_MIN_ERROR_REF  (2.0f)

/*
 * dError gain relation.
 */
#define FUZZY_SCALING_KDE_RATIO       (0.10f)

/*
 * Ku reduction when PV is moving rapidly.
 */
#define FUZZY_SCALING_DYNAMIC_KU_GAIN (0.50f)

/* ============================================================================
 * Internal Utility
 * ========================================================================== */

static float FuzzyScaling_Abs(
    float x
)
{
    return (x >= 0.0f) ? x : -x;
}

/* -------------------------------------------------------------------------- */

static float FuzzyScaling_Min(
    float a,
    float b
)
{
    return (a < b) ? a : b;
}

/* -------------------------------------------------------------------------- */

static float FuzzyScaling_Max(
    float a,
    float b
)
{
    return (a > b) ? a : b;
}

/* -------------------------------------------------------------------------- */

static float FuzzyScaling_Clamp(
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
 * Initialization
 * ========================================================================== */

void FB_FuzzyScaling_Init(
    FB_FuzzyScaling_t *fb
)
{
    if (fb == NULL)
    {
        return;
    }

    /*
     * ---------------------------------------------------------
     * Configuration
     * ---------------------------------------------------------
     */

    fb->Config.Ts =
        FUZZY_SCALING_DEFAULT_TS;

    fb->Config.MinTemperature =
        FUZZY_SCALING_DEFAULT_MIN_TEMP;

    fb->Config.MaxTemperature =
        FUZZY_SCALING_DEFAULT_MAX_TEMP;

    fb->Config.BaseErrorWindow =
        FUZZY_SCALING_DEFAULT_ERROR_WINDOW;

    fb->Config.MinErrorWindow =
        FUZZY_SCALING_MIN_ERROR_WINDOW;

    fb->Config.MaxErrorWindow =
        FUZZY_SCALING_MAX_ERROR_WINDOW;

    fb->Config.MinKe =
        FUZZY_SCALING_MIN_KE;

    fb->Config.MaxKe =
        FUZZY_SCALING_MAX_KE;

    fb->Config.MinKde =
        FUZZY_SCALING_MIN_KDE;

    fb->Config.MaxKde =
        FUZZY_SCALING_MAX_KDE;

    fb->Config.MinKu =
        FUZZY_SCALING_MIN_KU;

    fb->Config.MaxKu =
        FUZZY_SCALING_MAX_KU;

    fb->Config.DynamicGain =
        FUZZY_SCALING_DEFAULT_DYNAMIC_GAIN;

    fb->Config.MaxPVRate =
        FUZZY_SCALING_DEFAULT_MAX_PV_RATE;

    fb->Config.KuSlewRate =
        FUZZY_SCALING_DEFAULT_KU_SLEW_RATE;

    fb->Config.AutoScalingEnable =
        true;

    fb->Config.AdaptiveEnable =
        true;

    /*
     * ---------------------------------------------------------
     * Runtime state
     * ---------------------------------------------------------
     */

    fb->State.Ke = 0.05f;

    fb->State.Kde = 0.10f;

    fb->State.Ku = 1.00f;

    fb->State.TargetKe =
        fb->State.Ke;

    fb->State.TargetKde =
        fb->State.Kde;

    fb->State.TargetKu =
        fb->State.Ku;

    fb->State.ErrorWindow =
        fb->Config.BaseErrorWindow;

    fb->State.TargetErrorWindow =
        fb->State.ErrorWindow;

    fb->State.Error = 0.0f;

    fb->State.PreviousError = 0.0f;

    fb->State.dError = 0.0f;

    fb->State.PV = 0.0f;

    fb->State.PreviousPV = 0.0f;

    fb->State.PVRate = 0.0f;

    fb->State.DynamicFactor = 1.0f;

    fb->State.NormalizedError = 0.0f;

    fb->State.NormalizedDError = 0.0f;

    fb->State.Initialized = true;
}

/* ============================================================================
 * Reset
 * ========================================================================== */

void FB_FuzzyScaling_Reset(
    FB_FuzzyScaling_t *fb
)
{
    if (fb == NULL)
    {
        return;
    }

    fb->State.Error = 0.0f;

    fb->State.PreviousError = 0.0f;

    fb->State.dError = 0.0f;

    fb->State.PV = 0.0f;

    fb->State.PreviousPV = 0.0f;

    fb->State.PVRate = 0.0f;

    fb->State.DynamicFactor = 1.0f;

    fb->State.NormalizedError = 0.0f;

    fb->State.NormalizedDError = 0.0f;

    fb->State.ErrorWindow =
        fb->Config.BaseErrorWindow;

    fb->State.TargetErrorWindow =
        fb->State.ErrorWindow;

    fb->State.Ke = 0.05f;

    fb->State.Kde = 0.10f;

    fb->State.Ku = 1.0f;

    fb->State.TargetKe =
        fb->State.Ke;

    fb->State.TargetKde =
        fb->State.Kde;

    fb->State.TargetKu =
        fb->State.Ku;
}

/* ============================================================================
 * Error Window Calculation
 * ========================================================================== */

float FB_FuzzyScaling_CalculateErrorWindow(
    FB_FuzzyScaling_t *fb,
    float sv,
    float pv
)
{
    float error;
    float absoluteError;
    float window;

    if (fb == NULL)
    {
        return FUZZY_SCALING_DEFAULT_ERROR_WINDOW;
    }

    error = sv - pv;

    absoluteError =
        FuzzyScaling_Abs(error);

    /*
     * ---------------------------------------------------------
     * Adaptive Error Window
     * ---------------------------------------------------------
     *
     * Base example:
     *
     * BaseWindow = 20°C
     *
     * Error = 5°C
     *
     * Window ≈ 20°C
     *
     * Error = 50°C
     *
     * Window becomes larger.
     *
     */

    window =
        fb->Config.BaseErrorWindow;

    if (fb->Config.AdaptiveEnable)
    {
        if (absoluteError >
            FUZZY_SCALING_MIN_ERROR_REF)
        {
            float adaptiveWindow;

            adaptiveWindow =
                absoluteError *
                FUZZY_SCALING_ERROR_RATIO;

            /*
             * Do not allow the adaptive
             * window to become too small.
             */
            adaptiveWindow =
                FuzzyScaling_Max(
                    adaptiveWindow,
                    fb->Config.BaseErrorWindow
                );

            window = adaptiveWindow;
        }
    }

    /*
     * Clamp Error Window.
     */
    window =
        FuzzyScaling_Clamp(
            window,
            fb->Config.MinErrorWindow,
            fb->Config.MaxErrorWindow
        );

    fb->State.TargetErrorWindow =
        window;

    return window;
}

/* ============================================================================
 * Dynamic Factor
 *
 * DynamicFactor:
 *
 * Slow PV movement:
 *
 *      Factor > 1
 *
 * Fast PV movement:
 *
 *      Factor < 1
 *
 * This is mainly used to reduce control aggressiveness
 * when the thermal system is moving rapidly.
 * ========================================================================== */

float FB_FuzzyScaling_CalculateDynamicFactor(
    FB_FuzzyScaling_t *fb,
    float pvRate
)
{
    float normalizedRate;
    float factor;

    if (fb == NULL)
    {
        return 1.0f;
    }

    /*
     * Normalize PV rate.
     *
     * Example:
     *
     * MaxPVRate = 20 degC/s
     *
     * PVRate = 10 degC/s
     *
     * normalizedRate = 0.5
     */
    if (fb->Config.MaxPVRate >
        FUZZY_SCALING_EPSILON)
    {
        normalizedRate =
            FuzzyScaling_Abs(pvRate) /
            fb->Config.MaxPVRate;
    }
    else
    {
        normalizedRate = 0.0f;
    }

    normalizedRate =
        FuzzyScaling_Clamp(
            normalizedRate,
            0.0f,
            1.0f
        );

    /*
     * Dynamic factor.
     *
     * At zero speed:
     *
     *     Factor ≈ 1.0
     *
     * At maximum speed:
     *
     *     Factor ≈ 1.0 - DynamicGain
     *
     * This keeps the adaptation conservative.
     */
    factor =
        1.0f -
        (
            normalizedRate *
            fb->Config.DynamicGain
        );

    /*
     * Additional dynamic reduction.
     *
     * This becomes useful when PV is moving
     * very quickly.
     */
    if (normalizedRate > 0.75f)
    {
        factor -=
            normalizedRate *
            FUZZY_SCALING_DYNAMIC_KU_GAIN;
    }

    factor =
        FuzzyScaling_Clamp(
            factor,
            FUZZY_SCALING_MIN_DYNAMIC_FACTOR,
            FUZZY_SCALING_MAX_DYNAMIC_FACTOR
        );

    fb->State.DynamicFactor =
        factor;

    return factor;
}

/* ============================================================================
 * Calculate Ke
 * ========================================================================== */

float FB_FuzzyScaling_CalculateKe(
    FB_FuzzyScaling_t *fb
)
{
    float ke;

    if (fb == NULL)
    {
        return 0.05f;
    }

    if (fb->State.ErrorWindow >
        FUZZY_SCALING_EPSILON)
    {
        /*
         * Normalization:
         *
         * Error × Ke
         *
         * Ke = 1 / ErrorWindow
         */
        ke =
            1.0f /
            fb->State.ErrorWindow;
    }
    else
    {
        ke = fb->Config.MaxKe;
    }

    /*
     * Adaptive dynamic factor.
     *
     * Fast thermal movement:
     *
     *     reduce Ke
     *
     * Slow movement:
     *
     *     keep Ke higher.
     */
    if (fb->Config.AdaptiveEnable)
    {
        ke *=
            fb->State.DynamicFactor;
    }

    ke =
        FuzzyScaling_Clamp(
            ke,
            fb->Config.MinKe,
            fb->Config.MaxKe
        );

    fb->State.TargetKe = ke;

    return ke;
}

/* ============================================================================
 * Calculate Kde
 * ========================================================================== */

float FB_FuzzyScaling_CalculateKde(
    FB_FuzzyScaling_t *fb
)
{
    float kde;

    if (fb == NULL)
    {
        return 0.10f;
    }

    /*
     * dError is expressed in degC/sec.
     *
     * We relate Kde to the Error Window.
     *
     * Example:
     *
     * ErrorWindow = 20°C
     *
     * Kde = 1 / (20 × 0.10)
     *
     * = 0.5
     *
     * Then the configured limits prevent
     * excessive dError sensitivity.
     */
    kde =
        1.0f /
        (
            fb->State.ErrorWindow *
            FUZZY_SCALING_KDE_RATIO
        );

    /*
     * Dynamic factor.
     */
    if (fb->Config.AdaptiveEnable)
    {
        kde *=
            fb->State.DynamicFactor;
    }

    kde =
        FuzzyScaling_Clamp(
            kde,
            fb->Config.MinKde,
            fb->Config.MaxKde
        );

    fb->State.TargetKde = kde;

    return kde;
}

/* ============================================================================
 * Calculate Ku
 * ========================================================================== */

float FB_FuzzyScaling_CalculateKu(
    FB_FuzzyScaling_t *fb
)
{
    float ku;

    if (fb == NULL)
    {
        return 1.0f;
    }

    ku = 1.0f;

    /*
     * Fast thermal movement:
     *
     * Reduce output gain.
     */
    if (fb->Config.AdaptiveEnable)
    {
        ku *=
            fb->State.DynamicFactor;
    }

    ku =
        FuzzyScaling_Clamp(
            ku,
            fb->Config.MinKu,
            fb->Config.MaxKu
        );

    fb->State.TargetKu = ku;

    return ku;
}

/* ============================================================================
 * Slew Rate
 * ========================================================================== */

float FB_FuzzyScaling_Slew(
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
 * Normalize Error
 * ========================================================================== */

float FB_FuzzyScaling_NormalizeError(
    float error,
    float ke
)
{
    float normalized;

    normalized =
        error *
        ke;

    /*
     * Membership Engine expects:
     *
     * -1.0 ~ +1.0
     */
    normalized =
        FuzzyScaling_Clamp(
            normalized,
            -1.0f,
            1.0f
        );

    return normalized;
}

/* ============================================================================
 * Normalize dError
 * ========================================================================== */

float FB_FuzzyScaling_NormalizeDError(
    float dError,
    float kde
)
{
    float normalized;

    normalized =
        dError *
        kde;

    normalized =
        FuzzyScaling_Clamp(
            normalized,
            -1.0f,
            1.0f
        );

    return normalized;
}

/* ============================================================================
 * Main Run
 * ========================================================================== */

void FB_FuzzyScaling_Run(
    FB_FuzzyScaling_t *fb,
    float sv,
    float pv
)
{
    float error;
    float pvRate;

    if (fb == NULL)
    {
        return;
    }

    if (!fb->State.Initialized)
    {
        FB_FuzzyScaling_Init(fb);
    }

    /*
     * ---------------------------------------------------------
     * Store input
     * ---------------------------------------------------------
     */

    fb->State.PV = pv;

    /*
     * ---------------------------------------------------------
     * Calculate Error
     * ---------------------------------------------------------
     */

    error =
        sv - pv;

    fb->State.Error =
        error;

    /*
     * ---------------------------------------------------------
     * Calculate dError
     * ---------------------------------------------------------
     *
     * dError:
     *
     *     Error(k) - Error(k-1)
     *     ---------------------
     *             Ts
     *
     */
    if (fb->Config.Ts >
        FUZZY_SCALING_EPSILON)
    {
        fb->State.dError =
            (
                fb->State.Error -
                fb->State.PreviousError
            )
            /
            fb->Config.Ts;
    }
    else
    {
        fb->State.dError = 0.0f;
    }

    /*
     * ---------------------------------------------------------
     * Calculate PV Rate
     * ---------------------------------------------------------
     *
     * PVRate:
     *
     *     PV(k) - PV(k-1)
     *     ----------------
     *           Ts
     *
     */
    if (fb->Config.Ts >
        FUZZY_SCALING_EPSILON)
    {
        pvRate =
            (
                pv -
                fb->State.PreviousPV
            )
            /
            fb->Config.Ts;
    }
    else
    {
        pvRate = 0.0f;
    }

    fb->State.PVRate =
        pvRate;

    /*
     * ---------------------------------------------------------
     * Automatic Scaling
     * ---------------------------------------------------------
     */

    if (fb->Config.AutoScalingEnable)
    {
        /*
         * Error Window.
         */
        FB_FuzzyScaling_CalculateErrorWindow(
            fb,
            sv,
            pv
        );

        /*
         * Dynamic Factor.
         */
        if (fb->Config.AdaptiveEnable)
        {
            FB_FuzzyScaling_CalculateDynamicFactor(
                fb,
                pvRate
            );
        }
        else
        {
            fb->State.DynamicFactor =
                1.0f;
        }

        /*
         * Calculate target gains.
         */
        FB_FuzzyScaling_CalculateKe(fb);

        FB_FuzzyScaling_CalculateKde(fb);

        FB_FuzzyScaling_CalculateKu(fb);
    }

    /*
     * ---------------------------------------------------------
     * Gain Slew
     * ---------------------------------------------------------
     *
     * Ke and Kde are changed smoothly.
     */
    fb->State.Ke =
        FB_FuzzyScaling_Slew(
            fb->State.Ke,
            fb->State.TargetKe,
            5.0f,
            fb->Config.Ts
        );

    fb->State.Kde =
        FB_FuzzyScaling_Slew(
            fb->State.Kde,
            fb->State.TargetKde,
            5.0f,
            fb->Config.Ts
        );

    /*
     * Ku has its own configurable slew rate.
     */
    fb->State.Ku =
        FB_FuzzyScaling_Slew(
            fb->State.Ku,
            fb->State.TargetKu,
            fb->Config.KuSlewRate,
            fb->Config.Ts
        );

    /*
     * ---------------------------------------------------------
     * Normalize
     * ---------------------------------------------------------
     */

    fb->State.NormalizedError =
        FB_FuzzyScaling_NormalizeError(
            fb->State.Error,
            fb->State.Ke
        );

    fb->State.NormalizedDError =
        FB_FuzzyScaling_NormalizeDError(
            fb->State.dError,
            fb->State.Kde
        );

    /*
     * ---------------------------------------------------------
     * Update previous values
     * ---------------------------------------------------------
     */

    fb->State.PreviousError =
        fb->State.Error;

    fb->State.PreviousPV =
        fb->State.PV;
}

/* ============================================================================
 * Set Configuration
 * ========================================================================== */

bool FB_FuzzyScaling_SetConfig(
    FB_FuzzyScaling_t *fb,
    const FuzzyScalingConfig_t *config
)
{
    if ((fb == NULL) ||
        (config == NULL))
    {
        return false;
    }

    /*
     * Basic validation.
     */
    if (config->Ts <= 0.0f)
    {
        return false;
    }

    if (config->MinTemperature >=
        config->MaxTemperature)
    {
        return false;
    }

    if (config->MinErrorWindow <= 0.0f)
    {
        return false;
    }

    if (config->MaxErrorWindow <
        config->MinErrorWindow)
    {
        return false;
    }

    if (config->MinKe <= 0.0f)
    {
        return false;
    }

    if (config->MaxKe <
        config->MinKe)
    {
        return false;
    }

    if (config->MinKde <= 0.0f)
    {
        return false;
    }

    if (config->MaxKde <
        config->MinKde)
    {
        return false;
    }

    if (config->MinKu <= 0.0f)
    {
        return false;
    }

    if (config->MaxKu <
        config->MinKu)
    {
        return false;
    }

    /*
     * Copy configuration.
     */
    fb->Config = *config;

    /*
     * Clamp current runtime values
     * according to new configuration.
     */
    fb->State.Ke =
        FuzzyScaling_Clamp(
            fb->State.Ke,
            fb->Config.MinKe,
            fb->Config.MaxKe
        );

    fb->State.Kde =
        FuzzyScaling_Clamp(
            fb->State.Kde,
            fb->Config.MinKde,
            fb->Config.MaxKde
        );

    fb->State.Ku =
        FuzzyScaling_Clamp(
            fb->State.Ku,
            fb->Config.MinKu,
            fb->Config.MaxKu
        );

    return true;
}

/* ============================================================================
 * Get Configuration
 * ========================================================================== */

bool FB_FuzzyScaling_GetConfig(
    const FB_FuzzyScaling_t *fb,
    FuzzyScalingConfig_t *config
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
 * Set Ke
 * ========================================================================== */

bool FB_FuzzyScaling_SetKe(
    FB_FuzzyScaling_t *fb,
    float ke
)
{
    if (fb == NULL)
    {
        return false;
    }

    if ((ke < fb->Config.MinKe) ||
        (ke > fb->Config.MaxKe))
    {
        return false;
    }

    fb->State.TargetKe =
        ke;

    /*
     * Manual setting disables
     * automatic Ke calculation only
     * for the current target.
     */
    return true;
}

/* ============================================================================
 * Set Kde
 * ========================================================================== */

bool FB_FuzzyScaling_SetKde(
    FB_FuzzyScaling_t *fb,
    float kde
)
{
    if (fb == NULL)
    {
        return false;
    }

    if ((kde < fb->Config.MinKde) ||
        (kde > fb->Config.MaxKde))
    {
        return false;
    }

    fb->State.TargetKde =
        kde;

    return true;
}

/* ============================================================================
 * Set Ku
 * ========================================================================== */

bool FB_FuzzyScaling_SetKu(
    FB_FuzzyScaling_t *fb,
    float ku
)
{
    if (fb == NULL)
    {
        return false;
    }

    if ((ku < fb->Config.MinKu) ||
        (ku > fb->Config.MaxKu))
    {
        return false;
    }

    fb->State.TargetKu =
        ku;

    return true;
}

/* ============================================================================
 * Set Error Window
 * ========================================================================== */

bool FB_FuzzyScaling_SetErrorWindow(
    FB_FuzzyScaling_t *fb,
    float window
)
{
    if (fb == NULL)
    {
        return false;
    }

    if ((window <
         fb->Config.MinErrorWindow) ||
        (window >
         fb->Config.MaxErrorWindow))
    {
        return false;
    }

    fb->State.TargetErrorWindow =
        window;

    fb->State.ErrorWindow =
        window;

    return true;
}

/* ============================================================================
 * Enable Auto Scaling
 * ========================================================================== */

void FB_FuzzyScaling_EnableAuto(
    FB_FuzzyScaling_t *fb
)
{
    if (fb == NULL)
    {
        return;
    }

    fb->Config.AutoScalingEnable =
        true;
}

/* ============================================================================
 * Disable Auto Scaling
 * ========================================================================== */

void FB_FuzzyScaling_DisableAuto(
    FB_FuzzyScaling_t *fb
)
{
    if (fb == NULL)
    {
        return;
    }

    fb->Config.AutoScalingEnable =
        false;
}

/* ============================================================================
 * Enable Adaptive Scaling
 * ========================================================================== */

void FB_FuzzyScaling_EnableAdaptive(
    FB_FuzzyScaling_t *fb
)
{
    if (fb == NULL)
    {
        return;
    }

    fb->Config.AdaptiveEnable =
        true;
}

/* ============================================================================
 * Disable Adaptive Scaling
 * ========================================================================== */

void FB_FuzzyScaling_DisableAdaptive(
    FB_FuzzyScaling_t *fb
)
{
    if (fb == NULL)
    {
        return;
    }

    fb->Config.AdaptiveEnable =
        false;

    fb->State.DynamicFactor =
        1.0f;
}