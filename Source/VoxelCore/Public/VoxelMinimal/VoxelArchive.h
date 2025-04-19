// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "Serialization/MemoryArchive.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"
#include "VoxelMinimal/Utilities/VoxelArrayUtilities.h"

class VOXELCORE_API FVoxelWriter
{
public:
	FVoxelWriter();

	FORCEINLINE FArchive& Ar()
	{
		return Impl;
	}
	FORCEINLINE void Reserve(const int64 Num)
	{
		Impl.Bytes.Reserve(Num);
	}
	FORCEINLINE TVoxelArray64<uint8>&& Move()
	{
		return MoveTemp(Impl.Bytes);
	}

	FORCEINLINE operator TConstVoxelArrayView64<uint8>() const
	{
		return Impl.Bytes;
	}
	template<typename T, typename = decltype(std::declval<FArchive&>() << std::declval<T&>())>
	FORCEINLINE FVoxelWriter& operator<<(const T& Value)
	{
		Impl << ConstCast(Value);
		return *this;
	}
	template<typename T>
	FORCEINLINE FVoxelWriter& operator<<(const TConstVoxelArrayView64<T> Data)
	{
		if (TVoxelCanBulkSerialize<T>::Value)
		{
			Impl.Serialize(ConstCast(Data.GetData()), Data.Num() * sizeof(T));
		}
		else
		{
			for (const T& Value : Data)
			{
				Impl << ConstCast(Value);
			}
		}
		return *this;
	}

private:
	class VOXELCORE_API FArchiveImpl final : public FMemoryArchive
	{
	public:
		TVoxelArray64<uint8> Bytes;

		//~ Begin FMemoryArchive Interface
		virtual void Serialize(void* Data, int64 NumToSerialize) override;
		virtual int64 TotalSize() override;
		virtual FString GetArchiveName() const override;
		//~ End FMemoryArchive Interface
	};
	FArchiveImpl Impl;
};

class VOXELCORE_API FVoxelReader
{
public:
	explicit FVoxelReader(TConstVoxelArrayView64<uint8> Bytes);

	FORCEINLINE bool HasError() const
	{
		return Impl.GetError();
	}
	FORCEINLINE bool IsAtEndWithoutError() const
	{
		return
			Impl.Offset == Impl.Bytes.Num() &&
			!HasError();
	}

	FORCEINLINE FArchive& Ar()
	{
		return Impl;
	}

	template<typename T, typename = decltype(std::declval<FArchive&>() << std::declval<T&>())>
	FORCEINLINE FVoxelReader& operator<<(T& Value)
	{
		Impl << Value;
		return *this;
	}
	template<typename T>
	FORCEINLINE FVoxelReader& operator<<(const TVoxelArrayView64<T> Data)
	{
		if (TVoxelCanBulkSerialize<T>::Value)
		{
			Impl.Serialize(Data.GetData(), Data.Num() * sizeof(T));
		}
		else
		{
			for (T& Value : Data)
			{
				Impl << Value;
			}
		}
		return *this;
	}

private:
	class VOXELCORE_API FArchiveImpl final : public FMemoryArchive
	{
	public:
		const TConstVoxelArrayView64<uint8> Bytes;
		using FMemoryArchive::Offset;

		explicit FArchiveImpl(const TConstVoxelArrayView64<uint8>& Bytes)
			: Bytes(Bytes)
		{
		}

		//~ Begin FMemoryArchive Interface
		virtual void Serialize(void* Data, int64 NumToSerialize) override;
		virtual int64 TotalSize() override;
		virtual FString GetArchiveName() const override;
		//~ End FMemoryArchive Interface
	};
	FArchiveImpl Impl;
};
