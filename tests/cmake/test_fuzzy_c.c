#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "FB_FuzzyController.h"
#include "FB_FuzzyConfigManager.h"
#include "FB_FuzzyHybridOutput.h"

static int nearly_equal(float a,float b){return fabsf(a-b)<0.000001f;}

int main(void)
{
    FB_FuzzyController_t c;
    FB_FuzzyConfigManager_t cfg;
    FB_FuzzyHybridOutput_t hybrid;
    float pwm,p0,p1,p2,ff130,bias;

    FB_FuzzyController_Init(&c);
    FB_FuzzyConfig_Init(&cfg);
    assert(FB_FuzzyConfig_Check(&cfg));

    assert(FB_FuzzyController_GetSampleTime(&c)==20U);
    assert(nearly_equal(c.config.Ts,0.020f));
    assert(nearly_equal(c.config.DErrorFilterTau_s,0.50f));
    assert(nearly_equal(c.config.DErrorDeadband_c_per_s,0.20f));
    assert(c.config.EnableBoost==true);
    assert(c.config.EnablePercentApproach==true);
    assert(nearly_equal(c.config.FullPowerErrorRatio,0.05f));
    assert(nearly_equal(c.config.PrecisionErrorRatio,0.03f));
    assert(nearly_equal(c.config.FullPowerErrorMin_c,3.0f));
    assert(nearly_equal(c.config.PrecisionErrorMin_c,1.5f));
    assert(nearly_equal(c.config.ApproachDownSlewRate_pwm_per_s,1500.0f));
    assert(nearly_equal(c.hybridOutput.config.biasPositiveLearningBand_c,1.0f));
    assert(nearly_equal(c.hybridOutput.config.biasNegativeLearningBand_c,5.0f));

    assert(!FB_FuzzyController_SetSampleTime(&c,0U));
    assert(!FB_FuzzyController_SetSampleTime(&c,6001U));
    assert(FB_FuzzyController_SetSampleTime(&c,1U));
    assert(nearly_equal(c.config.Ts,0.001f));
    assert(FB_FuzzyController_SetSampleTime(&c,6000U));
    assert(nearly_equal(c.config.Ts,6.0f));
    assert(FB_FuzzyController_SetSampleTime(&c,20U));

    /* 0.4 C sample jump at 20 ms => raw derivative near 20 C/s; LPF must tame it. */
    FB_FuzzyController_Reset(&c);
    assert(FB_FuzzyController_SetDerivativeFilter(&c,0.50f,0.20f));
    (void)FB_FuzzyController_Run(&c,130.0f,129.0f);
    (void)FB_FuzzyController_Run(&c,130.0f,129.4f);
    assert(c.state.RawDError<-19.0f);
    assert(fabsf(c.state.FilteredDError)<0.80f);
    assert(fabsf(c.state.dError)<0.60f);

    /* SV step at constant PV must not kick derivative. */
    FB_FuzzyController_Reset(&c);
    (void)FB_FuzzyController_Run(&c,100.0f,80.0f);
    (void)FB_FuzzyController_Run(&c,130.0f,80.0f);
    assert(fabsf(c.state.RawDError)<0.0001f);
    assert(fabsf(c.state.dError)<0.0001f);

    assert(!FB_FuzzyController_SetDerivativeFilter(&c,-0.1f,0.2f));
    assert(!FB_FuzzyController_SetDerivativeFilter(&c,0.2f,-0.1f));
    assert(!FB_FuzzyController_SetBoostConfig(&c,true,18.0f,20.0f));
    assert(FB_FuzzyController_SetBoostConfig(&c,true,20.0f,18.0f));

    /* Approach config validation. */
    assert(!FB_FuzzyController_SetApproachConfig(&c,true,0.03f,0.05f,3.0f,1.5f,1500.0f));
    assert(!FB_FuzzyController_SetApproachConfig(&c,true,0.05f,0.03f,1.0f,1.5f,1500.0f));
    assert(FB_FuzzyController_SetApproachConfig(&c,true,0.05f,0.03f,3.0f,1.5f,1500.0f));

    ff130=FB_FuzzyHybridOutput_CalcFF(&c.hybridOutput,130.0f);
    assert(ff130>390.0f&&ff130<405.0f);

    /* Heater bias learning remains asymmetric. */
    FB_FuzzyHybridOutput_Init(&hybrid);
    hybrid.config.ffSize=0U;
    hybrid.config.enableFeedForward=false;
    hybrid.state.biasPWM=25.0f;
    bias=hybrid.state.biasPWM;
    (void)FB_FuzzyHybridOutput_Run(&hybrid,130.0f,127.3f,100.0f,0.02f);
    assert(nearly_equal(hybrid.state.biasPWM,bias));
    (void)FB_FuzzyHybridOutput_Run(&hybrid,130.0f,129.0f,100.0f,0.02f);
    assert(hybrid.state.biasPWM>bias);
    bias=hybrid.state.biasPWM;
    (void)FB_FuzzyHybridOutput_Run(&hybrid,130.0f,132.7f,100.0f,0.02f);
    assert(hybrid.state.biasPWM<bias);

    /*
     * SV=130 C default percentage zones:
     *   full-power threshold = 6.5 C (5%)
     *   precision threshold  = 3.9 C (3%)
     */
    FB_FuzzyController_Reset(&c);
    FB_FuzzyController_EnableHybridOutput(&c,true);
    c.config.Enable=true;
    c.hybridOutput.state.biasPWM=100.0f;

    pwm=FB_FuzzyController_Run(&c,130.0f,120.0f); /* error 10 C */
    assert(c.state.BoostActive==true);
    assert(c.state.ApproachActive==false);
    assert(nearly_equal(c.state.FullPowerError_c,6.5f));
    assert(nearly_equal(c.state.PrecisionError_c,3.9f));
    assert(nearly_equal(c.hybridOutput.state.biasPWM,0.0f));
    assert(nearly_equal(pwm,1000.0f));

    /* Just inside Soft Landing: no 1000 -> 5xx one-cycle drop. Max drop=30 PWM/cycle. */
    p0=pwm;
    p1=FB_FuzzyController_Run(&c,130.0f,123.6f); /* error 6.4 C */
    assert(c.state.BoostActive==false);
    assert(c.state.ApproachActive==true);
    assert(c.state.ApproachBlend>0.90f);
    assert(p1<=p0);
    assert((p0-p1)<=30.01f);

    /* Mid blend zone must remain continuous and bounded. */
    p2=FB_FuzzyController_Run(&c,130.0f,125.0f); /* error 5.0 C */
    assert(c.state.ApproachActive==true);
    assert(c.state.ApproachBlend>0.40f&&c.state.ApproachBlend<0.45f);
    assert((p1-p2)<=30.01f);
    assert(p2>=0.0f&&p2<=1000.0f);

    /* Precision zone: Hybrid owns output; approach blend is fully released. */
    pwm=FB_FuzzyController_Run(&c,130.0f,126.2f); /* error 3.8 C */
    assert(c.state.BoostActive==false);
    assert(c.state.ApproachActive==false);
    assert(nearly_equal(c.state.ApproachBlend,0.0f));
    assert(isfinite(pwm));
    assert(pwm>=0.0f&&pwm<=1000.0f);

    /* Legacy fixed-error boost remains available by disabling percent approach. */
    assert(FB_FuzzyController_SetApproachConfig(&c,false,0.05f,0.03f,3.0f,1.5f,1500.0f));
    FB_FuzzyController_Reset(&c);
    FB_FuzzyController_EnableHybridOutput(&c,true);
    pwm=FB_FuzzyController_Run(&c,130.0f,100.0f); /* error 30 */
    assert(c.state.BoostActive==true);
    assert(nearly_equal(pwm,1000.0f));
    pwm=FB_FuzzyController_Run(&c,130.0f,111.0f); /* error 19 */
    assert(c.state.BoostActive==true);
    pwm=FB_FuzzyController_Run(&c,130.0f,112.1f); /* error 17.9 */
    assert(c.state.BoostActive==false);

    printf("C branch3 soft-landing regression PASS (FF130=%.3f)\n",ff130);
    return 0;
}
