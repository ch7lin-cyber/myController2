/******************************************************************************
 * File    : FB_FuzzySelfTuningPersistence.c
 * Brief   : Safe persistence wrappers for self-tuning temperature profiles.
 ******************************************************************************/
#include "FB_FuzzySelfTuningBridge.h"

FuzzyProfilePersistenceResult_e FB_FuzzySelfTuningBridge_ExportProfile(
    const FB_FuzzySelfTuningBridge_t *fb,
    uint8_t *buffer,
    size_t capacity,
    size_t *writtenSize)
{
    if (fb == (const FB_FuzzySelfTuningBridge_t *)0)
    {
        return FUZZY_PROFILE_PERSIST_INVALID_ARGUMENT;
    }

    return FB_FuzzyProfilePersistence_Export(
        &fb->TemperatureProfile,
        buffer,
        capacity,
        writtenSize);
}

FuzzyProfilePersistenceResult_e FB_FuzzySelfTuningBridge_ImportProfile(
    FB_FuzzySelfTuningBridge_t *fb,
    const uint8_t *buffer,
    size_t size)
{
    FuzzyProfilePersistenceResult_e result;

    if (fb == (FB_FuzzySelfTuningBridge_t *)0)
    {
        return FUZZY_PROFILE_PERSIST_INVALID_ARGUMENT;
    }

    if (!fb->Initialized)
    {
        FB_FuzzySelfTuningBridge_Init(fb);
    }

    /* Do not replace profile context during an active tuning decision. */
    if (fb->Status.EpisodeActive ||
        fb->Status.CandidateAvailable ||
        fb->Status.CandidateApplied ||
        fb->Status.RollbackRecommended ||
        fb->Tuner.Status.CandidatePending ||
        fb->HasApplyBackup)
    {
        return FUZZY_PROFILE_PERSIST_BUSY;
    }

    result = FB_FuzzyProfilePersistence_Import(
        &fb->TemperatureProfile,
        buffer,
        size);

    if (result != FUZZY_PROFILE_PERSIST_OK)
    {
        return result;
    }

    /* Imported profile does not imply a runtime region or candidate selection. */
    fb->Status.ActiveRegion = FUZZY_SELF_TUNING_REGION_INVALID;
    fb->Status.CandidateRegion = FUZZY_SELF_TUNING_REGION_INVALID;
    fb->Status.ActiveRegionConfidence = 0.0f;
    fb->Status.CandidateRegionConfidence = 0.0f;

    return FUZZY_PROFILE_PERSIST_OK;
}
