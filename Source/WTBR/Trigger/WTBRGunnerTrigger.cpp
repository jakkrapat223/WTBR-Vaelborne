// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRGunnerTrigger.h"
#include "Actors/WTBRProjectileBase.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
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

TArray<AWTBRProjectileBase*> UWTBRGunnerTrigger::FireProjectileVolley(
    TSubclassOf<AWTBRProjectileBase> ProjClass,
    int32 CubeCount,
    float TotalDamage,
    float Speed,
    float ScatterRadius,
    float ConvergeDistance,
    float ImpactSpread,
    bool bExplode,
    float ExplodeRadius)
{
    TArray<AWTBRProjectileBase*> Spawned;
    if (!IsValid(ProjClass)) return Spawned;
    if (!OwnerCharacter.IsValid() || !GetWorld()) return Spawned;

    const int32 Count = FMath::Max(1, CubeCount);
    const FVector Dir = GetFireDirection();
    const FVector Muzzle = GetMuzzleLocation(Dir);
    const FRotationMatrix AimMatrix(Dir.Rotation());

    // Where the volley closes up. Traced, not assumed.
    //
    // A FIXED convergence distance is only accurate at that one distance. Cubes
    // leave the muzzle spread ~135uu wide and close linearly, so at a tenth of the
    // way they are still ~120uu off the aim line while a character capsule is about
    // 34uu across — a point-blank tap sends most of the volley past either side of
    // someone standing right in front of the player. That is the exact opposite of
    // what tap is for.
    //
    // Tracing what the player is actually looking at makes tap behave the same at
    // every range: aim at someone three metres away and the cubes converge at three
    // metres; aim down a corridor and they converge where the corridor ends.
    float Distance = FMath::Max(1.0f, ConvergeDistance);
    {
        FHitResult AimHit;
        FCollisionQueryParams TraceParams(
            SCENE_QUERY_STAT(WTBRVolleyConvergence), /*bTraceComplex=*/false, OwnerCharacter.Get());
        if (GetWorld()->LineTraceSingleByChannel(
                AimHit, Muzzle, Muzzle + Dir * Distance, ECC_Visibility, TraceParams))
        {
            // Floored so that firing while pressed against a wall does not collapse
            // the volley into a single point at the muzzle, which would spawn every
            // cube on top of every other one.
            constexpr float MinConvergeDistance = 200.0f;
            Distance = FMath::Max(MinConvergeDistance, AimHit.Distance);
        }
    }

    const FVector AimPoint = Muzzle + Dir * Distance;

    const float PerCubeDamage = TotalDamage / static_cast<float>(Count);

    Spawned.Reserve(Count);
    for (int32 CubeIndex = 0; CubeIndex < Count; ++CubeIndex)
    {
        // ONE direction per cube, used for both ends of its flight. A cube conjured
        // on the upper left also lands on the upper left, so the volley holds its
        // formation the whole way instead of crossing through itself.
        //
        // An earlier version walked the spiral backwards for the impact point, to
        // decorrelate the two. It looked wrong in play: the cubes pinched into a
        // waist mid-flight and opened back out, reading as a volley that had drifted
        // rather than one that was aimed. Straight and parallel is what a cluster
        // should look like.
        const FVector CubeAxis =
            UWTBRCompositeRegistryDataAsset::ComputeFibonacciSphereOffset(
                CubeIndex, Count, 1.0f);

        const FVector Origin = Muzzle + AimMatrix.TransformVector(CubeAxis * ScatterRadius);

        // ImpactSpread is the same pattern re-scaled at the far end, so it reads as
        // one aperture: below ScatterRadius the volley closes to a point (Solux,
        // Venyx — a tap that always lands where it was aimed), above it the volley
        // opens into a footprint (Fulgrix's cluster).
        const FVector Target = AimPoint + AimMatrix.TransformVector(CubeAxis * ImpactSpread);

        const FVector CubeDir = (Target - Origin).GetSafeNormal();
        if (CubeDir.IsNearlyZero()) continue;

        const FRotator CubeRot = CubeDir.Rotation();
        const FTransform CubeTransform(CubeRot, Origin);

        AWTBRProjectileBase* Proj = GetWorld()->SpawnActorDeferred<AWTBRProjectileBase>(
            ProjClass, CubeTransform, OwnerCharacter.Get(), OwnerCharacter.Get(),
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!IsValid(Proj)) continue;

        Proj->InitializeProjectile(
            PerCubeDamage, Speed, ETriggerCategory::Gunner,
            /*bSniper=*/false, bExplode, ExplodeRadius);
        Proj->FinishSpawning(CubeTransform);
        Proj->Launch(CubeDir, OwnerCharacter.Get());

        Spawned.Add(Proj);
    }

    // Fired once for the whole volley, not per cube — this drives muzzle VFX and
    // firing a Blueprint event eight times per tap would stack eight flashes.
    if (Spawned.Num() > 0)
    {
        OnGunnerFired(false, Dir);
    }
    return Spawned;
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
