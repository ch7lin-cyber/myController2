"""Part 10 - closed-loop thermal plant simulation for myController2.

The plant is a first-order-plus-dead-time (FOPDT) approximation intended for
controller verification, not hardware identification. The model parameters
are explicit so measured identification data can replace them later.

PWM: 0..1000 (0..100 %)
Temperature: degC
Controller sample: 20 ms
"""

from dataclasses import dataclass
import math


@dataclass
class ThermalPlant:
    ambient_c: float = 25.0
    gain_c_per_100pct: float = 175.0
    tau_s: float = 8.0
    dead_time_s: float = 0.0
    dt_s: float = 0.02
    loss_gain: float = 0.0

    def __post_init__(self):
        self.temperature_c = self.ambient_c
        self._steps = max(0, int(round(self.dead_time_s / self.dt_s)))
        self._pwm_history = [0.0] * (self._steps + 1)

    def reset(self, temperature_c=None):
        self.temperature_c = self.ambient_c if temperature_c is None else temperature_c
        self._pwm_history = [0.0] * (self._steps + 1)

    def step(self, pwm):
        pwm = max(0.0, min(1000.0, float(pwm)))
        self._pwm_history.append(pwm)
        delayed_pwm = self._pwm_history.pop(0)
        duty = delayed_pwm / 1000.0

        # Heating contribution is represented as a first-order approach to
        # ambient + heater gain. loss_gain can later model temperature losses.
        target = self.ambient_c + self.gain_c_per_100pct * duty
        if self.loss_gain > 0.0:
            target -= self.loss_gain * max(0.0, self.temperature_c - self.ambient_c)

        alpha = 1.0 - math.exp(-self.dt_s / max(self.tau_s, 1e-9))
        self.temperature_c += alpha * (target - self.temperature_c)
        return self.temperature_c


def run_step_test(controller, sv, initial_pv, duration_s):
    plant = ThermalPlant(temperature_c if False else 25.0)
    # Keep construction explicit for Python versions without positional clarity.
    plant.reset(initial_pv)
    n = int(round(duration_s / plant.dt_s))
    log = []
    for k in range(n):
        pv = plant.temperature_c
        pwm = controller(sv, pv)
        pv_next = plant.step(pwm)
        log.append((k * plant.dt_s, sv, pv, pwm))
    return log


def verify_plant_response():
    """Run open-loop sanity checks used before closed-loop integration."""
    plant = ThermalPlant()
    plant.reset(25.0)
    samples = []
    for k in range(int(8.0 / plant.dt_s)):
        pv = plant.step(1000.0)
        samples.append((k * plant.dt_s, pv))

    # At t=tau, a first-order response should reach about 63.2% of its final
    # temperature rise. This is a model sanity check, not a controller limit.
    expected = 25.0 + 0.632 * 175.0
    actual = samples[-1][1]
    return {"pv_at_8s": actual, "expected_first_order_at_tau": expected}


if __name__ == "__main__":
    result = verify_plant_response()
    print("Part 10 thermal plant sanity check")
    print(f"PV after 8 s at 100% PWM: {result['pv_at_8s']:.3f} C")
    print(f"FOPDT first-order reference: {result['expected_first_order_at_tau']:.3f} C")
