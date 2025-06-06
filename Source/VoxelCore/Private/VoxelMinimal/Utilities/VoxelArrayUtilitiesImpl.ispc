// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.isph"

export void ArrayUtilities_FloatToDouble(
	uniform double Dest[],
	const uniform float Src[],
	const uniform int32 Num)
{
	FOREACH(Index, 0, Num)
	{
		Dest[Index] = Src[Index];
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

export uniform bool ArrayUtilities_AllEqual_uint8(
	const uniform uint8 Data[],
	const uniform int32 Num,
	const uniform uint8 Constant)
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
	const uniform uint16 Data[],
	const uniform int32 Num,
	const uniform uint16 Constant)
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
	const uniform uint32 Data[],
	const uniform int32 Num,
	const uniform uint32 Constant)
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
	const uniform uint64 Data[],
	const uniform int32 Num,
	const uniform uint64 Constant)
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

export uniform uint16 ArrayUtilities_GetMin_uint16(
	const uniform uint16 Values[],
	const uniform int32 Num)
{
	const uniform int32 AlignedNum = programCount * (Num / programCount);

	uniform int32 Result;
	if (AlignedNum != 0)
	{
		varying int32 Value = Values[0];
		for (uniform int32 Index = 0; Index < AlignedNum; Index += programCount)
		{
			Value = min(Value, (int32)Values[Index + programIndex]);
		}
		Result = reduce_min((int32)Value);
	}
	else
	{
		Result = Values[0];
	}

	for (uniform int32 Index = AlignedNum; Index < Num; Index++)
	{
		Result = min(Result, (uniform int32)Values[Index]);
	}
	return Result;
}

export uniform uint16 ArrayUtilities_GetMax_uint16(
	const uniform uint16 Values[],
	const uniform int32 Num)
{
	const uniform int32 AlignedNum = programCount * (Num / programCount);

	uniform int32 Result;
	if (AlignedNum != 0)
	{
		varying int32 Value = Values[0];
		for (uniform int32 Index = 0; Index < AlignedNum; Index += programCount)
		{
			Value = max(Value, (int32)Values[Index + programIndex]);
		}
		Result = reduce_max((int32)Value);
	}
	else
	{
		Result = Values[0];
	}

	for (uniform int32 Index = AlignedNum; Index < Num; Index++)
	{
		Result = max(Result, (uniform int32)Values[Index]);
	}
	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

export uniform float ArrayUtilities_GetMin_float(
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

export uniform float ArrayUtilities_GetMax_float(
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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

export uniform float ArrayUtilities_GetAbsMin(
	const uniform float Values[],
	const uniform int32 Num)
{
	const uniform int32 AlignedNum = programCount * (Num / programCount);

	uniform float Result;
	if (AlignedNum != 0)
	{
		varying float Value = abs(Values[0]);
		for (uniform int32 Index = 0; Index < AlignedNum; Index += programCount)
		{
			Value = min(Value, abs(Values[Index + programIndex]));
		}
		Result = reduce_min(Value);
	}
	else
	{
		Result = abs(Values[0]);
	}

	for (uniform int32 Index = AlignedNum; Index < Num; Index++)
	{
		Result = min(Result, abs(Values[Index]));
	}
	return Result;
}

export uniform float ArrayUtilities_GetAbsMax(
	const uniform float Values[],
	const uniform int32 Num)
{
	const uniform int32 AlignedNum = programCount * (Num / programCount);

	uniform float Result;
	if (AlignedNum != 0)
	{
		varying float Value = abs(Values[0]);
		for (uniform int32 Index = 0; Index < AlignedNum; Index += programCount)
		{
			Value = max(Value, abs(Values[Index + programIndex]));
		}
		Result = reduce_max(Value);
	}
	else
	{
		Result = abs(Values[0]);
	}

	for (uniform int32 Index = AlignedNum; Index < Num; Index++)
	{
		Result = max(Result, abs(Values[Index]));
	}
	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

export uniform float ArrayUtilities_GetAbsMinSafe(
	const uniform float Values[],
	const uniform int32 Num)
{
	const uniform int32 AlignedNum = programCount * (Num / programCount);

	uniform float Result;
	if (AlignedNum != 0)
	{
		varying float LocalResult = MAX_flt;
		for (uniform int32 Index = 0; Index < AlignedNum; Index += programCount)
		{
			const varying float Value = Values[Index + programIndex];
			if (!IsFinite(Value))
			{
				continue;
			}

			LocalResult = min(LocalResult, abs(Value));
		}
		Result = reduce_min(LocalResult);
	}
	else
	{
		Result = MAX_flt;
	}

	for (uniform int32 Index = AlignedNum; Index < Num; Index++)
	{
		const uniform float Value = Values[Index];
		if (!IsFinite(Value))
		{
			continue;
		}

		Result = min(Result, abs(Value));
	}

	return Result;
}

export uniform float ArrayUtilities_GetAbsMaxSafe(
	const uniform float Values[],
	const uniform int32 Num)
{
	const uniform int32 AlignedNum = programCount * (Num / programCount);

	uniform float Result;
	if (AlignedNum != 0)
	{
		varying float LocalResult = -MAX_flt;
		for (uniform int32 Index = 0; Index < AlignedNum; Index += programCount)
		{
			const varying float Value = Values[Index + programIndex];
			if (!IsFinite(Value))
			{
				continue;
			}

			LocalResult = max(LocalResult, abs(Value));
		}
		Result = reduce_max(LocalResult);
	}
	else
	{
		Result = -MAX_flt;
	}

	for (uniform int32 Index = AlignedNum; Index < Num; Index++)
	{
		const uniform float Value = Values[Index];
		if (!IsFinite(Value))
		{
			continue;
		}

		Result = max(Result, abs(Value));
	}

	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

export void ArrayUtilities_GetMinMax_uint8(
	const uniform uint8 Values[],
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

export void ArrayUtilities_GetMinMax_uint16(
	const uniform uint16 Values[],
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

export void ArrayUtilities_GetMinMax_int32(
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

export void ArrayUtilities_GetMinMax_int64(
	const uniform int64 Values[],
	const uniform int32 Num,
	int64* uniform OutMin,
	int64* uniform OutMax)
{
	const uniform int32 AlignedNum = programCount * (Num / programCount);

	uniform int64 MinResult;
	uniform int64 MaxResult;
	if (AlignedNum != 0)
	{
		varying int64 MinValue = MAX_int64;
		varying int64 MaxValue = -MAX_int64;
		for (uniform int32 Index = 0; Index < AlignedNum; Index += programCount)
		{
			const varying int64 Value = Values[Index + programIndex];
			MinValue = min(MinValue, Value);
			MaxValue = max(MaxValue, Value);
		}
		MinResult = reduce_min(MinValue);
		MaxResult = reduce_max(MaxValue);
	}
	else
	{
		MinResult = MAX_int64;
		MaxResult = -MAX_int64;
	}

	for (uniform int32 Index = AlignedNum; Index < Num; Index++)
	{
		const uniform int64 Value = Values[Index];
		MinResult = min(MinResult, Value);
		MaxResult = max(MaxResult, Value);
	}

	*OutMin = MinResult;
	*OutMax = MaxResult;
}

export void ArrayUtilities_GetMinMax_float(
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

export void ArrayUtilities_GetMinMax_double(
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
		varying double MinValue = Values[0];
		varying double MaxValue = Values[0];
		for (uniform int32 Index = 0; Index < AlignedNum; Index += programCount)
		{
			const varying double Value = Values[Index + programIndex];
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
		const uniform double Value = Values[Index];
		MinResult = min(MinResult, Value);
		MaxResult = max(MaxResult, Value);
	}

	*OutMin = MinResult;
	*OutMax = MaxResult;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

export void ArrayUtilities_GetMinMax_float2(
	const uniform float2 Values[],
	const uniform int32 Num,
	float2* uniform OutMin,
	float2* uniform OutMax)
{
	const uniform int32 AlignedNum = programCount * (Num / programCount);

	uniform float MinResultX;
	uniform float MaxResultX;
	uniform float MinResultY;
	uniform float MaxResultY;
	if (AlignedNum != 0)
	{
		varying float MinValueX = Values[0].x;
		varying float MaxValueX = Values[0].x;
		varying float MinValueY = Values[0].y;
		varying float MaxValueY = Values[0].y;
		for (uniform int32 Index = 0; Index < AlignedNum; Index += programCount)
		{
			IGNORE_PERF_WARNING
			const varying float2 Value = Values[Index + programIndex];

			MinValueX = min(MinValueX, Value.x);
			MaxValueX = max(MaxValueX, Value.x);
			MinValueY = min(MinValueY, Value.y);
			MaxValueY = max(MaxValueY, Value.y);
		}
		MinResultX = reduce_min(MinValueX);
		MaxResultX = reduce_max(MaxValueX);
		MinResultY = reduce_min(MinValueY);
		MaxResultY = reduce_max(MaxValueY);
	}
	else
	{
		MinResultX = Values[0].x;
		MaxResultX = Values[0].x;
		MinResultY = Values[0].y;
		MaxResultY = Values[0].y;
	}

	for (uniform int32 Index = AlignedNum; Index < Num; Index++)
	{
		const uniform float2 Value = Values[Index];
		MinResultX = min(MinResultX, Value.x);
		MaxResultX = max(MaxResultX, Value.x);
		MinResultY = min(MinResultY, Value.y);
		MaxResultY = max(MaxResultY, Value.y);
	}

	OutMin->x = MinResultX;
	OutMax->x = MaxResultX;
	OutMin->y = MinResultY;
	OutMax->y = MaxResultY;
}

export void ArrayUtilities_GetMinMax_Color(
	const uniform FColor Values[],
	const uniform int32 Num,
	FColor* uniform OutMin,
	FColor* uniform OutMax)
{
	const uniform int32 AlignedNum = programCount * (Num / programCount);

	uniform uint8 MinResultR;
	uniform uint8 MaxResultR;
	uniform uint8 MinResultG;
	uniform uint8 MaxResultG;
	uniform uint8 MinResultB;
	uniform uint8 MaxResultB;
	uniform uint8 MinResultA;
	uniform uint8 MaxResultA;
	if (AlignedNum != 0)
	{
		varying uint8 MinValueR = Values[0].R;
		varying uint8 MaxValueR = Values[0].R;
		varying uint8 MinValueG = Values[0].G;
		varying uint8 MaxValueG = Values[0].G;
		varying uint8 MinValueB = Values[0].B;
		varying uint8 MaxValueB = Values[0].B;
		varying uint8 MinValueA = Values[0].A;
		varying uint8 MaxValueA = Values[0].A;
		for (uniform int32 Index = 0; Index < AlignedNum; Index += programCount)
		{
			IGNORE_PERF_WARNING
			const varying FColor Value = Values[Index + programIndex];

			MinValueR = min(MinValueR, Value.R);
			MaxValueR = max(MaxValueR, Value.R);
			MinValueG = min(MinValueG, Value.G);
			MaxValueG = max(MaxValueG, Value.G);
			MinValueB = min(MinValueB, Value.B);
			MaxValueB = max(MaxValueB, Value.B);
			MinValueA = min(MinValueA, Value.A);
			MaxValueA = max(MaxValueA, Value.A);
		}
		MinResultR = reduce_min((int32)MinValueR);
		MaxResultR = reduce_max((int32)MaxValueR);
		MinResultG = reduce_min((int32)MinValueG);
		MaxResultG = reduce_max((int32)MaxValueG);
		MinResultB = reduce_min((int32)MinValueB);
		MaxResultB = reduce_max((int32)MaxValueB);
		MinResultA = reduce_min((int32)MinValueA);
		MaxResultA = reduce_max((int32)MaxValueA);
	}
	else
	{
		MinResultR = Values[0].R;
		MaxResultR = Values[0].R;
		MinResultG = Values[0].G;
		MaxResultG = Values[0].G;
		MinResultB = Values[0].B;
		MaxResultB = Values[0].B;
		MinResultA = Values[0].A;
		MaxResultA = Values[0].A;
	}

	for (uniform int32 Index = AlignedNum; Index < Num; Index++)
	{
		const uniform FColor Value = Values[Index];
		MinResultR = min(MinResultR, Value.R);
		MaxResultR = max(MaxResultR, Value.R);
		MinResultG = min(MinResultG, Value.G);
		MaxResultG = max(MaxResultG, Value.G);
		MinResultB = min(MinResultB, Value.B);
		MaxResultB = max(MaxResultB, Value.B);
		MinResultA = min(MinResultA, Value.A);
		MaxResultA = max(MaxResultA, Value.A);
	}

	OutMin->R = MinResultR;
	OutMax->R = MaxResultR;
	OutMin->G = MinResultG;
	OutMax->G = MaxResultG;
	OutMin->B = MinResultB;
	OutMax->B = MaxResultB;
	OutMin->A = MinResultA;
	OutMax->A = MaxResultA;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

export void ArrayUtilities_GetMinMaxSafe_float(
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

export void ArrayUtilities_GetMinMaxSafe_double(
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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

export void ArrayUtilities_MakeOctahedrons_AOS(
	const uniform float3 Vectors[],
	uniform FVoxelOctahedron Octahedrons[],
	const uniform int32 Num)
{
	FOREACH(Index, 0, Num)
	{
		IGNORE_PERF_WARNING
		Octahedrons[Index] = MakeOctahedron(UnitVectorToOctahedron(normalize(Vectors[Index])));
	}
}

export void ArrayUtilities_MakeOctahedrons_SOA(
	const uniform float X[],
	const uniform float Y[],
	const uniform float Z[],
	uniform FVoxelOctahedron Octahedrons[],
	const uniform int32 Num)
{
	FOREACH(Index, 0, Num)
	{
		IGNORE_PERF_WARNING
		Octahedrons[Index] = MakeOctahedron(UnitVectorToOctahedron(normalize(MakeFloat3(
			X[Index],
			Y[Index],
			Z[Index]))));
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

export void ArrayUtilities_FixupSignBit(
	uniform float Values[],
	const uniform int32 Num)
{
	FOREACH(Index, 0, Num)
	{
		if (intbits(Values[Index]) == 0x80000000)
		{
			Values[Index] = 0;
		}
	}
}