# Temperature-Region Self-Tuning Profile

## Purpose

`branch4_selfTuning` keeps fast fuzzy Auto/Adaptive Scaling separate from slow self-tuning learning.

The temperature profile layer stores accepted slow-tuning results by operating temperature region and exposes them as **read-only recommendations**.

It does **not** automatically modify controller parameters.

## Default temperature regions

| Region | Temperature range | Center anchor |
|---|---:|---:|
| 0 | 50 to <80 C | 65 C |
| 1 | 80 to <120 C | 100 C |
| 2 | 120 to <160 C | 140 C |
| 3 | 160 to <200 C | 180 C |
| 4 | 200 to 250 C | 225 C |

The last region includes 250 C.

SV values outside 50 to 250 C are not learned by the default profile.

## Per-region learned state

Each region stores:

- `LearnedParameters`
  - `Ke`
  - `Kde`
  - `Ku`
  - `ErrorWindow`
  - `FullPowerErrorRatio`
  - `PrecisionErrorRatio`
- `ObservationCount`
- `AcceptedCount`
- `RollbackCount`
- `Confidence`
- `HasLearnedParameters`

Only a candidate that was explicitly applied and subsequently verified as better is recorded as accepted learned data.

A rollback is counted only when the application explicitly performs physical rollback.

## Confidence

Confidence combines:

1. successful accepted ratio,
2. accumulated observation experience,
3. rollback penalty.

A single successful run intentionally does not produce high confidence.

Default display/recommendation levels are:

- confidence < 0.30: `NONE`
- 0.30 <= confidence < 0.70: `EXPERIMENTAL`
- confidence >= 0.70: `HIGH_CONFIDENCE`

These labels are advisory only. They do not trigger controller writes.

## Direct recommendation

Profile API:

```c
FuzzyTemperatureRecommendation_t recommendation;

FB_FuzzyTemperatureProfile_GetRecommendation(
    &profile,
    sv,
    0.30f,
    0.70f,
    &recommendation);
```

Bridge convenience API:

```c
FuzzyTemperatureRecommendation_t recommendation;

FB_FuzzySelfTuningBridge_GetRecommendation(
    &selfTuning,
    sv,
    &recommendation);
```

The bridge API uses the default 0.30 / 0.70 confidence thresholds.

Both APIs are **read-only**.

They do not:

- change `Ke/Kde/Ku`,
- change SelfTune trims,
- create a candidate,
- apply a candidate,
- change Shadow Mode,
- increment observation/accept/rollback counters.

## Smooth interpolated recommendation

Using a hard region switch can create a parameter discontinuity around boundaries such as 160 C.

The interpolation API therefore treats each learned region as an anchor at the region center.

Example:

```text
Region 2 center = 140 C
Region 3 center = 180 C

SV = 160 C
blend = (160 - 140) / (180 - 140)
      = 0.5
```

Each tunable parameter is linearly interpolated:

```text
P(SV) = P_lower + blend * (P_upper - P_lower)
```

API:

```c
FuzzyTemperatureInterpolatedRecommendation_t recommendation;

FB_FuzzySelfTuningBridge_GetInterpolatedRecommendation(
    &selfTuning,
    sv,
    &recommendation);
```

The result reports:

- containing `RegionIndex`,
- `LowerRegionIndex`,
- `UpperRegionIndex`,
- `BlendFactor`,
- `Interpolated`,
- interpolated parameters,
- effective confidence,
- recommendation level.

## Conservative interpolation rules

Interpolation occurs only when both adjacent source regions contain accepted learned parameters.

If either source region has no learned parameters, the API falls back to the direct recommendation for the containing region.

Effective interpolation confidence is:

```text
min(lowerRegion.Confidence, upperRegion.Confidence)
```

This is intentionally conservative. Interpolation must never appear more trustworthy than its weaker source profile.

Interpolation also remains **read-only**. It is not an automatic gain scheduler.

## Controller safety boundary

The architecture remains:

```text
Controller 20 ms loop
    |
    +--> fast Auto / Adaptive Scaling
    |
    +--> PWM output
    |
    +--> Performance Monitor
             |
             v
         Self Tuner
             |
             v
          Candidate
             |
       explicit decision
        /           \
     Reject       Apply
                    |
                 Verify
                /      \
             Accept   RollbackRecommended
                         |
                   explicit Rollback
```

Temperature profile recommendations are outside the write path:

```text
Temperature Profile
       |
       v
Read-only Recommendation
       |
       v
HMI / Python / Application decision
```

No profile recommendation API calls `ApplyCandidate()` internally.

## Future safe extension

A future phase may add an explicit API that converts a high-confidence profile recommendation into a candidate. That operation should still follow the same safety gates:

1. create candidate only,
2. Shadow Mode remains default,
3. application explicitly applies,
4. comparable verification episode is required,
5. accept or explicit rollback.

Direct automatic loading of learned parameters on every SV change is intentionally not part of this phase.
