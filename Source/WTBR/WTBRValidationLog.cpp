// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "WTBRValidationLog.h"

static TAutoConsoleVariable<int32> CVarWTBRValidationLogs(
    TEXT("wtbr.Debug.ValidationLogs"),
    0,
    TEXT("Enables temporary validation logs for multiplayer damage, triggers, HUD hints, and cleanup test groups."),
    ECVF_Default);

bool WTBRShouldLogValidation()
{
    return CVarWTBRValidationLogs.GetValueOnAnyThread() != 0;
}
