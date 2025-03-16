// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMaterialDiffing.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"

#define RECURSE_GUARD() \
	FGuard Guard(*this); \
	if (Depth > 256) \
	{ \
		ensure(false); \
		VOXEL_MESSAGE(Error, "Infinite recursion when diffing materials"); \
		return false; \
	}

#define SET_DIFF(Format, ...) Diff += FString::Printf(TEXT(Format), ##__VA_ARGS__)

#if WITH_EDITOR
bool FVoxelMaterialDiffing::Equal(
	const UMaterial& OldMaterial,
	const UMaterial& NewMaterial)
{
	VOXEL_FUNCTION_COUNTER();
	RECURSE_GUARD();

	if (OldMaterial.DisplacementScaling != NewMaterial.DisplacementScaling)
	{
		SET_DIFF("DisplacementScaling %f %f -> %f %f",
			OldMaterial.DisplacementScaling.Center,
			OldMaterial.DisplacementScaling.Magnitude,
			NewMaterial.DisplacementScaling.Center,
			NewMaterial.DisplacementScaling.Magnitude);

		return false;
	}

	const TVoxelArray<UMaterialExpression*> OldExpressions = FVoxelUtilities::GetMaterialExpressions(OldMaterial);
	const TVoxelArray<UMaterialExpression*> NewExpressions = FVoxelUtilities::GetMaterialExpressions(NewMaterial);

	if (OldExpressions.Num() != NewExpressions.Num())
	{
		if (OldExpressions.Num() < NewExpressions.Num())
		{
			SET_DIFF("%d expressions added", NewExpressions.Num() - OldExpressions.Num());
		}
		else
		{
			SET_DIFF("%d expressions removed", OldExpressions.Num() - NewExpressions.Num());
		}

		return false;
	}

	for (int32 Index = 0; Index < OldExpressions.Num(); Index++)
	{
		if (!Equal(
			OldExpressions[Index],
			NewExpressions[Index]))
		{
			return false;
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelMaterialDiffing::Equal(
	const FProperty& Property,
	const void* OldValue,
	const void* NewValue)
{
	RECURSE_GUARD();

	if (Property.HasAnyPropertyFlags(CPF_Transient))
	{
		// Skip PropertyConnectedMask
		return true;
	}

	if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		FScriptArrayHelper OldArray(ArrayProperty, OldValue);
		FScriptArrayHelper NewArray(ArrayProperty, NewValue);

		if (OldArray.Num() != NewArray.Num())
		{
			SET_DIFF("%s changed", *Property.GetPathName());
			return false;
		}

		for (int32 Index = 0; Index < OldArray.Num(); Index++)
		{
			if (!Equal(
				*ArrayProperty->Inner,
				OldArray.GetRawPtr(Index),
				NewArray.GetRawPtr(Index)))
			{
				return false;
			}
		}

		return true;
	}

	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		if (StructProperty->Struct->GetCppStructOps() &&
			StructProperty->Struct->GetCppStructOps()->HasIdentical())
		{
			if (!Property.Identical(OldValue, NewValue))
			{
				SET_DIFF("%s changed", *Property.GetPathName());
				return false;
			}

			return true;
		}

		for (FProperty& ChildProperty : GetStructProperties(StructProperty->Struct))
		{
			if (!Equal(
				ChildProperty,
				ChildProperty.ContainerPtrToValuePtr<void>(OldValue),
				ChildProperty.ContainerPtrToValuePtr<void>(NewValue)))
			{
				return false;
			}
		}

		return true;
	}

	if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
	{
		if (ObjectProperty->PropertyClass->IsChildOf<UMaterialExpression>())
		{
			const UMaterialExpression* OldExpression = CastEnsured<UMaterialExpression>(ObjectProperty->GetPropertyValue(OldValue));
			const UMaterialExpression* NewExpression = CastEnsured<UMaterialExpression>(ObjectProperty->GetPropertyValue(NewValue));

			return Equal(OldExpression, NewExpression);
		}

		if (ObjectProperty->PropertyClass->IsChildOf<UMaterialFunctionInterface>())
		{
			const UMaterialFunction* OldFunction = CastEnsured<UMaterialFunction>(ObjectProperty->GetPropertyValue(OldValue));
			const UMaterialFunction* NewFunction = CastEnsured<UMaterialFunction>(ObjectProperty->GetPropertyValue(NewValue));

			return Equal(OldFunction, NewFunction);
		}
	}

	if (!Property.Identical(OldValue, NewValue))
	{
		SET_DIFF("%s changed", *Property.GetPathName());
		return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelMaterialDiffing::Equal(
	const UMaterialExpression* OldExpression,
	const UMaterialExpression* NewExpression)
{
	RECURSE_GUARD();

	if (!OldExpression &&
		!NewExpression)
	{
		return true;
	}

	if (!OldExpression ||
		!NewExpression)
	{
		if (OldExpression)
		{
			SET_DIFF("%s removed", *OldExpression->GetPathName());
		}
		else
		{
			SET_DIFF("%s added", *NewExpression->GetPathName());
		}
		return false;
	}

	return Equal(
		*OldExpression,
		*NewExpression);
}

bool FVoxelMaterialDiffing::Equal(
	const UMaterialExpression& OldExpression,
	const UMaterialExpression& NewExpression)
{
	RECURSE_GUARD();

	if (&OldExpression == &NewExpression)
	{
		return true;
	}

	if (EqualExpressions.Contains({ &OldExpression, &NewExpression }))
	{
		return true;
	}

	if (OldExpression.GetClass() != NewExpression.GetClass())
	{
		SET_DIFF("%s is now %s", *OldExpression.GetPathName(), *NewExpression.GetPathName());
		return false;
	}

	for (const FProperty& Property : GetClassProperties(OldExpression.GetClass()))
	{
		if (Property.HasAnyPropertyFlags(RF_Transient))
		{
			continue;
		}

		if (&Property == &FindFPropertyChecked(UMaterialExpression, Material) ||
			&Property == &FindFPropertyChecked(UMaterialExpression, Function) ||
			&Property == &FindFPropertyChecked(UMaterialExpression, MaterialExpressionGuid))
		{
			continue;
		}

		if (Property.GetFName() == STATIC_FNAME("ExpressionGUID") ||
			Property.GetFName() == STATIC_FNAME("DeclarationGUID") ||
			Property.GetFName() == STATIC_FNAME("VariableGUID"))
		{
			// ExpressionGUID is for parameters, DeclarationGUID and VariableGUID for named reroute nodes
			continue;
		}

		if (!Equal(
			Property,
			Property.ContainerPtrToValuePtr<void>(&OldExpression),
			Property.ContainerPtrToValuePtr<void>(&NewExpression)))
		{
			return false;
		}
	}

	EqualExpressions.Add_EnsureNew({ &OldExpression, &NewExpression });
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelMaterialDiffing::Equal(
	const UMaterialFunction* OldFunction,
	const UMaterialFunction* NewFunction)
{
	RECURSE_GUARD();

	if (!OldFunction &&
		!NewFunction)
	{
		return true;
	}

	if (!OldFunction ||
		!NewFunction)
	{
		if (OldFunction)
		{
			SET_DIFF("%s removed", *OldFunction->GetPathName());
		}
		else
		{
			SET_DIFF("%s added", *NewFunction->GetPathName());
		}
		return false;
	}

	return Equal(
		*OldFunction,
		*NewFunction);
}

bool FVoxelMaterialDiffing::Equal(
	const UMaterialFunction& OldFunction,
	const UMaterialFunction& NewFunction)
{
	RECURSE_GUARD();

	if (&OldFunction == &NewFunction)
	{
		return true;
	}

	if (EqualFunctions.Contains({ &OldFunction, &NewFunction }))
	{
		return true;
	}

	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_COUNTER_FNAME(OldFunction.UserExposedCaption.IsEmpty() ? OldFunction.GetFName() : FName(OldFunction.UserExposedCaption));

	if (OldFunction.UserExposedCaption != NewFunction.UserExposedCaption)
	{
		SET_DIFF("%s.UserExposedCaption vs %s.UserExposedCaption: %s -> %s",
			*OldFunction.GetPathName(),
			*NewFunction.GetPathName(),
			*OldFunction.UserExposedCaption,
			*NewFunction.UserExposedCaption);

		return false;
	}

	const TVoxelArray<UMaterialExpression*> OldExpressions = FVoxelUtilities::GetMaterialExpressions(OldFunction);
	const TVoxelArray<UMaterialExpression*> NewExpressions = FVoxelUtilities::GetMaterialExpressions(NewFunction);

	if (OldExpressions.Num() != NewExpressions.Num())
	{
		if (OldExpressions.Num() < NewExpressions.Num())
		{
			SET_DIFF("%s: %d expressions added", *NewFunction.GetPathName(), NewExpressions.Num() - OldExpressions.Num());
		}
		else
		{
			SET_DIFF("%s: %d expressions removed", *NewFunction.GetPathName(), OldExpressions.Num() - NewExpressions.Num());
		}

		return false;
	}

	for (int32 Index = 0; Index < OldExpressions.Num(); Index++)
	{
		if (!Equal(
			OldExpressions[Index],
			NewExpressions[Index]))
		{
			return false;
		}
	}

	EqualFunctions.Add_EnsureNew({ &OldFunction, &NewFunction });
	return true;
}
#endif

#undef SET_DIFF
#undef RECURSE_GUARD