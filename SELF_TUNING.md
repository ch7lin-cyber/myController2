# Fuzzy Self-Tuning Architecture — branch4_selfTuning

## 1. Design goal

`branch4_selfTuning` adds a slow supervisory learning layer above the existing fast Fuzzy / Auto Scaling controller.

The most important rule is:

> **Self tuning never changes active controller parameters automatically.**

The default mode is `ShadowMode = true`.

The controller continues to run normally while the self tuner observes `SV / PV / PWM`, measures response quality, and may produce a candidate parameter set for review.

## 2. Control hierarchy

```text
20 ms fast loop

SV / PV
  |
  v
FB_FuzzyController
  |
  +-- Auto / Adaptive Scaling
  |     Ke / Kde / Ku / ErrorWindow
  |
  +-- Membership
  +-- 7x7 Sugeno Rule
  +-- Hybrid / Approach Output
  |
  v
PWM -> Plant -> PV

Slow supervisory layer

SV / PV / PWM
  |
  v
FB_FuzzyPerformanceMonitor
  |
  v
FB_FuzzySelfTuner
  |
  v
Candidate parameters
  |
  +-- default: Shadow only, no write
  |
  +-- explicit ApplyCandidate()
          |
          v
   persistent SelfTune trims
          |
          v
   Auto Scaling remains active
```

## 3. Tunable parameters — phase 1

The first self-tuning phase manages:

- `Ke`
- `Kde`
- `Ku`
- `ErrorWindow`
- `FullPowerErrorRatio`
- `PrecisionErrorRatio`

The Fuzzy 7x7 rule table and membership functions are intentionally not self-modified in this phase.

## 4. Fast adaptation versus slow learning

The existing `FB_FuzzyScaling` remains the fast adaptation layer.

`branch4` adds persistent supervisory multipliers:

```c
SelfTuneKeTrim
SelfTuneKdeTrim
SelfTuneKuTrim
SelfTuneErrorWindowTrim
```

Default value:

```text
1.00 = no supervisory correction
```

Allowed range:

```text
0.50 .. 1.50
```

The effective design is:

```text
Fast Auto/Adaptive Scaling target
              x
Slow persistent SelfTuneTrim
              =
Final scaling target
```

Therefore Self-Tuning does not have to disable the existing adaptive controller.

## 5. Performance metrics

`FB_FuzzyPerformanceMonitor` records one response episode and calculates:

- Overshoot
- Undershoot
- Rise time
- Settling time
- Steady-state error
- IAE
- ISE
- PWM activity
- Maximum PV rate
- Error zero-cross count

If SV changes again during an active episode, the old episode is discarded and a new episode starts. Two different setpoint responses are never mixed into one metric set.

## 6. Normalized cost

Raw IAE cannot be compared directly across different SV step magnitudes.

The self tuner therefore uses normalized terms:

```text
Normalized IAE = IAE / |TargetSV - StartPV|

Normalized PWM Activity = PWMActivity / SampleCount
```

The overall cost includes penalties for:

```text
Overshoot
Rise time
Settling time
Normalized IAE
Steady-state error
Normalized PWM activity
Zero-cross / oscillation
```

## 7. Comparable verification episodes

A candidate is not accepted or rejected from an unrelated operating condition.

Candidate verification requires a sufficiently similar response episode using:

```text
VerificationTargetTolerance_c
VerificationStepRatioTolerance
MinimumStepMagnitude_c
```

Default values:

```text
Target SV tolerance       = 10 C
Step magnitude tolerance  = 25 %
Minimum useful step       = 2 C
```

If an episode is not comparable:

```text
VerificationDeferred = true
```

The candidate remains pending. It is not accepted and it is not rolled back.

## 8. Candidate lifecycle

### State A — baseline

A completed valid episode establishes the baseline performance and controller snapshot.

### State B — suggested candidate

A later poor episode may produce a candidate.

```text
CandidateAvailable = true
CandidateApplied   = false
```

At this point the active controller remains unchanged.

New learning episodes are frozen until the application explicitly chooses one of:

```c
FB_FuzzySelfTuningBridge_RejectCandidate(...)
```

or

```c
FB_FuzzySelfTuningBridge_ApplyCandidate(...)
```

### State C — explicit apply

Apply is rejected while `ShadowMode == true`.

The application must explicitly execute:

```c
FB_FuzzySelfTuningBridge_SetShadowMode(&selfTuning, false);
FB_FuzzySelfTuningBridge_ApplyCandidate(&selfTuning, &controller);
```

The bridge stores an exact rollback snapshot before changing anything.

The candidate is converted into slow persistent scaling trims; fast Auto Scaling continues to run.

### State D — verification

After an explicit apply, a comparable response episode is evaluated.

If performance improves sufficiently:

```text
ACCEPT
```

The new trim becomes the learned baseline.

If performance becomes worse:

```text
RollbackRecommended = true
```

**No physical rollback occurs automatically.**

The application must explicitly call:

```c
FB_FuzzySelfTuningBridge_Rollback(&selfTuning, &controller);
```

The exact pre-apply scaling configuration and approach ratios are then restored.

## 9. 20 ms application loop

Recommended order:

```c
pv = Temperature_GetC();

pwm = FB_FuzzyController_Run(
    &controller,
    sv,
    pv);

FB_FuzzySelfTuningBridge_Run(
    &selfTuning,
    &controller,
    sv,
    pv,
    pwm);

PWM_SetDuty((uint16_t)pwm);
```

The bridge must observe the same PWM command that is sent to the plant.

The performance monitor automatically synchronizes its `Ts` with `controller.config.Ts` when a new episode starts.

## 10. Recommended production policy

For early validation and production commissioning:

```text
ShadowMode = true
```

Recommended HMI / service workflow:

```text
1. Show baseline metrics
2. Show current parameters
3. Show candidate parameters
4. Show predicted direction of change
5. Operator/service selects Apply or Reject
6. After Apply, collect verification episode
7. Show Accepted or Rollback Recommended
8. Rollback remains an explicit command
```

Do not enable unattended automatic Apply until the plant model, parameter bounds, region scheduling, fault handling, and long-duration regression tests have all been validated.

## 11. Current phase and next phase

Implemented in Phase 1:

- Shadow observation
- Episode performance metrics
- Bounded parameter candidate generation
- Persistent slow trims over Auto Scaling
- Explicit Apply / Reject
- Comparable verification episodes
- Explicit rollback with exact configuration restore
- Cross-platform C/C++ regression tests

Recommended Phase 2:

- Temperature-region learned profiles
- Persistent storage / NVM versioning of learned trims
- Confidence score and minimum episode count
- Load-change classification
- Learned feed-forward table correction
- HMI / Modbus diagnostic registers for metrics and candidate review
