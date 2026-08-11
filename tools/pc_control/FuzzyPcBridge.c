#include "FuzzyPcBridge.h"
#include "FB_FuzzyController.h"

#include <stddef.h>

static FB_FuzzyController_t g_controller;
static int g_initialized = 0;

int FuzzyPc_Init(uint32_t sample_time_ms)
{
    FB_FuzzyController_Init(&g_controller);
    if (!FB_FuzzyController_SetSampleTime(&g_controller, sample_time_ms))
    {
        g_initialized = 0;
        return 0;
    }

    g_controller.config.Enable = false;
    g_initialized = 1;
    return 1;
}

int FuzzyPc_SetSampleTime(uint32_t sample_time_ms)
{
    if (!g_initialized)
        return 0;

    return FB_FuzzyController_SetSampleTime(&g_controller, sample_time_ms) ? 1 : 0;
}

uint32_t FuzzyPc_GetSampleTime(void)
{
    if (!g_initialized)
        return 0U;

    return FB_FuzzyController_GetSampleTime(&g_controller);
}

void FuzzyPc_Reset(void)
{
    if (!g_initialized)
        return;

    FB_FuzzyController_Reset(&g_controller);
}

void FuzzyPc_SetEnable(int enable)
{
    if (!g_initialized)
        return;

    g_controller.config.Enable = (enable != 0);

    if (!g_controller.config.Enable)
        FB_FuzzyController_Reset(&g_controller);
}

int FuzzyPc_GetEnable(void)
{
    if (!g_initialized)
        return 0;

    return g_controller.config.Enable ? 1 : 0;
}

float FuzzyPc_Run(float sv, float pv)
{
    if (!g_initialized)
        return 0.0f;

    return FB_FuzzyController_Run(&g_controller, sv, pv);
}

float FuzzyPc_GetError(void)
{
    return g_initialized ? g_controller.state.Error : 0.0f;
}

float FuzzyPc_GetDError(void)
{
    return g_initialized ? g_controller.state.dError : 0.0f;
}

float FuzzyPc_GetNormalizedError(void)
{
    return g_initialized ? g_controller.scaling.State.NormalizedError : 0.0f;
}

float FuzzyPc_GetNormalizedDError(void)
{
    return g_initialized ? g_controller.scaling.State.NormalizedDError : 0.0f;
}

float FuzzyPc_GetRulePWM(void)
{
    return g_initialized ? g_controller.ruleEngine.Result.RuleOutput : 0.0f;
}

float FuzzyPc_GetPWM(void)
{
    return g_initialized ? g_controller.state.PWM : 0.0f;
}

float FuzzyPc_GetCentroid(void)
{
    return g_initialized ? g_controller.state.Centroid : 0.0f;
}
