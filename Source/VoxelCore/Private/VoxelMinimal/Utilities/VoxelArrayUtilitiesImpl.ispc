// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelMinimal.isph"

export uniform bool ArrayUtilities_AllEqual_uint8(
	const uniform uint8 Constant,
	const uniform uint8 Data[],
	const uniform int32 Num)
{
	const uniform int32 AlignedNum = programCount * (Num / programCount);

	for (uniform int32 Index = 0; Index < AlignedNum; Index += programCount)
	{
		if (any(Data[Index + programIndex] != Constant))
		{
			return false;
		}
	}

	for (uniform int32 Index = AlignedNum; Index < Num; Index++)
	{
		if (Data[Index] != Constant)
		{
			return false;
		}
	}

	return true;
}

export uniform bool ArrayUtilities_AllEqual_uint16(
	const uniform uint16 Constant,
	const uniform uint16 Data[],
	const uniform int32 Num)
{
	const uniform int32 AlignedNum = programCount * (Num / programCount);

	for (uniform int32 Index = 0; Index < AlignedNum; Index += programCount)
	{
		if (any(Data[Index + programIndex] != Constant))
		{
			return false;
		}
	}

	for (uniform int32 Index = AlignedNum; Index < Num; Index++)
	{
		if (Data[Index] != Constant)
		{
			return false;
		}
	}

	return true;
}

export uniform bool ArrayUtilities_AllEqual_uint32(
	const uniform uint32 Constant,
	const uniform uint32 Data[],
	const uniform int32 Num)
{
	const uniform int32 AlignedNum = programCount * (Num / programCount);

	for (uniform int32 Index = 0; Index < AlignedNum; Index += programCount)
	{
		if (any(Data[Index + programIndex] != Constant))
		{
			return false;
		}
	}

	for (uniform int32 Index = AlignedNum; Index < Num; Index++)
	{
		if (Data[Index] != Constant)
		{
			return false;
		}
	}

	return true;
}

export uniform bool ArrayUtilities_AllEqual_uint64(
	const uniform uint64 Constant,
	const uniform uint64 Data[],
	const uniform int32 Num)
{
	const uniform int32 AlignedNum = programCount * (Num / programCount);

	for (uniform int32 Index = 0; Index < AlignedNum; Index += programCount)
	{
		if (any(Data[Index + programIndex] != Constant))
		{
			return false;
		}
	}

	for (uniform int32 Index = AlignedNum; Index < Num; Index++)
	{
		if (Data[Index] != Constant)
		{
			return false;
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

export uniform float ArrayUtilities_GetMin(
	const uniform float Values[],
	const uniform int32 Num)
{
	const uniform int32 AlignedNum = programCount * (Num / programCount);

	uniform float Result;
	if (AlignedNum != 0)
	{
		varying float Value = Values[0];
		for (uniform int32 Index = 0; Index < AlignedNum; Index += programCount)
		{
			Value = min(Value, Values[Index + programIndex]);
		}
		Result = reduce_min(Value);
	}
	else
	{
		Result = Values[0];
	}

	for (uniform int32 Index = AlignedNum; Index < Num; Index++)
	{
		Result = min(Result, Values[Index]);
	}
	return Result;
}

export uniform float ArrayUtilities_GetMax(
	const uniform float Values[],
	const uniform int32 Num)
{
	const uniform int32 AlignedNum = programCount * (Num / programCount);

	uniform float Result;
	if (AlignedNum != 0)
	{
		varying float Value = Values[0];
		for (uniform int32 Index = 0; Index < AlignedNum; Index += programCount)
		{
			Value = max(Value, Values[Index + programIndex]);
		}
		Result = reduce_max(Value);
	}
	else
	{
		Result = Values[0];
	}

	for (uniform int32 Index = AlignedNum; Index < Num; Index++)
	{
		Result = max(Result, Values[Index]);
	}
	return Result;
}

export void ArrayUtilities_GetMinMax_Int32(
	const uniform int32 Values[],
	const uniform int32 Num,
	int32* uniform OutMin,
	int32* uniform OutMax)
{
	const uniform int32 AlignedNum = programCount * (Num / programCount);

	uniform int32 MinResult;
	uniform int32 MaxResult;
	if (AlignedNum != 0)
	{
		varying int32 MinValue = MAX_int32;
		varying int32 MaxValue = -MAX_int32;
		for (uniform int32 Index = 0; Index < AlignedNum; Index += programCount)
		{
			const varying int32 Value = Values[Index + programIndex];
			MinValue = min(MinValue, Value);
			MaxValue = max(MaxValue, Value);
		}
		MinResult = reduce_min(MinValue);
		MaxResult = reduce_max(MaxValue);
	}
	else
	{
		MinResult = MAX_int32;
		MaxResult = -MAX_int32;
	}

	for (uniform int32 Index = AlignedNum; Index < Num; Index++)
	{
		const uniform int32 Value = Values[Index];
		MinResult = min(MinResult, Value);
		MaxResult = max(MaxResult, Value);
	}

	*OutMin = MinResult;
	*OutMax = MaxResult;
}

export void ArrayUtilities_GetMinMax_Float(
	const uniform float Values[],
	const uniform int32 Num,
	float* uniform OutMin,
	float* uniform OutMax)
{
	const uniform int32 AlignedNum = programCount * (Num / programCount);

	uniform float MinResult;
	uniform float MaxResult;
	if (AlignedNum != 0)
	{
		varying float MinValue = Values[0];
		varying float MaxValue = Values[0];
		for (uniform int32 Index = 0; Index < AlignedNum; Index += programCount)
		{
			const varying float Value = Values[Index + programIndex];
			MinValue = min(MinValue, Value);
			MaxValue = max(MaxValue, Value);
		}
		MinResult = reduce_min(MinValue);
		MaxResult = reduce_max(MaxValue);
	}
	else
	{
		MinResult = Values[0];
		MaxResult = Values[0];
	}

	for (uniform int32 Index = AlignedNum; Index < Num; Index++)
	{
		const uniform float Value = Values[Index];
		MinResult = min(MinResult, Value);
		MaxResult = max(MaxResult, Value);
	}

	*OutMin = MinResult;
	*OutMax = MaxResult;
}

export void ArrayUtilities_GetMinMaxSafe_Float(
	const uniform float Values[],
	const uniform int32 Num,
	float* uniform OutMin,
	float* uniform OutMax)
{
	const uniform int32 AlignedNum = programCount * (Num / programCount);

	uniform float MinResult;
	uniform float MaxResult;
	if (AlignedNum != 0)
	{
		varying float MinValue = MAX_flt;
		varying float MaxValue = -MAX_flt;
		for (uniform int32 Index = 0; Index < AlignedNum; Index += programCount)
		{
			const varying float Value = Values[Index + programIndex];
			if (!IsFinite(Value))
			{
				continue;
			}

			MinValue = min(MinValue, Value);
			MaxValue = max(MaxValue, Value);
		}
		MinResult = reduce_min(MinValue);
		MaxResult = reduce_max(MaxValue);
	}
	else
	{
		MinResult = MAX_flt;
		MaxResult = -MAX_flt;
	}

	for (uniform int32 Index = AlignedNum; Index < Num; Index++)
	{
		const uniform float Value = Values[Index];
		if (!IsFinite(Value))
		{
			continue;
		}

		MinResult = min(MinResult, Value);
		MaxResult = max(MaxResult, Value);
	}

	*OutMin = MinResult;
	*OutMax = MaxResult;
}

export void ArrayUtilities_GetMinMaxSafe_Double(
	const uniform double Values[],
	const uniform int32 Num,
	double* uniform OutMin,
	double* uniform OutMax)
{
	const uniform int32 AlignedNum = programCount * (Num / programCount);

	uniform double MinResult;
	uniform double MaxResult;
	if (AlignedNum != 0)
	{
		varying double MinValue = MAX_dbl;
		varying double MaxValue = -MAX_dbl;
		for (uniform int32 Index = 0; Index < AlignedNum; Index += programCount)
		{
			const varying double Value = Values[Index + programIndex];
			if (!IsFinite(Value))
			{
				continue;
			}

			MinValue = min(MinValue, Value);
			MaxValue = max(MaxValue, Value);
		}
		MinResult = reduce_min(MinValue);
		MaxResult = reduce_max(MaxValue);
	}
	else
	{
		MinResult = MAX_dbl;
		MaxResult = -MAX_dbl;
	}

	for (uniform int32 Index = AlignedNum; Index < Num; Index++)
	{
		const uniform double Value = Values[Index];
		if (!IsFinite(Value))
		{
			continue;
		}

		MinResult = min(MinResult, Value);
		MaxResult = max(MaxResult, Value);
	}

	*OutMin = MinResult;
	*OutMax = MaxResult;
}