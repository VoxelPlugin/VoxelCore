// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelArrayUtilitiesImpl.ispc.generated.h"

bool FVoxelUtilities::AllEqual(const uint8 Value, const TConstArrayView<uint8> Data)
{
	return ispc::ArrayUtilities_AllEqual_uint8(Value, Data.GetData(), Data.Num());
}

bool FVoxelUtilities::AllEqual(const uint16 Value, const TConstArrayView<uint16> Data)
{
	return ispc::ArrayUtilities_AllEqual_uint16(Value, Data.GetData(), Data.Num());
}

bool FVoxelUtilities::AllEqual(const uint32 Value, const TConstArrayView<uint32> Data)
{
	return ispc::ArrayUtilities_AllEqual_uint32(Value, Data.GetData(), Data.Num());
}

bool FVoxelUtilities::AllEqual(const uint64 Value, const TConstArrayView<uint64> Data)
{
	return ispc::ArrayUtilities_AllEqual_uint64(Value, Data.GetData(), Data.Num());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

float FVoxelUtilities::GetMin(const TConstArrayView<float> Data)
{
	if (!ensure(Data.Num() > 0))
	{
		return 0.f;
	}

	return ispc::ArrayUtilities_GetMin(Data.GetData(), Data.Num());
}

float FVoxelUtilities::GetMax(const TConstArrayView<float> Data)
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

FInt32Interval FVoxelUtilities::GetMinMax(const TConstArrayView<int32> Data)
{
	if (!ensure(Data.Num() > 0))
	{
		return FInt32Interval(MAX_int32, -MAX_int32);
	}

	FInt32Interval Result;
	ispc::ArrayUtilities_GetMinMax_Int32(Data.GetData(), Data.Num(), &Result.Min, &Result.Max);
	return Result;
}

FFloatInterval FVoxelUtilities::GetMinMax(const TConstArrayView<float> Data)
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

FFloatInterval FVoxelUtilities::GetMinMaxSafe(const TConstArrayView<float> Data)
{
	if (!ensure(Data.Num() > 0))
	{
		return FFloatInterval(MAX_flt, -MAX_flt);
	}

	FFloatInterval Result;
	ispc::ArrayUtilities_GetMinMaxSafe_Float(Data.GetData(), Data.Num(), &Result.Min, &Result.Max);
	return Result;
}

FDoubleInterval FVoxelUtilities::GetMinMaxSafe(const TConstArrayView<double> Data)
{
	if (!ensure(Data.Num() > 0))
	{
		return FDoubleInterval(MAX_dbl, -MAX_dbl);
	}

	FDoubleInterval Result;
	ispc::ArrayUtilities_GetMinMaxSafe_Double(Data.GetData(), Data.Num(), &Result.Min, &Result.Max);
	return Result;
}