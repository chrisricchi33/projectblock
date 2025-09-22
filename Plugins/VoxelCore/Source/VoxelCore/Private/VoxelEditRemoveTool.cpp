#include "VoxelEditRemoveTool.h"
#include "VoxelWorldManager.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

AVoxelEditRemoveTool::AVoxelEditRemoveTool()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void AVoxelEditRemoveTool::BeginPlay()
{
    Super::BeginPlay();
    EnableInput(UGameplayStatics::GetPlayerController(this, 0));
}

void AVoxelEditRemoveTool::Tick(float)
{
    if (!WorldManager) return;
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC) return;

    if (PC->WasInputKeyJustPressed(EKeys::RightMouseButton))
    {
        TryRemove();
    }
}

void AVoxelEditRemoveTool::TryRemove()
{
    if (!WorldManager) return;

    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC) return;

    // Camera ray
    FVector O; FRotator R;
    PC->GetPlayerViewPoint(O, R);
    const FVector D = R.Vector().GetSafeNormal();
    const FVector End = O + D * MaxRange;

    // Raycast against chunk meshes
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(VoxelRemoveTrace), false, this);
    Params.bTraceComplex = true;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, O, End, ECC_Visibility, Params))
        return;

    // Persist the click ray
    DrawDebugLine(GetWorld(), O, Hit.ImpactPoint, FColor::Red, /*bPersistentLines=*/true, /*LifeTime=*/0.f, 0, 2.0f);

    // Push a hair *into* the block along the ray ? deterministic interior sample
    const float   Eps = FMath::Max(1.f, WorldManager->BlockSize * 0.01f);
    const FVector PInside = Hit.ImpactPoint + D * Eps;   // +D = inside the clicked voxel
    DrawDebugPoint(GetWorld(), PInside, 8.f, FColor::Yellow, /*bPersistent=*/true, /*LifeTime=*/0.f);

    // Map world ? (chunk key, local x/y/z) using centered grid
    FChunkKey Key; int32 X = 0, Y = 0, Z = 0;
    if (!WorldManager->WorldToVoxel_Centered(PInside, Key, X, Y, Z))
        return;

    // Clear that cell and rebuild
    const bool OK = WorldManager->RemoveBlock_Local(Key, X, Y, Z);

    if (bDebug)
    {
        GEngine->AddOnScreenDebugMessage(
            -1, 1.2f, OK ? FColor::Green : FColor::Red,
            OK ? TEXT("Removed") : TEXT("Remove failed"));
    }
}

