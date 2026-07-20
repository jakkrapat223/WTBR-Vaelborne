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

FVector UWTBRCompositeRegistryDataAsset::ComputeFibonacciSphereOffset(
    int32 Index, int32 Total, float Radius)
{
    if (Total < 2) return FVector::ZeroVector;

    static const float GoldenAngle = PI * (3.0f - FMath::Sqrt(5.0f));

    const float Z = 1.0f - (2.0f * Index) / static_cast<float>(Total - 1);
    const float RingRadius = FMath::Sqrt(FMath::Max(0.0f, 1.0f - Z * Z));
    const float Theta = GoldenAngle * Index;

    return FVector(
        FMath::Cos(Theta) * RingRadius,
        FMath::Sin(Theta) * RingRadius,
        Z) * Radius;
}

void UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
    const FWTBRPathPreset& Preset,
    const FVector& SpawnOrigin,
    const FRotator& AimRotation,
    float Range,
    TArray<TArray<FVector>>& OutCubeWorldPaths,
    float ScatterRadius,
    bool bIsMainSlot)
{
    OutCubeWorldPaths.Reset();
    if (Range <= 0.0f) return;

    const FRotationMatrix AimMatrix(AimRotation);
    const bool bUseScatter = ScatterRadius > 0.0f;

    // Main takes the even indices of a double-sized sphere, Sub the odd ones, so
    // a simultaneous Main+Sub volley interleaves instead of spawning into itself.
    int32 ScatterTotal = 0;
    if (bUseScatter)
    {
        for (const FWTBRPathLane& Lane : Preset.Lanes)
        {
            if (Lane.NormalizedWaypoints.Num() < 2) continue;
            ScatterTotal += FMath::Max(1, Lane.CubeCount);
        }
        ScatterTotal *= 2;
    }

    const int32 LaneCount = Preset.Lanes.Num();
    int32 LaneIndex = INDEX_NONE;

    for (const FWTBRPathLane& Lane : Preset.Lanes)
    {
        ++LaneIndex;
        if (Lane.NormalizedWaypoints.Num() < 2) continue;

        const int32 CubeCount = FMath::Max(1, Lane.CubeCount);
        for (int32 CubeIndex = 0; CubeIndex < CubeCount; ++CubeIndex)
        {
            FVector CubeOrigin;
            if (bUseScatter)
            {
                // Mapping B: stride by lane count, so every lane spans the full
                // height of the sphere. Flip to (LaneIndex * CubeCount + CubeIndex)
                // for mapping A — one narrow height band per lane.
                const int32 LocalIndex = CubeIndex * FMath::Max(1, LaneCount) + LaneIndex;
                const int32 FibIndex = LocalIndex * 2 + (bIsMainSlot ? 0 : 1);
                CubeOrigin = SpawnOrigin + AimMatrix.TransformVector(
                    ComputeFibonacciSphereOffset(FibIndex, ScatterTotal, ScatterRadius));
            }
            else
            {
                const float FormationStep = CubeIndex - (CubeCount - 1) * 0.5f;
                CubeOrigin = SpawnOrigin
                    + AimMatrix.TransformVector(Lane.FormationOffset * FormationStep);
            }

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
