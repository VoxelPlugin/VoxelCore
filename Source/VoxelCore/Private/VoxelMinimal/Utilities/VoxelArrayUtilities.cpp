// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelArrayUtilitiesImpl.ispc.generated.h"

bool FVoxelUtilities::AllEqual(const TConstVoxelArrayView<uint8> Data, const uint8 Value)
{
	return ispc::ArrayUtilities_AllEqual_uint8(Data.GetData(), Data.Num(), Value);
}

bool FVoxelUtilities::AllEqual(const TConstVoxelArrayView<uint16> Data, const uint16 Value)
{
	return ispc::ArrayUtilities_AllEqual_uint16(Data.GetData(), Data.Num(), Value);
}

bool FVoxelUtilities::AllEqual(const TConstVoxelArrayView<uint32> Data, const uint32 Value)
{
	return ispc::ArrayUtilities_AllEqual_uint32(Data.GetData(), Data.Num(), Value);
}

bool FVoxelUtilities::AllEqual(const TConstVoxelArrayView<uint64> Data, const uint64 Value)
{
	return ispc::ArrayUtilities_AllEqual_uint64(Data.GetData(), Data.Num(), Value);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

float FVoxelUtilities::GetMin(const TConstVoxelArrayView<float> Data)
{
	if (!ensure(Data.Num() > 0))
	{
		return 0.f;
	}

	return ispc::ArrayUtilities_GetMin(Data.GetData(), Data.Num());
}

float FVoxelUtilities::GetMax(const TConstVoxelArrayView<float> Data)
{
	if (!ensure(Data.Num() > 0))
	{
		return 0.f;
	}

	return ispc::ArrayUtilities_GetMax(Data.GetData(), Data.Num());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FInt32Interval FVoxelUtilities::GetMinMax(const TConstVoxelArrayView<int32> Data)
{
	if (!ensure(Data.Num() > 0))
	{
		return FInt32Interval(MAX_int32, -MAX_int32);
	}

	FInt32Interval Result;
	ispc::ArrayUtilities_GetMinMax_Int32(Data.GetData(), Data.Num(), &Result.Min, &Result.Max);
	return Result;
}

FFloatInterval FVoxelUtilities::GetMinMax(const TConstVoxelArrayView<float> Data)
{
	if (!ensure(Data.Num() > 0))
	{
		return FFloatInterval(0.f, 0.f);
	}

	FFloatInterval Result;
	ispc::ArrayUtilities_GetMinMax_Float(Data.GetData(), Data.Num(), &Result.Min, &Result.Max);
	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FFloatInterval FVoxelUtilities::GetMinMaxSafe(const TConstVoxelArrayView<float> Data)
{
	if (!ensure(Data.Num() > 0))
	{
		return FFloatInterval(MAX_flt, -MAX_flt);
	}

	FFloatInterval Result;
	ispc::ArrayUtilities_GetMinMaxSafe_Float(Data.GetData(), Data.Num(), &Result.Min, &Result.Max);
	return Result;
}

FDoubleInterval FVoxelUtilities::GetMinMaxSafe(const TConstVoxelArrayView<double> Data)
{
	if (!ensure(Data.Num() > 0))
	{
		return FDoubleInterval(MAX_dbl, -MAX_dbl);
	}

	FDoubleInterval Result;
	ispc::ArrayUtilities_GetMinMaxSafe_Double(Data.GetData(), Data.Num(), &Result.Min, &Result.Max);
	return Result;
}