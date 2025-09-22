#include "VoxelChunkActor.h"
#include "VoxelMesher.h" // <-- for FVoxelMesher_Naive

AVoxelChunkActor::AVoxelChunkActor()
{
    PrimaryActorTick.bCanEverTick = false;

    ProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProcMesh"));
    SetRootComponent(ProcMesh);

    ProcMesh->bUseAsyncCooking = true;
    ProcMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    ProcMesh->SetCollisionObjectType(ECC_WorldStatic);
    ProcMesh->SetCollisionResponseToAllChannels(ECR_Block);
}

void AVoxelChunkActor::BuildFromBuffers(const TArray<FVector>& Vertices,
    const TArray<int32>& Triangles,
    const TArray<FVector>& Normals,
    const TArray<FVector2D>& UVs,
    const TArray<FLinearColor>& Colors,
    const TArray<FProcMeshTangent>& Tangents,
    UMaterialInterface* UseMaterial)
{
    ProcMesh->ClearAllMeshSections();
    ProcMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, true);

    ProcMesh->bUseAsyncCooking = true;
    ProcMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    ProcMesh->SetCollisionObjectType(ECC_WorldStatic);
    ProcMesh->SetCollisionResponseToAllChannels(ECR_Block);

    if (UseMaterial)
    {
        ProcMesh->SetMaterial(0, UseMaterial);
    }
}

void AVoxelChunkActor::BuildFromChunk(const FVoxelChunkData& Chunk, float InBlockSize, UMaterialInterface* UseMaterial)
{
    BlockSize = InBlockSize;

    TArray<FVector> V;
    TArray<int32>   I;
    TArray<FVector> N;
    TArray<FVector2D> UV;
    TArray<FLinearColor> C;
    TArray<FProcMeshTangent> T;

    // Naive mesher (Phase 3 path)
    FVoxelMesher_Naive::BuildMesh(Chunk, BlockSize, V, I, N, UV, C, T);

    BuildFromBuffers(V, I, N, UV, C, T, UseMaterial);
}
