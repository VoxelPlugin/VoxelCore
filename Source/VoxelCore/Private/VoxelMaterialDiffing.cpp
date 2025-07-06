// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMaterialDiffing.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"

#define RECURSE_GUARD(Name) \
	FGuard Guard(*this, Name); \
	if (Callstack.Num() > 256) \
	{ \
		ensureVoxelSlow(false); \
		VOXEL_MESSAGE(Error, "Infinite recursion when diffing materials. Callstack: {0}", Callstack); \
		return false; \
	}

#define SET_DIFF(Format, ...) Diff += FString::Printf(TEXT(Format), ##__VA_ARGS__)

#if WITH_EDITOR
bool FVoxelMaterialDiffing::Equal(
	const UMaterial& OldMaterial,
	const UMaterial& NewMaterial)
{
	VOXEL_FUNCTION_COUNTER();
	RECURSE_GUARD("FVoxelMaterialDiffing::Equal");

	for (const FProperty& Property : GetClassProperties<UMaterial>())
	{
		if (&Property == &FindFPropertyChecked(UMaterial, StateId) ||
			&Property == &FindFPropertyChecked(UMaterial, ParameterOverviewExpansion) ||
			Property.GetFName() == STATIC_FNAME("LightingGuid") ||
			Property.GetFName() == STATIC_FNAME("ReferencedTextureGuids"))
		{
			continue;
		}

		if (!Equal(
			Property,
			Property.ContainerPtrToValuePtr<void>(&OldMaterial),
			Property.ContainerPtrToValuePtr<void>(&NewMaterial)))
		{
			return false;
		}
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
	RECURSE_GUARD(Property.GetFName());

	if (Property.HasAnyPropertyFlags(CPF_Transient))
	{
		// Skip PropertyConnectedMask
		return true;
	}

	if (Property.HasAnyPropertyFlags(CPF_InstancedReference))
	{
		// EditorOnlyData
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

	if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property))
	{
		FScriptSetHelper OldSet(SetProperty, OldValue);
		FScriptSetHelper NewSet(SetProperty, NewValue);

		if (OldSet.Num() != NewSet.Num())
		{
			SET_DIFF("%s changed", *Property.GetPathName());
			return false;
		}

		for (int32 Index = 0; Index < OldSet.GetMaxIndex(); Index++)
		{
			if (OldSet.IsValidIndex(Index) != NewSet.IsValidIndex(Index))
			{
				SET_DIFF("%s changed", *Property.GetPathName());
				return false;
			}

			if (!OldSet.IsValidIndex(Index))
			{
				continue;
			}

			if (!Equal(
				*SetProperty->ElementProp,
				OldSet.GetElementPtr(Index),
				NewSet.GetElementPtr(Index)))
			{
				return false;
			}
		}

		return true;
	}

	if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))
	{
		FScriptMapHelper OldMap(MapProperty, OldValue);
		FScriptMapHelper NewMap(MapProperty, NewValue);

		if (OldMap.Num() != NewMap.Num())
		{
			SET_DIFF("%s changed", *Property.GetPathName());
			return false;
		}

		for (int32 Index = 0; Index < OldMap.GetMaxIndex(); Index++)
		{
			if (OldMap.IsValidIndex(Index) != NewMap.IsValidIndex(Index))
			{
				SET_DIFF("%s changed", *Property.GetPathName());
				return false;
			}

			if (!OldMap.IsValidIndex(Index))
			{
				continue;
			}

			if (!Equal(
				*MapProperty->KeyProp,
				OldMap.GetKeyPtr(Index),
				NewMap.GetKeyPtr(Index)))
			{
				return false;
			}

			if (!Equal(
				*MapProperty->ValueProp,
				OldMap.GetValuePtr(Index),
				NewMap.GetValuePtr(Index)))
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
	RECURSE_GUARD("");

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
	RECURSE_GUARD(OldExpression.GetFName());

	if (&OldExpression == &NewExpression)
	{
		return true;
	}

	if (EqualExpressions.Contains({ &OldExpression, &NewExpression }))
	{
		return true;
	}

	const bool bHasLoop = !EqualExpressions_Started.TryAdd({ &OldExpression, &NewExpression });

	if (bHasLoop &&
		OldExpression.IsA<UMaterialExpressionNamedRerouteUsage>() &&
		NewExpression.IsA<UMaterialExpressionNamedRerouteUsage>())
	{
		// Reroute nodes can safely loop when used behind static switches
		// In that case, assume the sub graph is equal - the parent check will fail for us if not
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

	// No _EnsureNew, we might end up adding this in the calls above
	EqualExpressions.Add({ &OldExpression, &NewExpression });
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelMaterialDiffing::Equal(
	const UMaterialFunction* OldFunction,
	const UMaterialFunction* NewFunction)
{
	RECURSE_GUARD("");

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
	RECURSE_GUARD(OldFunction.GetFName());

	if (&OldFunction == &NewFunction)
	{
		return true;
	}

	if (EqualFunctions.Contains({ &OldFunction, &NewFunction }))
	{
		return true;
	}

	ensureVoxelSlow(EqualFunctions_Started.TryAdd({ &OldFunction, &NewFunction }));

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