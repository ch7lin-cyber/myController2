/******************************************************************************
 * File    : FB_FuzzyPerformanceMonitor.h
 * Brief   : Episode-based performance monitor for fuzzy self tuning.
 ******************************************************************************/
#ifndef FB_FUZZY_PERFORMANCE_MONITOR_H
#define FB_FUZZY_PERFORMANCE_MONITOR_H

#include <stdbool.h>
#include <stdint.h>
#include "ssm_std_define.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    float Ts;
    float SvChangeThreshold_c;
    float RiseBandRatio;
    float SettlingBand_c;
    float SettlingHold_s;
    float MinEpisode_s;
    float MaxEpisode_s;
} FuzzyPerformanceMonitorConfig_t;

typedef struct
{
    float StartSV;
    float TargetSV;
    float StartPV;
    float PeakPV;
    float ValleyPV;

    float Overshoot_c;
    float Undershoot_c;
    float RiseTime_s;
    float SettlingTime_s;
    float SteadyStateError_c;
    float IAE;
    float ISE;
    float PWMActivity;
    float MaxPVRate_c_per_s;

    uint32_t SampleCount;
    uint16_t ErrorZeroCrossCount;
    bool RiseReached;
    bool Settled;
    bool Complete;
} FuzzyPerformanceMetrics_t;

typedef struct
{
    FuzzyPerformanceMonitorConfig_t Config;
    FuzzyPerformanceMetrics_t Metrics;

    float PreviousSV;
    float PreviousPV;
    float PreviousPWM;
    float PreviousError;
    float EpisodeTime_s;
    float SettlingHoldTime_s;
    bool Initialized;
    bool EpisodeActive;
} FB_FuzzyPerformanceMonitor_t;

MY_API void FB_FuzzyPerformanceMonitor_Init(FB_FuzzyPerformanceMonitor_t *fb);
MY_API void FB_FuzzyPerformanceMonitor_Reset(FB_FuzzyPerformanceMonitor_t *fb);
MY_API bool FB_FuzzyPerformanceMonitor_SetConfig(
    FB_FuzzyPerformanceMonitor_t *fb,
    const FuzzyPerformanceMonitorConfig_t *config);
MY_API void FB_FuzzyPerformanceMonitor_StartEpisode(
    FB_FuzzyPerformanceMonitor_t *fb,
    float sv,
    float pv,
    float pwm);
MY_API void FB_FuzzyPerformanceMonitor_Run(
    FB_FuzzyPerformanceMonitor_t *fb,
    float sv,
    float pv,
    float pwm);
MY_API bool FB_FuzzyPerformanceMonitor_IsComplete(
    const FB_FuzzyPerformanceMonitor_t *fb);
MY_API const FuzzyPerformanceMetrics_t *FB_FuzzyPerformanceMonitor_GetMetrics(
    const FB_FuzzyPerformanceMonitor_t *fb);

#ifdef __cplusplus
}
#endif

#endif /* FB_FUZZY_PERFORMANCE_MONITOR_H */
