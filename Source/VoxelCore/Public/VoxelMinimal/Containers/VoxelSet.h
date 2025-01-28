// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Utilities/VoxelHashUtilities.h"

template<typename Type>
struct TVoxelSetElement
{
	Type Value;
	int32 NextElementIndex;

	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, TVoxelSetElement& Element)
	{
		Ar << Element.Value;
		return Ar;
	}
};

struct FVoxelSetIndex
{
public:
	FVoxelSetIndex() = default;
	FVoxelSetIndex(const int32 Index)
		: Index(Index)
	{
	}

	FORCEINLINE bool IsValid() const
	{
		return Index != -1;
	}
	FORCEINLINE operator int32() const
	{
		return Index;
	}

	operator bool() const = delete;

private:
	int32 Index = -1;
};

// Smaller footprint than TSet
// Much faster to Reserve as no sparse array/free list
template<typename Type, typename AllocatorType = FDefaultAllocator>
class TVoxelSet
{
public:
	using FElement = TVoxelSetElement<Type>;

	TVoxelSet() = default;
	TVoxelSet(const TVoxelSet&) = default;
	TVoxelSet& operator=(const TVoxelSet&) = default;

	TVoxelSet(TVoxelSet&& Other)
		: HashSize(Other.HashSize)
		, HashTable(MoveTemp(Other.HashTable))
		, Elements(MoveTemp(Other.Elements))
	{
		Other.Reset();
	}
	TVoxelSet& operator=(TVoxelSet&& Other)
	{
		HashSize = Other.HashSize;
		HashTable = MoveTemp(Other.HashTable);
		Elements = MoveTemp(Other.Elements);
		Other.Reset();
		return *this;
	}

	TVoxelSet(const std::initializer_list<Type> Initializer)
	{
		this->Append(MakeVoxelArrayView(Initializer));
	}
	explicit TVoxelSet(const TConstVoxelArrayView<Type> Array)
	{
		this->Append(Array);
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

	void Reset()
	{
		HashSize = 0;
		Elements.Reset();
		HashTable.Reset();
	}
	void Empty()
	{
		HashSize = 0;
		Elements.Empty();
		HashTable.Empty();
	}
	void Shrink()
	{
		VOXEL_FUNCTION_COUNTER();

		if (Num() == 0)
		{
			// Needed as GetHashSize(0) = 1
			Empty();
			return;
		}

		HashTable.Shrink();
		Elements.Shrink();

		const int32 NewHashSize = GetHashSize(Num());
		if (HashSize != NewHashSize)
		{
			ensure(HashSize > NewHashSize);
			HashSize = NewHashSize;

			Rehash();
		}
	}
	void Reserve(const int32 Number)
	{
		if (Number <= Elements.Num())
		{
			return;
		}

		VOXEL_FUNCTION_COUNTER_NUM(Number, 1024);

		Elements.Reserve(Number);

		const int32 NewHashSize = GetHashSize(Number);
		if (HashSize < NewHashSize)
		{
			HashSize = NewHashSize;
			Rehash();
		}
	}

	template<typename PredicateType>
	void Sort(const PredicateType& Predicate)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		Elements.Sort([&](const FElement& A, const FElement& B)
		{
			return Predicate(A.Value, B.Value);
		});

		Rehash();
	}
	void Append(const TConstVoxelArrayView<Type> Array)
	{
		this->Append<Type>(Array);
	}
	template<typename OtherType, typename = std::enable_if_t<TIsConstructible<Type, OtherType>::Value>>
	void Append(const TConstVoxelArrayView<OtherType> Array)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Array.Num(), 1024);

		this->Reserve(Num() + Array.Num());

		for (const OtherType& Value : Array)
		{
			this->Add(Value);
		}
	}
	template<typename OtherType, typename = std::enable_if_t<TIsConstructible<Type, OtherType>::Value>>
	void Append(const TVoxelSet<OtherType>& Set)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Set.Num(), 1024);

		this->Reserve(Num() + Set.Num());

		for (const OtherType& Value : Set)
		{
			this->Add(Value);
		}
	}
	TVoxelSet Intersect(const TVoxelSet& Other) const
	{
		if (Num() < Other.Num())
		{
			return Other.Intersect(*this);
		}
		checkVoxelSlow(Other.Num() <= Num());

		VOXEL_FUNCTION_COUNTER_NUM(Other.Num(), 1024);

		TVoxelSet Result;
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
	TVoxelSet Union(const TVoxelSet& Other) const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num() + Other.Num(), 1024);

		TVoxelSet Result;
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
	TVoxelSet Difference(const TVoxelSet& Other) const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		TVoxelSet Result;
		Result.Reserve(Num()); // Worst case is no elements of this are in Other

		for (const Type& Value : *this)
		{
			const uint32 Hash = this->HashValue(Value);

			if (!Other.ContainsHashed(Hash, Value))
			{
				Result.AddHashed_CheckNew(Hash, Value);
			}
		}
		return Result;
	}
	bool Includes(const TVoxelSet& Other) const
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
	TVoxelSet<Type> Reverse() const
	{
		TVoxelSet<Type> Result;
		Result.Reserve(Num());
		for (int32 Index = Elements.Num() - 1; Index >= 0; Index--)
		{
			Result.Add(Elements[Index].Value);
		}
		return Result;
	}
	TVoxelArray<Type> Array() const
	{
		TVoxelArray<Type> Result;
		Result.Reserve(Num());
		for (const FElement& Element : Elements)
		{
			Result.Add(Element.Value);
		}
		return Result;
	}
	bool OrderIndependentEqual(const TVoxelSet& Other) const
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
	template<typename ArrayType>
	void BulkAdd(const ArrayType& NewElements)
	{
		VOXEL_FUNCTION_COUNTER_NUM(NewElements.Num(), 1024);
		checkVoxelSlow(Num() == 0);

		FVoxelUtilities::SetNumFast(Elements, NewElements.Num());

		for (int32 Index = 0; Index < NewElements.Num(); Index++)
		{
			Elements[Index].Value = NewElements[Index];
		}

		Rehash();
	}

	friend FArchive& operator<<(FArchive& Ar, TVoxelSet& Set)
	{
		Ar << Set.Elements;

		if (Ar.IsLoading())
		{
			Set.Rehash();
		}

		Set.CheckInvariants();
		return Ar;
	}

public:
	FORCEINLINE const Type& GetValue(const FVoxelSetIndex& Index) const
	{
		return Elements[Index].Value;
	}
	FORCEINLINE const Type& GetUniqueValue() const
	{
		checkVoxelSlow(Elements.Num() == 1);
		return Elements[0].Value;
	}

	FORCEINLINE FVoxelSetIndex Find(const Type& Value) const
	{
		return this->FindHashed(this->HashValue(Value), Value);
	}
	FORCEINLINE FVoxelSetIndex FindHashed(const uint32 Hash, const Type& Value) const
	{
		checkVoxelSlow(Hash == this->HashValue(Value));
		CheckInvariants();

		if (HashSize == 0)
		{
			return -1;
		}

		int32 ElementIndex = this->GetElementIndex(Hash);
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
		CheckInvariants();

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
		CheckInvariants();

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

	FORCEINLINE FVoxelSetIndex Add_CheckNew(const Type& Value)
	{
		return this->AddHashed_CheckNew(this->HashValue(Value), Value);
	}
	FORCEINLINE FVoxelSetIndex Add_EnsureNew(const Type& Value)
	{
		const uint32 Hash = this->HashValue(Value);

		const FVoxelSetIndex Index = this->FindHashed(Hash, Value);
		if (Index.IsValid())
		{
			ensure(false);
			return Index;
		}

		return this->AddHashed_CheckNew(Hash, Value);
	}

	FORCEINLINE FVoxelSetIndex AddHashed_CheckNew(const uint32 Hash, const Type& Value)
	{
		checkVoxelSlow(Hash == this->HashValue(Value));
		checkVoxelSlow(!this->Contains(Value));
		CheckInvariants();

		const int32 NewElementIndex = Elements.Emplace();
		FElement& Element = Elements[NewElementIndex];
		Element.Value = Value;

		if (HashSize < GetHashSize(Elements.Num()))
		{
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

	FORCEINLINE FVoxelSetIndex Add(const Type& Value)
	{
		bool bIsInSet = false;
		return this->FindOrAdd(Value, bIsInSet);
	}
	FORCEINLINE FVoxelSetIndex FindOrAdd(const Type& Value, bool& bIsInSet)
	{
		CheckInvariants();

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

		const int32 NewElementIndex = Elements.Emplace(FElement
		{
			Value
		});
		FElement& Element = Elements[NewElementIndex];

		if (HashSize < GetHashSize(Elements.Num()))
		{
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
		CheckInvariants();

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

		checkVoxelSlow(HashSize >= GetHashSize(Elements.Num()));

		int32& ElementIndex = GetElementIndex(Hash);
		Element.NextElementIndex = ElementIndex;
		ElementIndex = NewElementIndex;
	}

public:
	// Not order-preserving
	FORCEINLINE bool Remove(const Type& Value)
	{
		const uint32 Hash = this->HashValue(Value);
		if (!this->ContainsHashed(Hash, Value))
		{
			return false;
		}

		this->RemoveHashedChecked(Hash, Value);
		return true;
	}
	FORCEINLINE void RemoveChecked(const Type& Value)
	{
		this->RemoveHashedChecked(this->HashValue(Value), Value);
	}
	FORCEINLINE void RemoveHashedChecked(const uint32 Hash, const Type& Value)
	{
		checkVoxelSlow(this->Contains(Value));
		checkVoxelSlow(this->HashValue(Value) == Hash);
		CheckInvariants();

		// Find element index, removing any reference to it
		int32 ElementIndex;
		{
			int32* ElementIndexPtr = &this->GetElementIndex(Hash);
			while (true)
			{
				FElement& Element = Elements[*ElementIndexPtr];
				if (!(Element.Value == Value))
				{
					ElementIndexPtr = &Element.NextElementIndex;
					continue;
				}

				ElementIndex = *ElementIndexPtr;
				*ElementIndexPtr = Element.NextElementIndex;
				break;
			}
		}
		checkVoxelSlow(Elements[ElementIndex].Value == Value);

		// If we're the last element just pop
		if (ElementIndex == Elements.Num() - 1)
		{
			Elements.Pop();
			return;
		}

		// Otherwise move the last element to our index

		const Type LastValue = Elements.Last().Value;
		const uint32 LastHash = this->HashValue(LastValue);

		int32* ElementIndexPtr = &this->GetElementIndex(LastHash);
		while (*ElementIndexPtr != Elements.Num() - 1)
		{
			ElementIndexPtr = &Elements[*ElementIndexPtr].NextElementIndex;
		}

		*ElementIndexPtr = ElementIndex;
		Elements[ElementIndex] = Elements.Pop();
	}

public:
	template<bool bConst>
	struct TIterator
	{
		template<typename T>
		using TType = std::conditional_t<bConst, const T, T>;

		TType<TVoxelSet>* SetPtr = nullptr;
		TType<FElement>* ElementPtr = nullptr;
		int32 Index = 0;

		TIterator() = default;
		FORCEINLINE explicit TIterator(TType<TVoxelSet>& Set)
			: SetPtr(&Set)
		{
			if (Set.Elements.Num() > 0)
			{
				ElementPtr = &Set.Elements[0];
			}
		}

		FORCEINLINE TIterator& operator++()
		{
			Index++;
			if (Index < SetPtr->Elements.Num())
			{
				ElementPtr = &SetPtr->Elements[Index];
			}
			else
			{
				ElementPtr = nullptr;
			}
			return *this;
		}
		FORCEINLINE explicit operator bool() const
		{
			return ElementPtr != nullptr;
		}
		FORCEINLINE TType<Type>& operator*() const
		{
			checkVoxelSlow(ElementPtr);
			return ElementPtr->Value;
		}
		FORCEINLINE TType<Type>* operator->() const
		{
			checkVoxelSlow(ElementPtr);
			return &ElementPtr->Value;
		}
		FORCEINLINE bool operator!=(const TIterator&) const
		{
			return ElementPtr != nullptr;
		}

		FORCEINLINE TType<Type>& Value() const
		{
			checkVoxelSlow(ElementPtr);
			return ElementPtr->Value;
		}

		FORCEINLINE void RemoveCurrent()
		{
			SetPtr->RemoveChecked(MakeCopy(Value()));
			// Check for invalid access
			ElementPtr = nullptr;
			Index--;
		}
	};
	using FIterator = TIterator<false>;
	using FConstIterator = TIterator<true>;

	FORCEINLINE FIterator CreateIterator()
	{
		return FIterator(*this);
	}
	FORCEINLINE FConstIterator CreateIterator() const
	{
		return FConstIterator(*this);
	}

	FORCEINLINE FIterator begin()
	{
		return CreateIterator();
	}
	FORCEINLINE FIterator end()
	{
		return {};
	}

	FORCEINLINE FConstIterator begin() const
	{
		return CreateIterator();
	}
	FORCEINLINE FConstIterator end() const
	{
		return {};
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

	FORCEINLINE static int32 GetHashSize(const int32 NumElements)
	{
		return TSetAllocator<>::GetNumberOfHashBuckets(NumElements);
	}
	FORCEINLINE void CheckInvariants() const
	{
		if (Elements.Num() > 0)
		{
			checkVoxelSlow(HashSize >= GetHashSize(Elements.Num()));
		}
	}

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
		HashSize = FMath::Max(HashSize, GetHashSize(Elements.Num()));
		checkVoxelSlow(HashSize >= 0);

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
using TVoxelInlineSet = TVoxelSet<T, TInlineAllocator<NumInlineElements>>;