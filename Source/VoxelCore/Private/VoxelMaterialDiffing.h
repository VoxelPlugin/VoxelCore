// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class UMaterialFunction;
class UMaterialExpression;

#if WITH_EDITOR
class FVoxelMaterialDiffing
{
public:
	FString Diff;

	bool Equal(
		const UMaterial& OldMaterial,
		const UMaterial& NewMaterial);

private:
	TVoxelSet<TPair<const UMaterialExpression*, const UMaterialExpression*>> EqualExpressions;
	TVoxelSet<TPair<const UMaterialFunction*, const UMaterialFunction*>> EqualFunctions;

private:
	bool Equal(
		const FProperty& Property,
		const void* OldValue,
		const void* NewValue);

private:
	bool Equal(
		const UMaterialExpression* OldExpression,
		const UMaterialExpression* NewExpression);

	bool Equal(
		const UMaterialExpression& OldExpression,
		const UMaterialExpression& NewExpression);

private:
	bool Equal(
		const UMaterialFunction* OldFunction,
		const UMaterialFunction* NewFunction);

	bool Equal(
		const UMaterialFunction& OldFunction,
		const UMaterialFunction& NewFunction);

private:
	TVoxelArray<FName> Callstack;

	struct FGuard
	{
		FVoxelMaterialDiffing& This;

		FGuard(
			FVoxelMaterialDiffing& This,
			const FName Name)
			: This(This)
		{
			This.Callstack.Add(Name);
		}
		~FGuard()
		{
			This.Callstack.Pop();
		}
	};
};
#endif