// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Containers/VoxelBitArray.h"

template<typename Type>
class TVoxelSparseArray
{
private:
	union FValue
	{
		TTypeCompatibleBytes<Type> Value;
		int32 NextFreeIndex = -1;
	};

public:
	TVoxelSparseArray() = default;
	FORCEINLINE ~TVoxelSparseArray()
	{
		Reset();
	}

	void Reserve(const int32 Number)
	{
		AllocationFlags.Reserve(Number);
		Values.Reserve(Number);
	}
	void Reset()
	{
		if (!std::is_trivially_destructible_v<Type>)
		{
			VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

			this->Foreach([&](Type& Value)
			{
				Value.~Type();
			});
		}

		ArrayNum = 0;
		FirstFreeIndex = -1;
		AllocationFlags.Reset();
		Values.Reset();
	}
	void Empty()
	{
		Reset();

		AllocationFlags.Empty();
		Values.Empty();
	}
	FORCEINLINE int32 Num() const
	{
		return ArrayNum;
	}
	FORCEINLINE int64 GetAllocatedSize() const
	{
		return
			AllocationFlags.GetAllocatedSize() +
			Values.GetAllocatedSize();
	}
	FORCEINLINE bool IsValidIndex(const int32 Index) const
	{
		checkVoxelSlow(AllocationFlags.Num() == Values.Num());

		if (!Values.IsValidIndex(Index))
		{
			return false;
		}

		return AllocationFlags[Index];
	}
	FORCEINLINE bool IsAllocated(const int32 Index) const
	{
		checkVoxelSlow(AllocationFlags.Num() == Values.Num());
		return AllocationFlags[Index];
	}

	template<typename LambdaType>
	FORCEINLINE EVoxelIterate Foreach(LambdaType Lambda)
	{
		return AllocationFlags.ForAllSetBits([&](const int32 Index)
		{
			if constexpr (LambdaArgTypes_T<LambdaType>::Num == 1)
			{
				return Lambda(ReinterpretCastRef<Type>(Values[Index].Value));
			}
			else
			{
				return Lambda(ReinterpretCastRef<Type>(Values[Index].Value), Index);
			}
		});
	}
	template<typename LambdaType>
	FORCEINLINE EVoxelIterate Foreach(LambdaType Lambda) const
	{
		return AllocationFlags.ForAllSetBits([&](const int32 Index)
		{
			if constexpr (LambdaArgTypes_T<LambdaType>::Num == 1)
			{
				return Lambda(ReinterpretCastRef<Type>(Values[Index].Value));
			}
			else
			{
				return Lambda(ReinterpretCastRef<Type>(Values[Index].Value), Index);
			}
		});
	}

public:
	FORCEINLINE int32 Add(const Type& Value)
	{
		const int32 Index = this->AddUninitialized();
		new (&(*this)[Index]) Type(Value);
		return Index;
	}
	FORCEINLINE int32 Add(Type&& Value)
	{
		const int32 Index = this->AddUninitialized();
		new (&(*this)[Index]) Type(MoveTemp(Value));
		return Index;
	}

	template<typename... ArgTypes, typename = std::enable_if_t<TIsConstructible<Type, ArgTypes...>::Value>>
	FORCEINLINE int32 Emplace(ArgTypes&&... Args)
	{
		const int32 Index = this->AddUninitialized();
		new (&(*this)[Index]) Type(Forward<ArgTypes>(Args)...);
		return Index;
	}
	template<typename... ArgTypes, typename = std::enable_if_t<TIsConstructible<Type, ArgTypes...>::Value>>
	FORCEINLINE Type& Emplace_GetRef(ArgTypes&&... Args)
	{
		const int32 Index = this->AddUninitialized();
		return *new (&(*this)[Index]) Type(Forward<ArgTypes>(Args)...);
	}

	FORCEINLINE void RemoveAt(const int32 Index)
	{
		checkVoxelSlow(IsValidIndex(Index));

		ArrayNum--;
		checkVoxelSlow(ArrayNum >= 0);

		checkVoxelSlow(AllocationFlags[Index]);
		AllocationFlags[Index] = false;

		FValue& Value = Values[Index];

		ReinterpretCastRef<Type>(Value.Value).~Type();

		Value.NextFreeIndex = FirstFreeIndex;
		FirstFreeIndex = Index;
	}

public:
	FORCEINLINE Type& operator[](const int32 Index)
	{
		checkVoxelSlow(IsValidIndex(Index));

		return ReinterpretCastRef<Type>(Values[Index].Value);
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
#if VOXEL_DEBUG
			const int32 Index = ValuePtr - Array->Values.GetData();
			check(Array->IsAllocated(Index));
#endif
			return ReinterpretCastRef<InType>(*ValuePtr);
		}
		FORCEINLINE void operator++()
		{
#if VOXEL_DEBUG
			{
				const int32 IndexAccordingToWords = 32 * (WordPtr - Array->AllocationFlags.GetWordData()) + (32 - BitsLeftInWord);
				const int32 IndexAccordingToValues = ValuePtr - Array->Values.GetData();
				check(IndexAccordingToWords == IndexAccordingToValues);
			}

			const int32 FirstIndex = 1 + ValuePtr - Array->Values.GetData();
			check(FirstIndex == 1 || Array->IsAllocated(FirstIndex - 1));
			ON_SCOPE_EXIT
			{
				if (WordPtr == LastWordPtr)
				{
					for (int32 Index = FirstIndex; Index < Array->Values.Num(); Index++)
					{
						check(!Array->IsAllocated(Index));
					}
					return;
				}

				{
					const int32 IndexAccordingToWords = 32 * (WordPtr - Array->AllocationFlags.GetWordData()) + (32 - BitsLeftInWord);
					const int32 IndexAccordingToValues = ValuePtr - Array->Values.GetData();
					check(IndexAccordingToWords == IndexAccordingToValues);
				}

				const int32 LastIndex = ValuePtr - Array->Values.GetData();

				for (int32 Index = FirstIndex; Index < LastIndex; Index++)
				{
					check(!Array->IsAllocated(Index));
				}

				check(LastIndex == Array->Values.Num() || Array->IsAllocated(LastIndex));
			};
#endif

			ValuePtr++;
			Word >>= 1;
			BitsLeftInWord--;

			FindFirstSetBit();
		}
		FORCEINLINE bool operator!=(const int32) const
		{
			checkVoxelSlow(WordPtr <= LastWordPtr);
			return WordPtr != LastWordPtr;
		}

	private:
		uint32* WordPtr = nullptr;
		uint32* LastWordPtr = nullptr;
		FValue* ValuePtr = nullptr;
		uint32 Word = 0;
		int32 BitsLeftInWord = 32;
#if VOXEL_DEBUG
		TVoxelSparseArray* Array = nullptr;
#endif

		TIterator() = default;

		FORCEINLINE explicit TIterator(TVoxelSparseArray& Array)
			: WordPtr(Array.AllocationFlags.GetWordData())
			, LastWordPtr(WordPtr + Array.AllocationFlags.NumWords())
			, ValuePtr(Array.Values.GetData())
#if VOXEL_DEBUG
			, Array(&Array)
#endif
		{
			checkVoxelSlow(WordPtr);
			Word = *WordPtr;

			FindFirstSetBit();
		}

		FORCEINLINE void FindFirstSetBit()
		{
		NextBit:
			checkVoxelSlow(BitsLeftInWord >= 0);
			checkVoxelSlow(BitsLeftInWord > 0 || !Word);

			if (Word & 0x1)
			{
				// Fast path, no need to skip
				return;
			}

			if (Word)
			{
				// There's still a valid index, skip to that

				const uint32 IndexInShiftedWord = FVoxelUtilities::FirstBitLow(Word);
				checkVoxelSlow(Word & (1u << IndexInShiftedWord));
				// If 0 handled by fast path above
				checkVoxelSlow(IndexInShiftedWord > 0);

				ValuePtr += IndexInShiftedWord;
				Word >>= IndexInShiftedWord;
				BitsLeftInWord -= IndexInShiftedWord;

				checkVoxelSlow(Word & 0x1);
				checkVoxelSlow(BitsLeftInWord > 0);
				return;
			}

			// Skip forward
			WordPtr++;
			ValuePtr += BitsLeftInWord;

			if (WordPtr == LastWordPtr)
			{
				return;
			}
			checkVoxelSlow(WordPtr < LastWordPtr);

			Word = *WordPtr;
			BitsLeftInWord = 32;

			while (!Word)
			{
				// Skip forward
				WordPtr++;
				ValuePtr += 32;

				if (WordPtr == LastWordPtr)
				{
					return;
				}
				checkVoxelSlow(WordPtr < LastWordPtr);

				Word = *WordPtr;
			}

			goto NextBit;
		}

		friend TVoxelSparseArray;
	};

	FORCEINLINE TIterator<Type> begin()
	{
		if (ArrayNum == 0)
		{
			// No need to iterate the full array
			return {};
		}

		return TIterator<Type>(*this);
	}
	FORCEINLINE TIterator<const Type> begin() const
	{
		return ReinterpretCastRef<TIterator<const Type>>(ConstCast(this)->begin());
	}
	FORCEINLINE int32 end() const
	{
		return 0;
	}

private:
	int32 ArrayNum = 0;
	int32 FirstFreeIndex = -1;

	FVoxelBitArray AllocationFlags;
	TVoxelArray<FValue> Values;

	FORCEINLINE int32 AddUninitialized()
	{
		ArrayNum++;

		if (FirstFreeIndex != -1)
		{
			const int32 Index = FirstFreeIndex;

			FValue& Value = Values[Index];

			FirstFreeIndex = Value.NextFreeIndex;

			checkVoxelSlow(!AllocationFlags[Index]);
			AllocationFlags[Index] = true;

			return Index;
		}
		else
		{
			const int32 Index0 = AllocationFlags.Add(true);
			const int32 Index1 = Values.AddUninitialized();

			checkVoxelSlow(Index0 == Index1);
			return Index0;
		}
	}
};