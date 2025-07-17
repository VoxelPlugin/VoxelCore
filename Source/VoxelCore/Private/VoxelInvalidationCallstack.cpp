// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelInvalidationCallstack.h"

uint64 FVoxelInvalidationFrame_String::GetHash() const
{
	return FVoxelUtilities::HashString(String);
}

FString FVoxelInvalidationFrame_String::ToString() const
{
	return String;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

uint64 FVoxelInvalidationSource_Object::GetHash() const
{
	return FVoxelUtilities::MurmurHash(Object);
}

FString FVoxelInvalidationSource_Object::ToString() const
{
	return Object.GetReadableName();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if VOXEL_INVALIDATION_TRACKING
TSharedRef<FVoxelInvalidationCallstack> FVoxelInvalidationCallstack::Create(const FVoxelInvalidationFrame& This)
{
	// MakeSharedCopy isn't safe when exiting, as UStruct might have been destroyed already
	if (IsEngineExitRequested())
	{
		return MakeShareable(new FVoxelInvalidationCallstack(MakeShared<FVoxelInvalidationFrame_String>()));
	}

	const TSharedRef<FVoxelInvalidationCallstack> Result = MakeShareable(new FVoxelInvalidationCallstack(This.MakeSharedCopy()));

	if (const TSharedPtr<const FVoxelInvalidationCallstack> Callstack = FVoxelInvalidationScope::GetThreadCallstack())
	{
		Result->AddCaller(Callstack.ToSharedRef());
	}

	return Result;
}

TSharedRef<FVoxelInvalidationCallstack> FVoxelInvalidationCallstack::Create(const FString& String)
{
	FVoxelInvalidationFrame_String Frame;
	Frame.String = String;
	return Create(Frame);
}

TSharedRef<FVoxelInvalidationCallstack> FVoxelInvalidationCallstack::Create(const TVoxelObjectPtr<const UObject> Object)
{
	FVoxelInvalidationSource_Object Frame;
	Frame.Object = Object;
	return Create(Frame);
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if VOXEL_INVALIDATION_TRACKING
FString FVoxelInvalidationCallstack::ToString(const int32 Depth) const
{
	VOXEL_FUNCTION_COUNTER();

	FString Result = "\n";
	for (int32 Index = 0; Index < Depth; Index++)
	{
		Result += "  ";
	}

	if (Depth > 0)
	{
		Result += "-> ";
	}

	Result += Frame->ToString();
	Result += " ";

	while (Result.Len() < 120)
	{
		Result += " ";
	}
	Result += "Callstack: ";

	int32 NumFramesAdded = 0;
	for (const FString& StackFrame : FVoxelUtilities::StackFramesToString_WithStats(StackFrames, false))
	{
		if (NumFramesAdded == 0)
		{
			if (StackFrame.Contains("FVoxelInvalidation") ||
				StackFrame.Contains("FVoxelDependency"))
			{
				continue;
			}
		}

		Result += StackFrame + " ";
		NumFramesAdded++;

		if (NumFramesAdded > 5)
		{
			break;
		}
	}

	for (const TSharedRef<const FVoxelInvalidationCallstack>& Caller : Callers)
	{
		Result += Caller->ToString(Depth + 1);
	}

	if (Depth == 0)
	{
		Result += "\n\t";
	}

	return Result;
}

void FVoxelInvalidationCallstack::AddCaller(const TSharedRef<const FVoxelInvalidationCallstack>& Caller)
{
	VOXEL_FUNCTION_COUNTER();
	check(!CachedHash);

	const uint64 Hash = Caller->GetHash();

	for (const TSharedRef<const FVoxelInvalidationCallstack>& OtherCaller : Callers)
	{
		if (Hash == OtherCaller->GetHash())
		{
			return;
		}
	}

	Callers.Add(Caller);
}

void FVoxelInvalidationCallstack::ComputeHash() const
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelArray<uint64> Hashes;
	Hashes.Reserve(1 + Callers.Num());

	Hashes.Add(Frame->GetHash());

	for (const TSharedRef<const FVoxelInvalidationCallstack>& Caller : Callers)
	{
		Hashes.Add(Caller->GetHash());
	}

	ensure(!CachedHash);
	CachedHash = FVoxelUtilities::MurmurHashView(Hashes);
}

void FVoxelInvalidationCallstack::ForeachFrameImpl(
	const TFunctionRef<void(const FVoxelInvalidationFrame&, TConstVoxelArrayView<const FVoxelInvalidationFrame*>)> Lambda,
	TVoxelArray<const FVoxelInvalidationFrame*> Parents) const
{
	Lambda(*Frame, Parents);

	Parents.Add(&Frame.Get());

	for (const TSharedRef<const FVoxelInvalidationCallstack>& Caller : Callers)
	{
		Caller->ForeachFrameImpl(Lambda, Parents);
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if VOXEL_INVALIDATION_TRACKING
const uint32 GVoxelInvalidationScopeTLS = FPlatformTLS::AllocTlsSlot();

FVoxelInvalidationScope::FVoxelInvalidationScope(const TSharedRef<const FVoxelInvalidationCallstack>& Callstack)
	: Callstack(Callstack)
	, PreviousScope(static_cast<FVoxelInvalidationScope*>(FPlatformTLS::GetTlsValue(GVoxelInvalidationScopeTLS)))
{
	FPlatformTLS::SetTlsValue(GVoxelInvalidationScopeTLS, this);
}

FVoxelInvalidationScope::~FVoxelInvalidationScope()
{
	check(FPlatformTLS::GetTlsValue(GVoxelInvalidationScopeTLS) == this);
	FPlatformTLS::SetTlsValue(GVoxelInvalidationScopeTLS, PreviousScope);
}

TSharedPtr<const FVoxelInvalidationCallstack> FVoxelInvalidationScope::GetThreadCallstack()
{
	const FVoxelInvalidationScope* InvalidationScope = static_cast<FVoxelInvalidationScope*>(FPlatformTLS::GetTlsValue(GVoxelInvalidationScopeTLS));
	if (!InvalidationScope)
	{
		return {};
	}

	return InvalidationScope->Callstack;
}
#endif