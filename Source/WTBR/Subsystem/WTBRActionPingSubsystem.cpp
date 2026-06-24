// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Subsystem/WTBRActionPingSubsystem.h"

void UWTBRActionPingSubsystem::RegisterActionPing(AActor* Source)
{
    if (IsSignalBlocked(Source)) return;
    OnActionPing.Broadcast(Source);
}

void UWTBRActionPingSubsystem::RegisterSignalBlock(AActor* Actor)
{
    if (!IsValid(Actor)) return;
    BlockedActors.Add(TWeakObjectPtr<AActor>(Actor));
}

void UWTBRActionPingSubsystem::UnregisterSignalBlock(AActor* Actor)
{
    BlockedActors.Remove(TWeakObjectPtr<AActor>(Actor));
}

bool UWTBRActionPingSubsystem::IsSignalBlocked(AActor* Actor) const
{
    if (!IsValid(Actor)) return false;
    const TWeakObjectPtr<AActor>* Found = BlockedActors.Find(TWeakObjectPtr<AActor>(Actor));
    return Found != nullptr && Found->IsValid();
}
