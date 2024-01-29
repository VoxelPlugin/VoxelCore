// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelArray.h"

template<typename Type>
struct TVoxelAddOnlySetElement
{
	Type Value;
	int32 NextElementIndex;

	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, TVoxelAddOnlySetElement& Element)
	{
		Ar << Element.Value;
		return Ar;
	}
};

// Smaller footprint than TSet
// Much faster to Reserve as no sparse array/free list
template<typename Type, typename AllocatorType = FVoxelAllocator>
class TVoxelAddOnlySet
{
public:
	using FElement = TVoxelAddOnlySetElement<Type>;

	TVoxelAddOnlySet() = default;
	TVoxelAddOnlySet(const TVoxelAddOnlySet&) = default;
	TVoxelAddOnlySet& operator=(const TVoxelAddOnlySet&) = default;

	TVoxelAddOnlySet(TVoxelAddOnlySet&& Other)
		: HashSize(Other.HashSize)
		, HashTable(MoveTemp(Other.HashTable))
		, Elements(MoveTemp(Other.Elements))
	{
		Other.Reset();
	}
	TVoxelAddOnlySet& operator=(TVoxelAddOnlySet&& Other)
	{
		HashSize = Other.HashSize;
		HashTable = MoveTemp(Other.HashTable);
		Elements = MoveTemp(Other.Elements);
		Other.Reset();
		return *this;
	}

public:
	FORCEINLINE int32 Num() const
	{
		return Elements.Num();
	}
	FORCEINLINE int64 GetAllocatedSize() const
	{
		return HashTable.GetAllocatedSize() + Elements.GetAllocatedSize();
	}
	FORCEINLINE void Reset()
	{
		HashSize = 0;
		Elements.Reset();
		HashTable.Reset();
	}
	FORCEINLINE void Empty()
	{
		HashSize = 0;
		Elements.Empty();
		HashTable.Empty();
	}
	FORCEINLINE void Reserve(const int32 Number)
	{
		if (Number <= Elements.Num())
		{
			return;
		}

		Elements.Reserve(Number);

		const int32 NewHashSize = TSetAllocator<>::GetNumberOfHashBuckets(Number);

		if (HashSize < NewHashSize)
		{
			HashSize = NewHashSize;
			Rehash();
		}
	}
	template<typename PredicateType>
	FORCENOINLINE void Sort(const PredicateType& Predicate)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		Elements.Sort([&](const FElement& A, const FElement& B)
		{
			return Predicate(A.Value, B.Value);
		});

		Rehash();
	}
	FORCENOINLINE void Append(const TConstVoxelArrayView<Type> Array)
	{
		this->Reserve(Num() + Array.Num());

		for (const Type& Value : Array)
		{
			this->Add(Value);
		}
	}
	FORCENOINLINE TVoxelAddOnlySet Intersect(const TVoxelAddOnlySet& Other) const
	{
		if (Num() < Other.Num())
		{
			return Other.Intersect(*this);
		}
		checkVoxelSlow(Other.Num() <= Num());

		VOXEL_FUNCTION_COUNTER_NUM(Other.Num(), 1024);

		TVoxelAddOnlySet Result;
		Result.Reserve(Other.Num());

		for(const Type& Value : Other)
		{
			const uint32 Hash = this->HashValue(Value);

			if (this->ContainsHashed(Hash, Value))
			{
				Result.AddHashed_CheckNew(Hash, Value);
			}
		}
		return Result;
	}
	FORCENOINLINE TVoxelAddOnlySet Union(const TVoxelAddOnlySet& Other) const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num() + Other.Num(), 1024);

		TVoxelAddOnlySet Result;
		Result.Reserve(Num() + Other.Num());

		for (const Type& Value : *this)
		{
			Result.Add(Value);
		}
		for (const Type& Value : Other)
		{
			Result.Add(Value);
		}
		return Result;
	}
	// Returns all the elements not in Other
	FORCENOINLINE TVoxelAddOnlySet Difference(const TVoxelAddOnlySet& Other) const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		TVoxelAddOnlySet Result;
		Result.Reserve(Num()); // Worst case is no elements of this are in Other

		for (const Type& Value : *this)
		{
			const uint32 Hash = this->HashValue(Value);

			if (!Other->ContainsHashed(Hash, Value))
			{
				Result.AddHashed_CheckNew(Hash, Value);
			}
		}
		return Result;
	}
	FORCENOINLINE bool Includes(const TVoxelAddOnlySet& Other) const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Other, 1024);

		if (Other.Num() > Num())
		{
			return false;
		}

		for (const Type& Value : Other)
		{
			if (!this->Contains(Value))
			{
				return false;
			}
		}
		return true;
	}
	FORCENOINLINE TVoxelAddOnlySet<Type> Reverse() const
	{
		TVoxelAddOnlySet<Type> Result;
		Result.Reserve(Num());
		for (int32 Index = Elements.Num() - 1; Index >= 0; Index--)
		{
			Result.Add(Elements[Index].Value);
		}
		return Result;
	}
	FORCENOINLINE TVoxelArray<Type> Array() const
	{
		TVoxelArray<Type> Result;
		Result.Reserve(Num());
		for (const FElement& Element : Elements)
		{
			Result.Add(Element.Value);
		}
		return Result;
	}
	FORCENOINLINE bool OrderIndependentEqual(const TVoxelAddOnlySet& Other) const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		if (Num() != Other.Num())
		{
			return false;
		}

		for (const Type& Value : Other)
		{
			if (!this->Contains(Value))
			{
				return false;
			}
		}
		return true;
	}

	FORCENOINLINE friend FArchive& operator<<(FArchive& Ar, TVoxelAddOnlySet& Set)
	{
		Ar << Set.Elements;

		if (Ar.IsLoading())
		{
			Set.Rehash();
		}

		return Ar;
	}

public:
	FORCEINLINE const Type& GetUniqueValue() const
	{
		checkVoxelSlow(Elements.Num() == 1);
		return Elements[0].Value;
	}
	FORCEINLINE int32 Find(const Type& Value) const
	{
		if (HashSize == 0)
		{
			return -1;
		}

		int32 ElementIndex = this->GetElementIndex(HashValue(Value));
		while (true)
		{
			if (ElementIndex == -1)
			{
				return -1;
			}

			const FElement& Element = Elements[ElementIndex];
			if (Element.Value == Value)
			{
				return ElementIndex;
			}
			ElementIndex = Element.NextElementIndex;
		}
	}

	FORCEINLINE bool Contains(const Type& Value) const
	{
		return this->ContainsHashed(this->HashValue(Value),	Value);
	}
	FORCEINLINE bool ContainsHashed(const uint32 Hash, const Type& Value) const
	{
		checkVoxelSlow(Hash == this->HashValue(Value));

		if (HashSize == 0)
		{
			return false;
		}

		int32 ElementIndex = this->GetElementIndex(Hash);
		while (true)
		{
			if (ElementIndex == -1)
			{
				return false;
			}

			const FElement& Element = Elements[ElementIndex];
			if (Element.Value == Value)
			{
				return true;
			}
			ElementIndex = Element.NextElementIndex;
		}
	}
	template<typename LambdaType>
	FORCEINLINE bool Contains(const uint32 Hash, LambdaType Matches) const
	{
		if (HashSize == 0)
		{
			return false;
		}

		int32 ElementIndex = this->GetElementIndex(Hash);
		while (true)
		{
			if (ElementIndex == -1)
			{
				return false;
			}

			const FElement& Element = Elements[ElementIndex];
			if (Matches(Element.Value))
			{
				return true;
			}
			ElementIndex = Element.NextElementIndex;
		}
	}

	FORCEINLINE int32 Add_CheckNew(const Type& Value)
	{
		return this->AddHashed_CheckNew(this->HashValue(Value), Value);
	}
	FORCEINLINE int32 AddHashed_CheckNew(const uint32 Hash, const Type& Value)
	{
		checkVoxelSlow(Hash == this->HashValue(Value));
		checkVoxelSlow(!this->Contains(Value));

		const int32 NewElementIndex = Elements.Emplace();
		FElement& Element = Elements[NewElementIndex];
		Element.Value = Value;

		const int32 DesiredHashSize = TSetAllocator<>::GetNumberOfHashBuckets(Elements.Num());
		if (HashSize < DesiredHashSize)
		{
			HashSize = DesiredHashSize;
			Rehash();
		}
		else
		{
			int32& ElementIndex = GetElementIndex(Hash);
			Element.NextElementIndex = ElementIndex;
			ElementIndex = NewElementIndex;
		}

		return NewElementIndex;
	}

	FORCEINLINE int32 Add(const Type& Value)
	{
		bool bIsInSet = false;
		return this->FindOrAdd(Value, bIsInSet);
	}
	FORCEINLINE int32 FindOrAdd(const Type& Value, bool& bIsInSet)
	{
		const uint32 Hash = this->HashValue(Value);

		if (HashSize > 0)
		{
			int32 ElementIndex = this->GetElementIndex(Hash);
			while (ElementIndex != -1)
			{
				FElement& Element = Elements[ElementIndex];
				if (Element.Value == Value)
				{
					bIsInSet = true;
					return ElementIndex;
				}
				ElementIndex = Element.NextElementIndex;
			}
		}

		bIsInSet = false;

		const int32 NewElementIndex = Elements.Emplace();
		FElement& Element = Elements[NewElementIndex];
		Element.Value = Value;

		const int32 DesiredHashSize = TSetAllocator<>::GetNumberOfHashBuckets(Elements.Num());
		if (HashSize < DesiredHashSize)
		{
			HashSize = DesiredHashSize;
			Rehash();
		}
		else
		{
			int32& ElementIndex = GetElementIndex(Hash);
			Element.NextElementIndex = ElementIndex;
			ElementIndex = NewElementIndex;
		}

		return NewElementIndex;
	}
	FORCEINLINE void Add_NoRehash(const Type& Value)
	{
		const uint32 Hash = this->HashValue(Value);

		if (HashSize > 0)
		{
			int32 ElementIndex = this->GetElementIndex(Hash);
			while (ElementIndex != -1)
			{
				FElement& Element = Elements[ElementIndex];
				if (Element.Value == Value)
				{
					return;
				}
				ElementIndex = Element.NextElementIndex;
			}
		}

		const int32 NewElementIndex = Elements.Emplace();
		FElement& Element = Elements[NewElementIndex];
		Element.Value = Value;

		const int32 DesiredHashSize = TSetAllocator<>::GetNumberOfHashBuckets(Elements.Num());
		checkVoxelSlow(HashSize >= DesiredHashSize);

		int32& ElementIndex = GetElementIndex(Hash);
		Element.NextElementIndex = ElementIndex;
		ElementIndex = NewElementIndex;
	}

	template<typename ArrayType>
	FORCENOINLINE void BulkAdd(const ArrayType& NewElements)
	{
		VOXEL_FUNCTION_COUNTER_NUM(NewElements.Num(), 1024);

		checkVoxelSlow(Num() == 0);
		HashSize = TSetAllocator<>::GetNumberOfHashBuckets(NewElements.Num());

		FVoxelUtilities::SetNumFast(Elements, NewElements.Num());

		for (int32 Index = 0; Index < NewElements.Num(); Index++)
		{
			Elements[Index].Value = NewElements[Index];
		}

		Rehash();
	}

public:
	struct FIterator
	{
		typename TVoxelArray<FElement>::RangedForConstIteratorType It;

		FORCEINLINE explicit FIterator(const typename TVoxelArray<FElement>::RangedForConstIteratorType& It)
			: It(It)
		{
		}

		FORCEINLINE FIterator& operator++()
		{
			++It;
			return *this;
		}
		FORCEINLINE explicit operator bool() const
		{
			return bool(It);
		}
		FORCEINLINE const Type& operator*() const
		{
			return (*It).Value;
		}
		FORCEINLINE bool operator!=(const FIterator& Other) const
		{
			return It != Other.It;
		}
	};

	FORCEINLINE FIterator begin() const
	{
		return FIterator(Elements.begin());
	}
	FORCEINLINE FIterator end() const
	{
		return FIterator(Elements.end());
	}

public:
	FORCEINLINE static uint32 HashValue(const Type& Value)
	{
		return FVoxelUtilities::HashValue(Value);
	}

private:
	int32 HashSize = 0;
	TVoxelArray<int32, AllocatorType> HashTable;
	TVoxelArray<FElement, AllocatorType> Elements;

	FORCEINLINE int32& GetElementIndex(const uint32 Hash)
	{
		checkVoxelSlow(HashSize != 0);
		checkVoxelSlow(FMath::IsPowerOfTwo(HashSize));
		return HashTable[Hash & (HashSize - 1)];
	}
	FORCEINLINE const int32& GetElementIndex(const uint32 Hash) const
	{
		checkVoxelSlow(HashSize != 0);
		checkVoxelSlow(FMath::IsPowerOfTwo(HashSize));
		return HashTable[Hash & (HashSize - 1)];
	}

	FORCENOINLINE void Rehash()
	{
		VOXEL_FUNCTION_COUNTER_NUM(HashSize, 1024);

		HashTable.Reset();

		if (HashSize == 0)
		{
			return;
		}

		checkVoxelSlow(FMath::IsPowerOfTwo(HashSize));
		FVoxelUtilities::SetNumFast(HashTable, HashSize);
		FVoxelUtilities::Memset(HashTable, 0xFF);

		for (int32 Index = 0; Index < Elements.Num(); Index++)
		{
			FElement& Element = Elements[Index];

			int32& ElementIndex = this->GetElementIndex(this->HashValue(Element.Value));
			Element.NextElementIndex = ElementIndex;
			ElementIndex = Index;
		}
	}
};

template<typename T, int32 NumInlineElements>
using TVoxelAddOnlySetInline = TVoxelAddOnlySet<T, TVoxelInlineAllocator<NumInlineElements>>;