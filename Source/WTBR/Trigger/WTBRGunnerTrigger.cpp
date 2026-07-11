// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRGunnerTrigger.h"
#include "Actors/WTBRProjectileBase.h"
#include "WTBRCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Math/UnrealMathUtility.h"

bool UWTBRGunnerTrigger::Activate_Implementation(
    const FInputActionValue& InputValue, bool bIsDualWield)
{
    return false;
}

FVector UWTBRGunnerTrigger::GetFireDirection() const
{
    if (!OwnerCharacter.IsValid()) return FVector::ForwardVector;
    if (AController* Ctrl = OwnerCharacter->GetController())
        return Ctrl->GetControlRotation().Vector();
    return OwnerCharacter->GetActorForwardVector();
}

FVector UWTBRGunnerTrigger::GetMuzzleLocation(const FVector& AimDirection) const
{
    if (!OwnerCharacter.IsValid()) return FVector::ZeroVector;

    static const FName HandSocketName(TEXT("hand_r"));
    if (const USkeletalMeshComponent* CharacterMesh = OwnerCharacter->GetMesh();
        IsValid(CharacterMesh) && CharacterMesh->DoesSocketExist(HandSocketName))
    {
        return CharacterMesh->GetSocketLocation(HandSocketName);
    }

    FVector EyeLoc;
    FRotator EyeRot;
    OwnerCharacter->GetActorEyesViewPoint(EyeLoc, EyeRot);

    UE_LOG(LogTemp, Warning,
        TEXT("[GunnerTrigger] Hand socket fallback | Owner=%s | Socket=%s | Reason=MeshMissingOrSocketAbsent"),
        *GetNameSafe(OwnerCharacter.Get()),
        *HandSocketName.ToString());

    return EyeLoc + (AimDirection * 100.0f);
}

AWTBRProjectileBase* UWTBRGunnerTrigger::FireProjectile(
    TSubclassOf<AWTBRProjectileBase> ProjClass,
    float Damage,
    float Speed,
    float SpreadAngle,
    bool bExplode,
    float ExplodeRadius)
{
    if (!IsValid(ProjClass)) return nullptr;
    if (!OwnerCharacter.IsValid() || !GetWorld()) return nullptr;

    FVector Dir = GetFireDirection();
    if (SpreadAngle > 0.0f)
        Dir = FMath::VRandCone(Dir, FMath::DegreesToRadians(SpreadAngle)).GetSafeNormal();

    const FVector  Loc      = GetMuzzleLocation(Dir);
    const FRotator SpawnRot = Dir.Rotation();

    AWTBRProjectileBase* Proj = GetWorld()->SpawnActorDeferred<AWTBRProjectileBase>(
        ProjClass,
        FTransform(SpawnRot, Loc),
        OwnerCharacter.Get(),
        OwnerCharacter.Get(),
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

    if (!IsValid(Proj)) return nullptr;

    Proj->InitializeProjectile(Damage, Speed, ETriggerCategory::Gunner, false, bExplode, ExplodeRadius);
    Proj->FinishSpawning(FTransform(SpawnRot, Loc));
    Proj->Launch(Dir, OwnerCharacter.Get());

    OnGunnerFired(false, Dir);
    return Proj;
}

void UWTBRGunnerTrigger::StartCooldown()
{
    if (!GetWorld()) return;
    bIsOnCooldown = true;
    GetWorld()->GetTimerManager().SetTimer(
        CooldownTimer,
        this, &UWTBRGunnerTrigger::OnCooldownExpired,
        GetCooldownDuration(),
        false);
}

void UWTBRGunnerTrigger::OnCooldownExpired()
{
    bIsOnCooldown = false;
}
