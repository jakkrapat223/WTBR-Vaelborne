// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Actors/WTBRCombatBlockoutGenerator.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Data/WTBRRandomSpawnConfigDataAsset.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr float CubeSizeUnits = 100.0f;
    constexpr float GroundThickness = 200.0f;
    const TCHAR* DefaultSafeSpawnConfigPath = TEXT("/Game/Data/DA_RandomSpawnConfig_15P_Blockout.DA_RandomSpawnConfig_15P_Blockout");

}

AWTBRCombatBlockoutGenerator::AWTBRCombatBlockoutGenerator()
{
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    GroundInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("GroundInstances"));
    GroundInstances->SetupAttachment(SceneRoot);
    RoadInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("RoadInstances"));
    RoadInstances->SetupAttachment(SceneRoot);
    StructureInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("StructureInstances"));
    StructureInstances->SetupAttachment(SceneRoot);
    CoverInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CoverInstances"));
    CoverInstances->SetupAttachment(SceneRoot);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        for (UInstancedStaticMeshComponent* Component : { GroundInstances.Get(), RoadInstances.Get(), StructureInstances.Get(), CoverInstances.Get() })
        {
            Component->SetStaticMesh(CubeMesh.Object);
            ConfigureBlockoutComponent(Component);
        }
    }

}

void AWTBRCombatBlockoutGenerator::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildBlockout();
}

void AWTBRCombatBlockoutGenerator::RebuildBlockout()
{
    // Do this at rebuild time instead of in the constructor: a Python setup
    // script may create the Data Asset after this class's CDO was constructed.
    // LoadObject can then resolve the newly-created asset without an editor
    // restart, while an explicitly assigned instance setting always wins.
    if (!IsValid(SafeSpawnConfig))
    {
        SafeSpawnConfig = LoadObject<UWTBRRandomSpawnConfigDataAsset>(nullptr, DefaultSafeSpawnConfigPath);
        if (!IsValid(SafeSpawnConfig))
        {
            UE_LOG(LogTemp, Warning, TEXT("WTBR Blockout: Safe Spawn config was not found at %s. Cover will not reserve spawn anchors until a config is assigned and Rebuild Blockout is run again."), DefaultSafeSpawnConfigPath);
        }
    }

    ClearBlockout();

    const float SafeMapSize = FMath::Max(10000.0f, MapSizeUnits);
    const float HalfExtent = SafeMapSize * 0.5f;
    const float Scale = SafeMapSize / 100000.0f;
    const float SafeRoadWidth = FMath::Clamp(RoadWidthUnits, 400.0f, SafeMapSize * 0.25f);

    AddBox(GroundInstances, FVector(0.0f, 0.0f, -GroundThickness * 0.5f), FVector(SafeMapSize, SafeMapSize, GroundThickness));

    // A three-by-three road grid gives long sight lines, crossings, and four
    // dense districts. Buildings remain independently distributed by design;
    // teams are not intentionally grouped.
    for (const float RoadOffset : { -SafeMapSize * 0.25f, 0.0f, SafeMapSize * 0.25f })
    {
        AddBox(RoadInstances, FVector(RoadOffset, 0.0f, 15.0f), FVector(SafeRoadWidth, SafeMapSize, 30.0f));
        AddBox(RoadInstances, FVector(0.0f, RoadOffset, 15.0f), FVector(SafeMapSize, SafeRoadWidth, 30.0f));
    }

    const float TowerOffset = 36000.0f * Scale;
    AddTower(FVector(-TowerOffset, -TowerOffset, 0.0f), 13);
    AddTower(FVector( TowerOffset, -TowerOffset, 0.0f), 11);
    AddTower(FVector(-TowerOffset,  TowerOffset, 0.0f), 12);
    AddTower(FVector( TowerOffset,  TowerOffset, 0.0f), 14);

    const float DistrictOffset = 13500.0f * Scale;
    const FVector HouseOffsets[] =
    {
        FVector(-4200.0f, -3600.0f, 0.0f), FVector(4200.0f, -3600.0f, 0.0f),
        FVector(-4200.0f,  3600.0f, 0.0f), FVector(4200.0f,  3600.0f, 0.0f),
    };
    for (const float SignX : { -1.0f, 1.0f })
    {
        for (const float SignY : { -1.0f, 1.0f })
        {
            const FVector DistrictCenter(SignX * DistrictOffset, SignY * DistrictOffset, 0.0f);
            for (int32 HouseIndex = 0; HouseIndex < UE_ARRAY_COUNT(HouseOffsets); ++HouseIndex)
            {
                AddTwoStoreyHouse(DistrictCenter + HouseOffsets[HouseIndex] * Scale, HouseIndex % 2 == 0 ? 0.0f : 90.0f);
            }
        }
    }

    FRandomStream RandomStream(LayoutSeed);
    AddCover(RandomStream, HalfExtent - 2500.0f * Scale);
}

void AWTBRCombatBlockoutGenerator::ClearBlockout()
{
    for (UInstancedStaticMeshComponent* Component : { GroundInstances.Get(), RoadInstances.Get(), StructureInstances.Get(), CoverInstances.Get() })
    {
        if (IsValid(Component))
        {
            Component->ClearInstances();
        }
    }
}

void AWTBRCombatBlockoutGenerator::ConfigureBlockoutComponent(UInstancedStaticMeshComponent* Component)
{
    Component->SetMobility(EComponentMobility::Static);
    Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Component->SetCollisionResponseToAllChannels(ECR_Block);
    Component->SetCanEverAffectNavigation(true);
}

void AWTBRCombatBlockoutGenerator::AddBox(
    UInstancedStaticMeshComponent* Component,
    const FVector& Location,
    const FVector& Size,
    float YawDegrees)
{
    if (!IsValid(Component))
    {
        return;
    }

    Component->AddInstance(FTransform(
        FRotator(0.0f, YawDegrees, 0.0f),
        Location,
        Size / CubeSizeUnits));
}

void AWTBRCombatBlockoutGenerator::AddTwoStoreyHouse(const FVector& Center, float YawDegrees)
{
    constexpr float Width = 1800.0f;
    constexpr float Depth = 1400.0f;
    constexpr float WallThickness = 80.0f;
    constexpr float StoreyHeight = 380.0f;
    constexpr float SlabThickness = 60.0f;
    constexpr float DoorWidth = 360.0f;

    const FTransform HouseTransform(FRotator(0.0f, YawDegrees, 0.0f), Center);
    const auto LocalBox = [this, &HouseTransform](const FVector& LocalLocation, const FVector& Size)
    {
        AddBox(StructureInstances, HouseTransform.TransformPosition(LocalLocation), Size, HouseTransform.Rotator().Yaw);
    };

    // Floors and roof; exterior walls leave one front doorway open. The internal
    // stair is deliberately coarse — it is a combat blockout, not final art.
    LocalBox(FVector(0.0f, 0.0f, SlabThickness * 0.5f), FVector(Width, Depth, SlabThickness));
    LocalBox(FVector(0.0f, 0.0f, StoreyHeight), FVector(Width, Depth, SlabThickness));
    LocalBox(FVector(0.0f, 0.0f, StoreyHeight * 2.0f), FVector(Width, Depth, SlabThickness));

    for (const float WallZ : { StoreyHeight * 0.5f, StoreyHeight * 1.5f })
    {
        LocalBox(FVector(0.0f, Depth * 0.5f - WallThickness * 0.5f, WallZ), FVector(Width, WallThickness, StoreyHeight));
        LocalBox(FVector(-Width * 0.5f + WallThickness * 0.5f, 0.0f, WallZ), FVector(WallThickness, Depth, StoreyHeight));
        LocalBox(FVector( Width * 0.5f - WallThickness * 0.5f, 0.0f, WallZ), FVector(WallThickness, Depth, StoreyHeight));
    }

    // Ground floor front wall is split around its entrance. The upper front is
    // solid enough for the first traversal prototype and gives clear cover.
    const float SideSegmentWidth = (Width - DoorWidth) * 0.5f;
    LocalBox(FVector(-(DoorWidth + SideSegmentWidth) * 0.5f, -Depth * 0.5f + WallThickness * 0.5f, StoreyHeight * 0.5f), FVector(SideSegmentWidth, WallThickness, StoreyHeight));
    LocalBox(FVector( (DoorWidth + SideSegmentWidth) * 0.5f, -Depth * 0.5f + WallThickness * 0.5f, StoreyHeight * 0.5f), FVector(SideSegmentWidth, WallThickness, StoreyHeight));
    LocalBox(FVector(0.0f, -Depth * 0.5f + WallThickness * 0.5f, StoreyHeight * 1.5f), FVector(Width, WallThickness, StoreyHeight));

    for (int32 Step = 0; Step < 7; ++Step)
    {
        const float StepHeight = 50.0f + Step * 48.0f;
        LocalBox(FVector(Width * 0.22f, 260.0f - Step * 105.0f, StepHeight * 0.5f), FVector(360.0f, 110.0f, StepHeight));
    }
}

void AWTBRCombatBlockoutGenerator::AddTower(const FVector& Center, int32 Floors)
{
    constexpr float Width = 3200.0f;
    constexpr float FloorHeight = 360.0f;
    constexpr float SlabThickness = 50.0f;
    constexpr float PillarSize = 180.0f;
    const float TotalHeight = Floors * FloorHeight;

    AddBox(StructureInstances, Center + FVector(0.0f, 0.0f, SlabThickness * 0.5f), FVector(Width, Width, SlabThickness));
    for (int32 Floor = 1; Floor <= Floors; ++Floor)
    {
        AddBox(StructureInstances, Center + FVector(0.0f, 0.0f, Floor * FloorHeight), FVector(Width, Width, SlabThickness));
        // Alternating facade bands create stacked firing ledges without copying
        // any specific fictional building design.
        const float FacadeWidth = Floor % 2 == 0 ? Width * 0.65f : Width * 0.85f;
        AddBox(StructureInstances, Center + FVector(0.0f, -Width * 0.5f + 100.0f, Floor * FloorHeight - FloorHeight * 0.45f), FVector(FacadeWidth, 200.0f, FloorHeight * 0.45f));
        AddBox(StructureInstances, Center + FVector(Width * 0.5f - 100.0f, 0.0f, Floor * FloorHeight - FloorHeight * 0.45f), FVector(200.0f, FacadeWidth, FloorHeight * 0.45f));
    }

    for (const FVector& Corner : { FVector(-1.0f, -1.0f, 1.0f), FVector(-1.0f, 1.0f, 1.0f), FVector(1.0f, -1.0f, 1.0f), FVector(1.0f, 1.0f, 1.0f) })
    {
        AddBox(StructureInstances, Center + FVector(Corner.X * (Width * 0.5f - PillarSize * 0.5f), Corner.Y * (Width * 0.5f - PillarSize * 0.5f), TotalHeight * 0.5f), FVector(PillarSize, PillarSize, TotalHeight));
    }

    // Broad external steps create a readable way up the first combat platform.
    for (int32 Step = 0; Step < 8; ++Step)
    {
        const float StepHeight = (Step + 1) * 45.0f;
        AddBox(StructureInstances, Center + FVector(0.0f, -Width * 0.5f - 500.0f + Step * 115.0f, StepHeight * 0.5f), FVector(900.0f, 130.0f, StepHeight));
    }
}

void AWTBRCombatBlockoutGenerator::AddCover(FRandomStream& RandomStream, float HalfExtent)
{
    for (int32 CoverIndex = 0; CoverIndex < CoverCount; ++CoverIndex)
    {
        const float Width = RandomStream.FRandRange(220.0f, 620.0f);
        const float Depth = RandomStream.FRandRange(180.0f, 420.0f);
        const float Height = RandomStream.FRandRange(160.0f, 400.0f);
        FVector Location;
        for (int32 Attempt = 0; Attempt < 24; ++Attempt)
        {
            Location = FVector(
                RandomStream.FRandRange(-HalfExtent, HalfExtent),
                RandomStream.FRandRange(-HalfExtent, HalfExtent),
                Height * 0.5f);
            bool bBlocksSafeAnchor = false;
            for (const FVector& Anchor : GetSafeSpawnAnchors())
            {
                if (FVector::Dist2D(Location, Anchor) < 1800.0f)
                {
                    bBlocksSafeAnchor = true;
                    break;
                }
            }
            if (!bBlocksSafeAnchor)
            {
                break;
            }
        }
        AddBox(CoverInstances, Location, FVector(Width, Depth, Height), RandomStream.FRandRange(0.0f, 180.0f));
    }
}

const TArray<FVector>& AWTBRCombatBlockoutGenerator::GetSafeSpawnAnchors() const
{
    static const TArray<FVector> EmptyAnchors;
    return IsValid(SafeSpawnConfig) ? SafeSpawnConfig->SafeSpawnAnchors : EmptyAnchors;
}
