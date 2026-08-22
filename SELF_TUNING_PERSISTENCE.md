# Self-Tuning Profile Persistence

`branch4_selfTuning` persists learned temperature-region profiles without changing the controller automatically.

## Safety boundary

Persistence saves and restores only `FB_FuzzyTemperatureProfile_t` learning data. Import does **not**:

- disable Shadow Mode,
- create a Candidate,
- apply `Ke/Kde/Ku/ErrorWindow`,
- change approach ratios,
- start verification,
- write PWM.

`FB_FuzzySelfTuningBridge_ImportProfile()` is rejected with `FUZZY_PROFILE_PERSIST_BUSY` while an episode, candidate, apply backup, or rollback decision is active.

## Binary format V1

The format is explicitly serialized instead of writing native C structs, so compiler padding and alignment do not become part of the stored format.

Header (16 bytes):

- Magic: `FSTP`
- Version: `1`
- Header size
- Payload size
- CRC32 of payload

Payload:

- Region count + reserved bytes
- Fixed 48-byte records for each region
  - Min/Max temperature
  - Learned `Ke/Kde/Ku/ErrorWindow`
  - Learned `FullPowerErrorRatio/PrecisionErrorRatio`
  - Observation/Accepted/Rollback counters
  - `HasLearnedParameters` flag

Maximum V1 blob size with 8 regions is 404 bytes.

## Confidence is recomputed

`Confidence` itself is deliberately not trusted as persisted state. Import restores the source counters and then lets `FB_FuzzyTemperatureProfile_SetRegions()` recalculate confidence. This prevents a stored blob from claiming a high confidence value inconsistent with its learning history.

## MCU integration

The persistence module is storage-backend agnostic. Application code owns Flash/EEPROM/FRAM/filesystem access.

Typical boot flow:

```c
uint8_t profileBlob[FUZZY_PROFILE_PERSIST_MAX_SIZE];
size_t profileSize = platform_flash_read(profileBlob, sizeof(profileBlob));

FB_FuzzySelfTuningBridge_Init(&HeaterSelfTuning);

if (profileSize > 0U)
{
    (void)FB_FuzzySelfTuningBridge_ImportProfile(
        &HeaterSelfTuning,
        profileBlob,
        profileSize);
}
```

Typical save flow:

```c
uint8_t profileBlob[FUZZY_PROFILE_PERSIST_MAX_SIZE];
size_t written = 0U;

if (FB_FuzzySelfTuningBridge_ExportProfile(
        &HeaterSelfTuning,
        profileBlob,
        sizeof(profileBlob),
        &written) == FUZZY_PROFILE_PERSIST_OK)
{
    platform_flash_write(profileBlob, written);
}
```

## Flash wear rule

Do not save every 20 ms control cycle and do not save every observation. Persist only at controlled application events, for example:

- after a verified Candidate is accepted,
- after an explicit rollback updates learning history,
- before a clean shutdown when profile data changed,
- on a slow maintenance interval if dirty-state tracking is used.

For raw MCU Flash, use a two-slot or wear-levelled scheme in the platform layer so a power failure during erase/program does not destroy the last valid profile.

## Import validation

Import rejects:

- wrong magic,
- unsupported version,
- invalid length,
- CRC mismatch,
- invalid region count/layout,
- impossible counters,
- out-of-bounds learned parameters,
- invalid learned approach ratio relationship.

The live profile is replaced only after the complete blob has passed validation, so failed imports leave the existing RAM profile intact.
