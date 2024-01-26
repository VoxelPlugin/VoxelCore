// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Containers/VoxelChunkedArray.h"
#include "VoxelMinimal/Utilities/VoxelTypeUtilities.h"

template<typename Allocator = FVoxelAllocator>
struct TVoxelMapArrayType
{
	template<typename Type>
	using TArrayType = TVoxelArray<Type, Allocator>;

	template<typename Type>
	static void SetNumFast(TArrayType<Type>& Array, const int32 Num)
	{
		FVoxelUtilities::SetNumFast(Array, Num);
	}
	template<typename Type>
	static void Memset(TArrayType<Type>& Array, const uint8 Value)
	{
		FVoxelUtilities::Memset(Array, Value);
	}
};

// Minimize padding by using the best of all possible permutation between Key, Value and NextElementIndex
// In practice we only need to check two permutations:
// - Key Value NextElementIndex: possible merges: KeyValue, ValueNextElementIndex
// - Value Key NextElementIndex: possible merges: KeyValue, KeyNextElementIndex
template<typename KeyType, typename ValueType>
struct TVoxelMapElementBase
{
private:
	struct FElementKeyValue
	{
		const KeyType Key;
		ValueType Value = FVoxelUtilities::MakeSafe<ValueType>();
		int32 NextElementIndex VOXEL_DEBUG_ONLY(= -16);

		FORCEINLINE explicit FElementKeyValue(const KeyType& Key)
			: Key(Key)
		{
		}
	};
	struct FElementValueKey
	{
		ValueType Value = FVoxelUtilities::MakeSafe<ValueType>();
		const KeyType Key;
		int32 NextElementIndex VOXEL_DEBUG_ONLY(= -16);

		FORCEINLINE explicit FElementValueKey(const KeyType& Key)
			: Key(Key)
		{
		}
	};


public:
	using FElement = typename TChooseClass<sizeof(FElementKeyValue) <= sizeof(FElementValueKey), FElementKeyValue, FElementValueKey>::Result;
};

template<typename KeyType, typename ValueType>
struct TVoxelMapElement : TVoxelMapElementBase<KeyType, ValueType>::FElement
{
public:
	using Super = typename TVoxelMapElementBase<KeyType, ValueType>::FElement;

	using Super::Key;
	using Super::Value;

private:
	using Super::NextElementIndex;
	using Super::Super;

	FORCEINLINE bool KeyEquals(const KeyType& OtherKey) const
	{
		return Key == OtherKey;
	}
	FORCEINLINE void MoveFrom(TVoxelMapElement&& Other)
	{
		const_cast<KeyType&>(Key) = MoveTemp(const_cast<KeyType&>(Other.Key));
		Value = MoveTemp(Other.Value);
		NextElementIndex = Other.NextElementIndex;
	}

	template<typename, typename, typename>
	friend class TVoxelMap;
};

// Simple map with an array of elements and a hash table
// The array isn't sparse, so removal will not keep order (it's basically a RemoveSwap)
//
// In a shipping build:
// TVoxelMap::FindChecked   1.1x faster
// TVoxelMap::Remove        1.2x faster
// TVoxelMap::Reserve(1M)  74.4x faster
// TVoxelMap::FindOrAdd     2.2x faster
// TVoxelMap::Add_CheckNew  4.0x faster
template<typename KeyType, typename ValueType, typename ArrayType = TVoxelMapArrayType<>>
class TVoxelMap
{
public:
	template<typename Type>
	using TMapArray = typename ArrayType::template TArrayType<Type>;

	using FElement = TVoxelMapElement<KeyType, ValueType>;

	TVoxelMap() = default;
	TVoxelMap(const TVoxelMap&) = default;
	TVoxelMap& operator=(const TVoxelMap&) = default;

	TVoxelMap(TVoxelMap&& Other)
		: HashSize(Other.HashSize)
		, HashTable(MoveTemp(Other.HashTable))
		, Elements(MoveTemp(Other.Elements))
	{
		Other.Reset();
	}
	TVoxelMap& operator=(TVoxelMap&& Other)
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
	FORCEINLINE void Shrink()
	{
		HashTable.Shrink();
		Elements.Shrink();
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

	FORCENOINLINE bool OrderIndependentEqual(const TVoxelMap& Other) const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		if (Num() != Other.Num())
		{
			return false;
		}

		for (const auto& It : Other)
		{
			const ValueType* Value = this->Find(It.Key);
			if (!Value)
			{
				return false;
			}
			if (!(*Value == It.Value))
			{
				return false;
			}
		}
		return true;
	}
	FORCENOINLINE bool OrderDependentEqual(const TVoxelMap& Other) const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		if (Num() != Other.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < Num(); Index++)
		{
			const FElement& Element = Elements[Index];
			const FElement& OtherElement = Other.Elements[Index];

			if (!(Element.Key == OtherElement.Key) ||
				!(Element.Value == OtherElement.Value))
			{
				return false;
			}
		}

		return true;
	}

	FORCENOINLINE void Append(const TVoxelMap& Other)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Other.Num(), 1024);

		this->Reserve(Num() + Other.Num());

		for (const FElement& Element : Other.Elements)
		{
			this->FindOrAdd(Element.Key) = Element.Value;
		}
	}
	FORCENOINLINE TVoxelArray<KeyType> KeyArray() const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		TVoxelArray<KeyType> Result;
		Result.Reserve(Elements.Num());
		for (const FElement& Element : Elements)
		{
			Result.Add_NoGrow(Element.Key);
		}
		return Result;
	}
	FORCENOINLINE TVoxelArray<ValueType> ValueArray() const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		TVoxelArray<ValueType> Result;
		Result.Reserve(Elements.Num());
		for (const FElement& Element : Elements)
		{
			Result.Add_NoGrow(Element.Value);
		}
		return Result;
	}

public:
	FORCEINLINE ValueType* Find(const KeyType& Key)
	{
		return this->FindHashed(this->HashValue(Key), Key);
	}
	FORCEINLINE ValueType* FindHashed(const uint32 Hash, const KeyType& Key)
	{
		checkVoxelSlow(this->HashValue(Key) == Hash);

		if (HashSize == 0)
		{
			return nullptr;
		}

		int32 ElementIndex = this->GetElementIndex(Hash);
		while (true)
		{
			if (ElementIndex == -1)
			{
				return nullptr;
			}

			FElement& Element = Elements[ElementIndex];
			if (!Element.KeyEquals(Key))
			{
				ElementIndex = Element.NextElementIndex;
				continue;
			}

			return &Element.Value;
		}
	}
	FORCEINLINE const ValueType* Find(const KeyType& Key) const
	{
		return ConstCast(this)->Find(Key);
	}

	FORCEINLINE ValueType FindRef(const KeyType& Key) const
	{
		checkStatic(
			TIsTriviallyDestructible<ValueType>::Value ||
			TIsTWeakPtr_V<ValueType> ||
			TIsTSharedPtr_V<ValueType>);

		if (const ValueType* Value = this->Find(Key))
		{
			return *Value;
		}
		return ValueType();
	}
	FORCEINLINE auto* FindSharedPtr(const KeyType& Key) const
	{
		if (const ValueType* Value = this->Find(Key))
		{
			return Value->Get();
		}
		return nullptr;
	}

	FORCEINLINE ValueType& FindChecked(const KeyType& Key)
	{
		int32 ElementIndex = this->GetElementIndex(this->HashValue(Key));
		while (true)
		{
			checkVoxelSlow(ElementIndex != -1);

			FElement& Element = Elements[ElementIndex];
			if (!Element.KeyEquals(Key))
			{
				ElementIndex = Element.NextElementIndex;
				continue;
			}

			return Element.Value;
		}
	}
	FORCEINLINE const ValueType& FindChecked(const KeyType& Key) const
	{
		return ConstCast(this)->FindChecked(Key);
	}

	FORCEINLINE bool Contains(const KeyType& Key) const
	{
		return this->Find(Key) != nullptr;
	}

	FORCEINLINE ValueType& operator[](const KeyType& Key)
	{
		return this->FindChecked(Key);
	}
	FORCEINLINE const ValueType& operator[](const KeyType& Key) const
	{
		return this->FindChecked(Key);
	}

public:
	FORCEINLINE ValueType& FindOrAdd(const KeyType& Key)
	{
		const uint32 Hash = this->HashValue(Key);

		if (ValueType* Value = this->FindHashed(Hash, Key))
		{
			return *Value;
		}

		return this->AddHashed_CheckNew(Hash, Key);
	}

public:
	// Will crash if Key is already in the map
	// 2x faster than FindOrAdd
	FORCEINLINE ValueType& Add_CheckNew(const KeyType& Key)
	{
		return this->AddHashed_CheckNew(this->HashValue(Key), Key);
	}
	FORCEINLINE ValueType& Add_CheckNew(const KeyType& Key, const ValueType& Value)
	{
		ValueType& ValueRef = this->Add_CheckNew(Key);
		ValueRef = Value;
		return ValueRef;
	}
	FORCEINLINE ValueType& Add_CheckNew(const KeyType& Key, ValueType&& Value)
	{
		ValueType& ValueRef = this->Add_CheckNew(Key);
		ValueRef = MoveTemp(Value);
		return ValueRef;
	}

public:
	FORCEINLINE ValueType& AddHashed_EnsureNew(const uint32 Hash, const KeyType& Key)
	{
		checkVoxelSlow(this->HashValue(Key) == Hash);

		if (ValueType* Value = this->FindHashed(Hash, Key))
		{
			ensure(false);
			return *Value;
		}

		return this->AddHashed_CheckNew(Hash, Key);
	}
	FORCEINLINE ValueType& Add_EnsureNew(const KeyType& Key)
	{
		return this->AddHashed_EnsureNew(this->HashValue(Key), Key);
	}
	FORCEINLINE ValueType& Add_EnsureNew(const KeyType& Key, const ValueType& Value)
	{
		ValueType& ValueRef = this->Add_EnsureNew(Key);
		ValueRef = Value;
		return ValueRef;
	}
	FORCEINLINE ValueType& Add_EnsureNew(const KeyType& Key, ValueType&& Value)
	{
		ValueType& ValueRef = this->Add_EnsureNew(Key);
		ValueRef = MoveTemp(Value);
		return ValueRef;
	}

public:
	FORCEINLINE ValueType& Add_CheckNew_NoRehash(const KeyType& Key)
	{
		return this->AddHashed_CheckNew_NoRehash(HashValue(Key), Key);
	}
	FORCEINLINE ValueType& Add_CheckNew_NoRehash(const KeyType& Key, const ValueType& Value)
	{
		ValueType& ValueRef = this->Add_CheckNew_NoRehash(Key);
		ValueRef = Value;
		return ValueRef;
	}
	FORCEINLINE ValueType& Add_CheckNew_NoRehash(const KeyType& Key, ValueType&& Value)
	{
		ValueType& ValueRef = this->Add_CheckNew_NoRehash(Key);
		ValueRef = MoveTemp(Value);
		return ValueRef;
	}

public:
	template<typename PredicateType>
	FORCENOINLINE void KeySort(const PredicateType& Predicate)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		Elements.Sort([&](const FElement& A, const FElement& B)
		{
			return Predicate(A.Key, B.Key);
		});

		Rehash();
	}
	template<typename PredicateType>
	FORCENOINLINE void ValueSort(const PredicateType& Predicate)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		Elements.Sort([&](const FElement& A, const FElement& B)
		{
			return Predicate(A.Value, B.Value);
		});

		Rehash();
	}

	template<typename PredicateType>
	FORCENOINLINE bool AreKeySorted(const PredicateType& Predicate) const
	{
		for (int32 Index = 1; Index < Elements.Num(); Index++)
		{
			if (!Predicate(Elements[Index - 1].Key, Elements[Index].Key))
			{
				return false;
			}
		}
		return true;
	}
	template<typename PredicateType>
	FORCENOINLINE bool AreValueSorted(const PredicateType& Predicate) const
	{
		for (int32 Index = 1; Index < Elements.Num(); Index++)
		{
			if (!Predicate(Elements[Index - 1].Value, Elements[Index].Value))
			{
				return false;
			}
		}
		return true;
	}

public:
	FORCEINLINE ValueType& AddHashed_CheckNew(const uint32 Hash, const KeyType& Key)
	{
		checkVoxelSlow(!this->Contains(Key));
		checkVoxelSlow(this->HashValue(Key) == Hash);

		const int32 NewElementIndex = Elements.Emplace(Key);
		FElement& Element = Elements[NewElementIndex];

		const int32 DesiredHashSize = TSetAllocator<>::GetNumberOfHashBuckets(Elements.Num());
		if (HashSize < DesiredHashSize)
		{
			HashSize = DesiredHashSize;
			Rehash();
		}
		else
		{
			int32& ElementIndex = this->GetElementIndex(Hash);
			Element.NextElementIndex = ElementIndex;
			ElementIndex = NewElementIndex;
		}

		return Element.Value;
	}
	FORCEINLINE ValueType& AddHashed_CheckNew_NoRehash(const uint32 Hash, const KeyType& Key)
	{
		checkVoxelSlow(!this->Contains(Key));
		checkVoxelSlow(this->HashValue(Key) == Hash);

		const int32 NewElementIndex = Elements.Emplace_NoGrow(Key);
		FElement& Element = Elements[NewElementIndex];

		const int32 DesiredHashSize = TSetAllocator<>::GetNumberOfHashBuckets(Elements.Num());
		checkVoxelSlow(HashSize >= DesiredHashSize);

		int32& ElementIndex = this->GetElementIndex(Hash);
		Element.NextElementIndex = ElementIndex;
		ElementIndex = NewElementIndex;

		return Element.Value;
	}

public:
	// Not order-preserving
	FORCEINLINE bool RemoveAndCopyValue(const KeyType& Key, ValueType& OutRemovedValue)
	{
		const uint32 Hash = this->HashValue(Key);

		ValueType* Value = this->FindHashed(Hash, Key);
		if (!Value)
		{
			return false;
		}
		OutRemovedValue = MoveTemp(*Value);

		this->RemoveHashedChecked(Hash, Key);
		return true;
	}

	// Not order-preserving
	FORCEINLINE bool Remove(const KeyType& Key)
	{
		const uint32 Hash = this->HashValue(Key);
		if (!this->FindHashed(Hash, Key))
		{
			return false;
		}

		this->RemoveHashedChecked(Hash, Key);
		return true;
	}
	FORCEINLINE void RemoveChecked(const KeyType& Key)
	{
		this->RemoveHashedChecked(this->HashValue(Key), Key);
	}
	FORCEINLINE void RemoveHashedChecked(const uint32 Hash, const KeyType& Key)
	{
		checkVoxelSlow(this->Contains(Key));
		checkVoxelSlow(this->HashValue(Key) == Hash);

		// Find element index, removing any reference to it
		int32 ElementIndex;
		{
			int32* ElementIndexPtr = &this->GetElementIndex(Hash);
			while (true)
			{
				FElement& Element = Elements[*ElementIndexPtr];
				if (!Element.KeyEquals(Key))
				{
					ElementIndexPtr = &Element.NextElementIndex;
					continue;
				}

				ElementIndex = *ElementIndexPtr;
				*ElementIndexPtr = Element.NextElementIndex;
				break;
			}
		}
		checkVoxelSlow(Elements[ElementIndex].KeyEquals(Key));

		// If we're the last element just pop
		if (ElementIndex == Elements.Num() - 1)
		{
			Elements.Pop();
			return;
		}

		// Otherwise move the last element to our index

		const KeyType LastKey = Elements.Last().Key;
		const uint32 LastHash = this->HashValue(LastKey);

		int32* ElementIndexPtr = &this->GetElementIndex(LastHash);
		while (*ElementIndexPtr != Elements.Num() - 1)
		{
			ElementIndexPtr = &Elements[*ElementIndexPtr].NextElementIndex;
		}

		*ElementIndexPtr = ElementIndex;
		Elements[ElementIndex].MoveFrom(Elements.Pop());
	}

public:
	template<bool bConst>
	struct TIterator
	{
		template<typename T>
		using TType = typename TChooseClass<bConst, const T, T>::Result;

		TType<TVoxelMap>* MapPtr = nullptr;
		TType<FElement>* ElementPtr = nullptr;
		int32 Index = 0;

		TIterator() = default;
		FORCEINLINE explicit TIterator(TType<TVoxelMap>& Map)
			: MapPtr(&Map)
		{
			if (Map.Elements.Num() > 0)
			{
				ElementPtr = &Map.Elements[0];
			}
		}

		FORCEINLINE TIterator& operator++()
		{
			Index++;
			if (Index < MapPtr->Elements.Num())
			{
				ElementPtr = &MapPtr->Elements[Index];
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
		FORCEINLINE TType<FElement>& operator*() const
		{
			checkVoxelSlow(ElementPtr);
			return *ElementPtr;
		}
		FORCEINLINE bool operator!=(const TIterator&) const
		{
			return ElementPtr != nullptr;
		}

		FORCEINLINE const KeyType& Key() const
		{
			checkVoxelSlow(ElementPtr);
			return ElementPtr->Key;
		}
		FORCEINLINE TType<ValueType>& Value() const
		{
			checkVoxelSlow(ElementPtr);
			return ElementPtr->Value;
		}

		FORCEINLINE void RemoveCurrent()
		{
			MapPtr->RemoveChecked(Key());
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
	FORCEINLINE static uint32 HashValue(const KeyType& Key)
	{
		return FVoxelUtilities::HashValue(Key);
	}

private:
	int32 HashSize = 0;
	TMapArray<int32> HashTable;
	TMapArray<FElement> Elements;

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
		ArrayType::SetNumFast(HashTable, HashSize);
		ArrayType::Memset(HashTable, 0xFF);

		for (int32 Index = 0; Index < Elements.Num(); Index++)
		{
			FElement& Element = Elements[Index];

			int32& ElementIndex = this->GetElementIndex(HashValue(Element.Key));
			Element.NextElementIndex = ElementIndex;
			ElementIndex = Index;
		}
	}
};

template<int32 MaxBytesPerChunk>
struct TVoxelMapChunkedArrayType
{
	template<typename Type>
	using TArrayType = TVoxelChunkedArray<Type, MaxBytesPerChunk>;

	template<typename Type>
	static void SetNumFast(TArrayType<Type>& Array, const int32 Num)
	{
		Array.SetNumUninitialized(Num);
	}
	template<typename Type>
	static void Memset(TArrayType<Type>& Array, const uint8 Value)
	{
		Array.Memset(Value);
	}
};

template<typename KeyType, typename ValueType, int32 MaxBytesPerChunk = GVoxelDefaultAllocationSize>
using TVoxelChunkedMap = TVoxelMap<KeyType, ValueType, TVoxelMapChunkedArrayType<MaxBytesPerChunk>>;

template<typename KeyType, typename ValueType, int32 NumInlineElements>
using TVoxelInlineMap = TVoxelMap<KeyType, ValueType, TVoxelMapArrayType<TVoxelInlineAllocator<NumInlineElements>>>;