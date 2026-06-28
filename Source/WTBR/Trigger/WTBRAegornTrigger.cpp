// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRAegornTrigger.h"
#include "WTBRValidationLog.h"
#include "Actors/WTBRAegornWallActor.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/World.h"

void UWTBRAegornTrigger::InitializeTrigger(
    AWTBRCharacter* InOwnerCharacter,
    UWTBRTriggerDataAsset* InDataAsset)
{
    Super::InitializeTrigger(InOwnerCharacter, InDataAsset);
    if (!IsValid(InDataAsset)) return;

    VaelDrainPerTick = InDataAsset->VaelCostPerUse / 10.0f;

    if (OwnerCharacter.IsValid())
    {
        ShieldCollisionBox = NewObject<UBoxComponent>(
            OwnerCharacter.Get(), TEXT("AegornShieldCollision"));
        if (IsValid(ShieldCollisionBox))
        {
            ShieldCollisionBox->RegisterComponent();
            ShieldCollisionBox->AttachToComponent(
                OwnerCharacter->GetRootComponent(),
                FAttachmentTransformRules::KeepRelativeTransform);
            ShieldCollisionBox->ComponentTags.Add(TEXT("AegornShield"));
            ShieldCollisionBox->SetCollisionEnabled(
                ECollisionEnabled::NoCollision);
            ShieldCollisionBox->SetRelativeLocation(
                FVector(60.0f, 0.0f, 0.0f));
        }
    }
}

void UWTBRAegornTrigger::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UWTBRAegornTrigger, bShieldActive);
    DOREPLIFETIME(UWTBRAegornTrigger, bIsDualShield);
}

bool UWTBRAegornTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    UE_LOG(LogTemp, Warning, TEXT("[Aegorn] Activate_Implementation called"));
    if (!OwnerCharacter.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Aegorn] FAIL: OwnerCharacter is null"));
        return false;
    }
    if (!OwnerCharacter->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Aegorn] FAIL: No Authority"));
        return false;
    }
    if (bShieldActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Aegorn] Shield already active"));
        return true;
    }
    UE_LOG(LogTemp, Warning, TEXT("[Aegorn] Calling RaiseShield"));
    RaiseShield(bIsDualWield);
    return true;
}

void UWTBRAegornTrigger::OnReleased_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid()) return;
    if (!OwnerCharacter->HasAuthority()) return;
    LowerShield();
}

void UWTBRAegornTrigger::Deactivate_Implementation()
{
    LowerShield();
    Super::Deactivate_Implementation();
}

bool UWTBRAegornTrigger::CancelShield()
{
    if (!OwnerCharacter.IsValid())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Cancel Test] Blocked | Reason=InvalidOwner | Trigger=Aegorn"));
        return false;
    }
    if (!OwnerCharacter->HasAuthority())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Cancel Test] Blocked | Reason=NoAuthority | Trigger=Aegorn | Owner=%s"),
            *GetNameSafe(OwnerCharacter.Get()));
        return false;
    }
    if (!bShieldActive)
    {
        return false;
    }

    LowerShield();
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Cancel Test] AegornShieldCanceled | Owner=%s | ShieldActive=false"),
        *GetNameSafe(OwnerCharacter.Get()));
    return true;
}

void UWTBRAegornTrigger::RaiseShield(bool bDualWield)
{
    UE_LOG(LogTemp, Warning, TEXT("[Aegorn] RaiseShield called — VaelDrainPerTick: %.1f"), VaelDrainPerTick);
    bShieldActive = true;
    bIsDualShield = bDualWield;
    UpdateShieldCollisionSize(bDualWield);
    if (IsValid(ShieldCollisionBox))
        ShieldCollisionBox->SetCollisionEnabled(
            ECollisionEnabled::QueryOnly);
    StartVaelDrain();
    OnShieldChanged.Broadcast(true);
    OnAegornShieldRaised(bDualWield);
}

void UWTBRAegornTrigger::LowerShield()
{
    if (!bShieldActive) return;
    bShieldActive = false;
    bIsDualShield = false;
    if (IsValid(ShieldCollisionBox))
        ShieldCollisionBox->SetCollisionEnabled(
            ECollisionEnabled::NoCollision);
    StopVaelDrain();
    OnShieldChanged.Broadcast(false);
    OnAegornShieldLowered();
}

void UWTBRAegornTrigger::UpdateShieldCollisionSize(bool bDualWield)
{
    if (!IsValid(ShieldCollisionBox)) return;
    const float Width = bDualWield ? 200.0f : 100.0f;
    ShieldCollisionBox->SetBoxExtent(FVector(20.0f, Width, 100.0f));
}

void UWTBRAegornTrigger::StartVaelDrain()
{
    if (!GetWorld()) return;
    GetWorld()->GetTimerManager().SetTimer(
        VaelDrainTimer,
        this, &UWTBRAegornTrigger::TickVaelDrain,
        DRAIN_INTERVAL,
        true);
}

void UWTBRAegornTrigger::StopVaelDrain()
{
    if (!GetWorld()) return;
    GetWorld()->GetTimerManager().ClearTimer(VaelDrainTimer);
}

void UWTBRAegornTrigger::TickVaelDrain()
{
    if (!OwnerCharacter.IsValid()) return;
    UWTBRVaelComponent* Vael = OwnerCharacter->VaelComponent;
    if (!IsValid(Vael)) return;
    UE_LOG(LogTemp, Log, TEXT("[Aegorn] TickVaelDrain — draining %.1f Vael"), VaelDrainPerTick);
    if (!Vael->TryConsumeVael(VaelDrainPerTick))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Aegorn] Vael exhausted — LowerShield"));
        LowerShield();
    }
}

void UWTBRAegornTrigger::PlaceWall()
{
    const AWTBRCharacter* OwnerChar = OwnerCharacter.Get();
    const bool bHasAuthority = OwnerChar && OwnerChar->HasAuthority();
    const float Cost = IsValid(DataAsset) ? DataAsset->VaelCostPerUse : 0.0f;
    UWTBRVaelComponent* Vael = OwnerCharacter.IsValid() ? OwnerCharacter->VaelComponent : nullptr;
    const float CurrentVael = IsValid(Vael) ? Vael->GetCurrentVael() : -1.0f;

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] PlaceWall Start | Owner=%s | Authority=%s | CurrentVael=%.2f | Cost=%.2f"),
        *GetNameSafe(OwnerChar),
        bHasAuthority ? TEXT("true") : TEXT("false"),
        CurrentVael,
        Cost);

    if (!OwnerCharacter.IsValid()) return;
    if (!OwnerCharacter->HasAuthority()) return;
    if (!IsValid(DataAsset))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] PlaceWall Abort | Owner=%s | Reason=DataAsset invalid"),
            *GetNameSafe(OwnerCharacter.Get()));
        return;
    }

    if (!IsValid(Vael) || !Vael->TryConsumeVael(DataAsset->VaelCostPerUse))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] ConsumeFail | CurrentVael=%.2f | Cost=%.2f"),
            IsValid(Vael) ? Vael->GetCurrentVael() : -1.0f,
            DataAsset->VaelCostPerUse);
        return;
    }

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] ConsumeSuccess | NewVael=%.2f | Cost=%.2f"),
        Vael->GetCurrentVael(),
        DataAsset->VaelCostPerUse);

    if (WallActorClass.IsNull())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] SpawnFail | Class=null | Reason=WallActorClass null"));
        return;
    }
    TSubclassOf<AWTBRAegornWallActor> WallClass =
        WallActorClass.LoadSynchronous();
    if (!IsValid(WallClass))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] SpawnFail | Class=%s | Reason=Load failed"),
            *WallActorClass.ToString());
        return;
    }

    const FVector SpawnLoc = OwnerCharacter->GetActorLocation()
        + OwnerCharacter->GetActorForwardVector() * 150.0f;
    const FRotator SpawnRot = OwnerCharacter->GetActorRotation();

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] SpawnAttempt | Class=%s | Location=%s | Rotation=%s"),
        *GetNameSafe(WallClass.Get()),
        *SpawnLoc.ToString(),
        *SpawnRot.ToString());

    FActorSpawnParameters Params;
    Params.Owner      = OwnerCharacter.Get();
    Params.Instigator = OwnerCharacter.Get();
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AWTBRAegornWallActor* Wall =
        GetWorld()->SpawnActor<AWTBRAegornWallActor>(
            WallClass, SpawnLoc, SpawnRot, Params);

    if (IsValid(Wall))
    {
        Wall->InitializeWall(DataAsset->AegornParams.AegornWallHP);
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] SpawnSuccess | Wall=%s | Location=%s | Rotation=%s | WallHP=%.1f | MaxWallHP=%.1f | Replicates=%s | AlwaysRelevant=%s | OnlyRelevantToOwner=%s | NetCullDistanceSq=%.1f"),
            *GetNameSafe(Wall),
            *Wall->GetActorLocation().ToString(),
            *Wall->GetActorRotation().ToString(),
            Wall->WallHP,
            Wall->MaxWallHP,
            Wall->GetIsReplicated() ? TEXT("true") : TEXT("false"),
            Wall->bAlwaysRelevant ? TEXT("true") : TEXT("false"),
            Wall->bOnlyRelevantToOwner ? TEXT("true") : TEXT("false"),
            Wall->NetCullDistanceSquared);
        OnWallPlaced.Broadcast();
        OnAegornWallSpawned(Wall);
    }
    else
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] SpawnFail | Class=%s"),
            *GetNameSafe(WallClass.Get()));
    }
}

void UWTBRAegornTrigger::OnRep_bShieldActive()
{
    OnShieldChanged.Broadcast(bShieldActive);
}
