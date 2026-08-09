# Part 10-2 - Identified Plant + Feed-Forward + Fuzzy Correction + Bias Trim

> **AUDIT NOTICE (2026-08-09):** The numerical replay below predates the correction of the plant's zero-MV equilibrium reference. The corrected plant now uses 25 degC as the reference equilibrium at MV=0, so the old 50/100/150/175 degC numerical results must be re-run before tuning or acceptance decisions. The hybrid architecture description remains valid.

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

The following values are retained for history only and are **stale after the plant-reference fix**:

| SV (degC) | PV at 180 s (degC) | Error SV-PV (degC) | Maximum PV (degC) | Overshoot (degC) |
|---:|---:|---:|---:|---:|
| 50 | 50.476 | -0.476 | 50.787 | +0.787 |
| 100 | 98.994 | +1.006 | 99.238 | no overshoot |
| 150 | 150.317 | -0.317 | 150.415 | +0.415 |
| 175 | 168.633 | +6.367 | 168.633 | unreachable |

## Architecture conclusion

The architecture direction remains:

`identified FF + fuzzy transient correction + slow bias trim`

However, current performance must be re-measured against the corrected plant before any statement about overshoot or steady-state error is accepted.

## Source added in Part 10-2

- `FuzzyController_src/FB_FuzzyHybridOutput.h`
- `FuzzyController_src/FB_FuzzyHybridOutput.c`
- `tests/Part10_2_HybridClosedLoop.c`
- `ControllPlant/myPlant.h`
- `ControllPlant/myPlant_1.c`

## Scope limitation

This report does not claim target-MCU compilation or hardware-in-loop execution. After the plant audit fix, a new closed-loop replay is required.
