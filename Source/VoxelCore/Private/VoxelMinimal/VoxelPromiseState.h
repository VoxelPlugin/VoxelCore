// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelTaskContext.h"

class FVoxelPromiseState : public IVoxelPromiseState
{
public:
	struct FContinuation
	{
	public:
		enum class EType : uint8
		{
			Future,
			VoidLambda,
			ValueLambda
		};

		const EVoxelFutureThread Thread;
		const EType Type;
		TVoxelStaticArray<uint64, 2> Storage{ ForceInit };

		TUniquePtr<FContinuation> NextContinuation;

	public:
		FORCEINLINE explicit FContinuation(const FVoxelFuture& Future)
			: Thread(EVoxelFutureThread::AnyThread)
			, Type(EType::Future)
		{
			new(&Storage[0]) TVoxelRefCountPtr<FVoxelPromiseState>(Future.PromiseState);
		}
		FORCEINLINE FContinuation(
			const EVoxelFutureThread Thread,
			TVoxelUniqueFunction<void()> Lambda)
			: Thread(Thread)
			, Type(EType::VoidLambda)
		{
			new(&Storage) TVoxelUniqueFunction<void()>(MoveTemp(Lambda));
		}
		FORCEINLINE FContinuation(
			const EVoxelFutureThread Thread,
			TVoxelUniqueFunction<void(const FSharedVoidRef&)> Lambda)
			: Thread(Thread)
			, Type(EType::ValueLambda)
		{
			new(&Storage) TVoxelUniqueFunction<void(const FSharedVoidRef&)>(MoveTemp(Lambda));
		}
		FORCEINLINE ~FContinuation()
		{
			switch (Type)
			{
			default: VOXEL_ASSUME(false);
			case EType::Future: GetFuture().~TVoxelRefCountPtr(); break;
			case EType::VoidLambda: GetVoidLambda().~TVoxelUniqueFunction(); break;
			case EType::ValueLambda: GetValueLambda().~TVoxelUniqueFunction(); break;
			}
		}

	public:
		FORCEINLINE TVoxelRefCountPtr<FVoxelPromiseState>& GetFuture()
		{
			checkVoxelSlow(Type == EType::Future);
			return ReinterpretCastRef<TVoxelRefCountPtr<FVoxelPromiseState>>(Storage[0]);
		}
		FORCEINLINE TVoxelUniqueFunction<void()>& GetVoidLambda()
		{
			checkVoxelSlow(Type == EType::VoidLambda);
			return ReinterpretCastRef<TVoxelUniqueFunction<void()>>(Storage);
		}
		FORCEINLINE TVoxelUniqueFunction<void(const FSharedVoidRef&)>& GetValueLambda()
		{
			checkVoxelSlow(Type == EType::ValueLambda);
			return ReinterpretCastRef<TVoxelUniqueFunction<void(const FSharedVoidRef&)>>(Storage);
		}

	public:
		void Execute(
			FVoxelTaskContext& Context,
			const FVoxelPromiseState& NewValue);
	};

public:
	const FVoxelTaskContextWeakRef ContextWeakRef;

	explicit FVoxelPromiseState(
		FVoxelTaskContext* ContextOverride,
		bool bHasValue);

	FORCEINLINE explicit FVoxelPromiseState(const FSharedVoidRef& NewValue)
		: IVoxelPromiseState(true)
	{
		ConstCast(ContextWeakRef) = FVoxelTaskScope::GetContext();

		bIsComplete.Set(true);
		Value = NewValue;
	}

	~FVoxelPromiseState();

public:
	void Set();
	void Set(const FSharedVoidRef& NewValue);
	void AddContinuation(TUniquePtr<FContinuation> Continuation);

private:
	TUniquePtr<FContinuation> Continuation_RequiresLock;

	void SetImpl(FVoxelTaskContext& Context);
};
checkStatic(sizeof(FVoxelPromiseState) == 48);
checkStatic(sizeof(FVoxelPromiseState::FContinuation) == 32);