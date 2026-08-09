/******************************************************************************
 * File    : myPlant.h
 * Brief   : Thermal Plant Model identified from measured MV/PV data.
 *
 * Model:
 *   dT/dt = (T_eq(MV) - T) / tau(MV)
 *
 * The identified operating points are MV = 20, 50 and 80 %.
 * T_eq and tau were obtained by fitting each measured heating response to
 * a first-order thermal response:
 *
 *   PV(t) = PV0 + A * (1 - exp(-t/tau))
 *
 * This is a simulation model for controller development, not a replacement
 * for the physical thermal system.
 ******************************************************************************/

#ifndef MY_PLANT_H
#define MY_PLANT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    float temperature_c;
    float ambient_c;
    float sample_time_s;
    float tau_s;
    float mv_percent;
    bool  initialized;
} ThermalPlant_t;

void ThermalPlant_Init(ThermalPlant_t *plant,
                       float initial_temperature_c,
                       float ambient_c,
                       float sample_time_s);
void ThermalPlant_Reset(ThermalPlant_t *plant,
                        float initial_temperature_c);
void ThermalPlant_SetSampleTime(ThermalPlant_t *plant,
                                float sample_time_s);
float ThermalPlant_Step(ThermalPlant_t *plant,
                        float mv_percent);
float ThermalPlant_GetEquilibrium(float mv_percent);
float ThermalPlant_GetTimeConstant(float mv_percent);
int16_t ThermalPlant_GetTemperature_x10(const ThermalPlant_t *plant);

#ifdef __cplusplus
}
#endif

#endif /* MY_PLANT_H */
