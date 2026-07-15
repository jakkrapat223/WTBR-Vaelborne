// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WTBRCombatBlockoutGenerator.generated.h"

class UInstancedStaticMeshComponent;
class USceneComponent;
class UWTBRRandomSpawnConfigDataAsset;

// Editor-placeable 1 x 1 km urban combat blockout. Place one actor in an empty
// level; its construction script generates collision-enabled ground, streets,
// two-storey houses, towers and simple cover from Engine's BasicShapes cube.
UCLASS(BlueprintType)
class WTBR_API AWTBRCombatBlockoutGenerator : public AActor
{
    GENERATED_BODY()

public:
    AWTBRCombatBlockoutGenerator();

    virtual void OnConstruction(const FTransform& Transform) override;

    UFUNCTION(CallInEditor, BlueprintCallable, Category="WTBR | Blockout")
    void RebuildBlockout();

    // This generator and its authored Safe Spawn Anchors form one fixed 1 x 1
    // km test map. Make a separate generator/config pair for a different size.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="WTBR | Blockout")
    float MapSizeUnits = 100000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WTBR | Blockout", meta=(ClampMin="400.0"))
    float RoadWidthUnits = 1800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WTBR | Blockout")
    int32 LayoutSeed = 15015;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WTBR | Blockout", meta=(ClampMin="0", ClampMax="100"))
    int32 CoverCount = 72;

private:
    UPROPERTY(VisibleAnywhere, Category="WTBR | Blockout")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category="WTBR | Blockout")
    TObjectPtr<UInstancedStaticMeshComponent> GroundInstances;

    UPROPERTY(VisibleAnywhere, Category="WTBR | Blockout")
    TObjectPtr<UInstancedStaticMeshComponent> RoadInstances;

    UPROPERTY(VisibleAnywhere, Category="WTBR | Blockout")
    TObjectPtr<UInstancedStaticMeshComponent> StructureInstances;

    UPROPERTY(VisibleAnywhere, Category="WTBR | Blockout")
    TObjectPtr<UInstancedStaticMeshComponent> CoverInstances;

    // Single source of truth for the map's road-safe spawn anchors. Cover
    // generation reads this same Data Asset that AWTBRGameMode uses at runtime.
    UPROPERTY(EditDefaultsOnly, Category="WTBR | Blockout | Safe Spawn")
    TObjectPtr<UWTBRRandomSpawnConfigDataAsset> SafeSpawnConfig;

    void ClearBlockout();
    static void ConfigureBlockoutComponent(UInstancedStaticMeshComponent* Component);
    static void AddBox(UInstancedStaticMeshComponent* Component, const FVector& Location, const FVector& Size, float YawDegrees = 0.0f);
    void AddTwoStoreyHouse(const FVector& Center, float YawDegrees);
    void AddTower(const FVector& Center, int32 Floors);
    void AddCover(FRandomStream& RandomStream, float HalfExtent);
    const TArray<FVector>& GetSafeSpawnAnchors() const;
};
