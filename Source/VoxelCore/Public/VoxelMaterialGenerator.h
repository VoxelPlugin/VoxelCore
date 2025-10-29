// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "MaterialExpressionIO.h"

class UMaterialInstance;
class UMaterialExpressionMakeMaterialAttributes;

#if WITH_EDITOR
class VOXELCORE_API FVoxelMaterialGenerator
{
public:
	FVoxelMaterialGenerator(
		const TVoxelObjectPtr<const UObject> ErrorOwner,
		UMaterial& NewMaterial,
		const FString& ParameterNamePrefix,
		const bool bSkipCustomOutputs,
		const TFunction<bool(const UMaterialExpression&)> ShouldDuplicateFunction_AdditionalHook,
		const TFunction<void(UMaterialFunction&)> OnTrackMaterialFunction)
		: ErrorOwner(ErrorOwner)
		, NewMaterial(NewMaterial)
		, ParameterNamePrefix(ParameterNamePrefix)
		, bSkipCustomOutputs(bSkipCustomOutputs)
		, ShouldDuplicateFunction_AdditionalHook(ShouldDuplicateFunction_AdditionalHook)
		, OnTrackMaterialFunction(OnTrackMaterialFunction)
	{
	}

public:
	void ForeachExpression(TFunctionRef<void(UMaterialExpression&)> Lambda);
	UMaterialFunction* DuplicateFunctionIfNeeded(UMaterialFunction& OldFunction);
	TVoxelOptional<FMaterialAttributesInput> CopyExpressions(const UMaterial& OldMaterial);

public:
	template<typename T>
	requires std::derived_from<T, UMaterialExpression>
	T& NewExpression()
	{
		T& Expression =  FVoxelUtilities::CreateMaterialExpression<T>(NewMaterial);
		OldToNewExpression.Add_EnsureNew(&Expression, &Expression);
		return Expression;
	}
	UMaterialExpression* GetNewExpression(const UMaterialExpression* OldExpression) const
	{
		return OldToNewExpression.FindRef(OldExpression);
	}

public:
	FVoxelOptionalIntBox2D GetBounds() const;
	void MoveExpressions(const FIntPoint& Offset) const;

private:
	const TVoxelObjectPtr<const UObject> ErrorOwner;
	UMaterial& NewMaterial;
	const FString ParameterNamePrefix;
	const bool bSkipCustomOutputs;
	const TFunction<bool(const UMaterialExpression&)> ShouldDuplicateFunction_AdditionalHook;
	const TFunction<void(UMaterialFunction&)> OnTrackMaterialFunction;

	TVoxelMap<FGuid, FGuid> OldToNewParameterGuid;
	TVoxelMap<FGuid, FGuid> OldToNewNamedRerouteGuid;
	TVoxelMap<const UMaterialExpression*, UMaterialExpression*> OldToNewExpression;
	TVoxelMap<const UMaterialFunction*, UMaterialFunction*> OldToNewFunction;
	TVoxelMap<const UMaterialFunction*, bool> FunctionToShouldDuplicate;

	bool ShouldDuplicateFunction(const UMaterialFunction& Function);
	bool PostCopyExpression(UMaterialExpression& Expression);

	bool CopyFunctionExpressions(
		const UMaterialFunction& OldFunction,
		UMaterialFunction& NewFunction);

	static UMaterialExpression* CloneExpression(
		const UMaterialExpression& Expression,
		UObject& Outer);
};
#endif