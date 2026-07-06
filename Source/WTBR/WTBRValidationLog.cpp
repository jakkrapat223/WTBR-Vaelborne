// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "WTBRValidationLog.h"

// ECVF_Cheat: this CVar also gates the Server_Debug* RPCs (Vael refill/reset etc.),
// so it must not be settable by players in Shipping/Test builds.
static TAutoConsoleVariable<int32> CVarWTBRValidationLogs(
    TEXT("wtbr.Debug.ValidationLogs"),
    0,
    TEXT("Enables temporary validation logs for multiplayer damage, triggers, HUD hints, and cleanup test groups."),
    ECVF_Cheat);

bool WTBRShouldLogValidation()
{
    return CVarWTBRValidationLogs.GetValueOnAnyThread() != 0;
}
