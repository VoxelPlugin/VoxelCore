// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelInvalidationCallstack.generated.h"

USTRUCT()
struct VOXELCORE_API FVoxelInvalidationFrame : public FVoxelVirtualStruct
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

	virtual uint64 GetHash() const VOXEL_PURE_VIRTUAL({});
	virtual FString ToString() const VOXEL_PURE_VIRTUAL({});
};

USTRUCT()
struct VOXELCORE_API FVoxelInvalidationFrame_String : public FVoxelInvalidationFrame
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

	FString String;

	//~ Begin FVoxelInvalidationSource Interface
	virtual uint64 GetHash() const override;
	virtual FString ToString() const override;
	//~ End FVoxelInvalidationSource Interface
};

USTRUCT()
struct VOXELCORE_API FVoxelInvalidationSource_Object : public FVoxelInvalidationFrame
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

	TVoxelObjectPtr<const UObject> Object;

	//~ Begin FVoxelInvalidationSource Interface
	virtual uint64 GetHash() const override;
	virtual FString ToString() const override;
	//~ End FVoxelInvalidationSource Interface
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if VOXEL_INVALIDATION_TRACKING
class VOXELCORE_API FVoxelInvalidationCallstack : public TSharedFromThis<FVoxelInvalidationCallstack>
{
public:
	static TSharedRef<FVoxelInvalidationCallstack> Create(const FVoxelInvalidationFrame& This);
	static TSharedRef<FVoxelInvalidationCallstack> Create(const FString& String);
	static TSharedRef<FVoxelInvalidationCallstack> Create(TVoxelObjectPtr<const UObject> Object);

	UE_NONCOPYABLE(FVoxelInvalidationCallstack);

public:
	FORCEINLINE uint64 GetHash() const
	{
		if (!CachedHash)
		{
			ComputeHash();
		}
		return CachedHash.GetValue();
	}

	template<typename Type = FVoxelInvalidationFrame, typename LambdaType>
	requires LambdaHasSignature_V<LambdaType, void(const Type&, TConstVoxelArrayView<const FVoxelInvalidationFrame*>)>
	void ForeachFrame(LambdaType&& Lambda) const
	{
		this->ForeachFrameImpl([&](
			const FVoxelInvalidationFrame& Frame,
			const TConstVoxelArrayView<const FVoxelInvalidationFrame*> Parents)
		{
			if (const Type* TypedFrame = CastStruct<Type>(Frame))
			{
				Lambda(*TypedFrame, Parents);
			}
		},
		{});
	}

	FString ToString(int32 Depth = 0) const;
	void AddCaller(const TSharedRef<const FVoxelInvalidationCallstack>& Caller);

private:
	void ComputeHash() const;

	void ForeachFrameImpl(
		TFunctionRef<void(const FVoxelInvalidationFrame&, TConstVoxelArrayView<const FVoxelInvalidationFrame*>)> Lambda,
		TVoxelArray<const FVoxelInvalidationFrame*> Parents) const;

	const TSharedRef<const FVoxelInvalidationFrame> Frame;
	const FVoxelStackFrames StackFrames = FVoxelUtilities::GetStackFrames_WithStats(3);
	TVoxelArray<TSharedRef<const FVoxelInvalidationCallstack>> Callers;

	mutable TVoxelOptional<uint64> CachedHash;

	explicit FVoxelInvalidationCallstack(const TSharedRef<const FVoxelInvalidationFrame>& Frame)
		: Frame(Frame)
	{
	}
};
#else
class FVoxelInvalidationCallstack
{
public:
	template<typename Type>
	FORCEINLINE static TSharedRef<FVoxelInvalidationCallstack> Create(Type&&)
	{
		return MakeShared<FVoxelInvalidationCallstack>();
	}
	FORCEINLINE void AddCaller(const TSharedRef<const FVoxelInvalidationCallstack>& Caller)
	{
	}

	FString ToString() const
	{
		return "<no debug data, set VOXEL_INVALIDATION_TRACKING to 1>";
	}
};
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if VOXEL_INVALIDATION_TRACKING
class VOXELCORE_API FVoxelInvalidationScope
{
public:
	explicit FVoxelInvalidationScope(const TSharedRef<const FVoxelInvalidationCallstack>& Callstack);

	template<typename T>
	requires
	(
		sizeof(decltype(FVoxelInvalidationCallstack::Create(std::declval<T>()))) > 0
	)
	explicit FVoxelInvalidationScope(const T& Value)
		: FVoxelInvalidationScope(FVoxelInvalidationCallstack::Create(Value))
	{
	}

	~FVoxelInvalidationScope();
	UE_NONCOPYABLE(FVoxelInvalidationScope);

public:
	TSharedRef<const FVoxelInvalidationCallstack> GetCallstack() const
	{
		return Callstack;
	}

	static TSharedPtr<const FVoxelInvalidationCallstack> GetThreadCallstack();

private:
	const TSharedRef<const FVoxelInvalidationCallstack> Callstack;
	FVoxelInvalidationScope* const PreviousScope;

	friend FVoxelInvalidationCallstack;
};
#else
class FVoxelInvalidationScope
{
public:
	template<typename Type>
	FORCEINLINE explicit FVoxelInvalidationScope(Type&&)
	{
	}

	FORCEINLINE TSharedRef<const FVoxelInvalidationCallstack> GetCallstack() const
	{
		return MakeShared<FVoxelInvalidationCallstack>();
	}

#if INTELLISENSE_PARSER
	~FVoxelInvalidationScope();
#endif
};
#endif