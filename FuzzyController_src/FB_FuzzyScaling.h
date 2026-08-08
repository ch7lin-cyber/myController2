/******************************************************************************
 * File    : FB_FuzzyScaling.h
 * Version : V2.0
 *
 * Brief   : Auto / Adaptive Scaling Engine
 *
 * Target:
 *   - Temperature control
 *   - 50 ~ 300 degC
 *   - 50 Hz controller
 *   - Ts = 20 ms
 *   - PWM = 0 ~ 1000
 *
 * Features:
 *   - Automatic Error Scaling
 *   - Adaptive Error Scaling
 *   - Adaptive dError Scaling
 *   - Output Scaling
 *   - PV Rate estimation
 *   - Dynamic Factor
 *   - Error Window adaptation
 *   - Gain slew rate limiting
 *   - Runtime configuration
 *
 ******************************************************************************/

#ifndef FB_FUZZY_SCALING_H
#define FB_FUZZY_SCALING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * Default Configuration
 * ========================================================================== */

#define FUZZY_SCALING_DEFAULT_TS              (0.020f)

/*
 * Temperature operating range.
 */
#define FUZZY_SCALING_DEFAULT_MIN_TEMP        (50.0f)
#define FUZZY_SCALING_DEFAULT_MAX_TEMP        (300.0f)

/*
 * Default Error Window.
 */
#define FUZZY_SCALING_DEFAULT_ERROR_WINDOW    (20.0f)

/*
 * Minimum / Maximum Error Window.
 */
#define FUZZY_SCALING_MIN_ERROR_WINDOW        (2.0f)
#define FUZZY_SCALING_MAX_ERROR_WINDOW        (100.0f)

/*
 * Scaling gain limits.
 */
#define FUZZY_SCALING_MIN_KE                  (0.005f)
#define FUZZY_SCALING_MAX_KE                  (0.500f)

#define FUZZY_SCALING_MIN_KDE                 (0.001f)
#define FUZZY_SCALING_MAX_KDE                 (0.500f)

#define FUZZY_SCALING_MIN_KU                  (0.10f)
#define FUZZY_SCALING_MAX_KU                  (1.50f)

/*
 * Dynamic factor limits.
 */
#define FUZZY_SCALING_MIN_DYNAMIC_FACTOR      (0.50f)
#define FUZZY_SCALING_MAX_DYNAMIC_FACTOR      (1.50f)

/*
 * Default adaptive gain.
 */
#define FUZZY_SCALING_DEFAULT_DYNAMIC_GAIN    (0.020f)

/*
 * Maximum expected PV rate.
 *
 * Unit:
 *   degC / sec
 */
#define FUZZY_SCALING_DEFAULT_MAX_PV_RATE     (20.0f)

/*
 * Ku slew rate.
 *
 * Unit:
 *   Ku / second
 */
#define FUZZY_SCALING_DEFAULT_KU_SLEW_RATE    (2.0f)

/* ============================================================================
 * Scaling Configuration
 * ========================================================================== */

typedef struct
{
    /*
     * Controller sampling time.
     */
    float Ts;

    /*
     * Temperature range.
     */
    float MinTemperature;
    float MaxTemperature;

    /*
     * Base Error Window.
     */
    float BaseErrorWindow;

    /*
     * Minimum / Maximum Error Window.
     */
    float MinErrorWindow;
    float MaxErrorWindow;

    /*
     * Ke limits.
     */
    float MinKe;
    float MaxKe;

    /*
     * Kde limits.
     */
    float MinKde;
    float MaxKde;

    /*
     * Ku limits.
     */
    float MinKu;
    float MaxKu;

    /*
     * Dynamic adaptation gain.
     */
    float DynamicGain;

    /*
     * Maximum expected PV rate.
     *
     * degC / sec
     */
    float MaxPVRate;

    /*
     * Ku slew rate.
     *
     * Ku / second
     */
    float KuSlewRate;

    /*
     * Enable automatic scaling.
     */
    bool AutoScalingEnable;

    /*
     * Enable dynamic adaptation.
     */
    bool AdaptiveEnable;

} FuzzyScalingConfig_t;

/* ============================================================================
 * Scaling Runtime State
 * ========================================================================== */

typedef struct
{
    /*
     * Current scaling gains.
     */
    float Ke;
    float Kde;
    float Ku;

    /*
     * Target gains.
     *
     * Slew rate is used to move current
     * gain toward target gain.
     */
    float TargetKe;
    float TargetKde;
    float TargetKu;

    /*
     * Error window.
     */
    float ErrorWindow;

    /*
     * Target Error Window.
     */
    float TargetErrorWindow;

    /*
     * Current error.
     */
    float Error;

    /*
     * Previous error.
     */
    float PreviousError;

    /*
     * Error derivative.
     *
     * Unit:
     *   degC / sec
     */
    float dError;

    /*
     * Current PV.
     */
    float PV;

    /*
     * Previous PV.
     */
    float PreviousPV;

    /*
     * PV rate.
     *
     * Unit:
     *   degC / sec
     */
    float PVRate;

    /*
     * Adaptive factor.
     */
    float DynamicFactor;

    /*
     * Normalized values.
     */
    float NormalizedError;
    float NormalizedDError;

    /*
     * Initialization state.
     */
    bool Initialized;

} FuzzyScalingState_t;

/* ============================================================================
 * Function Block
 * ========================================================================== */

typedef struct
{
    FuzzyScalingConfig_t Config;

    FuzzyScalingState_t State;

} FB_FuzzyScaling_t;

/* ============================================================================
 * Initialization
 * ========================================================================== */

/**
 * @brief Initialize Scaling Engine.
 */
void FB_FuzzyScaling_Init(
    FB_FuzzyScaling_t *fb
);

/**
 * @brief Reset Scaling Engine.
 */
void FB_FuzzyScaling_Reset(
    FB_FuzzyScaling_t *fb
);

/* ============================================================================
 * Main Execution
 * ========================================================================== */

/**
 * @brief Execute Adaptive Scaling.
 *
 * @param fb   Scaling Function Block.
 * @param sv   Set Value.
 * @param pv   Process Value.
 */
void FB_FuzzyScaling_Run(
    FB_FuzzyScaling_t *fb,
    float sv,
    float pv
);

/* ============================================================================
 * Manual / Automatic Scaling
 * ========================================================================== */

/**
 * @brief Calculate automatic Error Window.
 */
float FB_FuzzyScaling_CalculateErrorWindow(
    FB_FuzzyScaling_t *fb,
    float sv,
    float pv
);

/**
 * @brief Calculate Dynamic Factor.
 */
float FB_FuzzyScaling_CalculateDynamicFactor(
    FB_FuzzyScaling_t *fb,
    float pvRate
);

/* ============================================================================
 * Gain Calculation
 * ========================================================================== */

/**
 * @brief Calculate target Ke.
 */
float FB_FuzzyScaling_CalculateKe(
    FB_FuzzyScaling_t *fb
);

/**
 * @brief Calculate target Kde.
 */
float FB_FuzzyScaling_CalculateKde(
    FB_FuzzyScaling_t *fb
);

/**
 * @brief Calculate target Ku.
 */
float FB_FuzzyScaling_CalculateKu(
    FB_FuzzyScaling_t *fb
);

/* ============================================================================
 * Slew Rate
 * ========================================================================== */

/**
 * @brief Move current gain toward target gain.
 */
float FB_FuzzyScaling_Slew(
    float current,
    float target,
    float rate,
    float Ts
);

/* ============================================================================
 * Normalization
 * ========================================================================== */

/**
 * @brief Normalize Error.
 */
float FB_FuzzyScaling_NormalizeError(
    float error,
    float ke
);

/**
 * @brief Normalize dError.
 */
float FB_FuzzyScaling_NormalizeDError(
    float dError,
    float kde
);

/* ============================================================================
 * Configuration
 * ========================================================================== */

/**
 * @brief Set complete configuration.
 */
bool FB_FuzzyScaling_SetConfig(
    FB_FuzzyScaling_t *fb,
    const FuzzyScalingConfig_t *config
);

/**
 * @brief Get complete configuration.
 */
bool FB_FuzzyScaling_GetConfig(
    const FB_FuzzyScaling_t *fb,
    FuzzyScalingConfig_t *config
);

/* ============================================================================
 * Individual Parameter
 * ========================================================================== */

bool FB_FuzzyScaling_SetKe(
    FB_FuzzyScaling_t *fb,
    float ke
);

bool FB_FuzzyScaling_SetKde(
    FB_FuzzyScaling_t *fb,
    float kde
);

bool FB_FuzzyScaling_SetKu(
    FB_FuzzyScaling_t *fb,
    float ku
);

bool FB_FuzzyScaling_SetErrorWindow(
    FB_FuzzyScaling_t *fb,
    float window
);

/* ============================================================================
 * Enable / Disable
 * ========================================================================== */

void FB_FuzzyScaling_EnableAuto(
    FB_FuzzyScaling_t *fb
);

void FB_FuzzyScaling_DisableAuto(
    FB_FuzzyScaling_t *fb
);

void FB_FuzzyScaling_EnableAdaptive(
    FB_FuzzyScaling_t *fb
);

void FB_FuzzyScaling_DisableAdaptive(
    FB_FuzzyScaling_t *fb
);

#ifdef __cplusplus
}
#endif

#endif /* FB_FUZZY_SCALING_H */