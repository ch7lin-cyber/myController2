# Part 10 - Closed-loop test with identified heater plant

Branch: `branch1`

Plant source: `main/ControllPlant/myPlant.h` + `main/ControllPlant/myPlant_1.c`

Controller source: current `branch1` Part 1-9 fuzzy controller.

## Connection used

The controller returns absolute PWM in `0..1000` (0.1% units), while the plant
API `ThermalPlant_Step()` accepts MV in percent `0..100`.

Therefore the closed-loop adapter is:

`mv_percent = fuzzy_pwm * 0.1f`

and the execution order per 20 ms sample is:

`PV -> FB_FuzzyController_Run(SV,PV) -> PWM -> PWM/10 -> ThermalPlant_Step() -> next PV`

## Plant model reviewed

The identified model is:

`dT/dt = (T_eq(MV) - T) / tau(MV)`

with identified points:

- MV=20%: equilibrium 93.40 C, tau 14.69 s
- MV=50%: equilibrium 149.15 C, tau 13.13 s
- MV=80%: equilibrium 160.84 C, tau 13.56 s

For MV > 80%, `myPlant_1.c` linearly extrapolates the 50..80% equilibrium
characteristic and holds tau at the 80% identified value.

This gives an extrapolated equilibrium at MV=100% of approximately:

`T_eq(100%) = 168.633 C`

at the 25 C reference ambient.

## Closed-loop numerical replay

An independent numerical replay of the current C equations was run for 180 s
at Ts=20 ms from PV=25 C. Feed-forward is disabled and the default output slew
is active.

| SV | Final PV @180s | Final Error | Peak PV | Overshoot | Final PWM | Final MV |
|---:|---:|---:|---:|---:|---:|---:|
| 50 C | 52.509 C | -2.509 C | 52.540 C | +2.540 C | 62.2 | 6.22% |
| 100 C | 94.701 C | +5.299 C | 94.719 C | -5.281 C | 175.0 | 17.50% |
| 150 C | 139.602 C | +10.398 C | 139.615 C | -10.385 C | 397.7 | 39.77% |
| 175 C | 153.045 C | +21.955 C | 153.045 C | -21.955 C | 600.0 | 60.00% |

No case entered and remained inside a +/-0.1 C settling band during this test.

## P10-01 - 175 C is unreachable in the current plant model

This is the most important finding.

The plant model's extrapolated maximum equilibrium at 100% MV is only about
168.63 C. Therefore a controller connected to this model cannot physically
settle at 175 C, regardless of fuzzy tuning.

This does not automatically mean the real heater cannot reach 175 C. It means
the identification model currently stored in `ControllPlant` does not contain
enough high-power data to represent that operating point.

Required action before using this model to validate the 175 C requirement:

- identify additional steady/heating data above MV=80%, preferably including
  MV=90% and MV=100%, or
- refit the static equilibrium model using measurements that demonstrate the
  real 175 C operating region.

## P10-02 - Current fuzzy rule table has large steady-state bias

Even for reachable regions, the direct absolute-PWM Sugeno table does not map
steady Error to the plant's required equilibrium MV.

Examples after 180 s:

- SV=100 C stabilizes near 94.7 C at about 17.5% MV
- SV=150 C stabilizes near 139.6 C at about 39.8% MV

The controller therefore has no integral mechanism and no learned/static
feed-forward mapping that guarantees zero steady-state error.

For this architecture, one of the following is needed for tight steady-state
accuracy:

1. enable a plant-derived feed-forward/static MV table and let fuzzy logic apply
   correction around it, or
2. change the Sugeno rule output from absolute PWM to incremental correction
   around a baseline/FF command, or
3. add a slow integral trim / bias learner.

For the existing absolute-PWM architecture, option 1 plus a small integral/bias
trim is the least disruptive path.

## P10-03 - Low setpoint case overshoots

SV=50 C reaches about 52.54 C peak in this replay, exceeding the <=1 C target.
This confirms that the same fixed absolute RuleTable cannot be assumed to meet
the requested performance across the full temperature range.

## P10-04 - Model limitation above 80% MV must be visible in any report

The 80..100% region is extrapolation, not identified data. Any claimed 175 C
performance using the current model would therefore be unsupported.

## Recommended Part 10 next implementation

Do not tune the 49-rule table blindly against this plant yet.

Recommended sequence:

1. Keep `myPlant.c/h` as the closed-loop test plant.
2. Build a feed-forward inverse map `SV -> required MV` from
   `ThermalPlant_GetEquilibrium()` for reachable temperatures.
3. Add a slow bias/integral trim to remove residual steady-state error.
4. Re-run step tests at 50, 100, 150 C.
5. Extend/refit the plant with >80% measured data before testing 175 C as a
   physical performance requirement.

## Status

Part 10 closed-loop integration: PASS.

Controller performance against identified plant: FAIL current performance
requirements.

Root causes found:

- plant model maximum equilibrium below 175 C;
- absolute-PWM fuzzy table has steady-state bias;
- no integral/static inverse compensation currently active.
