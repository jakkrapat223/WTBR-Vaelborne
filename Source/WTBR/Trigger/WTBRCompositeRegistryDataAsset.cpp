// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "Math/RotationMatrix.h"

EWTBRCompositeBulletType UWTBRCompositeRegistryDataAsset::ResolveCompositeType(
    EWTBRBulletArchetype ArchetypeA,
    EWTBRBulletArchetype ArchetypeB) const
{
    if (ArchetypeA == EWTBRBulletArchetype::None ||
        ArchetypeB == EWTBRBulletArchetype::None ||
        ArchetypeA == EWTBRBulletArchetype::NonCombinable ||
        ArchetypeB == EWTBRBulletArchetype::NonCombinable)
    {
        return EWTBRCompositeBulletType::None;
    }

    for (const FWTBRCompositeDefinition& Definition : Definitions)
    {
        const bool bMatchesForward =
            Definition.RequiredArchetypeA == ArchetypeA &&
            Definition.RequiredArchetypeB == ArchetypeB;
        const bool bMatchesReverse =
            Definition.RequiredArchetypeA == ArchetypeB &&
            Definition.RequiredArchetypeB == ArchetypeA;

        if ((bMatchesForward || bMatchesReverse) &&
            Definition.CompositeType != EWTBRCompositeBulletType::None)
        {
            return Definition.CompositeType;
        }
    }

    return EWTBRCompositeBulletType::None;
}

bool UWTBRCompositeRegistryDataAsset::FindDefinition(
    EWTBRCompositeBulletType Type, FWTBRCompositeDefinition& OutDefinition) const
{
    if (Type == EWTBRCompositeBulletType::None) return false;

    for (const FWTBRCompositeDefinition& Definition : Definitions)
    {
        if (Definition.CompositeType == Type)
        {
            OutDefinition = Definition;
            return true;
        }
    }
    return false;
}

void UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
    const FWTBRPathPreset& Preset,
    const FVector& SpawnOrigin,
    const FRotator& AimRotation,
    float Range,
    TArray<TArray<FVector>>& OutCubeWorldPaths)
{
    OutCubeWorldPaths.Reset();
    if (Range <= 0.0f) return;

    const FRotationMatrix AimMatrix(AimRotation);

    for (const FWTBRPathLane& Lane : Preset.Lanes)
    {
        if (Lane.NormalizedWaypoints.Num() < 2) continue;

        const int32 CubeCount = FMath::Max(1, Lane.CubeCount);
        for (int32 CubeIndex = 0; CubeIndex < CubeCount; ++CubeIndex)
        {
            const float FormationStep = CubeIndex - (CubeCount - 1) * 0.5f;
            const FVector CubeOrigin = SpawnOrigin
                + AimMatrix.TransformVector(Lane.FormationOffset * FormationStep);

            TArray<FVector> WorldWaypoints;
            WorldWaypoints.Reserve(Lane.NormalizedWaypoints.Num());
            for (const FVector& Normalized : Lane.NormalizedWaypoints)
            {
                WorldWaypoints.Add(CubeOrigin + AimMatrix.TransformVector(Normalized * Range));
            }
            OutCubeWorldPaths.Add(MoveTemp(WorldWaypoints));
        }
    }
}
