/******************************************************************************
 * File    : FB_FuzzyHybridOutput.c
 * Brief   : Feed-forward + fuzzy transient correction + slow bias trim.
 ******************************************************************************/

#include "FB_FuzzyHybridOutput.h"
#include <float.h>
#include <stddef.h>

static bool isFiniteFloat(float x)
{
    return (x == x) && (x <= FLT_MAX) && (x >= -FLT_MAX);
}

static float clampf_local(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static float slew(float current, float target, float rate, float Ts)
{
    float diff;
    float maxStep;

    if (!isFiniteFloat(current) || !isFiniteFloat(target)) return 0.0f;
    if (!isFiniteFloat(rate) || !isFiniteFloat(Ts) || rate <= 0.0f || Ts <= 0.0f)
        return target;

    maxStep = rate * Ts;
    diff = target - current;
    if (diff > maxStep) diff = maxStep;
    else if (diff < -maxStep) diff = -maxStep;
    return current + diff;
}

void FB_FuzzyHybridOutput_Init(FB_FuzzyHybridOutput_t *fb)
{
    uint8_t i;
    if (fb == NULL) return;

    fb->config.pwmMin = 0.0f;
    fb->config.pwmMax = 1000.0f;
    fb->config.slewRate = 5000.0f;
    fb->config.ffSize = 0U;

    for (i = 0U; i < FUZZY_HYBRID_FF_TABLE_SIZE; ++i)
    {
        fb->config.ffTable[i].temperature = 0.0f;
        fb->config.ffTable[i].pwm = 0.0f;
    }

    /* The current ZE/ZE Sugeno singleton is 100 PWM. */
    fb->config.neutralFuzzyPWM = 100.0f;
    fb->config.positiveCorrectionGain = 0.50f;
    fb->config.negativeCorrectionGain = 5.00f;

    /* Slow steady-state trim. Unit: PWM / (degC * s). */
    fb->config.biasKi = 0.05f;
    fb->config.biasMin = -200.0f;
    fb->config.biasMax = 200.0f;

    /*
     * Real-data tuning for a heater (no active cooling):
     * - Below SV, learn positive holding bias only very close to target.
     * - Above SV, keep learning negative bias through moderate overshoot so
     *   the controller does not freeze with a residual +2..+3 degC offset.
     */
    fb->config.biasPositiveLearningBand_c = 1.0f;
    fb->config.biasNegativeLearningBand_c = 5.0f;

    fb->config.enableFeedForward = true;
    fb->config.enableBiasTrim = true;
    fb->config.enableSlew = true;

    FB_FuzzyHybridOutput_Reset(fb);
}

void FB_FuzzyHybridOutput_Reset(FB_FuzzyHybridOutput_t *fb)
{
    if (fb == NULL) return;
    fb->state.feedForwardPWM = 0.0f;
    fb->state.fuzzyCorrectionPWM = 0.0f;
    fb->state.biasPWM = 0.0f;
    fb->state.targetPWM = 0.0f;
    fb->state.outputPWM = 0.0f;
}

bool FB_FuzzyHybridOutput_SetConfig(
    FB_FuzzyHybridOutput_t *fb,
    const FuzzyHybridOutputConfig_t *config)
{
    uint8_t i;
    if ((fb == NULL) || (config == NULL)) return false;

    if (!isFiniteFloat(config->pwmMin) || !isFiniteFloat(config->pwmMax) ||
        config->pwmMin < 0.0f || config->pwmMax > 1000.0f ||
        config->pwmMin >= config->pwmMax) return false;

    if (!isFiniteFloat(config->slewRate) || config->slewRate < 0.0f) return false;
    if (!isFiniteFloat(config->neutralFuzzyPWM) ||
        config->neutralFuzzyPWM < 0.0f || config->neutralFuzzyPWM > 1000.0f) return false;
    if (!isFiniteFloat(config->positiveCorrectionGain) || config->positiveCorrectionGain < 0.0f) return false;
    if (!isFiniteFloat(config->negativeCorrectionGain) || config->negativeCorrectionGain < 0.0f) return false;
    if (!isFiniteFloat(config->biasKi) || config->biasKi < 0.0f) return false;
    if (!isFiniteFloat(config->biasMin) || !isFiniteFloat(config->biasMax) ||
        config->biasMin > config->biasMax) return false;
    if (!isFiniteFloat(config->biasPositiveLearningBand_c) ||
        config->biasPositiveLearningBand_c < 0.0f) return false;
    if (!isFiniteFloat(config->biasNegativeLearningBand_c) ||
        config->biasNegativeLearningBand_c < 0.0f) return false;
    if (config->ffSize > FUZZY_HYBRID_FF_TABLE_SIZE) return false;

    for (i = 0U; i < config->ffSize; ++i)
    {
        if (!isFiniteFloat(config->ffTable[i].temperature) ||
            !isFiniteFloat(config->ffTable[i].pwm) ||
            config->ffTable[i].pwm < config->pwmMin ||
            config->ffTable[i].pwm > config->pwmMax) return false;
        if ((i > 0U) &&
            (config->ffTable[i].temperature <= config->ffTable[i - 1U].temperature)) return false;
    }

    fb->config = *config;
    fb->state.biasPWM = clampf_local(fb->state.biasPWM, config->biasMin, config->biasMax);
    fb->state.outputPWM = clampf_local(fb->state.outputPWM, config->pwmMin, config->pwmMax);
    return true;
}

float FB_FuzzyHybridOutput_CalcFF(
    const FB_FuzzyHybridOutput_t *fb,
    float sv)
{
    uint8_t i;

    if ((fb == NULL) || !fb->config.enableFeedForward || fb->config.ffSize == 0U)
        return 0.0f;
    if (!isFiniteFloat(sv)) return 0.0f;

    if (sv <= fb->config.ffTable[0].temperature)
        return fb->config.ffTable[0].pwm;

    for (i = 0U; i + 1U < fb->config.ffSize; ++i)
    {
        const float x0 = fb->config.ffTable[i].temperature;
        const float x1 = fb->config.ffTable[i + 1U].temperature;
        if (sv <= x1)
        {
            const float t = (sv - x0) / (x1 - x0);
            return fb->config.ffTable[i].pwm +
                   t * (fb->config.ffTable[i + 1U].pwm - fb->config.ffTable[i].pwm);
        }
    }

    return fb->config.ffTable[fb->config.ffSize - 1U].pwm;
}

float FB_FuzzyHybridOutput_Run(
    FB_FuzzyHybridOutput_t *fb,
    float sv,
    float pv,
    float fuzzyPWM,
    float Ts)
{
    float error;
    float delta;
    float correction;
    float ff;
    float candidateBias;
    float candidateTarget;
    bool blockIntegration = false;
    bool allowBiasLearning = false;

    if (fb == NULL) return 0.0f;

    if (!isFiniteFloat(sv) || !isFiniteFloat(pv) || !isFiniteFloat(fuzzyPWM))
    {
        fb->state.outputPWM = fb->config.pwmMin;
        fb->state.targetPWM = fb->config.pwmMin;
        return fb->state.outputPWM;
    }

    error = sv - pv;
    ff = FB_FuzzyHybridOutput_CalcFF(fb, sv);
    ff = clampf_local(ff, fb->config.pwmMin, fb->config.pwmMax);

    delta = fuzzyPWM - fb->config.neutralFuzzyPWM;
    if (delta >= 0.0f)
        correction = delta * fb->config.positiveCorrectionGain;
    else
        correction = delta * fb->config.negativeCorrectionGain;

    fb->state.feedForwardPWM = ff;
    fb->state.fuzzyCorrectionPWM = correction;

    candidateBias = fb->state.biasPWM;

    if (fb->config.enableBiasTrim && isFiniteFloat(Ts) && (Ts > 0.0f))
    {
        if (error >= 0.0f)
        {
            allowBiasLearning =
                (error <= fb->config.biasPositiveLearningBand_c);
        }
        else
        {
            allowBiasLearning =
                ((-error) <= fb->config.biasNegativeLearningBand_c);
        }
    }

    if (allowBiasLearning)
    {
        candidateBias += fb->config.biasKi * error * Ts;
        candidateBias = clampf_local(candidateBias,
                                     fb->config.biasMin,
                                     fb->config.biasMax);
    }

    candidateTarget = ff + correction + candidateBias;

    /* Conditional anti-windup: do not integrate farther into saturation. */
    if (allowBiasLearning)
    {
        if ((candidateTarget > fb->config.pwmMax) && (error > 0.0f))
            blockIntegration = true;
        else if ((candidateTarget < fb->config.pwmMin) && (error < 0.0f))
            blockIntegration = true;
    }

    if (allowBiasLearning && !blockIntegration)
        fb->state.biasPWM = candidateBias;

    fb->state.targetPWM = ff + correction + fb->state.biasPWM;
    fb->state.targetPWM = clampf_local(fb->state.targetPWM,
                                       fb->config.pwmMin,
                                       fb->config.pwmMax);

    if (fb->config.enableSlew)
        fb->state.outputPWM = slew(fb->state.outputPWM,
                                   fb->state.targetPWM,
                                   fb->config.slewRate,
                                   Ts);
    else
        fb->state.outputPWM = fb->state.targetPWM;

    fb->state.outputPWM = clampf_local(fb->state.outputPWM,
                                       fb->config.pwmMin,
                                       fb->config.pwmMax);
    return fb->state.outputPWM;
}
