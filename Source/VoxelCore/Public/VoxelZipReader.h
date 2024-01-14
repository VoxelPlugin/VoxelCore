// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelZipBase.h"

class VOXELCORE_API FVoxelZipReader : public FVoxelZipBase
{
public:
	using FReadLambda = TFunction<bool(int64 Offset, TVoxelArrayView<uint8> OutData)>;

	static TSharedPtr<FVoxelZipReader> Create(
		int64 TotalSize,
		const FReadLambda& ReadLambda);

	static TSharedPtr<FVoxelZipReader> Create(TConstVoxelArrayView64<uint8> BulkData);

public:
	FORCEINLINE int32 NumFiles() const
	{
		return IndexToPath.Num();
	}
	FORCEINLINE const TVoxelArray<FString>& GetFiles() const
	{
		return IndexToPath;
	}
	FORCEINLINE bool HasFile(const FString& Path) const
	{
		return PathToIndex.Contains(Path);
	}

	bool TryLoad(
		const FString& Path,
		TVoxelArray64<uint8>& OutData,
		bool bAllowParallel = true,
		int64* OutCompressedSize = nullptr) const;

private:
	const FReadLambda ReadLambda;
	TVoxelArray<FString> IndexToPath;
	TVoxelMap<FString, int32> PathToIndex;

	explicit FVoxelZipReader(const FReadLambda& ReadLambda)
		: ReadLambda(ReadLambda)
	{
	}
};