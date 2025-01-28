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
#endif