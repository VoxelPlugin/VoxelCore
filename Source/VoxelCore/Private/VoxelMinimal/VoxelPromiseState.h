// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelTaskDispatcherInterface.h"

class FVoxelPromiseState
	: public IVoxelPromiseState
	, public TSharedFromThis<FVoxelPromiseState>
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
		TVoxelStaticArray<uint64, 2> Storage{ NoInit };

		TUniquePtr<FContinuation> NextContinuation;

	public:
		FORCEINLINE explicit FContinuation(const FVoxelFuture& Future)
			: Thread(EVoxelFutureThread::AnyThread)
			, Type(EType::Future)
		{
			new(&Storage) TSharedRef<FVoxelPromiseState>(StaticCastSharedRef<FVoxelPromiseState>(Future.PromiseState.ToSharedRef()));
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
			case EType::Future: GetFuture().~TSharedRef();
				break;
			case EType::VoidLambda: GetVoidLambda().~TVoxelUniqueFunction();
				break;
			case EType::ValueLambda: GetValueLambda().~TVoxelUniqueFunction();
				break;
			}
		}

	public:
		FORCEINLINE TSharedRef<FVoxelPromiseState>& GetFuture()
		{
			checkVoxelSlow(Type == EType::Future);
			return ReinterpretCastRef<TSharedRef<FVoxelPromiseState>>(Storage);
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
			IVoxelTaskDispatcher& Dispatcher,
			const FVoxelPromiseState& NewValue);
	};

public:
	const FVoxelTaskDispatcherRef DispatcherRef;

	explicit FVoxelPromiseState(
		IVoxelTaskDispatcher* DispatcherOverride,
		bool bHasValue);

	FORCEINLINE explicit FVoxelPromiseState(const FSharedVoidRef& NewValue)
		: IVoxelPromiseState(true)
	{
		ConstCast(DispatcherRef) = FVoxelTaskDispatcherScope::Get();

		bIsComplete.Set(true);
		Value = NewValue;
	}

	~FVoxelPromiseState();

	VOXEL_COUNT_INSTANCES();

public:
	void Set();
	void Set(const FSharedVoidRef& NewValue);
	void AddContinuation(TUniquePtr<FContinuation> Continuation);

private:
	int32 StackIndex = -1;
	TUniquePtr<FContinuation> Continuation_RequiresLock;

	void SetImpl(IVoxelTaskDispatcher& Dispatcher);
};
checkStatic(sizeof(FVoxelPromiseState) == 64);
checkStatic(sizeof(FVoxelPromiseState::FContinuation) == 32);