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
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionVertexInterpolator.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#endif

#if WITH_EDITOR
UMaterialFunction* FVoxelMaterialGenerator::DuplicateFunctionIfNeeded(const UMaterialFunction& OldFunction)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ShouldDuplicateFunction(OldFunction))
	{
		return ConstCast(&OldFunction);
	}

	if (UMaterialFunction* NewFunction = OldToNewFunction.FindRef(&OldFunction))
	{
		return NewFunction;
	}

	UMaterialFunction* NewFunction = NewObject<UMaterialFunction>(&NewMaterial);
	NewFunction->UserExposedCaption =
		"GENERATED: " +
		(OldFunction.UserExposedCaption.IsEmpty() ? OldFunction.GetName() : OldFunction.UserExposedCaption);

	OldToNewFunction.Add_EnsureNew(&OldFunction, NewFunction);

	if (!ensureVoxelSlow(CopyFunctionExpressions(OldFunction, *NewFunction)))
	{
		return nullptr;
	}

	NewFunction->UpdateDependentFunctionCandidates();
	return NewFunction;
}

TVoxelOptional<FMaterialAttributesInput> FVoxelMaterialGenerator::CopyExpressions(const UMaterial& OldMaterial)
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
		if (!ensure(Input.Expression))
		{
			return {};
		}

		UMaterialExpression* Expression = OldToNewExpression.FindRef(Input.Expression);
		if (!ensure(Expression))
		{
			return {};
		}

		Input.Expression = Expression;
		return Input;
	}

	UMaterialExpressionMakeMaterialAttributes& Attributes = NewExpression<UMaterialExpressionMakeMaterialAttributes>();
	Attributes.MaterialExpressionEditorX = OldMaterial.EditorX;
	Attributes.MaterialExpressionEditorY = OldMaterial.EditorY;

	bool bFailed = false;
	int32 AttributeIndex = 0;

	const auto Traverse = [&]<typename T>(FMaterialInput<T> Input) -> FExpressionInput
	{
		if (Input.Expression)
		{
			Input.Expression = OldToNewExpression.FindRef(Input.Expression);

			if (!ensure(Input.Expression))
			{
				bFailed = true;
			}

			return Input;
		}

		AttributeIndex++;

		if constexpr (std::is_same_v<T, float>)
		{
			UMaterialExpressionConstant& Expression = NewExpression<UMaterialExpressionConstant>();
			Expression.R = Input.Constant;
			Expression.MaterialExpressionEditorX = OldMaterial.EditorX - 250;
			Expression.MaterialExpressionEditorY = OldMaterial.EditorY + AttributeIndex * 50;

			FExpressionInput Result;
			Result.Expression = &Expression;
			return Result;
		}
		else if constexpr (std::is_same_v<T, FVector2f>)
		{
			UMaterialExpressionConstant2Vector& Expression = NewExpression<UMaterialExpressionConstant2Vector>();
			Expression.R = Input.Constant.X;
			Expression.G = Input.Constant.Y;
			Expression.MaterialExpressionEditorX = OldMaterial.EditorX - 250;
			Expression.MaterialExpressionEditorY = OldMaterial.EditorY + AttributeIndex * 50;

			FExpressionInput Result;
			Result.Expression = &Expression;
			return Result;
		}
		else if constexpr (std::is_same_v<T, FVector3f>)
		{
			UMaterialExpressionConstant3Vector& Expression = NewExpression<UMaterialExpressionConstant3Vector>();
			Expression.Constant = FLinearColor(Input.Constant);
			Expression.MaterialExpressionEditorX = OldMaterial.EditorX - 250;
			Expression.MaterialExpressionEditorY = OldMaterial.EditorY + AttributeIndex * 50;

			FExpressionInput Result;
			Result.Expression = &Expression;
			return Result;
		}
		else if constexpr (
			std::is_same_v<T, FColor> ||
			std::is_same_v<T, FLinearColor>)
		{
			UMaterialExpressionConstant4Vector& Expression = NewExpression<UMaterialExpressionConstant4Vector>();
			Expression.Constant = FLinearColor(Input.Constant);
			Expression.MaterialExpressionEditorX = OldMaterial.EditorX - 250;
			Expression.MaterialExpressionEditorY = OldMaterial.EditorY + AttributeIndex * 50;

			FExpressionInput Result;
			Result.Expression = &Expression;
			return Result;
		}
		else
		{
			checkStatic(std::is_void_v<T>);
			return FExpressionInput();
		}
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

	if (UMaterialExpression* Expression = OldMaterial.GetEditorOnlyData()->ShadingModelFromMaterialExpression.Expression)
	{
		Expression = OldToNewExpression.FindRef(Expression);

		if (!ensure(Expression))
		{
			return {};
		}

		Attributes.ShadingModel.Expression = Expression;
	}

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
		VisitedFunctions.Remove_Ensure(&Function);
	};

	if (const bool* Value = FunctionToShouldDuplicate.Find(&Function))
	{
		return *Value;
	}

	const bool bValue = INLINE_LAMBDA
	{
		for (UMaterialExpression* Expression : Function.GetExpressions())
		{
			if (!Expression)
			{
				continue;
			}

			if (!ParameterNamePrefix.IsEmpty() &&
				Expression->HasAParameterName())
			{
				return true;
			}

			if (ShouldDuplicateFunction_AdditionalHook &&
				ShouldDuplicateFunction_AdditionalHook(*Expression))
			{
				return true;
			}

			if (const UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
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

	UMaterialFunction* NewFunction = DuplicateFunctionIfNeeded(*OldFunction);
	if (!ensureVoxelSlow(NewFunction))
	{
		return false;
	}

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