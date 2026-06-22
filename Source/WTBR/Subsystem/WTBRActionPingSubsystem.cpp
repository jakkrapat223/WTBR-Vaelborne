// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Subsystem/WTBRActionPingSubsystem.h"

void UWTBRActionPingSubsystem::RegisterActionPing(AActor* Source)
{
    OnActionPing.Broadcast(Source);
}
