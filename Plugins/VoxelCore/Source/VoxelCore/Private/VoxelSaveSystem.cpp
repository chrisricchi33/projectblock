#include "VoxelSaveSystem.h"
#include "VoxelChunk.h"          // FVoxelChunkData, EBlockId, Data.Key, ModifiedBlocks, SetBlockAt
#include "ChunkConfig.h"         // CHUNK_SIZE_X/Y/Z
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"

static constexpr uint32 VCD_MAGIC = 0x44435631; // 'VCD1'
static constexpr uint16 VCD_VER = 1;

static FString ChunkPath(int32 Seed, const FChunkKey& Key)
{
	const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Voxel"), FString::Printf(TEXT("Seed_%d"), Seed));
	const FString Chunks = FPaths::Combine(Root, TEXT("Chunks"));
	IFileManager::Get().MakeDirectory(*Chunks, /*Tree=*/true);
	return FPaths::Combine(Chunks, FString::Printf(TEXT("%d_%d.vcd"), Key.X, Key.Z));
}

namespace VoxelSaveSystem
{
	bool LoadDelta(int32 Seed, FVoxelChunkData& Data)
	{
		const FString Path = ChunkPath(Seed, Data.Key);
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.Num() < 10) return false;

		FMemoryReader Ar(Bytes);
		uint32 Magic = 0; uint16 Ver = 0; uint32 Count = 0;
		Ar << Magic; Ar << Ver; Ar << Count;
		if (Magic != VCD_MAGIC || Ver != VCD_VER) return false;

		for (uint32 i = 0; i < Count; ++i)
		{
			int32 I; uint8 Id;
			Ar << I; Ar << Id;

			//const int32 X = I % CHUNK_SIZE_X;
			//const int32 Y = (I / CHUNK_SIZE_X) % CHUNK_SIZE_Y;
			//const int32 Z = I / (CHUNK_SIZE_X * CHUNK_SIZE_Y);

			const int32 X = I % CHUNK_SIZE_X;
			const int32 Z = (I / CHUNK_SIZE_X) % CHUNK_SIZE_Z;
			const int32 Y = I / (CHUNK_SIZE_X * CHUNK_SIZE_Z);

			Data.SetBlockAt(X, Y, Z, static_cast<EBlockId>(Id));
		}
		return true;
	}

	void SaveDelta(int32 Seed, const FVoxelChunkData& Data)
	{
		if (Data.ModifiedBlocks.Num() == 0) return;

		FBufferArchive Ar;
		uint32 Magic = VCD_MAGIC; uint16 Ver = VCD_VER;
		uint32 Count = (uint32)Data.ModifiedBlocks.Num();
		Ar << Magic; Ar << Ver; Ar << Count;

		//for (const TPair<int32, uint8>& P : Data.ModifiedBlocks)
		//{
		//	int32 I = P.Key; uint8 Id = P.Value;
		//	Ar << I; Ar << Id;
		//}

		for (const TPair<int32, uint16>& P : Data.ModifiedBlocks) // value is uint16 in your data
		{
			int32 I = P.Key;
			uint8 Id = static_cast<uint8>(P.Value);               // file stays compact (8-bit ids)
			Ar << I; Ar << Id;
		}

		const FString Path = ChunkPath(Seed, Data.Key);
		FFileHelper::SaveArrayToFile(Ar, *Path);
	}
}
