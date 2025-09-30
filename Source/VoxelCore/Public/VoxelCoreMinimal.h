// Copyright Voxel Plugin SAS. All Rights Reserved.

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
///////////                            DO NOT INCLUDE THIS FILE                            ///////////
/////////// Include VoxelMinimal.h instead and add it to your PCH for faster compile times ///////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// ReSharper disable CppUnusedIncludeDirective

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"

// Unreachable code, raises invalid warnings in templates when using if constexpr
#pragma warning(disable : 4702)

#define VOXEL_ENGINE_VERSION (ENGINE_MAJOR_VERSION * 100 + ENGINE_MINOR_VERSION)

// Common includes
#include "EngineUtils.h"
#include "RHIUtilities.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsEngine/BodyInstance.h"

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VOXELCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogVoxel, Log, All);

#define LOG_VOXEL(Verbosity, Format, ...) UE_LOG(LogVoxel, Verbosity, TEXT(Format), ##__VA_ARGS__)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if VOXEL_ENGINE_VERSION >= 507
#define UE_507_SWITCH(Before, AfterOrEqual) AfterOrEqual
#define UE_507_ONLY(...) __VA_ARGS__
#else
#define UE_507_SWITCH(Before, AfterOrEqual) Before
#define UE_507_ONLY(...)
#endif

#if VOXEL_ENGINE_VERSION >= 506
#define UE_506_SWITCH(Before, AfterOrEqual) AfterOrEqual
#define UE_506_ONLY(...) __VA_ARGS__
#else
#define UE_506_SWITCH(Before, AfterOrEqual) Before
#define UE_506_ONLY(...)
#endif

#define MIN_VOXEL_ENGINE_VERSION 505
#define MAX_VOXEL_ENGINE_VERSION 507

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Common forward declarations
class UTexture;
class UStaticMesh;
class UEditorEngine;
class UPhysicalMaterial;
class UMaterialInterface;
class UMaterialInstanceDynamic;
struct FSlateBrush;

namespace Chaos
{
	class FTriangleMeshImplicitObject;
}


#if WITH_EDITOR
extern UNREALED_API UEditorEngine* GEditor;
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#include <cstdint>

namespace std
{
	using uint64 = uint64_t;
	using int64 = int64_t;
}

// Fix clang errors due to long long vs long issues
#define int64_t int64
#define uint64_t uint64

// Engine one doesn't support restricted pointers
template<typename T>
FORCEINLINE void Swap(T* RESTRICT& A, T* RESTRICT& B)
{
	T* RESTRICT Temp = A;
	A = B;
	B = Temp;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if PLATFORM_ANDROID || (defined(PLATFORM_PS4) && PLATFORM_PS4) || (defined(PLATFORM_PS5) && PLATFORM_PS5)
namespace std
{
	template<typename DerivedType, typename BaseType>
	constexpr bool derived_from =
		__is_base_of(BaseType, DerivedType) &&
		__is_convertible_to(
			const volatile remove_reference_t<DerivedType>*,
			const volatile remove_reference_t<BaseType>*);

	template<typename Type>
	concept Voxel_CanCastToBool_Impl = __is_convertible_to(Type, bool);

	template<typename Type>
	concept Voxel_CanCastToBool =
		Voxel_CanCastToBool_Impl<Type>
		&&
		requires(Type&& Value)
		{
			{ !static_cast<Type&&>(Value) } -> Voxel_CanCastToBool_Impl;
		};

	template<typename TypeA, typename TypeB>
	concept Voxel_equality_comparable_OneWay = requires(const remove_reference_t<TypeA>& A, const remove_reference_t<TypeB>& B)
	{
		{ A == B } -> Voxel_CanCastToBool;
		{ A != B } -> Voxel_CanCastToBool;
	};

	template<typename TypeA, typename TypeB>
	concept Voxel_equality_comparable_TwoWays =
		Voxel_equality_comparable_OneWay<TypeA, TypeB> &&
		Voxel_equality_comparable_OneWay<TypeB, TypeA>;

	template<typename Type>
	concept equality_comparable = Voxel_equality_comparable_OneWay<Type, Type>;

	template<typename TypeA, typename TypeB>
	concept equality_comparable_with =
		equality_comparable<TypeA> &&
		equality_comparable<TypeB> &&
		Voxel_equality_comparable_TwoWays<TypeA, TypeB>;
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#include "VoxelMinimal/VoxelStats.h"
#include "VoxelMinimal/VoxelMacros.h"