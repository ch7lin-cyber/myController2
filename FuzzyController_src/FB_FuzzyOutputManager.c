#include "FB_FuzzyOutputManager.h"

#include <stddef.h>

static float clamp(float x, float minValue, float maxValue)
{
    if (x < minValue) return minValue;
    if (x > maxValue) return maxValue;
    return x;
}

static bool isFiniteFloat(float x)
{
    return (x == x) && (x < 3.402823466e+38F) && (x > -3.402823466e+38F);
}

void FB_FuzzyOutput_Init(FB_FuzzyOutputManager_t *fb)
{
    uint8_t i;

    if (fb == NULL) return;

    fb->config.fuzzyScale = 300.0f;
    fb->config.pwmMin = FUZZY_PWM_MIN;
    fb->config.pwmMax = FUZZY_PWM_MAX;
    fb->config.slewRate = 5000.0f;
    fb->config.ffSize = 0U;
    fb->config.enableFeedForward = false;
    fb->config.enableSlew = true;
    fb->config.ffBlend = 0.0f;

    for (i = 0U; i < FUZZY_FF_TABLE_SIZE; ++i)
    {
        fb->config.ffTable[i].temperature = 0.0f;
        fb->config.ffTable[i].pwm = 0.0f;
    }

    fb->state.pwmFF = 0.0f;
    fb->state.fuzzyCorrection = 0.0f;
    fb->state.targetPWM = 0.0f;
    fb->state.outputPWM = 0.0f;
    fb->state.previousPWM = 0.0f;
}

float FB_FuzzyOutput_CalcFF(
        FB_FuzzyOutputManager_t *fb,
        float temperature)
{
    if (fb == NULL || !fb->config.enableFeedForward || fb->config.ffSize == 0U)
        return 0.0f;

    return FB_FuzzyOutput_Interpolation(
        fb->config.ffTable,
        fb->config.ffSize,
        temperature);
}

float FB_FuzzyOutput_Interpolation(
        const FuzzyFFPoint_t *table,
        uint8_t size,
        float x)
{
    uint8_t i;

    if (table == NULL || size == 0U || size > FUZZY_FF_TABLE_SIZE)
        return 0.0f;

    if (!isFiniteFloat(x))
        return clamp(table[0].pwm, FUZZY_PWM_MIN, FUZZY_PWM_MAX);

    if (x <= table[0].temperature)
        return clamp(table[0].pwm, FUZZY_PWM_MIN, FUZZY_PWM_MAX);

    for (i = 0U; i + 1U < size; ++i)
    {
        if (x <= table[i + 1U].temperature)
        {
            float dx = table[i + 1U].temperature - table[i].temperature;

            if (dx <= 0.0f)
                return clamp(table[i].pwm, FUZZY_PWM_MIN, FUZZY_PWM_MAX);

            return clamp(
                table[i].pwm +
                ((x - table[i].temperature) / dx) *
                (table[i + 1U].pwm - table[i].pwm),
                FUZZY_PWM_MIN,
                FUZZY_PWM_MAX);
        }
    }

    return clamp(table[size - 1U].pwm, FUZZY_PWM_MIN, FUZZY_PWM_MAX);
}

float FB_FuzzyOutput_Slew(
        float current,
        float target,
        float rate,
        float Ts)
{
    float diff;
    float maxStep;

    if (!isFiniteFloat(current) || !isFiniteFloat(target))
        return 0.0f;

    if (rate <= 0.0f || Ts <= 0.0f)
        return target;

    maxStep = rate * Ts;
    diff = target - current;

    if (diff > maxStep) diff = maxStep;
    if (diff < -maxStep) diff = -maxStep;

    return current + diff;
}

float FB_FuzzyOutput_RunAbsolute(
        FB_FuzzyOutputManager_t *fb,
        float sv,
        float fuzzyPWM,
        float Ts)
{
    float pwmFF;
    float target;
    float blend;

    if (fb == NULL) return 0.0f;

    if (!isFiniteFloat(fuzzyPWM))
        fuzzyPWM = fb->config.pwmMin;

    fuzzyPWM = clamp(
        fuzzyPWM,
        fb->config.pwmMin,
        fb->config.pwmMax);

    /* Feed-forward is a blend with the absolute fuzzy command.
       It is NOT added to an already absolute PWM command. */
    pwmFF = FB_FuzzyOutput_CalcFF(fb, sv);
    fb->state.pwmFF = pwmFF;
    fb->state.fuzzyCorrection = fuzzyPWM;

    blend = clamp(fb->config.ffBlend, 0.0f, 1.0f);
    target = (1.0f - blend) * fuzzyPWM + blend * pwmFF;
    target = clamp(target, fb->config.pwmMin, fb->config.pwmMax);
    fb->state.targetPWM = target;

    if (fb->config.enableSlew)
    {
        fb->state.outputPWM = FB_FuzzyOutput_Slew(
            fb->state.outputPWM,
            target,
            fb->config.slewRate,
            Ts);
    }
    else
    {
        fb->state.outputPWM = target;
    }

    fb->state.outputPWM = clamp(
        fb->state.outputPWM,
        fb->config.pwmMin,
        fb->config.pwmMax);

    fb->state.previousPWM = fb->state.outputPWM;
    return fb->state.outputPWM;
}

/*
 * Compatibility wrapper for older callers.
 * Converts normalized centroid to absolute PWM, then uses the same safe path.
 */
float FB_FuzzyOutput_Run(
        FB_FuzzyOutputManager_t *fb,
        float sv,
        float pv,
        float centroid,
        float Ts)
{
    float normalized;
    float fuzzyPWM;

    (void)pv;

    if (fb == NULL) return 0.0f;

    normalized = clamp(centroid, -1.0f, 1.0f);
    fuzzyPWM = fb->config.pwmMin +
               ((normalized + 1.0f) * 0.5f) *
               (fb->config.pwmMax - fb->config.pwmMin);

    return FB_FuzzyOutput_RunAbsolute(fb, sv, fuzzyPWM, Ts);
}

bool FB_FuzzyOutput_SetConfig(
        FB_FuzzyOutputManager_t *fb,
        const FuzzyOutputConfig_t *cfg)
{
    uint8_t i;

    if (fb == NULL || cfg == NULL) return false;

    if (!isFiniteFloat(cfg->pwmMin) || !isFiniteFloat(cfg->pwmMax) ||
        cfg->pwmMin < FUZZY_PWM_MIN || cfg->pwmMax > FUZZY_PWM_MAX ||
        cfg->pwmMin >= cfg->pwmMax)
        return false;

    if (!isFiniteFloat(cfg->slewRate) || cfg->slewRate < 0.0f)
        return false;

    if (!isFiniteFloat(cfg->fuzzyScale) || cfg->fuzzyScale < 0.0f)
        return false;

    if (!isFiniteFloat(cfg->ffBlend) || cfg->ffBlend < 0.0f || cfg->ffBlend > 1.0f)
        return false;

    if (cfg->ffSize > FUZZY_FF_TABLE_SIZE)
        return false;

    for (i = 0U; i < cfg->ffSize; ++i)
    {
        if (!isFiniteFloat(cfg->ffTable[i].temperature) ||
            !isFiniteFloat(cfg->ffTable[i].pwm) ||
            cfg->ffTable[i].pwm < cfg->pwmMin ||
            cfg->ffTable[i].pwm > cfg->pwmMax)
            return false;

        if (i > 0U &&
            cfg->ffTable[i].temperature <= cfg->ffTable[i - 1U].temperature)
            return false;
    }

    fb->config = *cfg;

    fb->state.outputPWM = clamp(
        fb->state.outputPWM,
        fb->config.pwmMin,
        fb->config.pwmMax);
    fb->state.previousPWM = fb->state.outputPWM;

    return true;
}
