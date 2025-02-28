// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#if WITH_EDITOR
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialFunction.h"
#include "Materials/HLSLMaterialTranslator.h"
#endif

#if WITH_EDITOR
int32 FVoxelUtilities::ZeroDerivative(
	FMaterialCompiler& Compiler,
	const int32 Index)
{
	if (Index == -1)
	{
		return -1;
	}

	struct FHack : FHLSLMaterialTranslator
	{
		void Fix(const int32 Index) const
		{
			(*CurrentScopeChunks)[Index].DerivativeStatus = EDerivativeStatus::Zero;
		}
	};
	static_cast<FHack&>(Compiler).Fix(Index);

	return Index;
}

UMaterialExpression& FVoxelUtilities::CreateMaterialExpression(
	UObject& Outer,
	TSubclassOf<UMaterialExpression> ExpressionClass)
{
	if (UMaterial* Material = Cast<UMaterial>(&Outer))
	{
		UMaterialExpression* Expression = UMaterialEditingLibrary::CreateMaterialExpression(
			Material,
			ExpressionClass);

		check(Expression);

		return *Expression;
	}

	if (UMaterialFunction* Function = Cast<UMaterialFunction>(&Outer))
	{
		UMaterialExpression* Expression = UMaterialEditingLibrary::CreateMaterialExpressionInFunction(
			Function,
			ExpressionClass);

		check(Expression);

		return *Expression;
	}

	check(false);
	return *GetMutableDefault<UMaterialExpression>();
}

TVoxelArray<UMaterialExpression*> FVoxelUtilities::GetMaterialExpressions(const UMaterial& Material)
{
	VOXEL_FUNCTION_COUNTER();

	// See FMaterialEditorUtilities::InitExpressions, GetExpressionCollection() cannot be trusted

	TArray<UObject*> Objects;
	GetObjectsWithOuter(&Material, Objects, false);

	TVoxelArray<UMaterialExpression*> Expressions;
	for (int32 Index = 0; Index < Objects.Num(); ++Index)
	{
		UMaterialExpression* Expression = Cast<UMaterialExpression>(Objects[Index]);
		if (!IsValid(Expression))
		{
			continue;
		}

		Expressions.Add(Expression);
	}

	ensure(TVoxelSet<UMaterialExpression*>(Expressions).Contains(Material.GetExpressionCollection().Expressions));

	return Expressions;
}

TVoxelArray<UMaterialExpression*> FVoxelUtilities::GetMaterialExpressions(const UMaterialFunction& MaterialFunction)
{
	VOXEL_FUNCTION_COUNTER();

	// See FMaterialEditorUtilities::InitExpressions, GetExpressionCollection() cannot be trusted

	TArray<UObject*> Objects;
	GetObjectsWithOuter(&MaterialFunction, Objects, false);

	TVoxelArray<UMaterialExpression*> Expressions;
	for (int32 Index = 0; Index < Objects.Num(); ++Index)
	{
		UMaterialExpression* Expression = Cast<UMaterialExpression>(Objects[Index]);
		if (!IsValid(Expression))
		{
			continue;
		}

		Expressions.Add(Expression);
	}

	ensure(TVoxelSet<UMaterialExpression*>(Expressions).Contains(MaterialFunction.GetExpressionCollection().Expressions));

	return Expressions;
}
#endif