#include "VoxelWorldManager.h"
#include "VoxelChunkActor.h"
#include "VoxelGenerator.h"
#include "VoxelMesher.h"            // FVoxelMesher_Naive
#include "VoxelTypes.h"
#include "ChunkConfig.h"
#include "VoxelSaveSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"

AVoxelWorldManager::AVoxelWorldManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void AVoxelWorldManager::BeginPlay()
{
    Super::BeginPlay();
}

int32 AVoxelWorldManager::GetWorldRadiusLimit() const
{
    switch (WorldSize)
    {
    case EVoxelWorldSize::Small:  return 16;
    case EVoxelWorldSize::Medium: return 64;
    case EVoxelWorldSize::Large:  return 256;
    default: return 16;
    }
}

bool AVoxelWorldManager::IsWithinWorldLimit(const FChunkKey& Key) const
{
    const int32 R = GetWorldRadiusLimit();
    return FMath::Abs(Key.X) <= R && FMath::Abs(Key.Z) <= R;
}

FIntPoint AVoxelWorldManager::WorldToChunkXZ(const FVector& W) const
{
    const double CSX = (double)CHUNK_SIZE_X * BlockSize;
    const double CSZ = (double)CHUNK_SIZE_Z * BlockSize;
    const int32 Cx = FMath::FloorToInt(W.X / CSX);
    const int32 Cz = FMath::FloorToInt(W.Y / CSZ); // chunk Z -> world Y
    return FIntPoint(Cx, Cz);
}

// Robust world->voxel using a global grid (handles negatives).
bool AVoxelWorldManager::WorldToVoxel(const FVector& W, FChunkKey& OutKey, int32& x, int32& y, int32& z) const
{
    const double invBS = 1.0 / (double)BlockSize;
    const int32 GX = FMath::FloorToInt(W.X * invBS); // world X -> voxel X
    const int32 GZ = FMath::FloorToInt(W.Y * invBS); // world Y -> voxel Z
    const int32 GY = FMath::FloorToInt(W.Z * invBS); // world Z -> voxel Y

    const int32 CX = FMath::FloorToInt((double)GX / (double)CHUNK_SIZE_X);
    const int32 CZ = FMath::FloorToInt((double)GZ / (double)CHUNK_SIZE_Z);

    const int32 LX = GX - CX * CHUNK_SIZE_X;
    const int32 LZ = GZ - CZ * CHUNK_SIZE_Z;
    const int32 LY = FMath::Clamp(GY, 0, CHUNK_SIZE_Y - 1);

    OutKey = FChunkKey(CX, CZ);
    x = FMath::Clamp(LX, 0, CHUNK_SIZE_X - 1);
    z = FMath::Clamp(LZ, 0, CHUNK_SIZE_Z - 1);
    y = LY;
    return true;
}

void AVoxelWorldManager::RecomputeDesiredSet(const FIntPoint& Center, TSet<FChunkKey>& OutDesired) const
{
    OutDesired.Reset();
    const int32 R = RenderRadiusChunks;
    for (int32 dz = -R; dz <= R; ++dz)
    {
        for (int32 dx = -R; dx <= R; ++dx)
        {
            const FChunkKey Key(Center.X + dx, Center.Y + dz);
            if (IsWithinWorldLimit(Key)) OutDesired.Add(Key);
        }
    }
}

void AVoxelWorldManager::BuildDesiredOrdered(const FIntPoint& Center, TArray<FChunkKey>& OutOrdered) const
{
    OutOrdered.Reset();
    const int32 R = RenderRadiusChunks;
    for (int32 dz = -R; dz <= R; ++dz)
    {
        for (int32 dx = -R; dx <= R; ++dx)
        {
            const FChunkKey K(Center.X + dx, Center.Y + dz);
            if (IsWithinWorldLimit(K)) OutOrdered.Add(K);
        }
    }
    OutOrdered.Sort([&](const FChunkKey& A, const FChunkKey& B) {
        const int32 dA = FMath::Abs(A.X - Center.X) + FMath::Abs(A.Z - Center.Y);
        const int32 dB = FMath::Abs(B.X - Center.X) + FMath::Abs(B.Z - Center.Y);
        return dA < dB;
        });
}

void AVoxelWorldManager::KickBuild(const FChunkKey& Key, TSharedPtr<FVoxelChunkData> Existing)
{
    if (Pending.Contains(Key)) return;
    Pending.Add(Key);

    const int32 Seed = WorldSeed;
    const float BS = BlockSize;

    Async(EAsyncExecution::ThreadPool, [this, Key, Existing, Seed, BS]()
        {
            TSharedPtr<FVoxelChunkData> Data = Existing;
            if (!Data.IsValid())
            {
                Data = MakeShared<FVoxelChunkData>(Key);
                FVoxelGenerator Gen(Seed);
                Gen.GenerateBaseChunk(Key, *Data);
                VoxelSaveSystem::LoadDelta(Seed, *Data);
            }

            TSharedPtr<FChunkMeshResult> R = MakeShared<FChunkMeshResult>();
            R->Key = Key;
            R->BlockSize = BS;
            R->Data = Data;

            FVoxelMesher_Naive::BuildMesh(*Data, BS, R->V, R->I, R->N, R->UV, R->C, R->T);

            Completed.Enqueue(R);
        });
}

void AVoxelWorldManager::SpawnOrUpdateChunkFromResult(const TSharedPtr<FChunkMeshResult>& Res)
{
    if (!Res) return;
    if (!IsWithinWorldLimit(Res->Key)) return;

    FChunkRecord& Rec = Loaded.FindOrAdd(Res->Key);
    Rec.Data = Res->Data;

    AVoxelChunkActor* Actor = Rec.Actor.Get();
    if (!Actor || !IsValid(Actor))
    {
        const FVector Origin(
            (double)Res->Key.X * CHUNK_SIZE_X * Res->BlockSize,
            (double)Res->Key.Z * CHUNK_SIZE_Z * Res->BlockSize,
            0.0
        );
        FActorSpawnParameters SP;
        SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Actor = GetWorldChecked()->SpawnActor<AVoxelChunkActor>(Origin, FRotator::ZeroRotator, SP);
        if (!Actor) return;
        Actor->BlockSize = Res->BlockSize;
        Rec.Actor = Actor;
    }

    Actor->BuildFromBuffers(Res->V, Res->I, Res->N, Res->UV, Res->C, Res->T, ChunkMaterial);
}

void AVoxelWorldManager::UnloadNoLongerNeeded(const TSet<FChunkKey>& Desired)
{
    // Don’t erase from a TMap while iterating it — collect keys first.
    TArray<FChunkKey> ToUnload;
    ToUnload.Reserve(Loaded.Num());

    for (auto& Pair : Loaded)
    {
        const FChunkKey& Key = Pair.Key;
        FChunkRecord& Rec = Pair.Value;

        if (!Desired.Contains(Key))
        {
            // Destroy spawned actor if any
            if (AVoxelChunkActor* A = Rec.Actor.Get())
            {
                A->Destroy();
                Rec.Actor = nullptr;
            }

            // Persist edits (delta) if present
            if (Rec.Data.IsValid() && Rec.Data->ModifiedBlocks.Num() > 0)
            {
                VoxelSaveSystem::SaveDelta(WorldSeed, *Rec.Data);
            }

            // Mark for removal from the map after the loop
            ToUnload.Add(Key);
        }
    }

    // Actually remove records from memory now
    for (const FChunkKey& K : ToUnload)
    {
        Loaded.Remove(K);
        Pending.Remove(K); // optional: in case something slipped into the queue
    }
}


void AVoxelWorldManager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // Drain a few completed jobs per frame to avoid hitches
    {
        int32 Drain = 0;
        const int32 MaxDrainPerFrame = 4;
        TSharedPtr<FChunkMeshResult> Res;
        while (Drain < MaxDrainPerFrame && Completed.Dequeue(Res))
        {
            Pending.Remove(Res->Key);
            SpawnOrUpdateChunkFromResult(Res);

            // If edits landed while job was running, resubmit now
            if (FChunkRecord* Rec = Loaded.Find(Res->Key))
            {
                if (Rec->bDirty)
                {
                    Rec->bDirty = false;
                    KickBuild(Res->Key, Rec->Data);
                }
            }
            ++Drain;
        }
    }

    TimeAcc += DeltaSeconds;
    if (TimeAcc < UpdateIntervalSeconds) return;
    TimeAcc = 0.f;

    AActor* Target = TrackedActor.IsValid() ? TrackedActor.Get()
        : UGameplayStatics::GetPlayerPawn(GetWorldChecked(), 0);
    if (!Target) return;

    const FIntPoint Center = WorldToChunkXZ(Target->GetActorLocation());

    TSet<FChunkKey> Desired;
    RecomputeDesiredSet(Center, Desired);

    TArray<FChunkKey> DesiredOrdered;
    BuildDesiredOrdered(Center, DesiredOrdered);

    UnloadNoLongerNeeded(Desired);

    int32 Slots = FMath::Max(0, MaxConcurrentBackgroundTasks - Pending.Num());
    for (const FChunkKey& K : DesiredOrdered)
    {
        if (Slots <= 0) break;

        const FChunkRecord* Rec = Loaded.Find(K);
        const bool bHasActor = Rec && Rec->Actor.IsValid();
        if (bHasActor) continue;
        if (Pending.Contains(K)) continue;

        TSharedPtr<FVoxelChunkData> Existing = Rec ? Rec->Data : nullptr;
        KickBuild(K, Existing);
        --Slots;
    }
}

void AVoxelWorldManager::FlushAllDirtyChunks()
{
    for (auto& Pair : Loaded)
    {
        FChunkRecord& Rec = Pair.Value;
        if (Rec.Data.IsValid() && Rec.Data->ModifiedBlocks.Num() > 0)
        {
            VoxelSaveSystem::SaveDelta(WorldSeed, *Rec.Data);
            Rec.bDirty = false;
        }
    }
}

bool AVoxelWorldManager::WorldToVoxel_Centered(const FVector& World,
    FChunkKey& OutKey, int32& OutX, int32& OutY, int32& OutZ) const
{
    const double BS = (double)BlockSize;

    const int32 GX = FMath::FloorToInt(World.X / BS + 0.5);
    const int32 GZ = FMath::FloorToInt(World.Y / BS + 0.5);
    const int32 GY = FMath::FloorToInt(World.Z / BS + 0.5);

    const int32 CX = FMath::FloorToInt((double)GX / (double)CHUNK_SIZE_X);
    const int32 CZ = FMath::FloorToInt((double)GZ / (double)CHUNK_SIZE_Z);

    OutX = GX - CX * CHUNK_SIZE_X;
    OutZ = GZ - CZ * CHUNK_SIZE_Z;
    OutY = FMath::Clamp(GY, 0, CHUNK_SIZE_Y - 1);

    OutKey = FChunkKey(CX, CZ);
    return true;
}


bool AVoxelWorldManager::WorldToVoxel_ForEdit(const FVector& World,
    FChunkKey& OutKey, int32& OutX, int32& OutY, int32& OutZ) const
{
    return WorldToVoxel(World, OutKey, OutX, OutY, OutZ); // forward to your existing mapper
}

bool AVoxelWorldManager::RemoveBlock_Local(const FChunkKey& Key, int32 X, int32 Y, int32 Z)
{
    FChunkRecord* Rec = Loaded.Find(Key);
    if (!Rec || !Rec->Data.IsValid()) return false;

    Rec->Data->SetBlockAt(X, Y, Z, EBlockId::Air);
    Rec->bDirty = true;

    if (!Pending.Contains(Key))
        KickBuild(Key, Rec->Data);

    return true;
}

bool AVoxelWorldManager::PlaceBlock_Local(const FChunkKey& Key, int32 X, int32 Y, int32 Z, uint8 BlockId)
{
    FChunkRecord* Rec = Loaded.Find(Key);
    if (!Rec || !Rec->Data.IsValid()) return false;

    Rec->Data->SetBlockAt(X, Y, Z, static_cast<EBlockId>(BlockId));
    Rec->bDirty = true;

    if (!Pending.Contains(Key))
        KickBuild(Key, Rec->Data);

    return true;
}


void AVoxelWorldManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Save edits for all still-loaded chunks before we go away
    FlushAllDirtyChunks();

    Super::EndPlay(EndPlayReason);
}
