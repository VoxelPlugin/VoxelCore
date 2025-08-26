// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Utilities/VoxelLambdaUtilities.h"

template<typename>
class TVoxelFunctionRef;

template<typename>
constexpr bool IsVoxelFunctionRef_V = false;

template<typename T>
constexpr bool IsVoxelFunctionRef_V<TVoxelFunctionRef<T>> = true;

template<typename ReturnType, typename... ArgTypes>
class TVoxelFunctionRef<ReturnType(ArgTypes...)>
{
public:
	template<typename FunctorType>
	requires
	(
		!IsVoxelFunctionRef_V<std::decay_t<FunctorType>> &&
		std::is_invocable_v<FunctorType, ArgTypes...> &&
		(
			std::is_constructible_v<ReturnType, LambdaReturnType_T<FunctorType>> ||
			// For void
			std::is_same_v<ReturnType, LambdaReturnType_T<FunctorType>>
		)
	)
	FORCEINLINE TVoxelFunctionRef(const FunctorType& Functor)
	{
		Callable = &VoxelCall<FunctorType, ReturnType, ArgTypes...>;
		Storage = ConstCast(&Functor);
	}

	FORCEINLINE ReturnType operator()(ArgTypes... Args) const
	{
		checkVoxelSlow(Callable && Storage);
		return (*Callable)(Storage, Args...);
	}

private:
	ReturnType(*Callable)(void*, ArgTypes&...) = nullptr;
	void* Storage = nullptr;
};