/******************************************************************************
 * File    : Part10_ClosedLoop_PlantTest.c
 * Brief   : Closed-loop regression using the identified ThermalPlant model.
 *
 * Active loop:
 *   SV/PV -> Fuzzy Controller -> PWM[0..1000]
 *         -> MV[%] = PWM/10 -> ThermalPlant -> PV
 ******************************************************************************/

#include <stdio.h>
#include <math.h>
#include "../FuzzyController_src/FB_FuzzyController.h"
#include "../ControllPlant/myPlant.h"

#define TEST_TS_S                (0.020f)
#define TEST_INITIAL_PV_C        (25.0f)
#define TEST_DURATION_S          (180.0f)
#define TEST_PWM_TO_MV_SCALE     (0.1f)
#define TEST_SETTLE_BAND_C       (0.1f)

static void run_case(float sv_c)
{
    FB_FuzzyController_t controller;
    ThermalPlant_t plant;
    const int total_steps = (int)(TEST_DURATION_S / TEST_TS_S);
    float max_pv = TEST_INITIAL_PV_C;
    float final_pwm = 0.0f;
    float final_mv = 0.0f;
    float settle_time = -1.0f;
    int inside_count = 0;
    int k;

    FB_FuzzyController_Init(&controller);
    ThermalPlant_Init(&plant, TEST_INITIAL_PV_C, 25.0f, TEST_TS_S);

    for (k = 0; k < total_steps; ++k)
    {
        float pv = plant.temperature_c;
        float pwm = FB_FuzzyController_Run(&controller, sv_c, pv);
        float mv_percent = pwm * TEST_PWM_TO_MV_SCALE;
        float next_pv = ThermalPlant_Step(&plant, mv_percent);
        float error_abs = fabsf(sv_c - next_pv);

        if (next_pv > max_pv)
            max_pv = next_pv;

        final_pwm = pwm;
        final_mv = mv_percent;

        if (error_abs <= TEST_SETTLE_BAND_C)
        {
            ++inside_count;
            if ((inside_count >= 50) && (settle_time < 0.0f))
                settle_time = (float)(k - 49) * TEST_TS_S;
        }
        else
        {
            inside_count = 0;
        }
    }

    printf("SV=%.1f C\n", sv_c);
    printf("  Final PV        : %.3f C\n", plant.temperature_c);
    printf("  Final error     : %.3f C\n", sv_c - plant.temperature_c);
    printf("  Max PV          : %.3f C\n", max_pv);
    printf("  Overshoot       : %.3f C\n", max_pv - sv_c);
    printf("  Final PWM       : %.1f /1000\n", final_pwm);
    printf("  Final MV        : %.2f %%\n", final_mv);
    printf("  Settle time     : %.3f s\n", settle_time);
    printf("  Plant Teq@100%% : %.3f C (identified/extrapolated)\n",
           ThermalPlant_GetEquilibrium(100.0f));
}

int main(void)
{
    run_case(50.0f);
    run_case(100.0f);
    run_case(150.0f);
    run_case(175.0f);
    return 0;
}
