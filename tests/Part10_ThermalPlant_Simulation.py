"""Part 10 - thermal plant simulation scaffold for myController2.

This model is intentionally a verification scaffold, not a replacement for
measured system identification. Replace the parameters with identified data
before using it to tune the production fuzzy controller.

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

        target = self.ambient_c + self.gain_c_per_100pct * duty
        if self.loss_gain > 0.0:
            target -= self.loss_gain * max(0.0, self.temperature_c - self.ambient_c)

        alpha = 1.0 - math.exp(-self.dt_s / max(self.tau_s, 1e-9))
        self.temperature_c += alpha * (target - self.temperature_c)
        return self.temperature_c


def run_closed_loop(controller, sv, initial_pv, duration_s, plant=None):
    """Run a generic controller callback against the plant.

    controller signature: controller(sv_c, pv_c) -> pwm_0_to_1000
    Returns tuples of (time_s, sv_c, pv_c, pwm).
    """
    if plant is None:
        plant = ThermalPlant()

    plant.reset(initial_pv)
    n = int(round(duration_s / plant.dt_s))
    log = []

    for k in range(n):
        pv = plant.temperature_c
        pwm = controller(sv, pv)
        pwm = max(0.0, min(1000.0, float(pwm)))
        plant.step(pwm)
        log.append((k * plant.dt_s, sv, pv, pwm))

    return log


def verify_open_loop_step():
    """Verify the first-order plant response before closed-loop testing."""
    plant = ThermalPlant()
    plant.reset(25.0)

    samples = []
    n = int(round(8.0 / plant.dt_s))
    for k in range(n):
        pv = plant.step(1000.0)
        samples.append((k * plant.dt_s, pv))

    # For a first-order model, t=tau is about 63.2% of the temperature rise.
    expected = 25.0 + 0.632 * 175.0
    actual = samples[-1][1]
    return actual, expected


if __name__ == "__main__":
    actual, expected = verify_open_loop_step()
    print("Part 10 thermal plant sanity check")
    print(f"PV after 8 s at 100% PWM: {actual:.3f} C")
    print(f"First-order reference at tau: {expected:.3f} C")
