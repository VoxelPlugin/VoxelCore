// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreEditorMinimal.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"

class VOXELCOREEDITOR_API FVoxelSlateStyleSet : public FSlateStyleSet
{
public:
	explicit FVoxelSlateStyleSet(FName Name);

	friend class FVoxelGlobalSlateStyleSet;
};

#define VOXEL_INITIALIZE_STYLE(Name) \
	INTELLISENSE_ONLY(void Name();) \
	class FVoxel ## Name ## Style : public FVoxelSlateStyleSet \
	{ \
	public: \
		FVoxel ## Name ## Style() \
			: FVoxelSlateStyleSet(#Name) \
		{ \
			Init(); \
		} \
		void Init(); \
	}; \
	VOXEL_RUN_ON_STARTUP_EDITOR_COMMANDLET(Register ## Name ## Style) \
	{ \
		FVoxelEditorStyle::AddFactory([] \
		{ \
			return MakeVoxelShared<FVoxel ## Name ## Style>(); \
		}); \
	} \
	void FVoxel ## Name ## Style::Init()

using FVoxelStyleSetFactory = TFunction<TSharedRef<FVoxelSlateStyleSet>()>;

class VOXELCOREEDITOR_API FVoxelEditorStyle
{
public:
	static const FSlateStyleSet& Get();
	static void AddFactory(const FVoxelStyleSetFactory& Factory);

	static void Register();
	static void Unregister();
	static void Shutdown();

	static void ReloadTextures();
	static void ReinitializeStyle();

public:
	template<typename T>
	static const T& GetWidgetStyle(const FName PropertyName, const ANSICHAR* Specifier = nullptr)
	{
		return Get().GetWidgetStyle<T>(PropertyName, Specifier);
	}
	static float GetFloat(const FName PropertyName, const ANSICHAR* Specifier = nullptr)
	{
		return Get().GetFloat(PropertyName, Specifier);
	}
	static FVector2D GetVector(const FName PropertyName, const ANSICHAR* Specifier = nullptr)
	{
		return Get().GetVector(PropertyName, Specifier);
	}
	static const FLinearColor& GetColor(const FName PropertyName, const ANSICHAR* Specifier = nullptr)
	{
		return Get().GetColor(PropertyName, Specifier);
	}
	static FSlateColor GetSlateColor(const FName PropertyName, const ANSICHAR* Specifier = nullptr)
	{
		return Get().GetSlateColor(PropertyName, Specifier);
	}
	static const FMargin& GetMargin(const FName PropertyName, const ANSICHAR* Specifier = nullptr)
	{
		return Get().GetMargin(PropertyName, Specifier);
	}
	static const FSlateBrush* GetBrush(const FName PropertyName, const ANSICHAR* Specifier = nullptr)
	{
		return Get().GetBrush(PropertyName, Specifier);
	}
	static TSharedPtr<FSlateDynamicImageBrush> GetDynamicImageBrush(const FName BrushTemplate, const FName TextureName, const ANSICHAR* Specifier = nullptr)
	{
		return Get().GetDynamicImageBrush(BrushTemplate, TextureName, Specifier);
	}
	static TSharedPtr<FSlateDynamicImageBrush> GetDynamicImageBrush(const FName BrushTemplate, const ANSICHAR* Specifier, UTexture2D* TextureResource, const FName TextureName)
	{
		return Get().GetDynamicImageBrush(BrushTemplate, Specifier, TextureResource, TextureName);
	}
	static TSharedPtr<FSlateDynamicImageBrush> GetDynamicImageBrush(const FName BrushTemplate, UTexture2D* TextureResource, const FName TextureName)
	{
		return Get().GetDynamicImageBrush(BrushTemplate, TextureResource, TextureName);
	}
	static const FSlateSound& GetSound(const FName PropertyName, const ANSICHAR* Specifier = nullptr)
	{
		return Get().GetSound(PropertyName, Specifier);
	}
	static FSlateFontInfo GetFontStyle(const FName PropertyName, const ANSICHAR* Specifier = nullptr)
	{
		return Get().GetFontStyle(PropertyName, Specifier);
	}
	static const FSlateBrush* GetDefaultBrush()
	{
		return Get().GetDefaultBrush();
	}
	static const FSlateBrush* GetNoBrush()
	{
		return FStyleDefaults::GetNoBrush();
	}
	static const FSlateBrush* GetOptionalBrush(const FName PropertyName, const ANSICHAR* Specifier = nullptr, const FSlateBrush* const DefaultBrush = FStyleDefaults::GetNoBrush())
	{
		return Get().GetOptionalBrush(PropertyName, Specifier, DefaultBrush);
	}
	static void GetResources(TArray<const FSlateBrush*>& OutResources)
	{
		return Get().GetResources(OutResources);
	}
	static const FName& GetStyleSetName()
	{
		return Get().GetStyleSetName();
	}
};