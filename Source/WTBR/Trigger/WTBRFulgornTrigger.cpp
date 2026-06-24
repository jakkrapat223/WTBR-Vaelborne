// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRFulgornTrigger.h"
#include "Actors/WTBRProjectileBase.h"
#include "Subsystem/WTBRActionPingSubsystem.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Engine/World.h"

bool UWTBRFulgornTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    if (!IsValid(DataAsset)) return false;

    const FWTBRFulgornParams& Params = DataAsset->FulgornParams;
    if (!Params.FulgornProjectileClass) return false;
    if (Params.FulgornMaxTargets <= 0) return false;

    UWTBRVaelComponent* VaelComp = OwnerCharacter->VaelComponent;
    if (!IsValid(VaelComp) || !VaelComp->TryConsumeVael(Params.FulgornVaelCost)) return false;

    UWorld* World = OwnerCharacter->GetWorld();
    const FVector Origin = OwnerCharacter->GetActorLocation();

    // Sphere overlap — find all nearby pawns within lock radius
    TArray<FHitResult> Hits;
    FCollisionObjectQueryParams ObjParams(ECC_Pawn);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter.Get());

    World->SweepMultiByObjectType(
        Hits, Origin, Origin, FQuat::Identity,
        ObjParams,
        FCollisionShape::MakeSphere(Params.FulgornLockRadius),
        QueryParams);

    // Deduplicate and cap at FulgornMaxTargets
    TArray<AWTBRCharacter*> Targets;
    TSet<AWTBRCharacter*> Seen;
    for (const FHitResult& Hit : Hits)
    {
        AWTBRCharacter* Target = Cast<AWTBRCharacter>(Hit.GetActor());
        if (!IsValid(Target) || Seen.Contains(Target)) continue;
        Seen.Add(Target);
        Targets.Add(Target);
        if (Targets.Num() >= Params.FulgornMaxTargets) break;
    }

    OnFulgornLockOn(Targets.Num());

    const FVector Forward = OwnerCharacter->GetActorForwardVector();

    for (AWTBRCharacter* Target : Targets)
    {
        const FVector SpawnOrigin = Origin + Forward * 100.0f;
        const FTransform SpawnTF(Forward.Rotation(), SpawnOrigin);

        AWTBRProjectileBase* Proj = World->SpawnActorDeferred<AWTBRProjectileBase>(
            Params.FulgornProjectileClass, SpawnTF, nullptr, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!IsValid(Proj)) continue;

        Proj->MaxRange = Params.FulgornRange;
        Proj->InitializeProjectile(Params.FulgornDamage, Params.FulgornProjectileSpeed,
            ETriggerCategory::BlackTrigger, false, false, 0.0f);
        Proj->FinishSpawning(SpawnTF);
        Proj->Launch(Forward, OwnerCharacter.Get());
        Proj->EnableHoming(Target->GetRootComponent(), Params.FulgornHomingAcceleration);
    }

    if (UWTBRActionPingSubsystem* PingSys = World->GetSubsystem<UWTBRActionPingSubsystem>())
        PingSys->RegisterActionPing(OwnerCharacter.Get());

    OnFulgornFired();
    return true;
}
