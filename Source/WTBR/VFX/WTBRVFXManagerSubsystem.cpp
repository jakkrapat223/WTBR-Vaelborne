// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "VFX/WTBRVFXManagerSubsystem.h"

#include "Camera/CameraShakeBase.h"
#include "Components/DecalComponent.h"
#include "DrawDebugHelpers.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

bool UWTBRVFXManagerSubsystem::IsRequestVisibleToLocalPlayer(
    const FWTBRImpactVFXRequest& Request) const
{
    const UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_DedicatedServer)
    {
        return false;
    }
    if (Request.MaxDistance <= 0.0f)
    {
        return true;
    }

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        const APlayerController* Controller = It->Get();
        if (!IsValid(Controller) || !Controller->IsLocalController())
        {
            continue;
        }
        const AActor* ViewTarget = Controller->GetViewTarget();
        return !IsValid(ViewTarget)
            || FVector::DistSquared(ViewTarget->GetActorLocation(), Request.Location)
                <= FMath::Square(Request.MaxDistance);
    }

    // A non-dedicated world without a local controller is common during
    // startup/editor preview; do not accidentally suppress the cosmetic.
    return true;
}

bool UWTBRVFXManagerSubsystem::SpawnImpact(const FWTBRImpactVFXRequest& Request)
{
    UWorld* World = GetWorld();
    if (!World || !IsRequestVisibleToLocalPlayer(Request))
    {
        return false;
    }

    const FVector SafeNormal = Request.Normal.IsNearlyZero()
        ? FVector::UpVector : Request.Normal.GetSafeNormal();
    const FRotator SurfaceRotation = FRotationMatrix::MakeFromZ(SafeNormal).Rotator();
    const FVector SpawnLocation = Request.Location + (SafeNormal * 1.5f);

    if (IsValid(Request.Effect))
    {
        // AutoRelease opts into Niagara's component pool instead of allocating
        // one component for every projectile impact.
        UNiagaraComponent* NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            World, Request.Effect, SpawnLocation, SurfaceRotation, FVector::OneVector,
            true, true, ENCPoolMethod::AutoRelease, true);
        if (IsValid(NiagaraComponent))
        {
            for (const FWTBRNiagaraAssetParameter& Parameter : Request.AssetParameters)
            {
                if (!Parameter.ParameterName.IsNone() && IsValid(Parameter.Asset))
                {
                    NiagaraComponent->SetVariableObject(Parameter.ParameterName, Parameter.Asset);
                }
            }
        }
    }
    if (IsValid(Request.Sound))
    {
        UGameplayStatics::PlaySoundAtLocation(World, Request.Sound, SpawnLocation);
    }
    if (IsValid(Request.DecalMaterial) && Request.DecalLifeSpan > 0.0f)
    {
        UGameplayStatics::SpawnDecalAtLocation(
            World, Request.DecalMaterial, Request.DecalSize, SpawnLocation,
            SurfaceRotation, Request.DecalLifeSpan);
    }

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* Controller = It->Get();
        if (IsValid(Controller) && Controller->IsLocalController()
            && IsValid(Controller->PlayerCameraManager) && Request.CameraShake)
        {
            Controller->PlayerCameraManager->StartCameraShake(Request.CameraShake);
        }
    }

    if (Request.bDrawDebug)
    {
        DrawDebugPoint(World, SpawnLocation, 12.0f, FColor::Cyan, false, 1.0f);
        DrawDebugLine(World, SpawnLocation, SpawnLocation + (SafeNormal * 45.0f),
            FColor::Yellow, false, 1.0f, 0, 1.5f);
        DrawDebugString(World, SpawnLocation + FVector(0.0f, 0.0f, 20.0f),
            FString::Printf(TEXT("VFX Surface=%d"), Request.SurfaceType), nullptr,
            FColor::White, 1.0f, false);
    }
    return true;
}
