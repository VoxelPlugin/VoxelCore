// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelEditorMinimal.h"
#include "PropertyHandle.h"
#include "PropertyEditorModule.h"
#include "Engine/UserDefinedStruct.h"

#if VOXEL_ENGINE_VERSION <= 504
bool FPropertyEditorModule::IsCustomizedStruct(const UStruct* Struct, const FCustomPropertyTypeLayoutMap& InstancePropertyTypeLayoutMap) const
{
	bool bFound = false;
	if (Struct && !Struct->IsA<UUserDefinedStruct>())
	{
		bFound = InstancePropertyTypeLayoutMap.Contains( Struct->GetFName() );
		if( !bFound )
		{
			bFound = GlobalPropertyTypeToLayoutMap.Contains( Struct->GetFName() );
		}
		
		if( !bFound )
		{
			static const FName NAME_PresentAsTypeMetadata(TEXT("PresentAsType"));
			if (const FString* DisplayType = Struct->FindMetaData(NAME_PresentAsTypeMetadata))
			{
				// try finding DisplayType instead
				bFound = InstancePropertyTypeLayoutMap.Contains( FName(*DisplayType) );
				if( !bFound )
				{
					bFound = GlobalPropertyTypeToLayoutMap.Contains( FName(*DisplayType) );
				}
			}
		}
	}
	
	return bFound;
}
#endif

FPropertyTypeLayoutCallback FPropertyEditorModule::FindPropertyTypeLayoutCallback(FName PropertyTypeName, const IPropertyHandle& PropertyHandle, const FCustomPropertyTypeLayoutMap& InstancedPropertyTypeLayoutMap)
{
	if (PropertyTypeName != NAME_None)
	{
		const FPropertyTypeLayoutCallbackList* LayoutCallbacks = InstancedPropertyTypeLayoutMap.Find( PropertyTypeName );
	
		if( !LayoutCallbacks )
		{
			LayoutCallbacks = GlobalPropertyTypeToLayoutMap.Find(PropertyTypeName);
		}
		
		if( !LayoutCallbacks )
		{
			if (const FStructProperty* AsStructProperty = CastField<FStructProperty>(PropertyHandle.GetProperty()))
			{
				static const FName NAME_PresentAsTypeMetadata(TEXT("PresentAsType"));
				if (const FString* DisplayType = AsStructProperty->Struct->FindMetaData(NAME_PresentAsTypeMetadata))
				{
					// try finding DisplayType instead
					LayoutCallbacks = InstancedPropertyTypeLayoutMap.Find(FName(*DisplayType));
	
					if( !LayoutCallbacks )
					{
						LayoutCallbacks = GlobalPropertyTypeToLayoutMap.Find(FName(*DisplayType));
					}
				}
			}
		}

		if ( LayoutCallbacks )
		{
			const FPropertyTypeLayoutCallback& Callback = LayoutCallbacks->Find(PropertyHandle);
			return Callback;
		}
	}

	return FPropertyTypeLayoutCallback();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

const FPropertyTypeLayoutCallback& FPropertyTypeLayoutCallbackList::Find( const IPropertyHandle& PropertyHandle ) const 
{
	if( IdentifierList.Num() > 0 )
	{
		const FPropertyTypeLayoutCallback* Callback =
			IdentifierList.FindByPredicate
			(
				[&]( const FPropertyTypeLayoutCallback& InCallback )
				{
					return InCallback.PropertyTypeIdentifier->IsPropertyTypeCustomized( PropertyHandle );
				}
			);

		if( Callback )
		{
			return *Callback;
		}
	}

	return BaseCallback;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<IPropertyTypeCustomization> FPropertyTypeLayoutCallback::GetCustomizationInstance() const
{
	return PropertyTypeLayoutDelegate.Execute();
}