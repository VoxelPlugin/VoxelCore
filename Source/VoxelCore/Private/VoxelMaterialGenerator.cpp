// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMaterialGenerator.h"

#if WITH_EDITOR
#include "Engine/TextureCollection.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionVertexInterpolator.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#endif

#if WITH_EDITOR
TVoxelOptional<FMaterialAttributesInput> FVoxelMaterialGenerator::CopyExpressions()
{
	VOXEL_FUNCTION_COUNTER();

	for (const UMaterialExpression* Expression : FVoxelUtilities::GetMaterialExpressions(OldMaterial))
	{
		if (!Expression)
		{
			continue;
		}

		if (bSkipCustomOutputs &&
			Expression->IsA<UMaterialExpressionCustomOutput>() &&
			!Expression->IsA<UMaterialExpressionVertexInterpolator>())
		{
			continue;
		}

		UMaterialExpression* NewExpression = CloneExpression(*Expression, NewMaterial);

		if (!ensure(NewExpression) ||
			!ensure(!NewExpression->SubgraphExpression) ||
			!ensure(!NewExpression->Function))
		{
			return {};
		}

		NewExpression->Material = &NewMaterial;

		if (!ensureVoxelSlow(PostCopyExpression(*NewExpression)))
		{
			return {};
		}

		NewExpression->MaterialExpressionEditorX = Expression->MaterialExpressionEditorX;
		NewExpression->MaterialExpressionEditorY = Expression->MaterialExpressionEditorY;

		OldToNewExpression.Add_EnsureNew(Expression, NewExpression);
	}

	{
		VOXEL_SCOPE_COUNTER("Fix links");

		bool bFailed = false;
		for (const auto& It : OldToNewExpression)
		{
			ForeachObjectReference(*It.Value, [&](UObject*& Object)
			{
				const UMaterialExpression* Expression = Cast<UMaterialExpression>(Object);
				if (!Expression)
				{
					return;
				}

				if (!ensure(OldToNewExpression.Contains(Expression)))
				{
					LOG_VOXEL(Error, "Unknown expression: %s", *MakeVoxelObjectPtr(Expression).GetPathName());
					bFailed = true;
					return;
				}

				Object = OldToNewExpression[Expression];
			});
		}

		if (bFailed)
		{
			return {};
		}
	}

	if (OldMaterial.bUseMaterialAttributes)
	{
		FMaterialAttributesInput Input = OldMaterial.GetEditorOnlyData()->MaterialAttributes;
		if (Input.Expression)
		{
			Input.Expression = OldToNewExpression.FindRef(Input.Expression);

			if (!ensure(Input.Expression))
			{
				return {};
			}
		}
		return Input;
	}

	UMaterialExpressionMakeMaterialAttributes& Attributes = FVoxelUtilities::CreateMaterialExpression<UMaterialExpressionMakeMaterialAttributes>(NewMaterial);
	Attributes.MaterialExpressionEditorX = OldMaterial.EditorX;
	Attributes.MaterialExpressionEditorY = OldMaterial.EditorY;

	OldToNewExpression.Add_EnsureNew(&Attributes, &Attributes);

	bool bFailed = false;

	const auto Traverse = [&]<typename T>(FMaterialInput<T> Input) -> FExpressionInput
	{
		if (Input.Expression)
		{
			Input.Expression = OldToNewExpression.FindRef(Input.Expression);

			if (!ensure(Input.Expression))
			{
				bFailed = true;
			}
		}
		return Input;
	};

	// Need to add new properties below
	checkStatic(MP_MAX == 35);

	Attributes.BaseColor = Traverse(OldMaterial.GetEditorOnlyData()->BaseColor);
	Attributes.Metallic = Traverse(OldMaterial.GetEditorOnlyData()->Metallic);
	Attributes.Specular = Traverse(OldMaterial.GetEditorOnlyData()->Specular);
	Attributes.Roughness = Traverse(OldMaterial.GetEditorOnlyData()->Roughness);
	Attributes.Anisotropy = Traverse(OldMaterial.GetEditorOnlyData()->Anisotropy);
	Attributes.EmissiveColor = Traverse(OldMaterial.GetEditorOnlyData()->EmissiveColor);
	Attributes.Opacity = Traverse(OldMaterial.GetEditorOnlyData()->Opacity);
	Attributes.OpacityMask = Traverse(OldMaterial.GetEditorOnlyData()->OpacityMask);
	Attributes.Normal = Traverse(OldMaterial.GetEditorOnlyData()->Normal);
	Attributes.Tangent = Traverse(OldMaterial.GetEditorOnlyData()->Tangent);
	Attributes.WorldPositionOffset = Traverse(OldMaterial.GetEditorOnlyData()->WorldPositionOffset);
	Attributes.SubsurfaceColor = Traverse(OldMaterial.GetEditorOnlyData()->SubsurfaceColor);
	Attributes.ClearCoat = Traverse(OldMaterial.GetEditorOnlyData()->ClearCoat);
	Attributes.ClearCoatRoughness = Traverse(OldMaterial.GetEditorOnlyData()->ClearCoatRoughness);
	Attributes.AmbientOcclusion = Traverse(OldMaterial.GetEditorOnlyData()->AmbientOcclusion);
	Attributes.Refraction = Traverse(OldMaterial.GetEditorOnlyData()->Refraction);
	Attributes.PixelDepthOffset = Traverse(OldMaterial.GetEditorOnlyData()->PixelDepthOffset);
	Attributes.Displacement = Traverse(OldMaterial.GetEditorOnlyData()->Displacement);

	for (int32 Index = 0; Index < 8; Index++)
	{
		Attributes.CustomizedUVs[Index] = Traverse(OldMaterial.GetEditorOnlyData()->CustomizedUVs[Index]);
	}

	if (bFailed)
	{
		return {};
	}

	FMaterialAttributesInput Input;
	Input.Expression = &Attributes;
	return Input;
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
FVoxelOptionalIntBox2D FVoxelMaterialGenerator::GetBounds() const
{
	FVoxelOptionalIntBox2D PositionBounds;
	for (const auto& It : OldToNewExpression)
	{
		PositionBounds += FIntPoint(
			It.Value->MaterialExpressionEditorX,
			It.Value->MaterialExpressionEditorY);
	}
	return PositionBounds;
}

void FVoxelMaterialGenerator::MoveExpressions(const FIntPoint& Offset) const
{
	for (const auto& It : OldToNewExpression)
	{
		It.Value->MaterialExpressionEditorX += Offset.X;
		It.Value->MaterialExpressionEditorY += Offset.Y;
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
bool FVoxelMaterialGenerator::ShouldDuplicateFunction(const UMaterialFunction& Function)
{
	static TVoxelSet<const UMaterialFunction*> VisitedFunctions;

	VOXEL_SCOPE_COUNTER_FORMAT_COND(VisitedFunctions.Num() == 0, "ShouldDuplicateFunction %s",
		Function.UserExposedCaption.IsEmpty() ? *Function.GetName() : *Function.UserExposedCaption);

	if (VisitedFunctions.Contains(&Function))
	{
		VOXEL_MESSAGE(Error, "{0}: Recursive function calls: {1}, {2}",
			ErrorOwner,
			VisitedFunctions.Array(),
			Function);

		return false;
	}

	VisitedFunctions.Add_CheckNew(&Function);
	ON_SCOPE_EXIT
	{
		VisitedFunctions.RemoveChecked(&Function);
	};

	if (const bool* Value = FunctionToShouldDuplicate.Find(&Function))
	{
		return *Value;
	}

	const bool bValue = INLINE_LAMBDA
	{
		for (UMaterialExpression* FunctionExpression : Function.GetExpressions())
		{
			if (!FunctionExpression)
			{
				continue;
			}

			if (!ParameterNamePrefix.IsEmpty() &&
				FunctionExpression->HasAParameterName())
			{
				return true;
			}

			if (const UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(FunctionExpression))
			{
				if (!FunctionCall->MaterialFunction)
				{
					continue;
				}

				const UMaterialFunction* CalledFunction = Cast<UMaterialFunction>(FunctionCall->MaterialFunction);
				if (!ensureVoxelSlow(CalledFunction))
				{
					VOXEL_MESSAGE(Error, "{0}: {1}: material function instance or material function layers are not supported",
						ErrorOwner,
						FunctionCall->MaterialFunction);

					continue;
				}

				if (ShouldDuplicateFunction(*CalledFunction))
				{
					return true;
				}
			}
		}

		return false;
	};

	FunctionToShouldDuplicate.Add_EnsureNew(&Function, bValue);
	return bValue;
}

bool FVoxelMaterialGenerator::PostCopyExpression(UMaterialExpression& Expression)
{
	VOXEL_FUNCTION_COUNTER();

	if (UMaterialExpressionNamedRerouteDeclaration* Declaration = Cast<UMaterialExpressionNamedRerouteDeclaration>(&Expression))
	{
		FGuid& NewGuid = OldToNewNamedRerouteGuid.FindOrAdd(Declaration->VariableGuid);
		if (!NewGuid.IsValid())
		{
			NewGuid = FGuid::NewGuid();
		}
		Declaration->VariableGuid = NewGuid;

		if (!ParameterNamePrefix.IsEmpty())
		{
			Declaration->Name = FName(ParameterNamePrefix + Declaration->Name.ToString());
		}
	}

	if (UMaterialExpressionNamedRerouteUsage* Usage = Cast<UMaterialExpressionNamedRerouteUsage>(&Expression))
	{
		FGuid& NewGuid = OldToNewNamedRerouteGuid.FindOrAdd(Usage->DeclarationGuid);
		if (!NewGuid.IsValid())
		{
			NewGuid = FGuid::NewGuid();
		}
		Usage->DeclarationGuid = NewGuid;
	}

	if (Expression.HasAParameterName() &&
		!Expression.IsA<UMaterialExpressionCollectionParameter>())
	{
		FGuid& Guid = Expression.GetParameterExpressionId();

		FGuid& NewGuid = OldToNewParameterGuid.FindOrAdd(Guid);
		if (!NewGuid.IsValid())
		{
			NewGuid = FGuid::NewGuid();
		}
		Guid = NewGuid;

		if (!ParameterNamePrefix.IsEmpty())
		{
			Expression.SetParameterName(FName(ParameterNamePrefix + Expression.GetParameterName().ToString()));
		}
	}

	UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(&Expression);
	if (!FunctionCall ||
		!FunctionCall->MaterialFunction)
	{
		return true;
	}

	const UMaterialFunction* OldFunction = Cast<UMaterialFunction>(FunctionCall->MaterialFunction);
	if (!ensureVoxelSlow(OldFunction))
	{
		VOXEL_MESSAGE(Error, "{0}: {1}: material function instance or material function layers are not supported",
			ErrorOwner,
			FunctionCall->MaterialFunction);

		return true;
	}

	if (!ShouldDuplicateFunction(*OldFunction))
	{
		return true;
	}

	if (UMaterialFunction* NewFunction = OldToNewFunction.FindRef(OldFunction))
	{
		FunctionCall->MaterialFunction = NewFunction;
		return true;
	}

	UMaterialFunction* NewFunction = NewObject<UMaterialFunction>(&NewMaterial);
	NewFunction->UserExposedCaption =
		"GENERATED: " +
		(OldFunction->UserExposedCaption.IsEmpty() ? OldFunction->GetName() : OldFunction->UserExposedCaption);

	OldToNewFunction.Add_EnsureNew(OldFunction, NewFunction);

	if (!ensureVoxelSlow(CopyFunctionExpressions(*OldFunction, *NewFunction)))
	{
		return false;
	}

	NewFunction->UpdateDependentFunctionCandidates();

	FunctionCall->MaterialFunction = NewFunction;

	return true;
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
bool FVoxelMaterialGenerator::CopyFunctionExpressions(
	const UMaterialFunction& OldFunction,
	UMaterialFunction& NewFunction)
{
	VOXEL_SCOPE_COUNTER_FORMAT("CopyFunctionExpressions %s", OldFunction.UserExposedCaption.IsEmpty() ? *OldFunction.GetName() : *OldFunction.UserExposedCaption);

	TVoxelMap<const UMaterialExpression*, UMaterialExpression*> OldToNewFunctionExpression;

	for (const UMaterialExpression* Expression : FVoxelUtilities::GetMaterialExpressions(OldFunction))
	{
		if (!Expression)
		{
			continue;
		}

		if (bSkipCustomOutputs &&
			Expression->IsA<UMaterialExpressionCustomOutput>() &&
			!Expression->IsA<UMaterialExpressionVertexInterpolator>())
		{
			continue;
		}

		UMaterialExpression* NewExpression = CloneExpression(*Expression, NewFunction);

		if (!ensure(NewExpression) ||
			!ensure(!NewExpression->SubgraphExpression) ||
			!ensure(!NewExpression->Material))
		{
			return false;
		}

		NewExpression->Function = &NewFunction;

		if (!ensureVoxelSlow(PostCopyExpression(*NewExpression)))
		{
			return false;
		}

		// Ensure function inputs & outputs are deterministically sorted
		// Otherwise it's based on the expression collection order, which can vary
		if (UMaterialExpressionFunctionInput* Input = Cast<UMaterialExpressionFunctionInput>(NewExpression))
		{
			Input->SortPriority = Input->SortPriority * 1000000 + (FVoxelUtilities::MurmurHash(Input->MaterialExpressionGuid) % 1000000);
		}
		if (UMaterialExpressionFunctionOutput* Output = Cast<UMaterialExpressionFunctionOutput>(NewExpression))
		{
			Output->SortPriority = Output->SortPriority * 1000000 + (FVoxelUtilities::MurmurHash(Output->MaterialExpressionGuid) % 1000000);
		}

		NewExpression->MaterialExpressionEditorX = Expression->MaterialExpressionEditorX;
		NewExpression->MaterialExpressionEditorY = Expression->MaterialExpressionEditorY;

		OldToNewFunctionExpression.Add_EnsureNew(Expression, NewExpression);
	}

	bool bFailed = false;
	for (const auto& It : OldToNewFunctionExpression)
	{
		ForeachObjectReference(*It.Value, [&](UObject*& Object)
		{
			const UMaterialExpression* Expression = Cast<UMaterialExpression>(Object);
			if (!Expression)
			{
				return;
			}

			if (!ensure(OldToNewFunctionExpression.Contains(Expression)))
			{
				LOG_VOXEL(Error, "Unknown expression: %s", *MakeVoxelObjectPtr(Expression).GetPathName());
				bFailed = true;
				return;
			}

			Object = OldToNewFunctionExpression[Expression];
		});
	}

	return !bFailed;
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
UMaterialExpression* FVoxelMaterialGenerator::CloneExpression(
	const UMaterialExpression& Expression,
	UObject& Outer)
{
	VOXEL_FUNCTION_COUNTER();

	UMaterialExpression& NewExpression = FVoxelUtilities::CreateMaterialExpression(Outer, Expression.GetClass());

	for (const FProperty& Property : GetClassProperties(Expression.GetClass()))
	{
		if (Property.HasAnyPropertyFlags(CPF_Transient))
		{
			continue;
		}

		Property.CopyCompleteValue_InContainer(&NewExpression, &Expression);
	}

	return &NewExpression;
}
#endif