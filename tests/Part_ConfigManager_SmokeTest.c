/******************************************************************************
 * File  : Part_ConfigManager_SmokeTest.c
 * Brief : Source-level smoke regression for FB_FuzzyConfigManager.
 ******************************************************************************/

#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "../FuzzyController_src/FB_FuzzyConfigManager.h"

static int nearly_equal(float a, float b)
{
    return fabsf(a - b) < 0.0001f;
}

int main(void)
{
    FB_FuzzyConfigManager_t cfg;
    FB_FuzzyController_t controller;
    FuzzyMFConfig_t mf;
    int16_t old_rule;

    FB_FuzzyConfig_Init(&cfg);
    FB_FuzzyController_Init(&controller);

    /* Default configuration must be self-consistent. */
    assert(FB_FuzzyConfig_Check(&cfg));
    assert(cfg.config.magic == FUZZY_CONFIG_MAGIC);
    assert(cfg.config.version == FUZZY_CONFIG_VERSION);
    assert(cfg.changed == false);

    /* Invalid metadata must be rejected. */
    cfg.config.magic = 0U;
    assert(!FB_FuzzyConfig_Check(&cfg));
    cfg.config.magic = FUZZY_CONFIG_MAGIC;

    /* Rule edits are range + monotonicity protected and rollback on failure. */
    old_rule = cfg.config.rule[FUZZY_ZE][FUZZY_ZE];
    assert(!FB_FuzzyConfig_SetRule(&cfg,
                                   FUZZY_ZE,
                                   FUZZY_ZE,
                                   1000));
    assert(cfg.config.rule[FUZZY_ZE][FUZZY_ZE] == old_rule);

    assert(FB_FuzzyConfig_SetRule(&cfg,
                                  FUZZY_ZE,
                                  FUZZY_ZE,
                                  old_rule));
    assert(cfg.changed == true);

    /* A valid center MF adjustment must pass whole-set validation. */
    mf = cfg.config.MF[FUZZY_ZE];
    mf.Left = -0.30f;
    mf.Center = 0.0f;
    mf.Right = 0.30f;
    mf.Peak = 1.0f;
    assert(FB_FuzzyConfig_SetMF(&cfg, FUZZY_ZE, &mf));

    /* Unit-height engine rejects unsupported Peak values. */
    mf.Peak = 0.5f;
    assert(!FB_FuzzyConfig_SetMF(&cfg, FUZZY_ZE, &mf));

    /* Configure a legal FF table. Loading the table does not force FF policy. */
    cfg.config.ffSize = 2U;
    cfg.config.ff[0].temperature = 25.0f;
    cfg.config.ff[0].pwm = 0.0f;
    cfg.config.ff[1].temperature = 100.0f;
    cfg.config.ff[1].pwm = 200.0f;
    assert(FB_FuzzyConfig_Check(&cfg));

    /* Runtime compact scaling is explicitly manual/fixed after Apply(). */
    cfg.config.scaling.errorScale = 0.05f;
    cfg.config.scaling.dErrorScale = 0.10f;
    cfg.config.scaling.Ku = 1.0f;

    assert(FB_FuzzyConfig_Apply(&cfg, &controller));
    assert(cfg.changed == false);
    assert(controller.config.Enable == cfg.config.enable);
    assert(controller.scaling.Config.AutoScalingEnable == false);
    assert(controller.scaling.Config.AdaptiveEnable == false);
    assert(nearly_equal(controller.scaling.State.Ke, 0.05f));
    assert(nearly_equal(controller.scaling.State.Kde, 0.10f));
    assert(nearly_equal(controller.scaling.State.Ku, 1.0f));
    assert(controller.output.config.ffSize == 2U);
    assert(nearly_equal(controller.output.config.ffTable[1].temperature, 100.0f));
    assert(nearly_equal(controller.output.config.ffTable[1].pwm, 200.0f));

    printf("FB_FuzzyConfigManager smoke test: PASS\n");
    return 0;
}
