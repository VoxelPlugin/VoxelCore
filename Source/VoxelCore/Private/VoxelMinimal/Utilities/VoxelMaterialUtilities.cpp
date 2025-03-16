// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelMaterialDiffing.h"
#include "VoxelHLSLMaterialTranslator.h"
#if WITH_EDITOR
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
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

void FVoxelUtilities::ClearMaterialExpressions(UMaterial& Material)
{
	VOXEL_FUNCTION_COUNTER();

	Material.GetExpressionCollection().Empty();

	// Ensure future GetMaterialExpressions don't return the old expressions
	for (UMaterialExpression* Expression : GetMaterialExpressions(Material))
	{
		Expression->MarkAsGarbage();
	}
}

DEFINE_PRIVATE_ACCESS(UMaterialInstance, StaticParametersRuntime);

bool FVoxelUtilities::CopyParameterValues(
	FMaterialInstanceParameterUpdateContext& UpdateContext,
	UMaterialInstance& Target,
	const UMaterialInterface& Source,
	const FString& ParameterNamePrefix)
{
	VOXEL_FUNCTION_COUNTER();

	bool bChanged = false;
	for (int32 TypeIndex = 0; TypeIndex < int32(EMaterialParameterType::Num); TypeIndex++)
	{
		const EMaterialParameterType Type = EMaterialParameterType(TypeIndex);

		TArray<FMaterialParameterInfo> ParameterInfos;
		TArray<FGuid> ParameterIds;
		Source.GetAllParameterInfoOfType(Type, ParameterInfos, ParameterIds);

		for (const FMaterialParameterInfo& ParameterInfo : ParameterInfos)
		{
			FMaterialParameterMetadata Value;
			if (!ensureVoxelSlow(Source.GetParameterValue(
				Type,
				ParameterInfo,
				Value)))
			{
				continue;
			}

			const FName NewName = FName(ParameterNamePrefix + ParameterInfo.Name.ToString());

			FMaterialParameterMetadata CurrentValue;
			if (Target.GetParameterValue(Type, NewName, CurrentValue) &&
				CurrentValue.Value == Value.Value)
			{
				continue;
			}

			UpdateContext.SetParameterValueEditorOnly(NewName, Value, EMaterialSetParameterValueFlags::SetCurveAtlas);
			bChanged = true;
		}
	}
	return bChanged;
}

void FVoxelUtilities::MergeIdenticalTextureParameters(
	UMaterial& Material,
	const UMaterialInstance& MaterialInstance)
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelSet<FName> ParameterNamesToNeverRename;
	{
		const TFunction<void(TConstVoxelArrayView<UMaterialExpression*>)> TraverseExpressions = [&](const TConstVoxelArrayView<UMaterialExpression*> Expressions)
		{
			for (UMaterialExpression* Expression : Expressions)
			{
				if (const UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
				{
					const UMaterialFunction* Function = Cast<UMaterialFunction>(FunctionCall->MaterialFunction);
					if (!Function)
					{
						continue;
					}

					TraverseExpressions(GetMaterialExpressions(*Function));
					continue;
				}

				if (Expression->HasAParameterName() &&
					Expression->GetTypedOuter<UMaterial>() != &Material)
				{
					// We can't rename this expression as we don't own the function
					ensureVoxelSlow(false);
					ParameterNamesToNeverRename.Add(Expression->GetParameterName());
				}
			}
		};

		TraverseExpressions(GetMaterialExpressions(Material));
	}

	TArray<FMaterialParameterInfo> ParameterInfos;
	TArray<FGuid> ParameterIds;
	MaterialInstance.GetAllParameterInfoOfType(EMaterialParameterType::Texture, ParameterInfos, ParameterIds);

	TVoxelMap<FName, FName> OldToNewParameterName;
	TVoxelMap<UObject*, FName> TextureToParameterName;
	for (const FMaterialParameterInfo& ParameterInfo : ParameterInfos)
	{
		FMaterialParameterMetadata Value;
		if (!ensureVoxelSlow(MaterialInstance.GetParameterValue(
			EMaterialParameterType::Texture,
			ParameterInfo,
			Value)))
		{
			continue;
		}

		UObject* Texture = Value.Value.AsTextureObject();

		if (const FName* ExistingName = TextureToParameterName.Find(Texture))
		{
			if (!ParameterNamesToNeverRename.Contains(ParameterInfo.Name))
			{
				OldToNewParameterName.Add_EnsureNew(ParameterInfo.Name, *ExistingName);
			}
			continue;
		}

		TextureToParameterName.Add_EnsureNew(Texture, ParameterInfo.Name);
	}

	const TFunction<void(TConstVoxelArrayView<UMaterialExpression*>)> ProcessExpressions = [&](const TConstVoxelArrayView<UMaterialExpression*> Expressions)
	{
		for (UMaterialExpression* Expression : Expressions)
		{
			if (const UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
			{
				const UMaterialFunction* Function = Cast<UMaterialFunction>(FunctionCall->MaterialFunction);
				if (!Function ||
					// Cannot edit function
					Function->GetOuter() != &Material)
				{
					continue;
				}

				ProcessExpressions(GetMaterialExpressions(*Function));
				continue;
			}

			if (!Expression->HasAParameterName())
			{
				continue;
			}

			const FName* NewParameterName = OldToNewParameterName.Find(Expression->GetParameterName());
			if (!NewParameterName)
			{
				continue;
			}

			if (!ensureVoxelSlow(Expression->GetTypedOuter<UMaterial>() == &Material))
			{
				// Cannot edit function
				continue;
			}

			Expression->SetParameterName(*NewParameterName);
		}
	};

	ProcessExpressions(GetMaterialExpressions(Material));
}

bool FVoxelUtilities::AreMaterialsIdentical(
	const UMaterial& OldMaterial,
	const UMaterial& NewMaterial,
	FString& OutDiff)
{
	VOXEL_FUNCTION_COUNTER();

	FVoxelMaterialDiffing Diffing;

	const bool bResult = Diffing.Equal(OldMaterial, NewMaterial);
	OutDiff = Diffing.Diff;
	return bResult;
}

bool FVoxelUtilities::AreInstancesIdentical(
	const UMaterialInstanceConstant& OldInstance,
	const UMaterialInstanceConstant& NewInstance,
	FString& OutDiff)
{
	VOXEL_FUNCTION_COUNTER();

	for (int32 TypeIndex = 0; TypeIndex < int32(EMaterialParameterType::Num); TypeIndex++)
	{
		const EMaterialParameterType Type = EMaterialParameterType(TypeIndex);

		TArray<FMaterialParameterInfo> OldParameterInfos;
		TArray<FGuid> OldParameterIds;
		OldInstance.GetAllParameterInfoOfType(Type, OldParameterInfos, OldParameterIds);

		TArray<FMaterialParameterInfo> NewParameterInfos;
		TArray<FGuid> NewParameterIds;
		NewInstance.GetAllParameterInfoOfType(Type, NewParameterInfos, NewParameterIds);

		if (OldParameterInfos.Num() < NewParameterInfos.Num())
		{
			OutDiff = "Parameters added";
			return false;
		}
		if (OldParameterInfos.Num() > NewParameterInfos.Num())
		{
			OutDiff = "Parameters removed";
			return false;
		}
		check(OldParameterInfos.Num() == NewParameterInfos.Num());

		for (int32 Index = 0; Index < OldParameterInfos.Num(); Index++)
		{
			const FMaterialParameterInfo& OldParameterInfo = OldParameterInfos[Index];
			const FMaterialParameterInfo& NewParameterInfo = NewParameterInfos[Index];

			if (OldParameterInfo != NewParameterInfo)
			{
				OutDiff = "Parameter changed: " + OldParameterInfo.Name.ToString() + " vs " + NewParameterInfo.Name.ToString();
				return false;
			}

			FMaterialParameterMetadata OldValue;
			ensureVoxelSlow(OldInstance.GetParameterValue(
				Type,
				OldParameterInfo,
				OldValue));

			FMaterialParameterMetadata NewValue;
			ensureVoxelSlow(NewInstance.GetParameterValue(
				Type,
				NewParameterInfo,
				NewValue));

			if (OldValue.Value != NewValue.Value)
			{
				OutDiff = "Parameter value changed: " + NewParameterInfo.Name.ToString();
				return false;
			}
		}
	}

	return true;
}
#endif