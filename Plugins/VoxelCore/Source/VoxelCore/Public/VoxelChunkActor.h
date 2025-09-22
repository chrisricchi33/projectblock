#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "VoxelChunk.h" // <-- add this include for FVoxelChunkData
#include "VoxelChunkActor.generated.h"

UCLASS()
class AVoxelChunkActor : public AActor
{
    GENERATED_BODY()
public:
    AVoxelChunkActor();

    UPROPERTY(VisibleAnywhere)
    UProceduralMeshComponent* ProcMesh;

    UPROPERTY(EditAnywhere, Category = "Voxel")
    float BlockSize = 100.f;

    // Existing:
    void BuildFromBuffers(const TArray<FVector>& Vertices,
        const TArray<int32>& Triangles,
        const TArray<FVector>& Normals,
        const TArray<FVector2D>& UVs,
        const TArray<FLinearColor>& Colors,
        const TArray<FProcMeshTangent>& Tangents,
        UMaterialInterface* UseMaterial);

    // NEW: used by VoxelChunkSpawnCommand.cpp
    void BuildFromChunk(const FVoxelChunkData& Chunk, float InBlockSize, UMaterialInterface* UseMaterial);
};
