// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelSet.h"
#include "VoxelMinimal/Containers/VoxelArray.h"

class FMaterialCompiler;
class UMaterialFunction;
class UMaterialInstance;
class UMaterialExpression;
class UMaterialInstanceConstant;
class FHLSLMaterialTranslator;
class FMaterialInstanceParameterUpdateContext;

#if WITH_EDITOR
struct VOXELCORE_API FVoxelMaterialTranslatorNoCodeReuseScope
{
public:
	explicit FVoxelMaterialTranslatorNoCodeReuseScope(FHLSLMaterialTranslator& Translator);

private:
	struct FImpl;
	TPimplPtr<FImpl> Impl;
};
#endif

namespace FVoxelUtilities
{
#if WITH_EDITOR
	VOXELCORE_API int32 ZeroDerivative(
		FMaterialCompiler& Compiler,
		int32 Index);

	VOXELCORE_API UMaterialExpression& CreateMaterialExpression(
		UObject& Outer,
		TSubclassOf<UMaterialExpression> ExpressionClass);

	template<typename T>
	requires std::derived_from<T, UMaterialExpression>
	T& CreateMaterialExpression(UObject& Outer)
	{
		return *CastChecked<T>(&FVoxelUtilities::CreateMaterialExpression(Outer, T::StaticClass()));
	}

	VOXELCORE_API TVoxelArray<UMaterialExpression*> GetMaterialExpressions(const UMaterial& Material);
	VOXELCORE_API TVoxelArray<UMaterialExpression*> GetMaterialExpressions(const UMaterialFunction& MaterialFunction);

	VOXELCORE_API TVoxelArray<UMaterialExpression*> GetMaterialExpressions_Recursive(
		const UMaterial& Material,
		TVoxelSet<const UObject*>* Visited = nullptr);

	VOXELCORE_API TVoxelArray<UMaterialExpression*> GetMaterialExpressions_Recursive(
		const UMaterialFunction& MaterialFunction,
		TVoxelSet<const UObject*>* Visited = nullptr);

	VOXELCORE_API void ClearMaterialExpressions(UMaterial& Material);

	VOXELCORE_API bool CopyParameterValues(
		FMaterialInstanceParameterUpdateContext& UpdateContext,
		UMaterialInstance& Target,
		const UMaterialInterface& Source,
		const FString& ParameterNamePrefix);

	// Merge texture parameters with the same value to reduce the number of SRVs
	VOXELCORE_API void MergeIdenticalTextureParameters(
		UMaterial& Material,
		const UMaterialInstance& MaterialInstance);

	VOXELCORE_API bool AreMaterialsIdentical(
		const UMaterial& OldMaterial,
		const UMaterial& NewMaterial,
		FString& OutDiff);

	// Parent materials are assumed to be identical
	VOXELCORE_API bool AreInstancesIdentical(
		const UMaterialInstanceConstant& OldInstance,
		const UMaterialInstanceConstant& NewInstance,
		FString& OutDiff);
#endif
};