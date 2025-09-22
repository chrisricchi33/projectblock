#pragma once
#include "CoreMinimal.h"

struct FVoxelChunkData;

namespace VoxelSaveSystem
{
	// Apply saved edits (if any) into Data (keyed by Data.Key and Seed).
	bool LoadDelta(int32 Seed, FVoxelChunkData& Data);

	// Persist edited cells from Data.ModifiedBlocks for this chunk.
	void SaveDelta(int32 Seed, const FVoxelChunkData& Data);
}
