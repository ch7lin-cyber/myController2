/******************************************************************************
 * File    : myPlant_1.c
 * Brief   : Thermal Plant Model implementation.
 ******************************************************************************/

#include "myPlant.h"

#include <math.h>
#include <stddef.h>

/*
 * Identified reference data at 25 degC ambient:
 *
 * MV = 0 % : equilibrium = 25.00 degC (ambient reference)
 * MV = 20% : equilibrium ~= 93.40 degC, tau ~= 14.69 s
 * MV = 50% : equilibrium ~= 149.15 degC, tau ~= 13.13 s
 * MV = 80% : equilibrium ~= 160.84 degC, tau ~= 13.56 s
 *
 * Above 80% MV the static characteristic is extrapolated from 50..80%.
 */

#define PLANT_MIN_MV             (0.0f)
#define PLANT_MAX_MV             (100.0f)

#define ID_MV_0                  (0.0f)
#define ID_MV_20                 (20.0f)
#define ID_MV_50                 (50.0f)
#define ID_MV_80                 (80.0f)

#define ID_EQ_0                  (25.00f)
#define ID_EQ_20                 (93.40f)
#define ID_EQ_50                 (149.15f)
#define ID_EQ_80                 (160.84f)

#define ID_TAU_20                (14.69f)
#define ID_TAU_50                (13.13f)
#define ID_TAU_80                (13.56f)

#define DEFAULT_TAU_S            (15.0f)
#define MIN_TAU_S                (0.10f)
#define MAX_SAMPLE_TIME_S        (10.0f)
#define MIN_SAMPLE_TIME_S        (0.0001f)

static float clampf(float value, float low, float high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static float lerpf(float x0, float y0, float x1, float y1, float x)
{
    const float dx = x1 - x0;
    if (fabsf(dx) < 1.0e-6f) return y0;
    return y0 + ((x - x0) * (y1 - y0) / dx);
}

float ThermalPlant_GetEquilibrium(float mv_percent)
{
    const float mv = clampf(mv_percent, PLANT_MIN_MV, PLANT_MAX_MV);

    if (mv <= ID_MV_0)
        return ID_EQ_0;

    if (mv <= ID_MV_20)
        return lerpf(ID_MV_0, ID_EQ_0,
                     ID_MV_20, ID_EQ_20, mv);

    if (mv <= ID_MV_50)
        return lerpf(ID_MV_20, ID_EQ_20,
                     ID_MV_50, ID_EQ_50, mv);

    /* 50..80% is identified; >80% is extrapolation. */
    return lerpf(ID_MV_50, ID_EQ_50,
                 ID_MV_80, ID_EQ_80, mv);
}

float ThermalPlant_GetTimeConstant(float mv_percent)
{
    const float mv = clampf(mv_percent, PLANT_MIN_MV, PLANT_MAX_MV);

    if (mv <= ID_MV_20)
    {
        if (mv <= ID_MV_0)
            return DEFAULT_TAU_S;

        return lerpf(ID_MV_0, DEFAULT_TAU_S,
                     ID_MV_20, ID_TAU_20, mv);
    }

    if (mv <= ID_MV_50)
        return lerpf(ID_MV_20, ID_TAU_20,
                     ID_MV_50, ID_TAU_50, mv);

    if (mv <= ID_MV_80)
        return lerpf(ID_MV_50, ID_TAU_50,
                     ID_MV_80, ID_TAU_80, mv);

    return ID_TAU_80;
}

void ThermalPlant_Init(ThermalPlant_t *plant,
                       float initial_temperature_c,
                       float ambient_c,
                       float sample_time_s)
{
    if (plant == NULL) return;

    plant->temperature_c = initial_temperature_c;
    plant->ambient_c = ambient_c;
    plant->sample_time_s = clampf(sample_time_s,
                                  MIN_SAMPLE_TIME_S,
                                  MAX_SAMPLE_TIME_S);
    plant->tau_s = DEFAULT_TAU_S;
    plant->mv_percent = 0.0f;
    plant->initialized = true;
}

void ThermalPlant_Reset(ThermalPlant_t *plant,
                        float initial_temperature_c)
{
    if (plant == NULL) return;

    plant->temperature_c = initial_temperature_c;
    plant->tau_s = DEFAULT_TAU_S;
    plant->mv_percent = 0.0f;
}

void ThermalPlant_SetSampleTime(ThermalPlant_t *plant,
                                float sample_time_s)
{
    if (plant == NULL) return;

    plant->sample_time_s = clampf(sample_time_s,
                                  MIN_SAMPLE_TIME_S,
                                  MAX_SAMPLE_TIME_S);
}

float ThermalPlant_Step(ThermalPlant_t *plant,
                        float mv_percent)
{
    float equilibrium_c;
    float alpha;

    if ((plant == NULL) || !plant->initialized)
        return 0.0f;

    plant->mv_percent = clampf(mv_percent,
                               PLANT_MIN_MV,
                               PLANT_MAX_MV);

    plant->tau_s = ThermalPlant_GetTimeConstant(plant->mv_percent);
    plant->tau_s = clampf(plant->tau_s, MIN_TAU_S, 1000.0f);

    /*
     * The identification equilibrium curve is referenced to 25 degC.
     * Shift it to the configured ambient. Because ID_EQ_0 is 25 degC,
     * MV=0 now correctly gives equilibrium_c == ambient_c.
     */
    equilibrium_c = ThermalPlant_GetEquilibrium(plant->mv_percent) +
                    plant->ambient_c - ID_EQ_0;

    /* Exact ZOH discretization of dT/dt = (T_eq - T) / tau. */
    alpha = 1.0f - expf(-plant->sample_time_s / plant->tau_s);

    plant->temperature_c +=
        alpha * (equilibrium_c - plant->temperature_c);

    return plant->temperature_c;
}

int16_t ThermalPlant_GetTemperature_x10(const ThermalPlant_t *plant)
{
    float value;

    if (plant == NULL) return 0;

    value = plant->temperature_c * 10.0f;

    if (value > 32767.0f) value = 32767.0f;
    else if (value < -32768.0f) value = -32768.0f;

    return (int16_t)lroundf(value);
}
