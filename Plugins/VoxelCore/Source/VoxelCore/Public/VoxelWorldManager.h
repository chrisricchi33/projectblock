#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"     // FProcMeshTangent
#include "ChunkHelpers.h"                // FChunkKey
#include "VoxelChunk.h"                  // FVoxelChunkData
#include "VoxelWorldManager.generated.h"

class AVoxelChunkActor;

UENUM(BlueprintType)
enum class EVoxelWorldSize : uint8
{
    Small  UMETA(DisplayName = "Small"),
    Medium UMETA(DisplayName = "Medium"),
    Large  UMETA(DisplayName = "Large")
};

/** Off-thread result: mesh buffers + data. */
struct FChunkMeshResult
{
    FChunkKey Key;
    float     BlockSize = 100.f;

    TSharedPtr<FVoxelChunkData> Data;

    TArray<FVector>          V;
    TArray<int32>            I;
    TArray<FVector>          N;
    TArray<FVector2D>        UV;
    TArray<FLinearColor>     C;
    TArray<FProcMeshTangent> T;
};

USTRUCT()
struct FChunkRecord
{
    GENERATED_BODY()

    TSharedPtr<FVoxelChunkData>      Data;
    TWeakObjectPtr<AVoxelChunkActor> Actor;
    bool bDirty = false;
};

UCLASS(Blueprintable)
class AVoxelWorldManager : public AActor
{
    GENERATED_BODY()
public:
    AVoxelWorldManager();

    /** Material for chunks (your atlas). */
    UPROPERTY(EditAnywhere, Category = "Voxel|Streaming")
    UMaterialInterface* ChunkMaterial = nullptr;

    /** Size of a voxel in UU. */
    UPROPERTY(EditAnywhere, Category = "Voxel|Streaming")
    float BlockSize = 100.f;

    /** Square radius (in chunks). */
    UPROPERTY(EditAnywhere, Category = "Voxel|Streaming", meta = (ClampMin = "1", ClampMax = "32"))
    int32 RenderRadiusChunks = 6;

    /** Soft world clamp in chunks from origin. */
    UPROPERTY(EditAnywhere, Category = "Voxel|Streaming")
    EVoxelWorldSize WorldSize = EVoxelWorldSize::Small;

    /** Seed for deterministic world generation. */
    UPROPERTY(EditAnywhere, Category = "Voxel|Streaming")
    int32 WorldSeed = 1337;

    /** How often to update streaming (s). */
    UPROPERTY(EditAnywhere, Category = "Voxel|Streaming", meta = (ClampMin = "0.02", ClampMax = "2.0"))
    float UpdateIntervalSeconds = 0.15f;

    /** Max background jobs. */
    UPROPERTY(EditAnywhere, Category = "Voxel|Streaming", meta = (ClampMin = "1", ClampMax = "64"))
    int32 MaxConcurrentBackgroundTasks = 8;

    /** Optional: explicit actor to track; else Player0 pawn. */
    UPROPERTY(EditAnywhere, Category = "Voxel|Streaming")
    TWeakObjectPtr<AActor> TrackedActor;

    // Public wrapper to the existing private WorldToVoxel used by streaming
    bool WorldToVoxel_ForEdit(const FVector& World,
        FChunkKey& OutKey, int32& OutX, int32& OutY, int32& OutZ) const;

    bool WorldToVoxel_Centered(const FVector& World,
        FChunkKey& OutKey, int32& OutX, int32& OutY, int32& OutZ) const;


    // Mutate a single cell in a loaded chunk and trigger rebuild
    bool RemoveBlock_Local(const FChunkKey& Key, int32 X, int32 Y, int32 Z);

    // Sets a voxel to BlockId and schedules a rebuild for that chunk.
    bool PlaceBlock_Local(const FChunkKey& Key, int32 X, int32 Y, int32 Z, uint8 BlockId);


protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // Loaded chunk records
    TMap<FChunkKey, FChunkRecord> Loaded;

    // Work queues
    TSet<FChunkKey> Pending;
    TQueue<TSharedPtr<FChunkMeshResult>, EQueueMode::Mpsc> Completed;

    float TimeAcc = 0.f;

    // --- Streaming helpers ---
    int32 GetWorldRadiusLimit() const;
    bool  IsWithinWorldLimit(const FChunkKey& Key) const;

    FIntPoint WorldToChunkXZ(const FVector& World) const;

    /** Convert world?chunk key + local voxel coords. Robust for negatives. */
    bool WorldToVoxel(const FVector& World, FChunkKey& OutKey, int32& OutX, int32& OutY, int32& OutZ) const;

    void RecomputeDesiredSet(const FIntPoint& Center, TSet<FChunkKey>& OutDesired) const;
    void BuildDesiredOrdered(const FIntPoint& Center, TArray<FChunkKey>& OutOrdered) const;

    void KickBuild(const FChunkKey& Key, TSharedPtr<FVoxelChunkData> Existing);
    void SpawnOrUpdateChunkFromResult(const TSharedPtr<FChunkMeshResult>& Res);
    void UnloadNoLongerNeeded(const TSet<FChunkKey>& Desired);

    UWorld* GetWorldChecked() const { check(GetWorld()); return GetWorld(); }

    void FlushAllDirtyChunks();
};
