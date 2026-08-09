#include <cassert>
#include <cmath>
#include <iostream>

/*
 * Intentionally include the C headers directly from C++ WITHOUT wrapping
 * them in an external extern "C" block. Each public header must provide its
 * own C++ compatibility guard.
 */
#include "FB_FuzzyController.h"
#include "FB_FuzzyScaling.h"
#include "FB_FuzzyMembership.h"
#include "FB_FuzzyRule.h"
#include "FB_FuzzyDefuzzifier.h"
#include "FB_FuzzyOutputManager.h"
#include "FB_FuzzyHybridOutput.h"
#include "FB_FuzzyConfigManager.h"

int main()
{
    FB_FuzzyController_t controller{};
    FB_FuzzyConfigManager_t config{};

    FB_FuzzyController_Init(&controller);
    FB_FuzzyConfig_Init(&config);

    assert(FB_FuzzyConfig_Check(&config));

    const float pwm = FB_FuzzyController_Run(&controller, 130.0f, 25.0f);

    assert(std::isfinite(pwm));
    assert(pwm >= controller.config.OutputMin);
    assert(pwm <= controller.config.OutputMax);

    std::cout << "C++ all-header/linkage smoke test: PASS (PWM=" << pwm << ")\n";
    return 0;
}
