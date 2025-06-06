// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"
#include "VoxelMinimal/Utilities/VoxelTypeUtilities.h"
#include "VoxelMinimal/Utilities/VoxelHashUtilities.h"
#include "VoxelMinimal/Utilities/VoxelArrayUtilities.h"

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
	FORCEINLINE int32 GetIndex() const
	{
		checkVoxelSlow(IsValid());
		return Index;
	}

private:
	int32 Index = -1;
};

struct FVoxelDefaultSetAllocator
{
	static constexpr int32 MinHashSize = 0;

	using FHashArray = TVoxelArray<int32>;

	template<typename Type>
	using TElementArray = TVoxelArray<TVoxelSetElement<Type>>;
};

// Smaller footprint than TSet
// Much faster to Reserve as no sparse array/free list
template<typename Type, typename Allocator = FVoxelDefaultSetAllocator>
class TVoxelSet
{
public:
	using FElement = TVoxelSetElement<Type>;

	TVoxelSet() = default;
	TVoxelSet(const TVoxelSet&) = default;
	TVoxelSet& operator=(const TVoxelSet&) = default;

	TVoxelSet(TVoxelSet&& Other)
		: HashTable(MoveTemp(Other.HashTable))
		, Elements(MoveTemp(Other.Elements))
	{
		Other.Reset();
	}
	TVoxelSet& operator=(TVoxelSet&& Other)
	{
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

	template<typename OtherType, typename OtherAllocator>
	requires
	(
		!std::is_same_v<Type, OtherType> &&
		std::is_constructible_v<Type, OtherType>
	)
	explicit TVoxelSet(const TVoxelSet<OtherType, OtherAllocator>& Other)
	{
		this->Append(Other);
	}

public:
	template<typename OtherType>
	requires FVoxelUtilities::CanCastMemory<Type, OtherType>
	FORCEINLINE operator const TVoxelSet<OtherType, Allocator>&() const &
	{
		return ReinterpretCastRef<TVoxelSet<OtherType, Allocator>>(*this);
	}
	template<typename OtherType>
	requires FVoxelUtilities::CanCastMemory<Type, OtherType>
	FORCEINLINE operator TVoxelSet<OtherType, Allocator>&() &
	{
		return ReinterpretCastRef<TVoxelSet<OtherType, Allocator>>(*this);
	}
	template<typename OtherType>
	requires FVoxelUtilities::CanCastMemory<Type, OtherType>
	FORCEINLINE operator TVoxelSet<OtherType, Allocator>&&() &&
	{
		return ReinterpretCastRef<TVoxelSet<OtherType, Allocator>>(MoveTemp(*this));
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
	FORCEINLINE TVoxelArrayView<FElement> GetElements()
	{
		return Elements;
	}
	FORCEINLINE TConstVoxelArrayView<FElement> GetElements() const
	{
		return Elements;
	}

	void Reset()
	{
		Elements.Reset();
		HashTable.Reset();
	}
	void Empty()
	{
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

		if (HashTable.Num() != GetHashSize(Num()))
		{
			checkVoxelSlow(HashTable.Num() > GetHashSize(Num()));

			HashTable.Reset();
			Rehash();
		}

		HashTable.Shrink();
		Elements.Shrink();
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
		if (HashTable.Num() < NewHashSize)
		{
			FVoxelUtilities::SetNumFast(HashTable, NewHashSize);
			Rehash();
		}
	}
	void ReserveGrow(const int32 Number)
	{
		Reserve(Num() + Number);
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
	void Sort()
	{
		this->Sort(TLess<Type>());
	}
	void Append(const TConstVoxelArrayView<Type> Array)
	{
		this->Append<Type>(Array);
	}
	template<typename OtherType>
	requires std::is_constructible_v<Type, OtherType>
	void Append(const TConstVoxelArrayView<OtherType> Array)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Array.Num(), 1024);

		this->ReserveGrow(Array.Num());

		for (const OtherType& Value : Array)
		{
			this->Add(Value);
		}
	}
	template<typename OtherType, typename OtherAllocator>
	requires std::is_constructible_v<Type, OtherType>
	void Append(const TVoxelSet<OtherType, OtherAllocator>& Set)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Set.Num(), 1024);

		this->ReserveGrow(Set.Num());

		for (const OtherType& Value : Set)
		{
			this->Add(Type(Value));
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

	bool Contains(const TVoxelSet& Other) const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Other.Num(), 1024);

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
	bool Contains(const TConstVoxelArrayView<Type> Other) const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Other.Num(), 1024);

		for (const Type& Value : Other)
		{
			if (!this->Contains(Value))
			{
				return false;
			}
		}
		return true;
	}
	template<typename OtherType, typename AllocatorType>
	requires std::is_constructible_v<Type, OtherType>
	bool Contains(const TArray<OtherType, AllocatorType>& Other) const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Other.Num(), 1024);

		for (const OtherType& Value : Other)
		{
			if (!this->Contains(Type(Value)))
			{
				return false;
			}
		}
		return true;
	}

	TVoxelSet Reverse() const
	{
		TVoxelSet Result;
		Result.Reserve(Num());
		for (int32 Index = Elements.Num() - 1; Index >= 0; Index--)
		{
			Result.Add(Elements[Index].Value);
		}
		return Result;
	}
	template<typename ArrayAllocator = FDefaultAllocator>
	TVoxelArray<Type, ArrayAllocator> Array() const
	{
		TVoxelArray<Type, ArrayAllocator> Result;
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
	FORCEINLINE const Type& GetFirstValue() const
	{
		return Elements[0].Value;
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

		if (HashTable.Num() == 0)
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

		if (HashTable.Num() == 0)
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

		if (HashTable.Num() == 0)
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

public:
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
	FORCEINLINE FVoxelSetIndex Add_EnsureNew_EnsureNoGrow(const Type& Value)
	{
		const uint32 Hash = this->HashValue(Value);

		const FVoxelSetIndex Index = this->FindHashed(Hash, Value);
		if (Index.IsValid())
		{
			ensure(false);
			return Index;
		}

		return this->AddHashed_CheckNew_EnsureNoGrow(Hash, Value);
	}

public:
	FORCEINLINE FVoxelSetIndex Add_CheckNew(const Type& Value)
	{
		return this->AddHashed_CheckNew(this->HashValue(Value), Value);
	}
	FORCEINLINE FVoxelSetIndex Add_CheckNew_EnsureNoGrow(const Type& Value)
	{
		return this->AddHashed_CheckNew_EnsureNoGrow(this->HashValue(Value), Value);
	}

	FORCEINLINE FVoxelSetIndex AddHashed_CheckNew(const uint32 Hash, const Type& Value)
	{
		return this->AddHashed_CheckNew_Impl<true>(Hash, Value);
	}
	FORCEINLINE FVoxelSetIndex AddHashed_CheckNew_EnsureNoGrow(const uint32 Hash, const Type& Value)
	{
		return this->AddHashed_CheckNew_Impl<false>(Hash, Value);
	}

	FORCEINLINE bool TryAdd(const Type& Value)
	{
		bool bIsInSet = false;
		this->FindOrAdd(Value, bIsInSet);
		return !bIsInSet;
	}

	FORCEINLINE FVoxelSetIndex Add(const Type& Value)
	{
		bool bIsInSet = false;
		return this->FindOrAdd(Value, bIsInSet);
	}
	FORCEINLINE FVoxelSetIndex Add_EnsureNoGrow(const Type& Value)
	{
		bool bIsInSet = false;
		return this->FindOrAdd_EnsureNoGrow(Value, bIsInSet);
	}

	FORCEINLINE FVoxelSetIndex FindOrAdd(const Type& Value, bool& bIsInSet)
	{
		return this->FindOrAdd_Impl<true>(Value, bIsInSet);
	}
	FORCEINLINE FVoxelSetIndex FindOrAdd_EnsureNoGrow(const Type& Value, bool& bIsInSet)
	{
		return this->FindOrAdd_Impl<false>(Value, bIsInSet);
	}

private:
	template<bool bAllowGrow>
	FORCEINLINE FVoxelSetIndex AddHashed_CheckNew_Impl(const uint32 Hash, const Type& Value)
	{
		checkVoxelSlow(Hash == this->HashValue(Value));
		checkVoxelSlow(!this->Contains(Value));
		CheckInvariants();

		const int32 NewElementIndex = Elements.Emplace();
		FElement& Element = Elements[NewElementIndex];
		Element.Value = Value;

		if (HashTable.Num() < GetHashSize(Elements.Num()))
		{
			ensureVoxelSlow(bAllowGrow);
			RehashForAdd();
		}
		else
		{
			int32& ElementIndex = GetElementIndex(Hash);
			Element.NextElementIndex = ElementIndex;
			ElementIndex = NewElementIndex;
		}

		return NewElementIndex;
	}

	template<bool bAllowGrow>
	FORCEINLINE FVoxelSetIndex FindOrAdd_Impl(const Type& Value, bool& bIsInSet)
	{
		CheckInvariants();

		const uint32 Hash = this->HashValue(Value);

		if (HashTable.Num() > 0)
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

		if (HashTable.Num() < GetHashSize(Elements.Num()))
		{
			ensureVoxelSlow(bAllowGrow);
			RehashForAdd();
		}
		else
		{
			int32& ElementIndex = GetElementIndex(Hash);
			Element.NextElementIndex = ElementIndex;
			ElementIndex = NewElementIndex;
		}

		return NewElementIndex;
	}

public:
	// Not order-preserving
	FORCEINLINE bool Remove(const Type& Value)
	{
		return this->RemoveHashed(this->HashValue(Value), Value);
	}
	FORCEINLINE void Remove_Ensure(const Type& Value)
	{
		ensure(this->Remove(Value));
	}
	FORCEINLINE bool RemoveHashed(const uint32 Hash, const Type& Value)
	{
		checkVoxelSlow(this->HashValue(Value) == Hash);
		CheckInvariants();

		if (HashTable.Num() == 0)
		{
			return false;
		}

		// Find element index, removing any reference to it
		int32 ElementIndex;
		{
			int32* ElementIndexPtr = &this->GetElementIndex(Hash);
			while (true)
			{
				if (*ElementIndexPtr == -1)
				{
					return false;
				}

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
			return true;
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

		return true;
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
			SetPtr->Remove_Ensure(MakeCopy(Value()));
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
	typename Allocator::FHashArray HashTable;
	typename Allocator::template TElementArray<Type> Elements;

	FORCEINLINE static int32 GetHashSize(const int32 NumElements)
	{
		int32 NewHashSize = FVoxelUtilities::GetHashTableSize(NumElements);

		if constexpr (Allocator::MinHashSize != 0)
		{
			NewHashSize = FMath::Max(NewHashSize, Allocator::MinHashSize);
		}

		return NewHashSize;
	}
	FORCEINLINE void CheckInvariants() const
	{
		if (Elements.Num() > 0)
		{
			checkVoxelSlow(HashTable.Num() >= GetHashSize(Elements.Num()));
		}
	}

	FORCEINLINE int32& GetElementIndex(const uint32 Hash)
	{
		const int32 HashSize = HashTable.Num();
		checkVoxelSlow(HashSize != 0);
		checkVoxelSlow(FMath::IsPowerOfTwo(HashSize));
		return HashTable[Hash & (HashSize - 1)];
	}
	FORCEINLINE const int32& GetElementIndex(const uint32 Hash) const
	{
		return ConstCast(this)->GetElementIndex(Hash);
	}

	FORCENOINLINE void RehashForAdd()
	{
		VOXEL_SCOPE_COUNTER_FORMAT_COND(
			HashTable.Num() > 0,
			"%s::Add Rehash %d -> %d",
			GetGeneratedTypeName<TVoxelSet>(),
			HashTable.Num(),
			GetHashSize(Elements.Num()));

		Rehash();
	}
	FORCENOINLINE void Rehash()
	{
		VOXEL_FUNCTION_COUNTER_NUM(Elements.Num(), 1024);

		const int32 NewHashSize = FMath::Max(HashTable.Num(), GetHashSize(Elements.Num()));
		checkVoxelSlow(NewHashSize >= 0);
		checkVoxelSlow(FMath::IsPowerOfTwo(NewHashSize));

		HashTable.Reset();

		FVoxelUtilities::SetNumFast(HashTable, NewHashSize);
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

template<int32 NumInlineElements>
struct TVoxelInlineSetAllocator
{
	static constexpr int32 MinHashSize = FVoxelUtilities::GetHashTableSize<NumInlineElements>();

	using FHashArray = TVoxelInlineArray<int32, MinHashSize>;

	template<typename Type>
	using TElementArray = TVoxelInlineArray<TVoxelSetElement<Type>, NumInlineElements>;
};

template<typename Type, int32 NumInlineElements>
using TVoxelInlineSet = TVoxelSet<Type, TVoxelInlineSetAllocator<NumInlineElements>>;