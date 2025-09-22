#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelEditPlaceTool.generated.h"

class AVoxelWorldManager;

UCLASS()
class AVoxelEditPlaceTool : public AActor
{
    GENERATED_BODY()
public:
    AVoxelEditPlaceTool();

    UPROPERTY(EditAnywhere) AVoxelWorldManager* WorldManager = nullptr;
    UPROPERTY(EditAnywhere) float  MaxRange = 5000.f;
    UPROPERTY(EditAnywhere) uint8  PlaceBlockId = 3;   // Set to the block you want (e.g. Grass = 3)
    UPROPERTY(EditAnywhere) bool   bDebug = true;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    void TryPlace();

    // optional quick hotkeys 1..9 to switch block id
    void HandleHotkeys();
};
