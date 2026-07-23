// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Subsystem/WTBRActionPingSubsystem.h"

void UWTBRActionPingSubsystem::RegisterActionPing(AActor* Source)
{
    if (!IsValid(Source)) return;
    OnActionPing.Broadcast(Source);
    OnActionPingNative.Broadcast(Source);
}
