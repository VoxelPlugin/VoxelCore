// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelHLSLMaterialTranslator.h"
#if WITH_EDITOR
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialFunction.h"
#endif

#if WITH_EDITOR
struct FVoxelMaterialTranslatorNoCodeReuseScope::FImpl
{
	FVoxelHLSLMaterialTranslator& Translator;

	TArray<FShaderCodeChunk>* CurrentScopeChunks;
	TArray<FShaderCodeChunk> LocalChunks;
	TArray<FMaterialCustomExpressionEntry> LocalCustomExpressions;

	explicit FImpl(FVoxelHLSLMaterialTranslator& Translator)
		: Translator(Translator)
	{
		CurrentScopeChunks = Translator.CurrentScopeChunks;
		Translator.CurrentScopeChunks = &LocalChunks;

		// Clear the vtable cache, code can't be reused across inputs
		Translator.VTStackHash = {};

		// Forbid custom expression reuse
		LocalCustomExpressions = Translator.CustomExpressions;

		for (FMaterialCustomExpressionEntry& Entry : Translator.CustomExpressions)
		{
			Entry.Expression = nullptr;
		}
	}
	~FImpl()
	{
		Translator.CurrentScopeChunks = CurrentScopeChunks;
		Translator.CurrentScopeChunks->Append(LocalChunks);

		check(LocalCustomExpressions.Num() <= Translator.CustomExpressions.Num());
		for (int32 Index = 0; Index < LocalCustomExpressions.Num(); Index++)
		{
			const FMaterialCustomExpressionEntry& Source = LocalCustomExpressions[Index];
			FMaterialCustomExpressionEntry& Target = Translator.CustomExpressions[Index];

			ensure(Source.ScopeID == Target.ScopeID);
			ensure(Source.Expression && !Target.Expression);
			ensure(Source.Implementation == Target.Implementation);
			ensure(Source.InputHash == Target.InputHash);
			ensure(Source.OutputCodeIndex == Target.OutputCodeIndex);

			Target.Expression = Source.Expression;
		}

		// Clear cache as we are in a local scope
		FMaterialFunctionCompileState* CurrentFunctionState = Translator.FunctionStacks[Translator.ShaderFrequency].Last();
		CurrentFunctionState->ExpressionCodeMap.Reset();
		CurrentFunctionState->ClearSharedFunctionStates();
	}
};

FVoxelMaterialTranslatorNoCodeReuseScope::FVoxelMaterialTranslatorNoCodeReuseScope(FHLSLMaterialTranslator& Translator)
	: Impl(MakePimpl<FImpl>(static_cast<FVoxelHLSLMaterialTranslator&>(Translator)))
{
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
int32 FVoxelUtilities::ZeroDerivative(
	FMaterialCompiler& Compiler,
	const int32 Index)
{
	if (Index == -1)
	{
		return -1;
	}

	(*static_cast<FVoxelHLSLMaterialTranslator&>(Compiler).CurrentScopeChunks)[Index].DerivativeStatus = EDerivativeStatus::Zero;

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

	// Deterministic order
	Expressions.Sort([](const UMaterialExpression& A, const UMaterialExpression& B)
	{
		if (A.MaterialExpressionEditorX != B.MaterialExpressionEditorX)
		{
			return A.MaterialExpressionEditorX < B.MaterialExpressionEditorX;
		}
		if (A.MaterialExpressionEditorY != B.MaterialExpressionEditorY)
		{
			return A.MaterialExpressionEditorY < B.MaterialExpressionEditorY;
		}

		return A.GetName() < B.GetName();
	});

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

	// Deterministic order
	Expressions.Sort([](const UMaterialExpression& A, const UMaterialExpression& B)
	{
		if (A.MaterialExpressionEditorX != B.MaterialExpressionEditorX)
		{
			return A.MaterialExpressionEditorX < B.MaterialExpressionEditorX;
		}
		if (A.MaterialExpressionEditorY != B.MaterialExpressionEditorY)
		{
			return A.MaterialExpressionEditorY < B.MaterialExpressionEditorY;
		}

		return A.GetName() < B.GetName();
	});

	return Expressions;
}
#endif