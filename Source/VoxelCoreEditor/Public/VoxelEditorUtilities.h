// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreEditorMinimal.h"
#include "VoxelEditorStyle.h"

class FVoxelDetailInterface;

template<typename T>
class TVoxelCommands : public TCommands<T>
{
public:
	static FString Name;

	TVoxelCommands()
		: TCommands<T>(
			*Name,
			FText::FromString(Name),
			NAME_None,
			FVoxelEditorStyle::GetStyleSetName())
	{
	}
};

VOXELCOREEDITOR_API FString Voxel_GetCommandsName(FString Name);

#define DEFINE_VOXEL_COMMANDS(InName) \
	template<> \
	FString TVoxelCommands<InName>::Name = Voxel_GetCommandsName(#InName); \
	VOXEL_RUN_ON_STARTUP_EDITOR() \
	{ \
		InName::Register(); \
	}

#define VOXEL_UI_COMMAND( CommandId, FriendlyName, InDescription, CommandType, InDefaultChord, ... ) \
	MakeUICommand_InternalUseOnly( this, CommandId, *Name, TEXT(#CommandId), TEXT(#CommandId) TEXT("_ToolTip"), "." #CommandId, TEXT(FriendlyName), TEXT(InDescription), CommandType, InDefaultChord, ## __VA_ARGS__ );

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#undef DRAG_DROP_OPERATOR_TYPE

#define DRAG_DROP_OPERATOR_TYPE(TYPE, BASE) \
	void __Dummy_ ## TYPE() { checkStatic(std::is_same_v<VOXEL_THIS_TYPE, TYPE>); } \
	static const FString& GetTypeId() { static FString Type = TEXT(#TYPE); return Type; } \
	virtual bool IsOfTypeImpl(const FString& Type) const override { return GetTypeId() == Type || BASE::IsOfTypeImpl(Type); }

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct VOXELCOREEDITOR_API FVoxelEditorUtilities
{
public:
	static void EnableRealtime();
	static void TrackHandle(const TSharedPtr<IPropertyHandle>& PropertyHandle);
	static FSlateFontInfo Font();

public:
	static void HideComponentProperties(const IDetailLayoutBuilder& DetailLayout);

	static void SetSortOrder(
		IDetailLayoutBuilder& DetailLayout,
		FName Name,
		ECategoryPriority::Type Priority,
		int32 PriorityOffset);

	static void HideAndMoveToCategory(
		IDetailLayoutBuilder& DetailLayout,
		FName SourceCategory,
		FName DestCategory,
		const TSet<FName>& ExplicitProperties = {},
		bool bCreateGroup = true,
		ECategoryPriority::Type Priority = ECategoryPriority::Uncommon);

public:
	static FSimpleDelegate MakeRefreshDelegate(IDetailCustomization* DetailCustomization, const FVoxelDetailInterface& DetailInterface);
	static FSimpleDelegate MakeRefreshDelegate(IDetailCustomization* DetailCustomization, const IPropertyTypeCustomizationUtils& CustomizationUtils);
	static FSimpleDelegate MakeRefreshDelegate(IDetailCustomization* DetailCustomization, const IDetailLayoutBuilder& DetailLayout);
	static FSimpleDelegate MakeRefreshDelegate(IDetailCustomization* DetailCustomization, const IDetailCategoryBuilder& CategoryBuilder);
	static FSimpleDelegate MakeRefreshDelegate(IDetailCustomization* DetailCustomization, const IDetailChildrenBuilder& ChildrenBuilder);

	static FSimpleDelegate MakeRefreshDelegate(IPropertyTypeCustomization* DetailCustomization, const FVoxelDetailInterface& DetailInterface);
	static FSimpleDelegate MakeRefreshDelegate(IPropertyTypeCustomization* DetailCustomization, const IPropertyTypeCustomizationUtils& CustomizationUtils);
	static FSimpleDelegate MakeRefreshDelegate(IPropertyTypeCustomization* DetailCustomization, const IDetailLayoutBuilder& DetailLayout);
	static FSimpleDelegate MakeRefreshDelegate(IPropertyTypeCustomization* DetailCustomization, const IDetailCategoryBuilder& CategoryBuilder);
	static FSimpleDelegate MakeRefreshDelegate(IPropertyTypeCustomization* DetailCustomization, const IDetailChildrenBuilder& ChildrenBuilder);

private:
	template<typename T>
	static FSimpleDelegate MakeRefreshDelegate(
		T* DetailCustomization,
		const TSharedPtr<IPropertyUtilities>& PropertyUtilities,
		const IDetailsView* DetailsView);

public:
	static void RegisterAssetContextMenu(UClass* Class, const FText& Label, const FText& ToolTip, TFunction<void(UObject*)> Lambda);

	template<typename T>
	static void RegisterAssetContextMenu(const FText& Label, const FText& ToolTip, TFunction<void(T*)> Lambda)
	{
		FVoxelEditorUtilities::RegisterAssetContextMenu(T::StaticClass(), Label, ToolTip, [=](UObject* Object)
		{
			Lambda(CastChecked<T>(Object));
		});
	}

public:
	template<typename T = UObject>
	static TVoxelArray<T*> GetObjectsBeingCustomized(IDetailLayoutBuilder& DetailLayout)
	{
		TArray<TWeakObjectPtr<UObject>> Objects;
		DetailLayout.GetObjectsBeingCustomized(Objects);

		TVoxelArray<T*> Result;
		for (const TWeakObjectPtr<UObject>& Object : Objects)
		{
			T* TypedObject = Cast<T>(Object.Get());
			if (!ensure(TypedObject))
			{
				continue;
			}

			Result.Add(TypedObject);
		}
		return Result;
	}
	template<typename T = UObject>
	static T* GetUniqueObjectBeingCustomized(IDetailLayoutBuilder& DetailLayout)
	{
		TArray<TWeakObjectPtr<UObject>> Objects;
		DetailLayout.GetObjectsBeingCustomized(Objects);

		if (!ensure(Objects.Num() == 1))
		{
			return nullptr;
		}

		T* TypedObject = Cast<T>(Objects[0].Get());
		if (!ensure(TypedObject))
		{
			return nullptr;
		}
		return TypedObject;
	}
	template<typename T = UObject>
	static TVoxelArray<TVoxelObjectPtr<T>> GetTypedOuters(const TSharedRef<IPropertyHandle> PropertyHandle)
	{
		// Only GetOuterPackages works when using AddExternalStructure
		TArray<UPackage*> OuterPackages;
		PropertyHandle->GetOuterPackages(OuterPackages);

		TVoxelArray<TVoxelObjectPtr<T>> Outers;
		for (const UPackage* Package : OuterPackages)
		{
			ForEachObjectWithPackage(Package, [&](UObject* Object)
			{
				if (T* TypedObject = Cast<T>(Object))
				{
					Outers.Add(TypedObject);
				}
				return true;
			});
		}
		return Outers;
	}

	static bool IsSingleValue(const TSharedPtr<IPropertyHandle>& Handle)
	{
		if (!ensure(Handle))
		{
			return false;
		}

		void* Address = nullptr;
		return Handle->GetValueData(Address) == FPropertyAccess::Success;
	}

	template<typename T>
	static bool GetPropertyValue(const TSharedPtr<IPropertyHandle>& Handle, T*& OutValue)
	{
		checkStatic(!std::derived_from<T, UObject>);
		TrackHandle(Handle);

		OutValue = nullptr;

		if (!ensure(Handle))
		{
			return false;
		}

		void* Address = nullptr;
		if (Handle->GetValueData(Address) != FPropertyAccess::Success ||
			!ensure(Address))
		{
			return false;
		}

		OutValue = static_cast<T*>(Address);

		return true;
	}
	template<typename T>
	static T GetPropertyValue(const TSharedPtr<IPropertyHandle>& Handle)
	{
		TrackHandle(Handle);

		if (!ensure(Handle))
		{
			return {};
		}

		T Value;
		if (!ensure(Handle->GetValue(Value) == FPropertyAccess::Success))
		{
			return {};
		}

		return Value;
	}

public:
	template<typename T>
	static void ForeachData(const TSharedPtr<IPropertyHandle>& PropertyHandle, TFunctionRef<void(T&)> Lambda)
	{
		VOXEL_FUNCTION_COUNTER();

		TrackHandle(PropertyHandle);

		if (!ensure(PropertyHandle))
		{
			return;
		}

		FProperty* Property = PropertyHandle->GetProperty();
		if (Property &&
			!ensure(FVoxelUtilities::MatchesProperty<T>(*Property, false)))
		{
			return;
		}

		PropertyHandle->EnumerateRawData([&](void* RawData, const int32 DataIndex, const int32 NumDatas)
		{
			if (!RawData)
			{
				// Will happen when removing array entries
				return true;
			}

			Lambda(*static_cast<T*>(RawData));
			return true;
		});
	}
	template<typename T>
	static void ForeachDataPtr(const TSharedPtr<IPropertyHandle>& PropertyHandle, TFunctionRef<void(T*)> Lambda)
	{
		VOXEL_FUNCTION_COUNTER();

		TrackHandle(PropertyHandle);

		if (!ensure(PropertyHandle))
		{
			return;
		}

		FProperty* Property = PropertyHandle->GetProperty();
		if (Property &&
			!ensure(FVoxelUtilities::MatchesProperty<T>(*Property, false)))
		{
			return;
		}

		PropertyHandle->EnumerateRawData([&](void* RawData, const int32 DataIndex, const int32 NumDatas)
		{
			Lambda(static_cast<T*>(RawData));
			return true;
		});
	}

public:
	template<typename T>
	static const T* TryGetStructPropertyValue(const TSharedPtr<IPropertyHandle>& Handle)
	{
		TrackHandle(Handle);

		if (!ensure(Handle))
		{
			return nullptr;
		}

		FProperty* Property = Handle->GetProperty();
		if (!ensure(Property) ||
			!ensure(Property->IsA<FStructProperty>()) ||
			!ensure(CastFieldChecked<FStructProperty>(Property)->Struct == StaticStructFast<T>()))
		{
			return nullptr;
		}

		void* Address = nullptr;
		if (Handle->GetValueData(Address) != FPropertyAccess::Success ||
			!Address)
		{
			return nullptr;
		}

		return static_cast<T*>(Address);
	}
	template<typename T>
	static const T& GetStructPropertyValue(const TSharedPtr<IPropertyHandle>& Handle)
	{
		const T* Value = TryGetStructPropertyValue<T>(Handle);
		if (!ensure(Value))
		{
			static const T Default;
			return Default;
		}
		return *Value;
	}

	template<typename T>
	static void SetStructPropertyValue(const TSharedPtr<IPropertyHandle>& Handle, const T& Value)
	{
		TrackHandle(Handle);

		if (!ensure(Handle))
		{
			return;
		}

		FProperty* Property = Handle->GetProperty();
		if (!ensure(Property) ||
			!ensure(Property->IsA<FStructProperty>()) ||
			!ensure(CastFieldChecked<FStructProperty>(Property)->Struct == StaticStructFast<T>()))
		{
			return;
		}

		const FString StringValue = FVoxelUtilities::PropertyToText_Direct(*Property, static_cast<const void*>(&Value), nullptr);
		ensure(Handle->SetValueFromFormattedString(StringValue) == FPropertyAccess::Success);
	}

public:
	template<typename T>
	static T GetEnumPropertyValue(const TSharedPtr<IPropertyHandle>& Handle)
	{
		TrackHandle(Handle);

		if (!ensure(Handle))
		{
			return {};
		}

		uint8 Value = 0;
		if (!ensure(Handle->GetValue(Value) == FPropertyAccess::Success))
		{
			return {};
		}

		return T(Value);
	}
	template<typename T>
	static T* GetUObjectProperty(const TSharedPtr<IPropertyHandle>& Handle)
	{
		TrackHandle(Handle);

		if (!Handle)
		{
			return nullptr;
		}

		UObject* Object = nullptr;
		Handle->GetValue(Object);
		return Cast<T>(Object);
	}

	static TArray<FName> GetPropertyOptions(const TSharedRef<IPropertyHandle>& PropertyHandle);

	static TArray<TSharedRef<IPropertyHandle>> GetChildHandles(
		const TSharedPtr<IPropertyHandle>& PropertyHandle,
		bool bRecursive,
		bool bIncludeSelf);

public:
	static TSharedPtr<IPropertyHandle> MakePropertyHandle(
		const FVoxelDetailInterface& DetailInterface,
		const TArray<UObject*>& Objects,
		FName PropertyName);

	static TSharedPtr<IPropertyHandle> MakePropertyHandle(
		IDetailLayoutBuilder& DetailLayout,
		const TArray<UObject*>& Objects,
		FName PropertyName);

	static TSharedPtr<IPropertyHandle> MakePropertyHandle(
		IDetailLayoutBuilder& DetailLayout,
		UObject* Object,
		FName PropertyName);

	static TSharedPtr<IPropertyHandle> MakePropertyHandle(
		IDetailLayoutBuilder& DetailLayout,
		FName PropertyName);

	static TSharedPtr<IPropertyHandle> FindMapValuePropertyHandle(
		const IPropertyHandle& MapPropertyHandle,
		const FGuid& Guid);

public:
	static void RegisterClassLayout(const UClass* Class, FOnGetDetailCustomizationInstance Delegate);
	static void RegisterStructLayout(const UScriptStruct* Struct, FOnGetPropertyTypeCustomizationInstance Delegate, bool bRecursive);
	static void RegisterStructLayout(const UScriptStruct* Struct, FOnGetPropertyTypeCustomizationInstance Delegate, bool bRecursive, const TSharedPtr<IPropertyTypeIdentifier>& Identifier);
	static void RegisterEnumLayout(const UEnum* Enum, FOnGetPropertyTypeCustomizationInstance Delegate, const TSharedPtr<IPropertyTypeIdentifier>& Identifier);

public:
	static bool GetRayInfo(FEditorViewportClient* ViewportClient, FVector& OutStart, FVector& OutEnd);

public:
	static TSharedRef<FAssetThumbnailPool> GetThumbnailPool();
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define GetChildHandleStatic(Class, Property) GetChildHandle(GET_MEMBER_NAME_STATIC(Class, Property), false).ToSharedRef()++

FORCEINLINE TSharedRef<IPropertyHandle> operator++(const TSharedRef<IPropertyHandle>& PropertyHandle, int32)
{
	FVoxelEditorUtilities::TrackHandle(PropertyHandle);
	return PropertyHandle;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename CustomizationType>
class TVoxelDetailCustomizationWrapper : public IDetailCustomization
{
public:
	const TSharedRef<IDetailCustomization> Customization;

	template<typename... ArgTypes>
	explicit TVoxelDetailCustomizationWrapper(ArgTypes&&... Args)
		: Customization(MakeShared<CustomizationType>(Forward<ArgTypes>(Args)...))
	{
	}

	virtual void PendingDelete() override
	{
		Customization->PendingDelete();
	}
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
	{
		VOXEL_FUNCTION_COUNTER();

		Customization->CustomizeDetails(DetailBuilder);
	}
	virtual void CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder) override
	{
		VOXEL_FUNCTION_COUNTER();

		Customization->CustomizeDetails(DetailBuilder);
	}
};

template<typename CustomizationType>
class TVoxelPropertyTypeCustomizationWrapper : public IPropertyTypeCustomization
{
public:
	const TSharedRef<IPropertyTypeCustomization> Customization = MakeShared<CustomizationType>();

	virtual void CustomizeHeader(
		const TSharedRef<IPropertyHandle> PropertyHandle,
		FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
		VOXEL_FUNCTION_COUNTER();

		Customization->CustomizeHeader(PropertyHandle, HeaderRow, CustomizationUtils);
	}
	virtual void CustomizeChildren(
		const TSharedRef<IPropertyHandle> PropertyHandle,
		IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
		VOXEL_FUNCTION_COUNTER();

		Customization->CustomizeChildren(
			PropertyHandle,
			ChildBuilder,
			CustomizationUtils);
	}
	virtual bool ShouldInlineKey() const override
	{
		return Customization->ShouldInlineKey();
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename Class, typename Customization>
void RegisterVoxelClassLayout()
{
	FVoxelEditorUtilities::RegisterClassLayout(Class::StaticClass(), FOnGetDetailCustomizationInstance::CreateLambda([]
	{
		return MakeShared<TVoxelDetailCustomizationWrapper<Customization>>();
	}));
}

template<typename Struct, typename Customization, bool bRecursive>
void RegisterVoxelStructLayout()
{
	FVoxelEditorUtilities::RegisterStructLayout(Struct::StaticStruct(), FOnGetPropertyTypeCustomizationInstance::CreateLambda([]
	{
		return MakeShared<TVoxelPropertyTypeCustomizationWrapper<Customization>>();
	}), bRecursive);
}

template<typename Struct, typename Customization, bool bRecursive, typename Identifier>
void RegisterVoxelStructLayout()
{
	FVoxelEditorUtilities::RegisterStructLayout(Struct::StaticStruct(), FOnGetPropertyTypeCustomizationInstance::CreateLambda([]
	{
		return MakeShared<TVoxelPropertyTypeCustomizationWrapper<Customization>>();
	}), bRecursive, MakeShared<Identifier>());
}

template<typename Enum, typename Customization>
void RegisterVoxelEnumLayout()
{
	FVoxelEditorUtilities::RegisterEnumLayout(StaticEnum<Enum>(), FOnGetPropertyTypeCustomizationInstance::CreateLambda([]
	{
		return MakeShared<TVoxelPropertyTypeCustomizationWrapper<Customization>>();
	}), nullptr);
}

template<typename Enum, typename Customization, typename Identifier>
void RegisterVoxelEnumLayout()
{
	FVoxelEditorUtilities::RegisterEnumLayout(StaticEnum<Enum>(), FOnGetPropertyTypeCustomizationInstance::CreateLambda([]
	{
		return MakeShared<TVoxelPropertyTypeCustomizationWrapper<Customization>>();
	}), MakeShared<Identifier>());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELCOREEDITOR_API FVoxelDetailCustomization : public IDetailCustomization
{
public:
	VOXEL_COUNT_INSTANCES();

	template<typename T>
	void KeepAlive(const TSharedPtr<T>& Ptr)
	{
		PtrsToKeepAlive.Add(MakeSharedVoidPtr(Ptr));
	}
	template<typename T>
	void KeepAlive(const TSharedRef<T>& Ptr)
	{
		PtrsToKeepAlive.Add(MakeSharedVoidRef(Ptr));
	}

private:
	TVoxelArray<FSharedVoidPtr> PtrsToKeepAlive;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELCOREEDITOR_API FVoxelPropertyTypeCustomizationBase : public IPropertyTypeCustomization
{
public:
	VOXEL_COUNT_INSTANCES();

	template<typename T>
	void KeepAlive(const TSharedPtr<T>& Ptr)
	{
		PtrsToKeepAlive.Add(MakeSharedVoidPtr(Ptr));
	}
	template<typename T>
	void KeepAlive(const TSharedRef<T>& Ptr)
	{
		PtrsToKeepAlive.Add(MakeSharedVoidPtr(Ptr));
	}

private:
	TVoxelArray<FSharedVoidPtr> PtrsToKeepAlive;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define DEFINE_VOXEL_CLASS_LAYOUT(Class, Customization) \
	VOXEL_RUN_ON_STARTUP_EDITOR() \
	{ \
		RegisterVoxelClassLayout<Class, Customization>(); \
	}

#define DEFINE_VOXEL_STRUCT_LAYOUT(Struct, Customization) \
	VOXEL_RUN_ON_STARTUP_EDITOR() \
	{ \
		RegisterVoxelStructLayout<Struct, Customization, false>(); \
	}

#define DEFINE_VOXEL_STRUCT_LAYOUT_IDENTIFIER(Struct, Customization, Identifier) \
	VOXEL_RUN_ON_STARTUP_EDITOR() \
	{ \
		RegisterVoxelStructLayout<Struct, Customization, false, Identifier>(); \
	}

#define DEFINE_VOXEL_STRUCT_LAYOUT_RECURSIVE(Struct, Customization) \
	VOXEL_RUN_ON_STARTUP_EDITOR() \
	{ \
		RegisterVoxelStructLayout<Struct, Customization, true>(); \
	}

#define DEFINE_VOXEL_STRUCT_LAYOUT_RECURSIVE_IDENTIFIER(Struct, Customization, Identifier) \
	VOXEL_RUN_ON_STARTUP_EDITOR() \
	{ \
		RegisterVoxelStructLayout<Struct, Customization, true, Identifier>(); \
	}

#define DEFINE_VOXEL_ENUM_LAYOUT(Enum, Customization) \
	VOXEL_RUN_ON_STARTUP_EDITOR() \
	{ \
		RegisterVoxelEnumLayout<Enum, Customization>(); \
	}

#define DEFINE_VOXEL_ENUM_LAYOUT_IDENTIFIER(Enum, Customization, Identifier) \
	VOXEL_RUN_ON_STARTUP_EDITOR() \
	{ \
		RegisterVoxelEnumLayout<Enum, Customization, Identifier>(); \
	}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define VOXEL_CUSTOMIZE_CLASS(Class) \
	class F ## Class ## Customization : public FVoxelDetailCustomization \
	{ \
	public: \
		static TVoxelArray<Class*> GetObjectsBeingCustomized(IDetailLayoutBuilder& DetailLayout) \
		{ \
			return FVoxelEditorUtilities::GetObjectsBeingCustomized<Class>(DetailLayout); \
		} \
		static Class* GetUniqueObjectBeingCustomized(IDetailLayoutBuilder& DetailLayout) \
		{ \
			return FVoxelEditorUtilities::GetUniqueObjectBeingCustomized<Class>(DetailLayout); \
		} \
		static TVoxelArray<TVoxelObjectPtr<Class>> GetWeakObjectsBeingCustomized(IDetailLayoutBuilder& DetailLayout) \
		{ \
			return TVoxelArray<TVoxelObjectPtr<Class>>(GetObjectsBeingCustomized(DetailLayout)); \
		} \
		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override; \
	}; \
	DEFINE_VOXEL_CLASS_LAYOUT(Class, F ## Class ## Customization); \
	void F ## Class ## Customization::CustomizeDetails

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define VOXEL_CUSTOMIZE_STRUCT_HEADER_IMPL(Macro, Struct) \
	class F ## Struct ## Customization : public FVoxelPropertyTypeCustomizationBase \
	{ \
	public: \
		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override; \
		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override {} \
	}; \
	Macro(Struct, F ## Struct ## Customization); \
	void F ## Struct ## Customization::CustomizeHeader

#define VOXEL_CUSTOMIZE_STRUCT_HEADER(Struct) \
	VOXEL_CUSTOMIZE_STRUCT_HEADER_IMPL(DEFINE_VOXEL_STRUCT_LAYOUT, Struct)

#define VOXEL_CUSTOMIZE_STRUCT_HEADER_RECURSIVE(Struct) \
	VOXEL_CUSTOMIZE_STRUCT_HEADER_IMPL(DEFINE_VOXEL_STRUCT_LAYOUT_RECURSIVE, Struct)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define VOXEL_CUSTOMIZE_STRUCT_CHILDREN_IMPL(Macro, Struct) \
	class F ## Struct ## Customization : public FVoxelPropertyTypeCustomizationBase \
	{ \
	public: \
		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override {} \
		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override; \
	}; \
	Macro(Struct, F ## Struct ## Customization); \
	void F ## Struct ## Customization::CustomizeChildren

#define VOXEL_CUSTOMIZE_STRUCT_CHILDREN(Struct) \
	VOXEL_CUSTOMIZE_STRUCT_CHILDREN_IMPL(DEFINE_VOXEL_STRUCT_LAYOUT, Struct)

#define VOXEL_CUSTOMIZE_STRUCT_CHILDREN_RECURSIVE(Struct) \
	VOXEL_CUSTOMIZE_STRUCT_CHILDREN_IMPL(DEFINE_VOXEL_STRUCT_LAYOUT_RECURSIVE, Struct)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define VOXEL_CUSTOMIZE_ENUM_HEADER(Enum) \
	class F ## Enum ## Customization : public FVoxelPropertyTypeCustomizationBase \
	{ \
	public: \
		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override; \
		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override {} \
	}; \
	DEFINE_VOXEL_ENUM_LAYOUT(Enum, F ## Enum ## Customization); \
	void F ## Enum ## Customization::CustomizeHeader

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define VOXEL_CUSTOMIZE_ENUM_CHILDREN(Enum) \
	class F ## Enum ## Customization : public FVoxelPropertyTypeCustomizationBase \
	{ \
	public: \
		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override {} \
		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override; \
	}; \
	DEFINE_VOXEL_ENUM_LAYOUT(Enum, F ## Enum ## Customization); \
	void F ## Enum ## Customization::CustomizeChildren