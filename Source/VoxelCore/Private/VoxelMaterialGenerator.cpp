// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMaterialGenerator.h"

#if WITH_EDITOR
#include "Engine/TextureCollection.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
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
void FVoxelMaterialGenerator::ForeachExpression(TFunctionRef<void(UMaterialExpression&)> Lambda)
{
	VOXEL_FUNCTION_COUNTER();

	for (const auto& It : OldToNewExpression)
	{
		Lambda(*It.Value);
	}
}

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
		if (!Input.Expression)
		{
			// Nothing plugged into it
			return FMaterialAttributesInput();
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

	const auto Traverse = [&]<typename T>(const EMaterialProperty Property, FMaterialInput<T> Input) -> FExpressionInput
	{
		// Make sure to not connect inputs if they are the default value, otherwise they'll be incorrectly flagged as connected
		// This is critical, among others, for anisotropy to not be enabled
		const FVector4f DefaultValue = FMaterialAttributeDefinitionMap::GetDefaultValue(Property);

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
			if (Input.Constant == DefaultValue.X)
			{
				return {};
			}

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
			if (Input.Constant.X == DefaultValue.X &&
				Input.Constant.Y == DefaultValue.Y)
			{
				return {};
			}

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
			if (Input.Constant.X == DefaultValue.X &&
				Input.Constant.Y == DefaultValue.Y &&
				Input.Constant.Z == DefaultValue.Z)
			{
				return {};
			}

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
			const FLinearColor Constant = FLinearColor(Input.Constant);

			if (Constant.R == DefaultValue.X &&
				Constant.G == DefaultValue.Y &&
				Constant.B == DefaultValue.Z &&
				Constant.A == DefaultValue.W)
			{
				return {};
			}

			UMaterialExpressionConstant4Vector& Expression = NewExpression<UMaterialExpressionConstant4Vector>();
			Expression.Constant = Constant;
			Expression.MaterialExpressionEditorX = OldMaterial.EditorX - 250;
			Expression.MaterialExpressionEditorY = OldMaterial.EditorY + AttributeIndex * 50;

			FExpressionInput Result;
			Result.Expression = &Expression;
			return Result;
		}
		else
		{
			checkStatic(std::is_void_v<T>);
			return {};
		}
	};

	// Need to add new properties below
	checkStatic(MP_MAX == 35);

	Attributes.BaseColor = Traverse(MP_BaseColor, OldMaterial.GetEditorOnlyData()->BaseColor);
	Attributes.Metallic = Traverse(MP_Metallic, OldMaterial.GetEditorOnlyData()->Metallic);
	Attributes.Specular = Traverse(MP_Specular, OldMaterial.GetEditorOnlyData()->Specular);
	Attributes.Roughness = Traverse(MP_Roughness, OldMaterial.GetEditorOnlyData()->Roughness);
	Attributes.Anisotropy = Traverse(MP_Anisotropy, OldMaterial.GetEditorOnlyData()->Anisotropy);
	Attributes.EmissiveColor = Traverse(MP_EmissiveColor, OldMaterial.GetEditorOnlyData()->EmissiveColor);
	Attributes.Opacity = Traverse(MP_Opacity, OldMaterial.GetEditorOnlyData()->Opacity);
	Attributes.OpacityMask = Traverse(MP_OpacityMask, OldMaterial.GetEditorOnlyData()->OpacityMask);
	Attributes.Normal = Traverse(MP_Normal, OldMaterial.GetEditorOnlyData()->Normal);
	Attributes.Tangent = Traverse(MP_Tangent, OldMaterial.GetEditorOnlyData()->Tangent);
	Attributes.WorldPositionOffset = Traverse(MP_WorldPositionOffset, OldMaterial.GetEditorOnlyData()->WorldPositionOffset);
	Attributes.SubsurfaceColor = Traverse(MP_SubsurfaceColor, OldMaterial.GetEditorOnlyData()->SubsurfaceColor);
	Attributes.ClearCoat = Traverse(MP_CustomData0, OldMaterial.GetEditorOnlyData()->ClearCoat);
	Attributes.ClearCoatRoughness = Traverse(MP_CustomData1, OldMaterial.GetEditorOnlyData()->ClearCoatRoughness);
	Attributes.AmbientOcclusion = Traverse(MP_AmbientOcclusion, OldMaterial.GetEditorOnlyData()->AmbientOcclusion);
	Attributes.Refraction = Traverse(MP_Refraction, OldMaterial.GetEditorOnlyData()->Refraction);
	Attributes.PixelDepthOffset = Traverse(MP_PixelDepthOffset, OldMaterial.GetEditorOnlyData()->PixelDepthOffset);
	Attributes.Displacement = Traverse(MP_Displacement, OldMaterial.GetEditorOnlyData()->Displacement);

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
		Attributes.CustomizedUVs[Index] = Traverse(EMaterialProperty(MP_CustomizedUVs0 + Index), OldMaterial.GetEditorOnlyData()->CustomizedUVs[Index]);
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

	UMaterialFunction* OldFunction = Cast<UMaterialFunction>(FunctionCall->MaterialFunction);
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