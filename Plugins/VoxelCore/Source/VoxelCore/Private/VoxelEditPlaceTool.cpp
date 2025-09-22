#include "VoxelEditPlaceTool.h"
#include "VoxelWorldManager.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

AVoxelEditPlaceTool::AVoxelEditPlaceTool()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void AVoxelEditPlaceTool::BeginPlay()
{
    Super::BeginPlay();
    EnableInput(UGameplayStatics::GetPlayerController(this, 0));
}

void AVoxelEditPlaceTool::Tick(float)
{
    if (!WorldManager) return;

    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC) return;

    HandleHotkeys();

    // Use LMB for place on this tool (simple & consistent)
    if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
    {
        TryPlace();
    }
}

void AVoxelEditPlaceTool::HandleHotkeys()
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC) return;

    // Quick-select ids 1..9 (optional)
    if (PC->WasInputKeyJustPressed(EKeys::One))   PlaceBlockId = 1;
    if (PC->WasInputKeyJustPressed(EKeys::Two))   PlaceBlockId = 2;
    if (PC->WasInputKeyJustPressed(EKeys::Three)) PlaceBlockId = 3;
    if (PC->WasInputKeyJustPressed(EKeys::Four))  PlaceBlockId = 4;
    if (PC->WasInputKeyJustPressed(EKeys::Five))  PlaceBlockId = 5;
    if (PC->WasInputKeyJustPressed(EKeys::Six))   PlaceBlockId = 6;
    if (PC->WasInputKeyJustPressed(EKeys::Seven)) PlaceBlockId = 7;
    if (PC->WasInputKeyJustPressed(EKeys::Eight)) PlaceBlockId = 8;
    if (PC->WasInputKeyJustPressed(EKeys::Nine))  PlaceBlockId = 9;
}

void AVoxelEditPlaceTool::TryPlace()
{
    if (!WorldManager) return;

    // Camera ray
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    FVector O; FRotator R; PC->GetPlayerViewPoint(O, R);
    const FVector D = R.Vector().GetSafeNormal();
    const FVector End = O + D * MaxRange;

    // Raycast against chunk meshes (to get the face we’re targeting)
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(VoxelPlaceTrace), false, this);
    Params.bTraceComplex = true;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, O, End, ECC_Visibility, Params))
        return;

    // Persist the click ray
    DrawDebugLine(GetWorld(), O, Hit.ImpactPoint, FColor::Green, /*bPersistent=*/true, /*LifeTime=*/0.f, 0, 2.0f);

    // Place on the air side: nudge a hair opposite the ray into empty space
    const float   Eps = FMath::Max(1.f, WorldManager->BlockSize * 0.01f);
    const FVector POutside = Hit.ImpactPoint - D * Eps;
    DrawDebugPoint(GetWorld(), POutside, 8.f, FColor::Yellow, /*bPersistent=*/true, /*LifeTime=*/0.f);

    // Map world ? (chunk key, local x/y/z) using the centered grid
    FChunkKey Key; int32 X = 0, Y = 0, Z = 0;
    if (!WorldManager->WorldToVoxel_Centered(POutside, Key, X, Y, Z))
        return;

    const bool OK = WorldManager->PlaceBlock_Local(Key, X, Y, Z, PlaceBlockId);

    if (bDebug)
    {
        FString Msg = FString::Printf(TEXT("Placed ID %u @ (%d,%d,%d) in Chunk (%d,%d)"),
            (uint32)PlaceBlockId, X, Y, Z, Key.X, Key.Z);
        GEngine->AddOnScreenDebugMessage(-1, 1.2f, OK ? FColor::Green : FColor::Red, OK ? Msg : TEXT("Place failed"));
    }
}
