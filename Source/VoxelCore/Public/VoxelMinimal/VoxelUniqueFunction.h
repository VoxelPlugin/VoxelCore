// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelMemory.h"

template<typename FuncType>
class TVoxelUniqueFunction;

template<typename>
struct TIsTVoxelUniqueFunction : TIntegralConstant<bool, false>
{
};

template<typename T>
struct TIsTVoxelUniqueFunction<TVoxelUniqueFunction<T>> : TIntegralConstant<bool, true>
{
};

template<typename FunctorType, typename ReturnType, typename... ArgTypes>
ReturnType VoxelCall(void* RawFunctor, ArgTypes&... Args)
{
	return (*static_cast<FunctorType*>(RawFunctor))(Forward<ArgTypes>(Args)...);
}

template<typename ReturnType, typename... ArgTypes>
class TVoxelUniqueFunction<ReturnType(ArgTypes...)>
{
public:
	TVoxelUniqueFunction() = default;
	TVoxelUniqueFunction(decltype(nullptr)) {}

	template<typename FunctorType, typename = typename TEnableIf<
		!TIsTVoxelUniqueFunction<std::decay_t<FunctorType>>::Value&&
		UE::Core::Private::Function::TFuncCanBindToFunctor<ReturnType(ArgTypes...), FunctorType>::Value>::Type>
	FORCEINLINE TVoxelUniqueFunction(FunctorType&& Functor)
	{
		this->Bind(MoveTemp(Functor));
	}
	TVoxelUniqueFunction(const TVoxelUniqueFunction& Other) = delete;

	FORCEINLINE TVoxelUniqueFunction(TVoxelUniqueFunction&& Other)
		: Callable(Other.Callable)
		, Allocation(Other.Allocation)
	{
		Other.Callable = nullptr;
		Other.Allocation = nullptr;
	}
	FORCEINLINE ~TVoxelUniqueFunction()
	{
		if (Callable)
		{
			Unbind();
		}
		checkVoxelSlow(!Callable);
		checkVoxelSlow(!Allocation);
	}

	TVoxelUniqueFunction& operator=(const TVoxelUniqueFunction& Other) = delete;
	FORCEINLINE TVoxelUniqueFunction& operator=(TVoxelUniqueFunction&& Other)
	{
		if (Callable)
		{
			Unbind();
		}
		checkVoxelSlow(!Callable);
		checkVoxelSlow(!Allocation);

		Callable = Other.Callable;
		Allocation = Other.Allocation;

		Other.Callable = nullptr;
		Other.Allocation = nullptr;

		return *this;
	}

	FORCEINLINE ReturnType operator()(ArgTypes... Args) const
	{
		checkVoxelSlow(Callable && Allocation);
		return (*Callable)(Allocation, Args...);
	}

	FORCEINLINE operator bool() const
	{
		checkVoxelSlow((Callable != nullptr) == (Allocation != nullptr));
		return Callable != nullptr;
	}

private:
	struct FStorageBase
	{
		void (*Destroy)(void*);
		uint64 Padding;
	};
	template<typename FunctorType>
	struct TStorage : FStorageBase
	{
		FunctorType Functor;

		explicit TStorage(FunctorType&& Functor)
			: Functor(MoveTemp(Functor))
		{
		}

		static void CallDestroy(void* RawStorage)
		{
			static_cast<TStorage*>(RawStorage)->~TStorage();
			FVoxelMemory::Free(RawStorage);
		}
	};

	ReturnType(*Callable)(void*, ArgTypes&...) = nullptr;
	void* Allocation = nullptr;

	template<typename FunctorType>
	FORCEINLINE void Bind(FunctorType&& Functor)
	{
		checkVoxelSlow(!Callable);
		checkVoxelSlow(!Allocation);

		using FStorage = TStorage<FunctorType>;

		FStorage* Storage = new (GVoxelMemory) FStorage(MoveTemp(Functor));
		Storage->Destroy = &FStorage::CallDestroy;

		Callable = &VoxelCall<FunctorType, ReturnType, ArgTypes...>;

		checkStatic(offsetof(FStorage, Functor) == sizeof(FStorageBase));
		Allocation = reinterpret_cast<uint8*>(Storage) + sizeof(FStorageBase);
	}
	FORCEINLINE void Unbind()
	{
		checkVoxelSlow(Callable);
		checkVoxelSlow(Allocation);

		FStorageBase* Storage = reinterpret_cast<FStorageBase*>(static_cast<uint8*>(Allocation) - sizeof(FStorageBase));
		checkVoxelSlow(Storage->Destroy);
		(*Storage->Destroy)(Storage);

		Callable = nullptr;
		Allocation = nullptr;
	}
};