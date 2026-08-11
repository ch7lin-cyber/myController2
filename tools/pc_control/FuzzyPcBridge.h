#ifndef FUZZY_PC_BRIDGE_H
#define FUZZY_PC_BRIDGE_H

#include <stdint.h>
#include "ssm_std_define.h"

#ifdef __cplusplus
extern "C" {
#endif

MY_API int FuzzyPc_Init(uint32_t sample_time_ms);
MY_API int FuzzyPc_SetSampleTime(uint32_t sample_time_ms);
MY_API uint32_t FuzzyPc_GetSampleTime(void);
MY_API void FuzzyPc_Reset(void);
MY_API void FuzzyPc_SetEnable(int enable);
MY_API int FuzzyPc_GetEnable(void);
MY_API float FuzzyPc_Run(float sv, float pv);

MY_API float FuzzyPc_GetError(void);
MY_API float FuzzyPc_GetDError(void);
MY_API float FuzzyPc_GetNormalizedError(void);
MY_API float FuzzyPc_GetNormalizedDError(void);
MY_API float FuzzyPc_GetRulePWM(void);
MY_API float FuzzyPc_GetPWM(void);
MY_API float FuzzyPc_GetCentroid(void);

#ifdef __cplusplus
}
#endif

#endif /* FUZZY_PC_BRIDGE_H */
