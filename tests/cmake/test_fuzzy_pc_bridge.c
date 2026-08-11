#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "FuzzyPcBridge.h"

int main(void)
{
    float pwm;

    assert(FuzzyPc_Init(20U));
    assert(FuzzyPc_GetSampleTime() == 20U);
    assert(FuzzyPc_GetEnable() == 0);

    FuzzyPc_SetEnable(1);
    assert(FuzzyPc_GetEnable() == 1);

    pwm = FuzzyPc_Run(130.0f, 25.0f);
    assert(isfinite(pwm));
    assert(pwm >= 0.0f);
    assert(pwm <= 1000.0f);
    assert(isfinite(FuzzyPc_GetError()));
    assert(isfinite(FuzzyPc_GetDError()));
    assert(isfinite(FuzzyPc_GetNormalizedError()));
    assert(isfinite(FuzzyPc_GetNormalizedDError()));
    assert(isfinite(FuzzyPc_GetRulePWM()));

    FuzzyPc_SetEnable(0);
    assert(FuzzyPc_GetEnable() == 0);
    assert(FuzzyPc_Run(130.0f, 25.0f) == 0.0f);

    printf("Fuzzy PC bridge smoke test: PASS\n");
    return 0;
}
