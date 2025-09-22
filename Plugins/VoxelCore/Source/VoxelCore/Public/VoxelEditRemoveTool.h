#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelEditRemoveTool.generated.h"

class AVoxelWorldManager;

UCLASS()
class AVoxelEditRemoveTool : public AActor
{
    GENERATED_BODY()
public:
    AVoxelEditRemoveTool();

    UPROPERTY(EditAnywhere) AVoxelWorldManager* WorldManager = nullptr;
    UPROPERTY(EditAnywhere) float MaxRange = 5000.f;
    UPROPERTY(EditAnywhere) bool  bDebug = true;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    void TryRemove();
};
