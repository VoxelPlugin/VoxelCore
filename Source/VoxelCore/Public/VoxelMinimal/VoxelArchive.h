// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "Serialization/MemoryArchive.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"

class VOXELCORE_API FVoxelWriter : public FMemoryArchive
{
public:
	TVoxelArray64<uint8> Bytes;

	FVoxelWriter();

public:
	//~ Begin FMemoryArchive Interface
	virtual void Serialize(void* Data, int64 NumToSerialize) override;
	virtual int64 TotalSize() override;
	virtual FString GetArchiveName() const override;
	//~ End FMemoryArchive Interface
};

template<typename T, typename = void>
struct TCanSerializeWithFArchive : std::false_type {};

template<typename T>
struct TCanSerializeWithFArchive<T, std::void_t<decltype(DeclVal<FArchive&>() << DeclVal<T&>())>> : std::true_type {};

template<typename T>
requires TCanSerializeWithFArchive<T>::value
FORCEINLINE FVoxelWriter& operator<<(FVoxelWriter& Ar, T& Value)
{
	static_cast<FArchive&>(Ar) << Value;
	return Ar;
}
template<typename T>
requires TCanSerializeWithFArchive<T>::value
FORCEINLINE FVoxelWriter& operator<<(FVoxelWriter& Ar, const T& Value)
{
	static_cast<FArchive&>(Ar) << ConstCast(Value);
	return Ar;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELCORE_API FVoxelReader : public FMemoryArchive
{
public:
	const TConstVoxelArrayView64<uint8> Bytes;

	explicit FVoxelReader(const TConstVoxelArrayView64<uint8>& Bytes);

public:
	FORCEINLINE bool HasError() const
	{
		return GetError();
	}
	FORCEINLINE bool IsAtEndWithoutError() const
	{
		return
			Offset == Bytes.Num() &&
			!HasError();
	}

public:
	//~ Begin FMemoryArchive Interface
	virtual void Serialize(void* Data, int64 NumToSerialize) override;
	virtual int64 TotalSize() override;
	virtual FString GetArchiveName() const override;
	//~ End FMemoryArchive Interface
};