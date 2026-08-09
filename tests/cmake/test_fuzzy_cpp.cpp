#include <cassert>
#include <cmath>
#include <iostream>

#include "FB_FuzzyController.h"
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

    std::cout << "C++ header/linkage smoke test: PASS (PWM=" << pwm << ")\n";
    return 0;
}
