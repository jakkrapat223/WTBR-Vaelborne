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

void UWTBRCompositeRegistryDataAsset::ClampLaneTurns(
    const TArray<FVector>& InWaypoints, int32 MaxTurns, TArray<FVector>& OutWaypoints)
{
    // Start + turns + end. A lane at or under budget is copied verbatim, which keeps
    // every preset authored before the cap existed bit-identical.
    const int32 Allowed = MaxTurns + 2;
    if (MaxTurns <= 0 || InWaypoints.Num() <= Allowed)
    {
        OutWaypoints = InWaypoints;
        return;
    }

    OutWaypoints.Reset(Allowed);
    for (int32 i = 0; i < Allowed - 1; ++i)
    {
        OutWaypoints.Add(InWaypoints[i]);
    }
    OutWaypoints.Add(InWaypoints.Last());
}

int32 UWTBRCompositeRegistryDataAsset::ComputeTurnBudget(const FWTBRCompositeDefinition& Definition)
{
    int32 ViperCount = 0;
    if (Definition.RequiredArchetypeA == EWTBRBulletArchetype::Serpveil) ++ViperCount;
    if (Definition.RequiredArchetypeB == EWTBRBulletArchetype::Serpveil) ++ViperCount;
    return ViperCount * WTBR_TURNS_PER_VIPER;
}

bool UWTBRCompositeRegistryDataAsset::FiresSingleProjectile(
    const FWTBRCompositeDefinition& Definition)
{
    auto IsMass = [](EWTBRBulletArchetype A)
    {
        return A == EWTBRBulletArchetype::Solux || A == EWTBRBulletArchetype::Fulgrix;
    };
    auto IsViper = [](EWTBRBulletArchetype A)
    {
        return A == EWTBRBulletArchetype::Serpveil;
    };

    const bool bHasMass =
        IsMass(Definition.RequiredArchetypeA) || IsMass(Definition.RequiredArchetypeB);
    const bool bHasViper =
        IsViper(Definition.RequiredArchetypeA) || IsViper(Definition.RequiredArchetypeB);

    return bHasMass && !bHasViper;
}

void UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
    const FWTBRPathPreset& Preset,
    const FVector& SpawnOrigin,
    const FRotator& AimRotation,
    float Range,
    TArray<TArray<FVector>>& OutCubeWorldPaths,
    float ScatterRadius,
    bool bIsMainSlot,
    int32 TotalCubeOverride,
    TArray<FWTBRResolvedCubeLaunch>* OutCubeLaunches,
    int32 MaxTurns)
{
    OutCubeWorldPaths.Reset();
    if (OutCubeLaunches) OutCubeLaunches->Reset();
    if (Range <= 0.0f) return;

    const FRotationMatrix AimMatrix(AimRotation);
    const bool bUseScatter = ScatterRadius > 0.0f;

    // Per-lane cube counts. Normally the authored numbers are used as-is; a
    // composite instead passes ONE total and the authored numbers become weights,
    // so "set 8, get 8" holds no matter how many lanes the chosen preset has while
    // an authored 3:1 split still lands 3:1. Largest-remainder keeps the sum exact.
    TArray<int32> LaneCubeCounts;
    LaneCubeCounts.SetNumZeroed(Preset.Lanes.Num());
    {
        int32 WeightSum = 0;
        for (int32 i = 0; i < Preset.Lanes.Num(); ++i)
        {
            if (Preset.Lanes[i].NormalizedWaypoints.Num() < 2) continue;
            LaneCubeCounts[i] = FMath::Max(1, Preset.Lanes[i].CubeCount);
            WeightSum += LaneCubeCounts[i];
        }

        if (TotalCubeOverride > 0 && WeightSum > 0)
        {
            TArray<float> Remainders;
            Remainders.SetNumZeroed(Preset.Lanes.Num());
            int32 Assigned = 0;
            for (int32 i = 0; i < LaneCubeCounts.Num(); ++i)
            {
                if (LaneCubeCounts[i] <= 0) continue;
                const float Exact =
                    static_cast<float>(TotalCubeOverride) * LaneCubeCounts[i] / WeightSum;
                const int32 Floor = FMath::Max(1, FMath::FloorToInt(Exact));
                Remainders[i] = Exact - Floor;
                LaneCubeCounts[i] = Floor;
                Assigned += Floor;
            }
            // Hand out (or claw back) the rounding difference, biggest remainder first.
            while (Assigned != TotalCubeOverride)
            {
                int32 Best = INDEX_NONE;
                for (int32 i = 0; i < LaneCubeCounts.Num(); ++i)
                {
                    if (LaneCubeCounts[i] <= 0) continue;
                    if (Assigned > TotalCubeOverride && LaneCubeCounts[i] <= 1) continue;
                    if (Best == INDEX_NONE
                        || (Assigned < TotalCubeOverride ? Remainders[i] > Remainders[Best]
                                                         : Remainders[i] < Remainders[Best]))
                    {
                        Best = i;
                    }
                }
                if (Best == INDEX_NONE) break;
                const int32 Step = (Assigned < TotalCubeOverride) ? 1 : -1;
                LaneCubeCounts[Best] += Step;
                // MINUS, not plus. A lane that just won must become a WORSE
                // candidate, or it keeps winning every remaining round and the
                // whole surplus lands on it — 5 equal lanes sharing 8 cubes came
                // out 4/1/1/1/1 instead of 2/2/2/1/1. The count stayed right, so
                // only the shape gave it away. Subtracting works in both
                // directions: handing a cube out drops that lane's remainder,
                // clawing one back (Step = -1) raises it.
                Remainders[Best] -= Step;
                Assigned += Step;
            }
        }
    }

    // Main takes the even indices of a double-sized sphere, Sub the odd ones, so
    // a simultaneous Main+Sub volley interleaves instead of spawning into itself.
    int32 ScatterTotal = 0;
    if (bUseScatter)
    {
        for (const int32 Count : LaneCubeCounts)
        {
            ScatterTotal += FMath::Max(0, Count);
        }
        ScatterTotal *= 2;
    }

    const int32 LaneCount = Preset.Lanes.Num();
    int32 LaneIndex = INDEX_NONE;

    for (const FWTBRPathLane& Lane : Preset.Lanes)
    {
        ++LaneIndex;
        if (Lane.NormalizedWaypoints.Num() < 2) continue;

        // Enforced HERE, at fire time on the server, and not only in the preset
        // editor: the editor's counter is what the player sees, but a client that
        // sends a preset with twenty turns must not get twenty turns. Same reason
        // every other preset value is re-derived server-side rather than trusted.
        TArray<FVector> LaneWaypoints;
        ClampLaneTurns(Lane.NormalizedWaypoints, MaxTurns, LaneWaypoints);

        const int32 CubeCount = LaneCubeCounts[LaneIndex];
        if (CubeCount <= 0) continue;
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
            WorldWaypoints.Reserve(LaneWaypoints.Num());
            for (const FVector& Normalized : LaneWaypoints)
            {
                WorldWaypoints.Add(CubeOrigin + AimMatrix.TransformVector(Normalized * Range));
            }
            OutCubeWorldPaths.Add(MoveTemp(WorldWaypoints));

            if (OutCubeLaunches)
            {
                FWTBRResolvedCubeLaunch& Launch = OutCubeLaunches->AddDefaulted_GetRef();
                Launch.DelaySeconds = FMath::Max(0.0f, Lane.LaunchDelay);
                Launch.Events = Lane.Events;
                // Authored as a fraction of range for the same reason the waypoints
                // are: one preset then reads the same at every charge level. The
                // floor keeps a short shot from resolving to a radius too small to
                // catch what its own arc already flies through.
                //
                // The zero check is load-bearing: without it every non-Hound lane in
                // the game would silently gain a 400uu homing sweep.
                Launch.HomingRadiusUU = (Lane.HomingRadius > 0.0f)
                    ? FMath::Max(Lane.HomingRadius * Range, FMath::Max(0.0f, Lane.HomingRadiusFloorUU))
                    : 0.0f;
            }
        }
    }
}
