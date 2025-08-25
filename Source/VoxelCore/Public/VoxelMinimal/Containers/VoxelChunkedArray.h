// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelIterate.h"
#include "VoxelMinimal/VoxelTypeCompatibleBytes.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Containers/VoxelStaticArray.h"
#include "VoxelMinimal/Utilities/VoxelMathUtilities.h"
#include "VoxelMinimal/Utilities/VoxelArrayUtilities.h"
#include "VoxelMinimal/Utilities/VoxelLambdaUtilities.h"

template<typename Type, int32 MaxBytesPerChunk = 1 << 14>
class TVoxelChunkedArray
{
public:
	static constexpr int32 NumPerChunk_Log2 = FVoxelUtilities::FloorLog2<FMath::DivideAndRoundDown<int32>(MaxBytesPerChunk, sizeof(TVoxelTypeCompatibleBytes<Type>))>();
	static constexpr int32 NumPerChunk = 1 << NumPerChunk_Log2;

	using FChunk = TVoxelStaticArray<TVoxelTypeCompatibleBytes<Type>, NumPerChunk>;
	using FChunkArray = TVoxelInlineArray<TUniquePtr<FChunk>, 1>;

	TVoxelChunkedArray() = default;
	FORCEINLINE TVoxelChunkedArray(TVoxelChunkedArray&& Other)
		: ArrayNum(Other.ArrayNum)
		, NumChunks(Other.NumChunks)
		, ChunkArray(MoveTemp(Other.ChunkArray))
	{
		Other.ArrayNum = 0;
		Other.NumChunks = 0;
	}
	TVoxelChunkedArray(const TVoxelChunkedArray& Other)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Other.Num(), 1024);

		SetNum(Other.Num());

		for (int32 Index = 0; Index < Num(); Index++)
		{
			(*this)[Index] = Other[Index];
		}
	}

	FORCEINLINE ~TVoxelChunkedArray()
	{
		Empty();
	}

	FORCEINLINE TVoxelChunkedArray& operator=(TVoxelChunkedArray&& Other)
	{
		ArrayNum = Other.ArrayNum;
		NumChunks = Other.NumChunks;
		ChunkArray = MoveTemp(Other.ChunkArray);

		Other.ArrayNum = 0;
		Other.NumChunks = 0;

		return *this;
	}
	TVoxelChunkedArray& operator=(const TVoxelChunkedArray& Other)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Other.Num(), 1024);

		Reset();
		SetNum(Other.Num());

		for (int32 Index = 0; Index < Num(); Index++)
		{
			(*this)[Index] = Other[Index];
		}

		return *this;
	}

public:
	void SetNumUninitialized(const int32 NewNum)
	{
		checkStatic(std::is_trivially_destructible_v<Type>);

		ArrayNum = NewNum;

		const int32 NewNumChunks = FVoxelUtilities::DivideCeil_Positive_Log2(ArrayNum, NumPerChunk_Log2);
		if (NumChunks > NewNumChunks)
		{
			NumChunks = NewNumChunks;
			return;
		}

		VOXEL_FUNCTION_COUNTER_NUM(NewNum, 16384);

		ChunkArray.Reserve(NewNumChunks);

		while (NumChunks < NewNumChunks)
		{
			AllocateNewChunk();
		}
	}
	void SetNum(const int32 NewNum)
	{
		VOXEL_FUNCTION_COUNTER_NUM(NewNum, 1024);

		const int32 OldArrayNum = ArrayNum;
		const int32 NewNumChunks = FVoxelUtilities::DivideCeil_Positive_Log2(NewNum, NumPerChunk_Log2);

		if (NewNum < ArrayNum)
		{
			if constexpr (!std::is_trivially_destructible_v<Type>)
			{
				for (int32 Index = NewNum; Index < ArrayNum; Index++)
				{
					(*this)[Index].~Type();
				}
			}

			ArrayNum = NewNum;

			checkVoxelSlow(NumChunks >= NewNumChunks);
			NumChunks = NewNumChunks;

			return;
		}

		ArrayNum = NewNum;

		// Allocate chunks
		{
			checkVoxelSlow(NumChunks <= NewNumChunks);
			ChunkArray.Reserve(NewNumChunks);

			while (NumChunks < NewNumChunks)
			{
				AllocateNewChunk();
			}
		}

		if constexpr (TIsZeroConstructType<Type>::Value)
		{
			this->ForeachView(
				OldArrayNum,
				ArrayNum - OldArrayNum,
				[&](const int32 ViewIndex, const TVoxelArrayView<Type> View)
				{
					FVoxelUtilities::Memzero(View);
				});
		}
		else
		{
			for (int32 Index = OldArrayNum; Index < ArrayNum; Index++)
			{
				new(&(*this)[Index]) Type();
			}
		}
	}
	void Reserve(const int32 Number)
	{
		const int32 NewNumChunks = FVoxelUtilities::DivideCeil_Positive_Log2(Number, NumPerChunk_Log2);

		ChunkArray.Reserve(NewNumChunks);

		while (ChunkArray.Num() < NewNumChunks)
		{
			ChunkArray.Add(MakeUnique<FChunk>(NoInit));
		}
	}
	void Reset()
	{
		if constexpr (!std::is_trivially_destructible_v<Type>)
		{
			for (int32 Index = 0; Index < ArrayNum; Index++)
			{
				(*this)[Index].~Type();
			}
		}

		ArrayNum = 0;
		NumChunks = 0;
	}
	void Empty()
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 4 * NumPerChunk);

		Reset();
		ChunkArray.Empty();
	}
	void Shrink()
	{
		checkVoxelSlow(ChunkArray.Num() >= NumChunks);
		ChunkArray.SetNum(NumChunks);
		ChunkArray.Shrink();
	}
	void Memset(const uint8 Value)
	{
		for (int32 Index = 0; Index < NumChunks; Index++)
		{
			int32 NumToSet = NumPerChunk;
			if (Index == NumChunks - 1)
			{
				NumToSet = Num() % NumPerChunk;

				if (NumToSet == 0)
				{
					NumToSet = NumPerChunk;
				}
			}
			FVoxelUtilities::Memset(GetChunkView(Index).LeftOf(NumToSet), Value);
		}
	}

	TVoxelArray<Type> Array() const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		if constexpr (std::is_trivially_destructible_v<Type>)
		{
			TVoxelArray<Type> Result;
			FVoxelUtilities::SetNumFast(Result, Num());

			this->ForeachView(
				0,
				Num(),
				[&](const int32 ViewIndex, const TConstVoxelArrayView<Type> View)
				{
					FVoxelUtilities::Memcpy(
						MakeVoxelArrayView(Result).Slice(ViewIndex, View.Num()),
						View);
				});

			return Result;
		}
		else
		{
			TVoxelArray<Type> Result;
			Result.Reserve(Num());
			for (const Type& Value : *this)
			{
				Result.Add_EnsureNoGrow(Value);
			}
			return Result;
		}
	}

	void GatherInline(const TConstVoxelArrayView<int32> Indices)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 128);
		checkVoxelSlow(Algo::IsSorted(Indices));

		int32 WriteIndex = 0;
		for (const int32 Index : Indices)
		{
			checkVoxelSlow(WriteIndex <= Index);

			if (WriteIndex != Index)
			{
				(*this)[WriteIndex] = (*this)[Index];
			}

			WriteIndex++;
		}

		checkVoxelSlow(WriteIndex <= Num());
		SetNum(WriteIndex);
	}

	bool operator==(const TVoxelChunkedArray& Other) const
	{
		if (Num() != Other.Num())
		{
			return false;
		}

		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		int32 Index = 0;
		while (Index < Num())
		{
			const int32 ChunkIndex = FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index);
			const int32 NumInChunk = FMath::Min(NumPerChunk, Num() - Index);

			if (!CompareItems(
				GetChunkView(ChunkIndex).GetData(),
				Other.GetChunkView(ChunkIndex).GetData(),
				NumInChunk))
			{
				return false;
			}

			Index += NumInChunk;
		}
		checkVoxelSlow(Index == Num());

		return true;
	}

	friend FArchive& operator<<(FArchive& Ar, TVoxelChunkedArray& Array)
	{
		int32 Num = Array.Num();
		Ar << Num;

		VOXEL_FUNCTION_COUNTER_NUM(Num, 1024);

		if constexpr (TCanBulkSerialize<Type>::Value)
		{
			if (Ar.IsLoading())
			{
				Array.SetNumUninitialized(Num);
			}

			for (int32 Index = 0; Index < Num; Index += NumPerChunk)
			{
				const int32 NumInChunk = FMath::Min(Num - Index, NumPerChunk);
				checkVoxelSlow(&Array[Index + NumInChunk - 1] - &Array[Index] == (NumInChunk - 1));

				Ar.Serialize(&Array[Index], NumInChunk * sizeof(Type));
			}
		}
		else
		{
			if (Ar.IsLoading())
			{
				Array.SetNum(Num);
			}

			for (int32 Index = 0; Index < Num; Index++)
			{
				Ar << Array[Index];
			}
		}

		return Ar;
	}

	FORCEINLINE int32 Num() const
	{
		return ArrayNum;
	}
	FORCEINLINE int64 GetAllocatedSize() const
	{
		return ChunkArray.GetAllocatedSize() + ChunkArray.Num() * sizeof(FChunk);
	}
	FORCEINLINE bool IsValidIndex(const int32 Index) const
	{
		return 0 <= Index && Index < ArrayNum;
	}

public:
	FORCEINLINE TVoxelArrayView<Type> MakeView(
		const int32 Index,
		const int32 Count)
	{
		checkVoxelSlow(IsValidIndex(Index));
		checkVoxelSlow(IsValidIndex(Index + Count - 1));
		checkVoxelSlow(FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index) == FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index + Count - 1));

		const int32 ChunkIndex = FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index);
		const int32 ChunkOffset = FVoxelUtilities::GetChunkOffset<NumPerChunk>(Index);

		return this->GetChunkView(ChunkIndex).Slice(ChunkOffset, Count);
	}
	FORCEINLINE TConstVoxelArrayView<Type> MakeView(
		const int32 Index,
		const int32 Count) const
	{
		checkVoxelSlow(IsValidIndex(Index));
		checkVoxelSlow(IsValidIndex(Index + Count - 1));
		checkVoxelSlow(FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index) == FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index + Count - 1));

		return this->GetChunkView(FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index)).LeftOf(Count);
	}

	template<typename LambdaType>
	requires
	(
		LambdaHasSignature_V<LambdaType, void(int32, TVoxelArrayView<Type>)> ||
		LambdaHasSignature_V<LambdaType, EVoxelIterate(int32, TVoxelArrayView<Type>)>
	)
	FORCEINLINE EVoxelIterate ForeachView(
		const int32 StartIndex,
		const int32 Count,
		LambdaType Lambda)
	{
		const int32 EndIndex = StartIndex + Count;
		int32 Index = StartIndex;
		while (Index < EndIndex)
		{
			const int32 ChunkIndex = FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index);
			const int32 ChunkOffset = FVoxelUtilities::GetChunkOffset<NumPerChunk>(Index);
			const int32 NumInChunk = FMath::Min(NumPerChunk - ChunkOffset, EndIndex - Index);

			if constexpr (std::is_void_v<LambdaReturnType_T<LambdaType>>)
			{
				Lambda(Index, GetChunkView(ChunkIndex).Slice(ChunkOffset, NumInChunk));
			}
			else
			{
				if (Lambda(Index, GetChunkView(ChunkIndex).Slice(ChunkOffset, NumInChunk)) == EVoxelIterate::Stop)
				{
					return EVoxelIterate::Stop;
				}
			}

			Index += NumInChunk;
		}
		checkVoxelSlow(Index == EndIndex);

		return EVoxelIterate::Continue;
	}
	template<typename LambdaType>
	requires
	(
		LambdaHasSignature_V<LambdaType, void(int32, TConstVoxelArrayView<Type>)> ||
		LambdaHasSignature_V<LambdaType, EVoxelIterate(int32, TConstVoxelArrayView<Type>)>
	)
	FORCEINLINE EVoxelIterate ForeachView(
		const int32 StartIndex,
		const int32 Count,
		LambdaType Lambda) const
	{
		const int32 EndIndex = StartIndex + Count;
		int32 Index = StartIndex;
		while (Index < EndIndex)
		{
			const int32 ChunkIndex = FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index);
			const int32 ChunkOffset = FVoxelUtilities::GetChunkOffset<NumPerChunk>(Index);
			const int32 NumInChunk = FMath::Min(NumPerChunk - ChunkOffset, EndIndex - Index);

			if constexpr (std::is_void_v<LambdaReturnType_T<LambdaType>>)
			{
				Lambda(Index, GetChunkView(ChunkIndex).Slice(ChunkOffset, NumInChunk));
			}
			else
			{
				if (Lambda(Index, GetChunkView(ChunkIndex).Slice(ChunkOffset, NumInChunk)) == EVoxelIterate::Stop)
				{
					return EVoxelIterate::Stop;
				}
			}

			Index += NumInChunk;
		}
		checkVoxelSlow(Index == EndIndex);

		return EVoxelIterate::Continue;
	}

	template<typename LambdaType>
	requires
	(
		LambdaHasSignature_V<LambdaType, void(int32, TVoxelArrayView<Type>)> ||
		LambdaHasSignature_V<LambdaType, EVoxelIterate(int32, TVoxelArrayView<Type>)>
	)
	FORCEINLINE EVoxelIterate ForeachView(LambdaType Lambda)
	{
		return this->ForeachView(0, Num(), MoveTemp(Lambda));
	}
	template<typename LambdaType>
	requires
	(
		LambdaHasSignature_V<LambdaType, void(int32, TConstVoxelArrayView<Type>)> ||
		LambdaHasSignature_V<LambdaType, EVoxelIterate(int32, TConstVoxelArrayView<Type>)>
	)
	FORCEINLINE EVoxelIterate ForeachView(LambdaType Lambda) const
	{
		return this->ForeachView(0, Num(), MoveTemp(Lambda));
	}

public:
	FORCEINLINE int32 AddUninitialized()
	{
		checkStatic(std::is_trivially_destructible_v<Type>);
		return AddUninitializedImpl();
	}
	FORCEINLINE int32 AddUninitialized(const int32 Count)
	{
		checkStatic(std::is_trivially_destructible_v<Type>);
		return AddUninitializedImpl(Count);
	}

	FORCEINLINE int32 AddZeroed(const int32 Count)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Count, 1024);
		checkStatic(std::is_trivially_destructible_v<Type>);
		checkVoxelSlow(Count >= 0);

		const int32 Index = AddUninitializedImpl(Count);

		this->ForeachView(
			Index,
			Count,
			[&](const int32 ViewIndex, const TVoxelArrayView<Type> View)
			{
				FVoxelUtilities::Memzero(View);
			});

		return Index;
	}
	FORCEINLINE int32 Add(const Type& Value, const int32 Count)
	{
		checkVoxelSlow(Count >= 0);

		const int32 Index = AddUninitializedImpl(Count);

		this->ForeachView(
			Index,
			Count,
			[&](const int32 ViewIndex, const TVoxelArrayView<Type> View)
			{
				if constexpr (std::is_trivially_destructible_v<Type>)
				{
					FVoxelUtilities::SetAll(View, Value);
				}
				else
				{
					for (Type& It : View)
					{
						new (&It) Type(Value);
					}
				}
			});

		return Index;
	}

	FORCEINLINE int32 Add(const Type& Value)
	{
		const int32 Index = AddUninitializedImpl();
		new (&(*this)[Index]) Type(Value);
		return Index;
	}
	FORCEINLINE int32 Add(Type&& Value)
	{
		const int32 Index = AddUninitializedImpl();
		new (&(*this)[Index]) Type(MoveTemp(Value));
		return Index;
	}
	template<typename... ArgsType>
	FORCEINLINE int32 Emplace(ArgsType&&... Args)
	{
		const int32 Index = AddUninitializedImpl();
		new (&(*this)[Index]) Type(Forward<ArgsType>(Args)...);
		return Index;
	}

	FORCEINLINE int32 Append(const TConstVoxelArrayView<Type> Other)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Other.Num(), 1024);

		const int32 StartIndex = this->AddUninitializedImpl(Other.Num());

		if constexpr (std::is_trivially_destructible_v<Type>)
		{
			this->ForeachView(
				StartIndex,
				Other.Num(),
				[&](const int32 ViewIndex, const TVoxelArrayView<Type> View)
				{
					FVoxelUtilities::Memcpy(
						View,
						Other.Slice(ViewIndex - StartIndex, View.Num()));
				});
		}
		else
		{
			for (int32 Index = 0; Index < Other.Num(); Index++)
			{
				new (&(*this)[StartIndex + Index]) Type(Other[Index]);
			}
		}

		return StartIndex;
	}
	int32 Append(const TVoxelChunkedArray& Other)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Other.Num(), 1024);

		const int32 Index = Num();

		Other.ForeachView(
			0,
			Other.Num(),
			[&](const int32 ViewIndex, const TConstVoxelArrayView<Type> View)
			{
				this->Append(View);
			});

		return Index;
	}

	FORCEINLINE Type& Add_GetRef(Type&& Value)
	{
		const int32 Index = AddUninitializedImpl();
		Type& ValueRef = (*this)[Index];
		new (&ValueRef) Type(MoveTemp(Value));
		return ValueRef;
	}
	template<typename... ArgTypes>
	requires std::is_constructible_v<Type, ArgTypes&&...>
	FORCEINLINE Type& Emplace_GetRef(ArgTypes&&... Args)
	{
		const int32 Index = AddUninitializedImpl();
		Type& ValueRef = (*this)[Index];
		new (&ValueRef) Type(Forward<ArgTypes>(Args)...);
		return ValueRef;
	}

	[[nodiscard]] FORCEINLINE Type Pop()
	{
		Type Value = MoveTemp((*this)[ArrayNum - 1]);
		Pop_Discard();
		return Value;
	}
	FORCEINLINE void Pop_Discard()
	{
		(*this)[ArrayNum - 1].~Type();
		ArrayNum--;

		if (ArrayNum % NumPerChunk == 0)
		{
			NumChunks--;
		}
	}

	FORCEINLINE int32 Find(const Type& Item) const
	{
		int32 Index = -1;
		this->ForeachView([&](const int32 ViewIndex, const TConstVoxelArrayView<Type> View)
		{
			for (const Type& Value : View)
			{
				if (Value != Item)
				{
					continue;
				}

				Index = ViewIndex + int32(&Value - View.GetData());
				return EVoxelIterate::Stop;
			}

			return EVoxelIterate::Continue;
		});

		checkVoxelSlow(Index == -1 || (*this)[Index] == Item);
		return Index;
	}
	FORCEINLINE bool Contains(const Type& Item) const
	{
		return this->Find(Item) != -1;
	}

public:
	FORCEINLINE void CopyFrom(const int32 StartIndex, const TConstVoxelArrayView<Type> Other)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Other.Num(), 128);
		checkVoxelSlow(IsValidIndex(StartIndex));
		checkVoxelSlow(IsValidIndex(StartIndex + Other.Num() - 1));

		if constexpr (std::is_trivially_destructible_v<Type>)
		{
			this->ForeachView(
				StartIndex,
				Other.Num(),
				[&](const int32 ViewIndex, const TVoxelArrayView<Type> View)
				{
					FVoxelUtilities::Memcpy(
						View,
						Other.Slice(ViewIndex - StartIndex, View.Num()));
				});
		}
		else
		{
			for (int32 Index = 0; Index < Other.Num(); Index++)
			{
				(*this)[StartIndex + Index] = Type(Other[Index]);
			}
		}
	}
	FORCEINLINE void CopyTo(const int32 StartIndex, const TVoxelArrayView<Type> Other) const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Other.Num(), 128);
		checkVoxelSlow(IsValidIndex(StartIndex));
		checkVoxelSlow(IsValidIndex(StartIndex + Other.Num() - 1));

		if constexpr (std::is_trivially_destructible_v<Type>)
		{
			this->ForeachView(
				StartIndex,
				Other.Num(),
				[&](const int32 ViewIndex, const TConstVoxelArrayView<Type> View)
				{
					FVoxelUtilities::Memcpy(
						Other.Slice(ViewIndex - StartIndex, View.Num()),
						View);
				});
		}
		else
		{
			for (int32 Index = 0; Index < Other.Num(); Index++)
			{
				Other[Index] = Type((*this)[StartIndex + Index]);
			}
		}
	}

	FORCEINLINE void CopyFrom(const TConstVoxelArrayView<Type> Other)
	{
		checkVoxelSlow(Num() == Other.Num());
		this->CopyFrom(0, Other);
	}
	FORCEINLINE void CopyTo(const TVoxelArrayView<Type> Other) const
	{
		checkVoxelSlow(Num() == Other.Num());
		this->CopyTo(0, Other);
	}

public:
	class FChunkView : public TVoxelArrayView<Type>
	{
	public:
		~FChunkView()
		{
			if constexpr (!std::is_trivially_destructible_v<Type>)
			{
				for (Type& Value : *this)
				{
					Value.~Type();
				}
			}
		}

	private:
		const TUniquePtr<FChunk> Chunk;

		FChunkView(
			TUniquePtr<FChunk> Chunk,
			const int32 Num)
			: TVoxelArrayView<Type>(ReinterpretCastPtr<Type>(Chunk->GetData()), Num)
			, Chunk(MoveTemp(Chunk))
		{
		}

		friend TVoxelChunkedArray;
	};
	FChunkView PopFirstChunk()
	{
		VOXEL_FUNCTION_COUNTER();
		checkVoxelSlow(Num() > 0);

		TUniquePtr<FChunk> Chunk = MoveTemp(ChunkArray[0]);
		const int32 NumRemoved = FMath::Min(NumPerChunk, Num());

		ArrayNum -= NumRemoved;
		NumChunks--;
		ChunkArray.RemoveAt(0);

		return FChunkView(MoveTemp(Chunk), NumRemoved);
	}

public:
	FORCEINLINE Type& operator[](const int32 Index)
	{
		checkVoxelSlow(IsValidIndex(Index));
		return this->GetChunkView(FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index))[FVoxelUtilities::GetChunkOffset<NumPerChunk>(Index)];
	}
	FORCEINLINE const Type& operator[](const int32 Index) const
	{
		return ConstCast(this)->operator[](Index);
	}

public:
	template<typename InType>
	struct TIterator
	{
	public:
		FORCEINLINE InType& operator*() const
		{
			return *Value;
		}
		FORCEINLINE void operator++()
		{
			++Value;

			if (Value != ReinterpretCastPtr<Type>((*ChunkIterator)->GetData() + (*ChunkIterator)->Num()))
			{
				return;
			}

			++ChunkIterator;

			if (ChunkIterator == ChunkIteratorEnd)
			{
				// Last chunk
				Value = nullptr;
				return;
			}

			Value = ReinterpretCastPtr<Type>((*ChunkIterator)->GetData());
		}
		FORCEINLINE bool operator!=(const Type* End) const
		{
			return Value != End;
		}

	private:
		Type* Value = nullptr;
		const TUniquePtr<FChunk>* ChunkIterator = nullptr;
		const TUniquePtr<FChunk>* ChunkIteratorEnd = nullptr;

		friend TVoxelChunkedArray;
	};

	FORCEINLINE TIterator<Type> begin()
	{
		if (ArrayNum == 0)
		{
			return {};
		}

		TIterator<Type> Iterator;
		Iterator.Value = &GetChunkView(0)[0];
		Iterator.ChunkIterator = ChunkArray.GetData();
		Iterator.ChunkIteratorEnd = ChunkArray.GetData() + NumChunks;
		return Iterator;
	}
	FORCEINLINE TIterator<const Type> begin() const
	{
		return ReinterpretCastRef<TIterator<const Type>>(ConstCast(this)->begin());
	}

	FORCEINLINE const Type* end() const
	{
		if (ArrayNum == 0)
		{
			return nullptr;
		}

		const int32 ChunkIndex = FVoxelUtilities::GetChunkIndex<NumPerChunk>(ArrayNum);
		if (ChunkIndex >= NumChunks)
		{
			// ChunkIteratorEnd will handle termination
			return nullptr;
		}

		return &GetChunkView(ChunkIndex)[FVoxelUtilities::GetChunkOffset<NumPerChunk>(ArrayNum)];
	}

private:
	int32 ArrayNum = 0;
	int32 NumChunks = 0;
	FChunkArray ChunkArray;

	FORCEINLINE int32 AddUninitializedImpl()
	{
		if (ArrayNum % NumPerChunk == 0)
		{
			AllocateNewChunk();
		}
		return ArrayNum++;
	}
	FORCEINLINE int32 AddUninitializedImpl(const int32 Count)
	{
		checkVoxelSlow(Count >= 0);

		const int32 OldNum = ArrayNum;
		ArrayNum += Count;
		const int32 NewNum = ArrayNum;

		const int32 OldNumChunks = FVoxelUtilities::DivideCeil_Positive_Log2(OldNum, NumPerChunk_Log2);
		const int32 NewNumChunks = FVoxelUtilities::DivideCeil_Positive_Log2(NewNum, NumPerChunk_Log2);

		checkVoxelSlow(NumChunks == OldNumChunks);
		for (int32 ChunkIndex = OldNumChunks; ChunkIndex < NewNumChunks; ChunkIndex++)
		{
			AllocateNewChunk();
		}
		checkVoxelSlow(NumChunks == NewNumChunks);

		return OldNum;
	}

	FORCENOINLINE void AllocateNewChunk()
	{
		checkVoxelSlow(NumChunks <= ChunkArray.Num());

		if (NumChunks == ChunkArray.Num())
		{
			ChunkArray.Add(MakeUnique<FChunk>(NoInit));
		}

		NumChunks++;

		checkVoxelSlow(NumChunks <= ChunkArray.Num());
	}
	FORCEINLINE TVoxelArrayView<Type> GetChunkView(const int32 ChunkIndex)
	{
		checkVoxelSlow(0 <= ChunkIndex && ChunkIndex < NumChunks);
		return MakeVoxelArrayView(*ChunkArray[ChunkIndex]).template ReinterpretAs<Type>();
	}
	FORCEINLINE TConstVoxelArrayView<Type> GetChunkView(const int32 ChunkIndex) const
	{
		checkVoxelSlow(0 <= ChunkIndex && ChunkIndex < NumChunks);
		return MakeVoxelArrayView(*ChunkArray[ChunkIndex]).template ReinterpretAs<Type>();
	}
};