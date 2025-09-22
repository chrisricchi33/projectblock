#pragma once

#include "CoreMinimal.h"
#include <cstdint>

// Basic block ID list (start small, expand later)
enum class EBlockId : uint8
{
	Air = 0,
	Dirt = 1,
	Grass = 2,
	Stone = 3,
	Sand = 4,
	Water = 5,
	// Add more block types here
	Max
};


struct FBlockInfo
{
	uint8 Id; // small numeric id (same as EBlockId cast)
	bool bIsSolid;
	bool bIsTransparent;
	uint8 MaterialIndex; // index into a texture atlas or material array


	FBlockInfo()
		: Id(static_cast<uint8>(EBlockId::Air))
		, bIsSolid(false)
		, bIsTransparent(true)
		, MaterialIndex(0)
	{
	}
};