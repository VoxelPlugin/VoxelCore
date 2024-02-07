// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelZipBase.h"

class VOXELCORE_API FVoxelZipWriter : public FVoxelZipBase
{
public:
	using FWriteLambda = TFunction<bool(int64 Offset, TConstVoxelArrayView<uint8> Data)>;

	static TSharedRef<FVoxelZipWriter> Create(const FWriteLambda& WriteLambda);
	static TSharedRef<FVoxelZipWriter> Create(TVoxelArray64<uint8>& BulkData);

public:
	bool HasFile(const FString& Path) const;
	bool Finalize();

public:
	void Write(
		const FString& Path,
		TConstVoxelArrayView64<uint8> Data);

	void WriteCompressed(
		const FString& Path,
		TConstVoxelArrayView64<uint8> Data);

	void WriteCompressed(
		const FString& Path,
		const FString& String);

	void WriteCompressed_Oodle(
		const FString& Path,
		TConstVoxelArrayView64<uint8> Data,
		bool bAllowParallel = true,
		FOodleDataCompression::ECompressor Compressor = FOodleDataCompression::ECompressor::Leviathan,
		FOodleDataCompression::ECompressionLevel CompressionLevel = FOodleDataCompression::ECompressionLevel::Optimal3);

private:
	const FWriteLambda WriteLambda;
	TSet<FString> WrittenPaths;
	mutable FVoxelCriticalSection CriticalSection;

	explicit FVoxelZipWriter(const FWriteLambda& WriteLambda)
		: WriteLambda(WriteLambda)
	{
	}

	void WriteImpl(
		const FString& Path,
		TConstVoxelArrayView64<uint8> Data,
		int32 Compression);
};