# Part 10-2 - Identified Plant + Feed-Forward + Fuzzy Correction + Bias Trim

Branch: `branch1`

## Architecture

The Part 10-2 test does **not** replace the existing Part 1-9 controller path.
Instead it adds a separate hybrid output block for plant-model validation:

`SV/PV -> existing Fuzzy Controller -> Sugeno rule PWM`

then

`Plant inverse FF + fuzzy transient correction + slow bias trim -> final PWM -> identified plant`

The identified plant still uses:

`dT/dt = (T_eq(MV) - T) / tau(MV)`

with identification points at MV = 20, 50 and 80 percent.

## PWM / MV conversion

The controller uses PWM 0..1000 while `ThermalPlant_Step()` uses MV 0..100 percent.

The test therefore applies:

`MV_percent = PWM / 10`

## Feed-forward table

The steady-state inverse feed-forward table is derived directly from the
identified static characteristic at 25 degC ambient:

| Temperature (degC) | PWM | MV (%) |
|---:|---:|---:|
| 25.0000 | 0 | 0 |
| 93.4000 | 200 | 20 |
| 149.1500 | 500 | 50 |
| 160.8400 | 800 | 80 |
| 168.6333 | 1000 | 100 |

The last point is an extrapolation from the plant model because the real
identification data stop at 80 percent MV.

## Hybrid output law

The existing fuzzy rule output remains available as a transient demand.
The hybrid block uses:

- neutral fuzzy PWM = 100 (current ZE/ZE singleton)
- positive correction gain = 0.50
- negative correction gain = 5.00
- slow bias trim Ki = 0.05 PWM/(degC*s)
- bias clamp = +/-200 PWM
- output slew = 5000 PWM/s = 100 PWM per 20 ms cycle
- conditional anti-windup at PWM saturation

This intentionally makes heating correction modest while allowing much
stronger heater reduction when the fuzzy rule requests below the neutral
steady-state singleton.

## Independent numerical replay

The same controller equations, hybrid output equations and identified plant
coefficients were replayed independently for 180 s at Ts = 20 ms.

| SV (degC) | PV at 180 s (degC) | Error SV-PV (degC) | Maximum PV (degC) | Overshoot (degC) |
|---:|---:|---:|---:|---:|
| 50 | 50.476 | -0.476 | 50.787 | +0.787 |
| 100 | 98.994 | +1.006 | 99.238 | no overshoot |
| 150 | 150.317 | -0.317 | 150.415 | +0.415 |
| 175 | 168.633 | +6.367 | 168.633 | unreachable |

## Comparison with Part 10-1

The hybrid strategy is materially better than pure absolute Sugeno PWM for the
identified plant:

- 50 degC: overshoot reduced into the <= 1 degC region in the numerical replay.
- 100 degC: large steady-state bias is substantially reduced, but the 180 s
  error is still about +1.0 degC.
- 150 degC: steady-state bias is substantially reduced and overshoot remains
  below 1 degC, but +/-0.1 degC is not yet achieved.
- 175 degC: still impossible with the current plant model because the model's
  100 percent-MV equilibrium is only about 168.63 degC.

## Important conclusion

Part 10-2 validates the **architecture direction**:

`identified FF + fuzzy transient correction + slow bias trim`

is clearly more suitable than asking the absolute Sugeno rule table alone to
provide both transient response and exact steady-state heater demand.

However, the current Part 10-2 tuning is **not a final production tuning**.
The +/-0.1 degC requirement is not yet met at 50, 100 or 150 degC in the
180-second replay.

## Why the 100 degC case still has error

The fuzzy transient correction remains active near the setpoint and can oppose
the plant-derived feed-forward value. A very slow integral/bias trim then needs
time to compensate that residual offset.

The next tuning step should therefore investigate one or both of:

1. fading the fuzzy correction toward zero inside a small steady-state error
   band, so the inverse plant feed-forward dominates close to SV;
2. using a slightly stronger but bounded bias integrator only inside the
   near-SV region, with anti-windup and bumpless reset.

## Source added in Part 10-2

- `FuzzyController_src/FB_FuzzyHybridOutput.h`
- `FuzzyController_src/FB_FuzzyHybridOutput.c`
- `tests/Part10_2_HybridClosedLoop.c`
- `ControllPlant/myPlant.h`
- `ControllPlant/myPlant_1.c`

The plant files are mirrored from `main/ControllPlant` into `branch1` so the
Part 10 tests are self-contained on the test branch.

## Scope limitation

The numerical values above are an independent mathematical replay of the source
logic. This report does not claim target-MCU compilation or hardware-in-loop
execution yet.
