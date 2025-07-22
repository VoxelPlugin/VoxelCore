// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

namespace FVoxelTypeUtilitiesTest
{
	struct FTestNoForceInit
	{
		int32 Value = -1;
	};

	struct FTestNoForceInit2
	{
		FTestNoForceInit2(int32 Value) {}
	};

	checkStatic(!FVoxelUtilities::IsForceInitializable_V<FTestNoForceInit>);
	checkStatic(!FVoxelUtilities::IsForceInitializable_V<FTestNoForceInit2>);
	checkStatic(FVoxelUtilities::IsForceInitializable_V<FVector>);

	checkStatic(FVoxelUtilities::CanCastMemory<int32, int32>);
	checkStatic(FVoxelUtilities::CanCastMemory<int32, const int32>);
	checkStatic(!FVoxelUtilities::CanCastMemory<const int32, int32>);
	checkStatic(FVoxelUtilities::CanCastMemory<int32*, int32*>);
	checkStatic(FVoxelUtilities::CanCastMemory<int32*, const int32*>);
	checkStatic(!FVoxelUtilities::CanCastMemory<const int32*, int32*>);

	struct FBase
	{
	};
	struct FChild : FBase
	{
	};

	checkStatic(!FVoxelUtilities::CanCastMemory<FChild, FBase>);
	checkStatic(!FVoxelUtilities::CanCastMemory<FBase, FChild>);
	checkStatic(FVoxelUtilities::CanCastMemory<FChild*, FBase*>);
	checkStatic(!FVoxelUtilities::CanCastMemory<FBase*, FChild*>);
	checkStatic(FVoxelUtilities::CanCastMemory<FChild*, const FBase*>);
	checkStatic(FVoxelUtilities::CanCastMemory<FChild*, const FBase* const>);
	checkStatic(!FVoxelUtilities::CanCastMemory<const FChild*, FBase*>);

	checkStatic(FVoxelUtilities::CanCastMemory<TSharedPtr<FChild>, TSharedPtr<FBase>>);
	checkStatic(!FVoxelUtilities::CanCastMemory<TSharedPtr<FBase>, TSharedPtr<FChild>>);
	checkStatic(FVoxelUtilities::CanCastMemory<TSharedPtr<FChild>, TSharedPtr<const FBase>>);
	checkStatic(FVoxelUtilities::CanCastMemory<TSharedPtr<FChild>, const TSharedPtr<const FBase>>);
	checkStatic(!FVoxelUtilities::CanCastMemory<TSharedPtr<const FChild>, TSharedPtr<FBase>>);

	checkStatic(FVoxelUtilities::CanCastMemory<TSharedRef<FChild>, TSharedRef<FBase>>);
	checkStatic(!FVoxelUtilities::CanCastMemory<TSharedRef<FBase>, TSharedRef<FChild>>);
	checkStatic(FVoxelUtilities::CanCastMemory<TSharedRef<FChild>, TSharedRef<const FBase>>);
	checkStatic(FVoxelUtilities::CanCastMemory<TSharedRef<FChild>, const TSharedRef<const FBase>>);
	checkStatic(!FVoxelUtilities::CanCastMemory<TSharedRef<const FChild>, TSharedRef<FBase>>);

	checkStatic(FVoxelUtilities::CanCastMemory<TSharedRef<FChild>, TSharedPtr<FBase>>);
	checkStatic(!FVoxelUtilities::CanCastMemory<TSharedRef<const FChild>, TSharedPtr<FBase>>);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString FVoxelUtilities::Internal_GetCppName(FString Name)
{
	Name.ReplaceInline(TEXT("class "), TEXT(""));
	Name.ReplaceInline(TEXT("struct "), TEXT(""));
	Name.ReplaceInline(TEXT("enum "), TEXT(""));
	Name.TrimStartAndEndInline();

	// On Mac a space is added before pointers
	Name.ReplaceInline(TEXT(" *"), TEXT("*"));

	return Name;
}