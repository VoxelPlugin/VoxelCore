// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeExit.h"
#include "UObject/SoftObjectPtr.h"

#ifndef INTELLISENSE_PARSER
#if defined(__INTELLISENSE__) || defined(__RESHARPER__)
#define INTELLISENSE_PARSER 1
#else
#define INTELLISENSE_PARSER 0
#endif
#endif

#ifndef VOXEL_DEV_WORKFLOW
#define VOXEL_DEV_WORKFLOW 0
#endif

#ifndef VOXEL_DEBUG
#define VOXEL_DEBUG DO_CHECK
#endif

#if INTELLISENSE_PARSER
#define VOXEL_DEBUG 1
#define INTELLISENSE_ONLY(...) __VA_ARGS__
#define INTELLISENSE_SKIP(...)
#define INTELLISENSE_SWITCH(True, False) True

// Don't tag unused variables as errors
#pragma warning(default: 4101)

// Needed for Resharper to detect the printf hidden in the lambda
#undef UE_LOG
#define UE_LOG(CategoryName, Verbosity, Format, ...) \
	{ \
		(void)ELogVerbosity::Verbosity; \
		(void)(FLogCategoryBase*)&CategoryName; \
		FString::Printf(Format, ##__VA_ARGS__); \
	}

// Syntax highlighting is wrong otherwise
#undef TEXTVIEW
#define TEXTVIEW(String) FStringView(TEXT(String))

#undef offsetof
#define offsetof __builtin_offsetof

#error "Compiler defined as parser"
#else
#define INTELLISENSE_ONLY(...)
#define INTELLISENSE_SKIP(...) __VA_ARGS__
#define INTELLISENSE_SWITCH(True, False) False
#endif

// This is defined in the generated.h. It lets you use GetOuterASomeOuter. Resharper/intellisense are confused when it's used, so define it for them
#define INTELLISENSE_DECLARE_WITHIN(Name) INTELLISENSE_ONLY(DECLARE_WITHIN(Name))

#define INTELLISENSE_PRINTF(Format, ...) INTELLISENSE_ONLY((void)FString::Printf(TEXT(Format), __VA_ARGS__);)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if VOXEL_DEBUG
#define checkVoxelSlow(x) check(x)
#define checkfVoxelSlow(x, ...) checkf(x, ##__VA_ARGS__)
#define ensureVoxelSlow(x) ensure(x)
#define ensureMsgfVoxelSlow(x, ...) ensureMsgf(x, ##__VA_ARGS__)
#define ensureVoxelSlowNoSideEffects(x) ensure(x)
#define ensureMsgfVoxelSlowNoSideEffects(x, ...) ensureMsgf(x, ##__VA_ARGS__)
#define VOXEL_ASSUME(...) check(__VA_ARGS__)
#define VOXEL_DEBUG_ONLY(...) __VA_ARGS__
#else
#define checkVoxelSlow(x)
#define checkfVoxelSlow(x, ...)
#define ensureVoxelSlow(x) (!!(x))
#define ensureMsgfVoxelSlow(x, ...) (!!(x))
#define ensureVoxelSlowNoSideEffects(x)
#define ensureMsgfVoxelSlowNoSideEffects(...)
#define VOXEL_ASSUME(...) UE_ASSUME(__VA_ARGS__)
#define VOXEL_DEBUG_ONLY(...)
#endif

#if VOXEL_DEBUG && VOXEL_DEV_WORKFLOW
#undef FORCEINLINE
#define FORCEINLINE inline
#endif

class VOXELCORE_API FVoxelGCScopeGuard
{
public:
	FVoxelGCScopeGuard();
	~FVoxelGCScopeGuard();

private:
	struct FImpl;

	TPimplPtr<FImpl> Impl;
};

VOXELCORE_API bool Voxel_CanAccessUObject();

#if VOXEL_DEBUG
#define checkUObjectAccess() ensure(Voxel_CanAccessUObject())
#else
#define checkUObjectAccess()
#endif

#define checkStatic(...) static_assert(__VA_ARGS__, "Static assert failed: " #__VA_ARGS__)
#define checkfStatic(Expr, Error) static_assert(Expr, Error)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if RHI_RAYTRACING
#define RHI_RAYTRACING_ONLY(...) __VA_ARGS__
#else
#define RHI_RAYTRACING_ONLY(...)
#endif

#if VOXEL_DEBUG
#define VOXEL_STATIC_HELPER_CHECK() ON_SCOPE_EXIT { ensure(StaticRawValue != 0); }
#else
#define VOXEL_STATIC_HELPER_CHECK()
#endif

template<int32 IndexSize>
struct TVoxelBytesToUnsignedType;

template<> struct TVoxelBytesToUnsignedType<1>  { using Type = uint8; };
template<> struct TVoxelBytesToUnsignedType<2> { using Type = uint16; };
template<> struct TVoxelBytesToUnsignedType<4> { using Type = uint32; };
template<> struct TVoxelBytesToUnsignedType<8> { using Type = uint64; };

// Zero initialize static and check if it's 0
// That way the static TLS logic is optimized out and this is compiled to a single mov + test
// This works only if the initializer is deterministic & can be called multiple times
#define VOXEL_STATIC_HELPER(InType) \
	static TVoxelBytesToUnsignedType<sizeof(InType)>::Type StaticRawValue = 0; \
	VOXEL_STATIC_HELPER_CHECK(); \
	InType& StaticValue = ReinterpretCastRef<InType>(StaticRawValue); \
	if (!StaticRawValue)

template<typename T>
inline static FName GVoxelStaticName{ static_cast<T*>(nullptr)->operator()() };

template<typename T>
FORCEINLINE const FName& VoxelStaticName(const T&)
{
	return GVoxelStaticName<T>;
}

#define STATIC_FNAME(Name) VoxelStaticName([]{ return FName(Name); })

#define STATIC_FSTRING(String) \
	([]() -> const FString& \
	{ \
		static const FString StaticString = String; \
		return StaticString; \
	}())

#define STATIC_HASH(Name) \
	([]() -> uint64 \
	{ \
		VOXEL_STATIC_HELPER(uint64) \
		{ \
			StaticValue = FVoxelUtilities::HashString(TEXT(Name)); \
		} \
		return StaticValue; \
	}())

#define MAKE_TAG_64(Text) \
	[] \
	{ \
		checkStatic(sizeof(Text) == 9); \
		return \
			(uint64(Text[0]) << 0 * 8) | \
			(uint64(Text[1]) << 1 * 8) | \
			(uint64(Text[2]) << 2 * 8) | \
			(uint64(Text[3]) << 3 * 8) | \
			(uint64(Text[4]) << 4 * 8) | \
			(uint64(Text[5]) << 5 * 8) | \
			(uint64(Text[6]) << 6 * 8) | \
			(uint64(Text[7]) << 7 * 8); \
	}()

#define GET_MEMBER_NAME_STATIC(ClassName, MemberName) STATIC_FNAME(GET_MEMBER_NAME_STRING_CHECKED(ClassName, MemberName))
#define GET_OWN_MEMBER_NAME(MemberName) GET_MEMBER_NAME_CHECKED(std::decay_t<decltype(*this)>, MemberName)

#define VOXEL_GET_TYPE(Value) std::decay_t<decltype(Value)>
#define VOXEL_THIS_TYPE VOXEL_GET_TYPE(*this)
// This is needed in classes, where just doing class Name would fwd declare it in the class scope
#define VOXEL_FWD_DECLARE_CLASS(Name) void PREPROCESSOR_JOIN(__VoxelDeclareDummy_, __LINE__)(class Name*);

// This makes the macro parameter show up as a class in Resharper
#if INTELLISENSE_PARSER
#define VOXEL_FWD_DECLARE_CLASS_INTELLISENSE(Name) VOXEL_FWD_DECLARE_CLASS(Name)
#else
#define VOXEL_FWD_DECLARE_CLASS_INTELLISENSE(Name)
#endif

FORCEINLINE void VoxelConsoleVariable_CallOnChanged() {}
FORCEINLINE void VoxelConsoleVariable_CallOnChanged(TFunction<void()> OnChanged) { OnChanged(); }
FORCEINLINE void VoxelConsoleVariable_CallOnChanged(TFunction<void()> OnChanged, TFunction<void()> Tick) { OnChanged(); }

FORCEINLINE void VoxelConsoleVariable_CallTick() {}
FORCEINLINE void VoxelConsoleVariable_CallTick(TFunction<void()> OnChanged) {}
FORCEINLINE void VoxelConsoleVariable_CallTick(TFunction<void()> OnChanged, TFunction<void()> Tick) { Tick(); }

namespace Voxel
{
	FORCEINLINE void Void()
	{
	}
}

struct VOXELCORE_API FVoxelConsoleVariableHelper
{
	explicit FVoxelConsoleVariableHelper(
		TFunction<void()> OnChanged,
		TFunction<void()> Tick);
};

#define VOXEL_CONSOLE_VARIABLE(Api, Type, Name, Default, Command, Description, ...) \
	Api Type Name = Default; \
	static FAutoConsoleVariableRef CVar_ ## Name( \
		TEXT(Command),  \
		Name,  \
		TEXT(Description)); \
	\
	static const FVoxelConsoleVariableHelper PREPROCESSOR_JOIN(VoxelConsoleVariableHelper, __COUNTER__)([] \
	{ \
		static Type LastValue = Default; \
		if (LastValue != Name) \
		{ \
			LastValue = Name; \
			VoxelConsoleVariable_CallOnChanged(__VA_ARGS__); \
		} \
	}, \
	[] \
	{ \
		VoxelConsoleVariable_CallTick(__VA_ARGS__); \
	});

#define VOXEL_CONSOLE_COMMAND(Command, Description) \
	static void VoxelConsoleCommand(TVoxelCounterDummy<__COUNTER__>, const TArray<FString>& Args); \
	static FAutoConsoleCommand PREPROCESSOR_JOIN(VoxelAutoCmd, __COUNTER__)( \
	    TEXT(Command), \
	    TEXT(Description), \
		MakeLambdaDelegate([](const TArray<FString>& Args) \
		{ \
			static_assert(sizeof(Description) > 1, "Missing description"); \
			VOXEL_SCOPE_COUNTER(Command); \
			VoxelConsoleCommand(TVoxelCounterDummy<__COUNTER__ - 2>(), Args); \
		})); \
	\
	static void VoxelConsoleCommand(TVoxelCounterDummy<__COUNTER__ - 3>, const TArray<FString>& Args)

#define VOXEL_CONSOLE_WORLD_COMMAND(Command, Description) \
	static void VoxelConsoleCommand(TVoxelCounterDummy<__COUNTER__>, const TArray<FString>& Args, UWorld* World); \
	static FAutoConsoleCommand PREPROCESSOR_JOIN(VoxelAutoCmd, __COUNTER__)( \
	    TEXT(Command), \
	    TEXT(Description), \
		MakeLambdaDelegate([](const TArray<FString>& Args, UWorld* World, FOutputDevice&) \
		{ \
			VOXEL_SCOPE_COUNTER(Command); \
			VoxelConsoleCommand(TVoxelCounterDummy<__COUNTER__ - 2>(), Args, World); \
		})); \
	\
	static void VoxelConsoleCommand(TVoxelCounterDummy<__COUNTER__ - 3>, const TArray<FString>& Args, UWorld* World)

#define VOXEL_EXPAND(X) X

#define VOXEL_APPEND_LINE(X) PREPROCESSOR_JOIN(X, __LINE__)

#define ON_SCOPE_EXIT_IMPL(Suffix) const auto PREPROCESSOR_JOIN(PREPROCESSOR_JOIN(ScopeGuard_, __LINE__), Suffix) = ::ScopeExitSupport::FScopeGuardSyntaxSupport() + [&]()

// Unlike GENERATE_MEMBER_FUNCTION_CHECK, this supports inheritance
// However, it doesn't do any signature check
#define GENERATE_VOXEL_MEMBER_FUNCTION_CHECK(MemberName) \
template<typename T> \
struct THasMemberFunction_ ## MemberName \
{ \
private: \
	template<typename OtherType> \
	static int16 Test(decltype(&OtherType::MemberName)); \
	template<typename> \
	static int32 Test(...); \
\
public: \
	static constexpr bool Value = sizeof(Test<T>(nullptr)) == sizeof(int16); \
};

#define VOXEL_FOLD_EXPRESSION(...) \
	{ \
		int32 Temp[] = { 0, ((__VA_ARGS__), 0)... }; \
		(void)Temp; \
	}

#if !IS_MONOLITHIC
#define VOXEL_ISPC_ASSERT() \
 	extern "C" void VoxelISPC_Assert(const int32 Line) \
 	{ \
 		ensureAlwaysMsgf(false, TEXT("ISPC LINE: %d"), Line); \
 	}
#else
#define VOXEL_ISPC_ASSERT()
#endif

#define VOXEL_DEFAULT_MODULE(Name) \
 	IMPLEMENT_MODULE(FDefaultModuleImpl, Name) \
 	VOXEL_ISPC_ASSERT()

#define VOXEL_PURE_VIRTUAL(...) { ensureMsgf(false, TEXT("Pure virtual %s called"), *FString(__FUNCTION__)); return __VA_ARGS__; }

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

extern VOXELCORE_API bool GIsVoxelCoreModuleLoaded;
extern VOXELCORE_API FSimpleMulticastDelegate GOnVoxelModuleUnloaded_DoCleanup;
extern VOXELCORE_API FSimpleMulticastDelegate GOnVoxelModuleUnloaded;

enum class EVoxelRunOnStartupPhase
{
	Game,
	Editor,
	EditorCommandlet
};
struct VOXELCORE_API FVoxelRunOnStartupPhaseHelper
{
	FVoxelRunOnStartupPhaseHelper(EVoxelRunOnStartupPhase Phase, int32 Priority, TFunction<void()> Lambda);
};

template<int32>
struct TVoxelCounterDummy
{
	TVoxelCounterDummy() = default;
};

#define VOXEL_RUN_ON_STARTUP(Phase, Priority) \
	static void VoxelStartupFunction(TVoxelCounterDummy<__COUNTER__>); \
	static const FVoxelRunOnStartupPhaseHelper PREPROCESSOR_JOIN(VoxelRunOnStartupPhaseHelper, __COUNTER__)(EVoxelRunOnStartupPhase::Phase, Priority, [] \
	{ \
		VoxelStartupFunction(TVoxelCounterDummy<__COUNTER__ - 2>()); \
	}); \
	static void VoxelStartupFunction(TVoxelCounterDummy<__COUNTER__ - 3>)

#define VOXEL_RUN_ON_STARTUP_GAME() VOXEL_RUN_ON_STARTUP(Game, 0)
// Will only run if GIsEditor is true
#define VOXEL_RUN_ON_STARTUP_EDITOR() VOXEL_RUN_ON_STARTUP(Editor, 0)
// Will run before any package PostLoad to work for cooker
// Also useful to register editor styles for doc commandlet
#define VOXEL_RUN_ON_STARTUP_EDITOR_COMMANDLET() VOXEL_RUN_ON_STARTUP(EditorCommandlet, 0)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace Voxel::Internal
{
	struct FOnConstruct
	{
		template<typename LambdaType>
		FOnConstruct operator+(LambdaType&& Lambda)
		{
			Lambda();
			return {};
		}
	};
}

#define VOXEL_ON_CONSTRUCT() Voxel::Internal::FOnConstruct VOXEL_APPEND_LINE(__OnConstruct) = Voxel::Internal::FOnConstruct() + [this]

#define INITIALIZATION_LAMBDA static const Voxel::Internal::FOnConstruct PREPROCESSOR_JOIN(__UniqueId, __COUNTER__) = Voxel::Internal::FOnConstruct() + []

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define VOXEL_SLATE_ARGS() \
	struct FArguments; \
	using WidgetArgsType = FArguments; \
	struct FDummyArguments \
	{ \
		using FArguments = FArguments; \
	}; \
	\
	struct FArguments : public TSlateBaseNamedArgs<FDummyArguments>

// Useful for templates
#define VOXEL_WRAP(...) __VA_ARGS__

#define UE_NONCOPYABLE_MOVEABLE(TypeName) \
	TypeName(TypeName&&) = default; \
	TypeName& operator=(TypeName&&) = default; \
	TypeName(const TypeName&) = delete; \
	TypeName& operator=(const TypeName&) = delete;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FVoxelLambdaCaller
{
	template<typename T>
	FORCEINLINE auto operator+(T&& Lambda) -> decltype(auto)
	{
		return Lambda();
	}
};

#define INLINE_LAMBDA FVoxelLambdaCaller() + [&]()

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// UniqueClass: to forbid copying ids from different classes
template<typename UniqueClass>
class TVoxelUniqueId
{
public:
	TVoxelUniqueId() = default;

	FORCEINLINE bool IsValid() const { return Id != 0; }

	FORCEINLINE bool operator==(const TVoxelUniqueId& Other) const { return Id == Other.Id; }

	FORCEINLINE bool operator<(const TVoxelUniqueId& Other) const { return Id < Other.Id; }
	FORCEINLINE bool operator>(const TVoxelUniqueId& Other) const { return Id > Other.Id; }

	FORCEINLINE bool operator<=(const TVoxelUniqueId& Other) const { return Id <= Other.Id; }
	FORCEINLINE bool operator>=(const TVoxelUniqueId& Other) const { return Id >= Other.Id; }

	FORCEINLINE friend uint32 GetTypeHash(const TVoxelUniqueId UniqueId)
	{
		return uint32(UniqueId.Id);
	}

	FORCEINLINE uint64 GetId() const
	{
		return Id;
	}

	FORCEINLINE static TVoxelUniqueId New()
	{
		return TVoxelUniqueId(TVoxelUniqueId_MakeNew(static_cast<UniqueClass*>(nullptr)));
	}

private:
	FORCEINLINE TVoxelUniqueId(const uint64 Id)
		: Id(Id)
	{
		ensureVoxelSlow(IsValid());
	}

	uint64 Id = 0;
};

#define DECLARE_UNIQUE_VOXEL_ID_EXPORT(Api, Name) \
	Api uint64 TVoxelUniqueId_MakeNew(class __ ## Name ##_Unique*); \
	using Name = TVoxelUniqueId<class __ ## Name ##_Unique>;

#define DECLARE_UNIQUE_VOXEL_ID(Name) DECLARE_UNIQUE_VOXEL_ID_EXPORT(,Name)

#define DEFINE_UNIQUE_VOXEL_ID(Name) \
	INTELLISENSE_ONLY(void VOXEL_APPEND_LINE(Dummy)(Name);) \
	uint64 TVoxelUniqueId_MakeNew(class __ ## Name ##_Unique*) \
	{ \
		static FVoxelCounter64 Counter; \
		return Counter.Increment_ReturnNew(); \
	}

template<typename T>
class TVoxelIndex
{
public:
	TVoxelIndex() = default;

	FORCEINLINE bool IsValid() const
	{
		return Index != -1;
	}
	FORCEINLINE operator bool() const
	{
		return IsValid();
	}

	FORCEINLINE bool operator==(const TVoxelIndex& Other) const
	{
		return Index == Other.Index;
	}

	FORCEINLINE friend uint32 GetTypeHash(const TVoxelIndex InIndex)
	{
		return InIndex.Index;
	}

protected:
	int32 Index = -1;

	FORCEINLINE TVoxelIndex(const int32 Index)
		: Index(Index)
	{
	}

	FORCEINLINE operator int32() const
	{
		return Index;
	}

	friend T;
};
static_assert(sizeof(TVoxelIndex<class FVoxelIndexDummy>) == sizeof(int32), "");

#define DECLARE_VOXEL_INDEX(Name, FriendClass) using Name = TVoxelIndex<class FriendClass>;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename ToType, typename FromType>
constexpr bool CanReinterpretCast_V =
	sizeof(ToType) == sizeof(FromType) &&
	// See https://blog.quarkslab.com/unaligned-accesses-in-cc-what-why-and-solutions-to-do-it-properly.html
	alignof(ToType) <= alignof(FromType);

template<typename ToType, typename FromType>
requires CanReinterpretCast_V<ToType, FromType>
FORCEINLINE ToType* ReinterpretCastPtr(FromType* From)
{
	return reinterpret_cast<ToType*>(From);
}
template<typename ToType, typename FromType>
requires CanReinterpretCast_V<ToType, FromType>
FORCEINLINE const ToType* ReinterpretCastPtr(const FromType* From)
{
	return reinterpret_cast<const ToType*>(From);
}

template<typename ToType, typename FromType>
requires CanReinterpretCast_V<ToType, FromType>
FORCEINLINE ToType& ReinterpretCastRef(FromType& From)
{
	return reinterpret_cast<ToType&>(From);
}
template<typename ToType, typename FromType>
requires CanReinterpretCast_V<ToType, FromType>
FORCEINLINE const ToType& ReinterpretCastRef(const FromType& From)
{
	return reinterpret_cast<const ToType&>(From);
}

template<typename ToType, typename FromType>
requires
(
	sizeof(ToType) == sizeof(FromType) &&
	// Should use regular ReinterpretCastRef if true
	!CanReinterpretCast_V<ToType, FromType> &&
	std::is_trivially_destructible_v<ToType> &&
	std::is_trivially_destructible_v<FromType>
)
FORCEINLINE const ToType ReinterpretCastRef_Unaligned(const FromType& From)
{
	// Memcpy will be optimized away on x86 (unaligned loads allowed)
	// Memcpy will be replaced by multiple aligned loads on arm (unaligned loads not allowed)
	// See https://godbolt.org/z/5hbhj48sP

	ToType Result;
	memcpy(&Result, &From, sizeof(ToType));
	return Result;
}

template<typename ToType, typename FromType>
requires
(
	CanReinterpretCast_V<ToType, FromType> &&
	!std::is_reference_v<FromType>
)
FORCEINLINE ToType&& ReinterpretCastRef(FromType&& From)
{
	return reinterpret_cast<ToType&&>(From);
}

template<typename ToType, typename FromType>
requires
(
	CanReinterpretCast_V<ToType, FromType> &&
	std::is_const_v<FromType> == std::is_const_v<ToType>
)
FORCEINLINE TSharedPtr<ToType>& ReinterpretCastSharedPtr(TSharedPtr<FromType>& From)
{
	return ReinterpretCastRef<TSharedPtr<ToType>>(From);
}

template<typename ToType, typename FromType>
requires
(
	CanReinterpretCast_V<ToType, FromType> &&
	std::is_const_v<FromType> == std::is_const_v<ToType>
)
FORCEINLINE TSharedRef<ToType>& ReinterpretCastSharedPtr(TSharedRef<FromType>& From)
{
	return ReinterpretCastRef<TSharedRef<ToType>>(From);
}

template<typename ToType, typename FromType>
requires
(
	CanReinterpretCast_V<ToType, FromType> &&
	std::is_const_v<FromType> == std::is_const_v<ToType>
)
FORCEINLINE const TSharedPtr<ToType>& ReinterpretCastSharedPtr(const TSharedPtr<FromType>& From)
{
	return ReinterpretCastRef<TSharedPtr<ToType>>(From);
}

template<typename ToType, typename FromType>
requires
(
	CanReinterpretCast_V<ToType, FromType> &&
	std::is_const_v<FromType> == std::is_const_v<ToType>
)
FORCEINLINE const TSharedRef<ToType>& ReinterpretCastSharedRef(const TSharedRef<FromType>& From)
{
	return ReinterpretCastRef<TSharedRef<ToType>>(From);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename ToType, typename FromType, typename Allocator>
requires CanReinterpretCast_V<ToType, FromType>
FORCEINLINE TArray<ToType, Allocator>& ReinterpretCastArray(TArray<FromType, Allocator>& Array)
{
	return reinterpret_cast<TArray<ToType, Allocator>&>(Array);
}
template<typename ToType, typename FromType, typename Allocator>
requires CanReinterpretCast_V<ToType, FromType>
FORCEINLINE const TArray<ToType, Allocator>& ReinterpretCastArray(const TArray<FromType, Allocator>& Array)
{
	return reinterpret_cast<const TArray<ToType, Allocator>&>(Array);
}

template<typename ToType, typename FromType, typename Allocator>
requires CanReinterpretCast_V<ToType, FromType>
FORCEINLINE TArray<ToType, Allocator>&& ReinterpretCastArray(TArray<FromType, Allocator>&& Array)
{
	return reinterpret_cast<TArray<ToType, Allocator>&&>(Array);
}
template<typename ToType, typename FromType, typename Allocator>
requires CanReinterpretCast_V<ToType, FromType>
FORCEINLINE const TArray<ToType, Allocator>&& ReinterpretCastArray(const TArray<FromType, Allocator>&& Array)
{
	return reinterpret_cast<const TArray<ToType, Allocator>&&>(Array);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename ToType, typename ToAllocator, typename FromType, typename Allocator>
requires (sizeof(FromType) != sizeof(ToType))
FORCEINLINE TArray<ToType, ToAllocator> ReinterpretCastArray_Copy(const TArray<FromType, Allocator>& Array)
{
	const int64 NumBytes = Array.Num() * sizeof(FromType);
	check(NumBytes % sizeof(ToType) == 0);
	return TArray<ToType, Allocator>(reinterpret_cast<const ToType*>(Array.GetData()), NumBytes / sizeof(ToType));
}
template<typename ToType, typename FromType, typename Allocator>
requires (sizeof(FromType) != sizeof(ToType))
FORCEINLINE TArray<ToType, Allocator> ReinterpretCastArray_Copy(const TArray<FromType, Allocator>& Array)
{
	return ReinterpretCastArray_Copy<ToType, Allocator>(Array);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename ToType, typename FromType, typename Allocator>
requires CanReinterpretCast_V<ToType, FromType>
FORCEINLINE TSet<ToType, DefaultKeyFuncs<ToType>, Allocator>& ReinterpretCastSet(TSet<FromType, DefaultKeyFuncs<FromType>, Allocator>& Set)
{
	return reinterpret_cast<TSet<ToType, DefaultKeyFuncs<ToType>, Allocator>&>(Set);
}
template<typename ToType, typename FromType, typename Allocator>
requires CanReinterpretCast_V<ToType, FromType>
FORCEINLINE const TSet<ToType, DefaultKeyFuncs<ToType>, Allocator>& ReinterpretCastSet(const TSet<FromType, DefaultKeyFuncs<FromType>, Allocator>& Set)
{
	return reinterpret_cast<const TSet<ToType, DefaultKeyFuncs<ToType>, Allocator>&>(Set);
}

template<typename ToType, typename FromType, typename Allocator>
requires CanReinterpretCast_V<ToType, FromType>
FORCEINLINE TSet<ToType, DefaultKeyFuncs<ToType>, Allocator>&& ReinterpretCastSet(TSet<FromType, DefaultKeyFuncs<FromType>, Allocator>&& Set)
{
	return reinterpret_cast<TSet<ToType, DefaultKeyFuncs<ToType>, Allocator>&&>(Set);
}
template<typename ToType, typename FromType, typename Allocator>
requires CanReinterpretCast_V<ToType, FromType>
FORCEINLINE const TSet<ToType, DefaultKeyFuncs<ToType>, Allocator>&& ReinterpretCastSet(const TSet<FromType, DefaultKeyFuncs<FromType>, Allocator>&& Set)
{
	return reinterpret_cast<const TSet<ToType, DefaultKeyFuncs<ToType>, Allocator>&&>(Set);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
struct TVoxelConstCast
{
	FORCEINLINE static T& ConstCast(T& Value)
	{
		return Value;
	}
};

template<typename T>
struct TVoxelConstCast<const T*>
{
	FORCEINLINE static T*& ConstCast(const T*& Value)
	{
		return const_cast<T*&>(Value);
	}
};

template<typename T>
struct TVoxelConstCast<const T* const>
{
	FORCEINLINE static T*& ConstCast(const T* const& Value)
	{
		return const_cast<T*&>(Value);
	}
};

template<typename T>
struct TVoxelConstCast<const T>
{
	FORCEINLINE static T& ConstCast(const T& Value)
	{
		return const_cast<T&>(Value);
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
struct TVoxelConstCast<TSharedPtr<const T>>
{
	FORCEINLINE static TSharedPtr<T>& ConstCast(TSharedPtr<const T>& Value)
	{
		return ReinterpretCastRef<TSharedPtr<T>>(Value);
	}
};
template<typename T>
struct TVoxelConstCast<TSharedRef<const T>>
{
	FORCEINLINE static TSharedRef<T>& ConstCast(TSharedRef<const T>& Value)
	{
		return ReinterpretCastRef<TSharedRef<T>>(Value);
	}
};

template<typename T>
struct TVoxelConstCast<const TSharedPtr<const T>>
{
	FORCEINLINE static const TSharedPtr<T>& ConstCast(const TSharedPtr<const T>& Value)
	{
		return ReinterpretCastRef<TSharedPtr<T>>(Value);
	}
};
template<typename T>
struct TVoxelConstCast<const TSharedRef<const T>>
{
	FORCEINLINE static const TSharedRef<T>& ConstCast(const TSharedRef<const T>& Value)
	{
		return ReinterpretCastRef<TSharedRef<T>>(Value);
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
struct TVoxelConstCast<TObjectPtr<const T>>
{
	FORCEINLINE static TObjectPtr<T>& ConstCast(TObjectPtr<const T>& Value)
	{
		return ReinterpretCastRef<TObjectPtr<T>>(Value);
	}
};
template<typename T>
struct TVoxelConstCast<const TObjectPtr<const T>>
{
	FORCEINLINE static const TObjectPtr<T>& ConstCast(const TObjectPtr<const T>& Value)
	{
		return ReinterpretCastRef<TObjectPtr<T>>(Value);
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
FORCEINLINE auto ConstCast(T& Value) -> decltype(auto)
{
	return TVoxelConstCast<T>::ConstCast(Value);
}
template<typename T>
FORCEINLINE auto ConstCast(T&& Value) -> decltype(auto)
{
	return TVoxelConstCast<T>::ConstCast(Value);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
requires (!std::is_reference_v<T>)
FORCEINLINE T MakeCopy(T&& Data)
{
	return MoveTemp(Data);
}
template<typename T>
FORCEINLINE T MakeCopy(const T& Data)
{
	return Data;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
FORCEINLINE TSoftObjectPtr<T> MakeSoftObjectPtr(const FString& Path)
{
	return TSoftObjectPtr<T>(FSoftObjectPath(Path));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define __VOXEL_GET_NTH_ARG(Dummy, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, N, ...) VOXEL_EXPAND(N)

#define __VOXEL_FOREACH_IMPL_XX(Prefix, Last, Suffix)
#define __VOXEL_FOREACH_IMPL_00(Prefix, Last, Suffix, X)      Last(X)
#define __VOXEL_FOREACH_IMPL_01(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_00(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_02(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_01(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_03(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_02(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_04(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_03(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_05(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_04(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_06(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_05(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_07(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_06(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_08(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_07(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_09(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_08(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_10(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_09(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_11(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_10(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_12(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_11(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_13(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_12(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_14(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_13(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_15(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_14(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_16(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_15(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_17(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_16(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_18(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_17(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_19(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_18(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_20(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_19(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_21(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_20(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_22(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_21(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_23(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_22(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_24(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_23(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_25(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_24(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_26(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_25(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_27(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_26(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_28(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_27(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_29(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_28(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_30(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_29(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_31(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_30(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)
#define __VOXEL_FOREACH_IMPL_32(Prefix, Last, Suffix, X, ...) Prefix(X) VOXEL_EXPAND(__VOXEL_FOREACH_IMPL_31(Prefix, Last, Suffix, __VA_ARGS__)) Suffix(X)

#define VOXEL_FOREACH_IMPL(Prefix, Last, /* Suffix (passed in VA_ARGS), */ ...) \
	VOXEL_EXPAND(__VOXEL_GET_NTH_ARG(__VA_ARGS__, \
	__VOXEL_FOREACH_IMPL_32, \
	__VOXEL_FOREACH_IMPL_31, \
	__VOXEL_FOREACH_IMPL_30, \
	__VOXEL_FOREACH_IMPL_29, \
	__VOXEL_FOREACH_IMPL_28, \
	__VOXEL_FOREACH_IMPL_27, \
	__VOXEL_FOREACH_IMPL_26, \
	__VOXEL_FOREACH_IMPL_25, \
	__VOXEL_FOREACH_IMPL_24, \
	__VOXEL_FOREACH_IMPL_23, \
	__VOXEL_FOREACH_IMPL_22, \
	__VOXEL_FOREACH_IMPL_21, \
	__VOXEL_FOREACH_IMPL_20, \
	__VOXEL_FOREACH_IMPL_19, \
	__VOXEL_FOREACH_IMPL_18, \
	__VOXEL_FOREACH_IMPL_17, \
	__VOXEL_FOREACH_IMPL_16, \
	__VOXEL_FOREACH_IMPL_15, \
	__VOXEL_FOREACH_IMPL_14, \
	__VOXEL_FOREACH_IMPL_13, \
	__VOXEL_FOREACH_IMPL_12, \
	__VOXEL_FOREACH_IMPL_11, \
	__VOXEL_FOREACH_IMPL_10, \
	__VOXEL_FOREACH_IMPL_09, \
	__VOXEL_FOREACH_IMPL_08, \
	__VOXEL_FOREACH_IMPL_07, \
	__VOXEL_FOREACH_IMPL_06, \
	__VOXEL_FOREACH_IMPL_05, \
	__VOXEL_FOREACH_IMPL_04, \
	__VOXEL_FOREACH_IMPL_03, \
	__VOXEL_FOREACH_IMPL_02, \
	__VOXEL_FOREACH_IMPL_01, \
	__VOXEL_FOREACH_IMPL_00, \
	__VOXEL_FOREACH_IMPL_XX)(Prefix, Last, __VA_ARGS__))

#define VOXEL_VOID_MACRO(...)

#define VOXEL_FOREACH(Macro, ...) VOXEL_FOREACH_IMPL(Macro, Macro, VOXEL_VOID_MACRO, ##__VA_ARGS__)
#define VOXEL_FOREACH_SUFFIX(Macro, ...) VOXEL_FOREACH_IMPL(VOXEL_VOID_MACRO, Macro, Macro, ##__VA_ARGS__)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define __VOXEL_FOREACH_ONE_ARG_XX(Macro, Arg)
#define __VOXEL_FOREACH_ONE_ARG_00(Macro, Arg, X)      Macro(Arg, X)
#define __VOXEL_FOREACH_ONE_ARG_01(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_00(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_02(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_01(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_03(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_02(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_04(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_03(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_05(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_04(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_06(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_05(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_07(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_06(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_08(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_07(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_09(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_08(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_10(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_09(Macro, Arg, __VA_ARGS__))

#define __VOXEL_FOREACH_ONE_ARG_11(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_10(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_12(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_11(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_13(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_12(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_14(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_13(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_15(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_14(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_16(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_15(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_17(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_16(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_18(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_17(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_19(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_18(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_20(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_19(Macro, Arg, __VA_ARGS__))

#define __VOXEL_FOREACH_ONE_ARG_21(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_20(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_22(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_21(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_23(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_22(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_24(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_23(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_25(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_24(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_26(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_25(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_27(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_26(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_28(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_27(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_29(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_28(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_30(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_29(Macro, Arg, __VA_ARGS__))

#define __VOXEL_FOREACH_ONE_ARG_31(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_30(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_32(Macro, Arg, X, ...) Macro(Arg, X) VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_31(Macro, Arg, __VA_ARGS__))

#define VOXEL_FOREACH_ONE_ARG(Macro, /* Arg (passed in VA_ARGS), */ ...) \
	VOXEL_EXPAND(__VOXEL_GET_NTH_ARG(__VA_ARGS__, \
	__VOXEL_FOREACH_ONE_ARG_32, \
	__VOXEL_FOREACH_ONE_ARG_31, \
	__VOXEL_FOREACH_ONE_ARG_30, \
	__VOXEL_FOREACH_ONE_ARG_29, \
	__VOXEL_FOREACH_ONE_ARG_28, \
	__VOXEL_FOREACH_ONE_ARG_27, \
	__VOXEL_FOREACH_ONE_ARG_26, \
	__VOXEL_FOREACH_ONE_ARG_25, \
	__VOXEL_FOREACH_ONE_ARG_24, \
	__VOXEL_FOREACH_ONE_ARG_23, \
	__VOXEL_FOREACH_ONE_ARG_22, \
	__VOXEL_FOREACH_ONE_ARG_21, \
	__VOXEL_FOREACH_ONE_ARG_20, \
	__VOXEL_FOREACH_ONE_ARG_19, \
	__VOXEL_FOREACH_ONE_ARG_18, \
	__VOXEL_FOREACH_ONE_ARG_17, \
	__VOXEL_FOREACH_ONE_ARG_16, \
	__VOXEL_FOREACH_ONE_ARG_15, \
	__VOXEL_FOREACH_ONE_ARG_14, \
	__VOXEL_FOREACH_ONE_ARG_13, \
	__VOXEL_FOREACH_ONE_ARG_12, \
	__VOXEL_FOREACH_ONE_ARG_11, \
	__VOXEL_FOREACH_ONE_ARG_10, \
	__VOXEL_FOREACH_ONE_ARG_09, \
	__VOXEL_FOREACH_ONE_ARG_08, \
	__VOXEL_FOREACH_ONE_ARG_07, \
	__VOXEL_FOREACH_ONE_ARG_06, \
	__VOXEL_FOREACH_ONE_ARG_05, \
	__VOXEL_FOREACH_ONE_ARG_04, \
	__VOXEL_FOREACH_ONE_ARG_03, \
	__VOXEL_FOREACH_ONE_ARG_02, \
	__VOXEL_FOREACH_ONE_ARG_01, \
	__VOXEL_FOREACH_ONE_ARG_00, \
	__VOXEL_FOREACH_ONE_ARG_XX)(Macro, __VA_ARGS__))

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define __VOXEL_FOREACH_COMMA_XX(Macro)
#define __VOXEL_FOREACH_COMMA_00(Macro, X)      Macro(X)
#define __VOXEL_FOREACH_COMMA_01(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_00(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_02(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_01(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_03(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_02(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_04(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_03(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_05(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_04(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_06(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_05(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_07(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_06(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_08(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_07(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_09(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_08(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_10(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_09(Macro, __VA_ARGS__))

#define __VOXEL_FOREACH_COMMA_11(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_10(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_12(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_11(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_13(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_12(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_14(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_13(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_15(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_14(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_16(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_15(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_17(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_16(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_18(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_17(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_19(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_18(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_20(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_19(Macro, __VA_ARGS__))

#define __VOXEL_FOREACH_COMMA_21(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_20(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_22(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_21(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_23(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_22(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_24(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_23(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_25(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_24(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_26(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_25(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_27(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_26(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_28(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_27(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_29(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_28(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_30(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_29(Macro, __VA_ARGS__))

#define __VOXEL_FOREACH_COMMA_31(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_30(Macro, __VA_ARGS__))
#define __VOXEL_FOREACH_COMMA_32(Macro, X, ...) Macro(X), VOXEL_EXPAND(__VOXEL_FOREACH_COMMA_31(Macro, __VA_ARGS__))

#define VOXEL_FOREACH_COMMA(/* Macro (passed in VA_ARGS), */ ...) \
	VOXEL_EXPAND(__VOXEL_GET_NTH_ARG(__VA_ARGS__, \
	__VOXEL_FOREACH_COMMA_32, \
	__VOXEL_FOREACH_COMMA_31, \
	__VOXEL_FOREACH_COMMA_30, \
	__VOXEL_FOREACH_COMMA_29, \
	__VOXEL_FOREACH_COMMA_28, \
	__VOXEL_FOREACH_COMMA_27, \
	__VOXEL_FOREACH_COMMA_26, \
	__VOXEL_FOREACH_COMMA_25, \
	__VOXEL_FOREACH_COMMA_24, \
	__VOXEL_FOREACH_COMMA_23, \
	__VOXEL_FOREACH_COMMA_22, \
	__VOXEL_FOREACH_COMMA_21, \
	__VOXEL_FOREACH_COMMA_20, \
	__VOXEL_FOREACH_COMMA_19, \
	__VOXEL_FOREACH_COMMA_18, \
	__VOXEL_FOREACH_COMMA_17, \
	__VOXEL_FOREACH_COMMA_16, \
	__VOXEL_FOREACH_COMMA_15, \
	__VOXEL_FOREACH_COMMA_14, \
	__VOXEL_FOREACH_COMMA_13, \
	__VOXEL_FOREACH_COMMA_12, \
	__VOXEL_FOREACH_COMMA_11, \
	__VOXEL_FOREACH_COMMA_10, \
	__VOXEL_FOREACH_COMMA_09, \
	__VOXEL_FOREACH_COMMA_08, \
	__VOXEL_FOREACH_COMMA_07, \
	__VOXEL_FOREACH_COMMA_06, \
	__VOXEL_FOREACH_COMMA_05, \
	__VOXEL_FOREACH_COMMA_04, \
	__VOXEL_FOREACH_COMMA_03, \
	__VOXEL_FOREACH_COMMA_02, \
	__VOXEL_FOREACH_COMMA_01, \
	__VOXEL_FOREACH_COMMA_00, \
	__VOXEL_FOREACH_COMMA_XX)(__VA_ARGS__))

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define __VOXEL_FOREACH_ONE_ARG_COMMA_XX(Macro, Arg)
#define __VOXEL_FOREACH_ONE_ARG_COMMA_00(Macro, Arg, X)      Macro(Arg, X)
#define __VOXEL_FOREACH_ONE_ARG_COMMA_01(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_00(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_02(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_01(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_03(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_02(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_04(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_03(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_05(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_04(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_06(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_05(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_07(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_06(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_08(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_07(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_09(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_08(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_10(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_09(Macro, Arg, __VA_ARGS__))

#define __VOXEL_FOREACH_ONE_ARG_COMMA_11(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_10(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_12(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_11(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_13(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_12(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_14(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_13(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_15(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_14(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_16(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_15(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_17(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_16(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_18(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_17(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_19(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_18(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_20(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_19(Macro, Arg, __VA_ARGS__))

#define __VOXEL_FOREACH_ONE_ARG_COMMA_21(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_20(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_22(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_21(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_23(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_22(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_24(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_23(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_25(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_24(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_26(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_25(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_27(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_26(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_28(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_27(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_29(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_28(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_30(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_29(Macro, Arg, __VA_ARGS__))

#define __VOXEL_FOREACH_ONE_ARG_COMMA_31(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_30(Macro, Arg, __VA_ARGS__))
#define __VOXEL_FOREACH_ONE_ARG_COMMA_32(Macro, Arg, X, ...) Macro(Arg, X), VOXEL_EXPAND(__VOXEL_FOREACH_ONE_ARG_COMMA_31(Macro, Arg, __VA_ARGS__))

#define VOXEL_FOREACH_ONE_ARG_COMMA(Macro, /* Arg (passed in VA_ARGS), */ ...) \
	VOXEL_EXPAND(__VOXEL_GET_NTH_ARG(__VA_ARGS__, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_32, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_31, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_30, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_29, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_28, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_27, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_26, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_25, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_24, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_23, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_22, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_21, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_20, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_19, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_18, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_17, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_16, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_15, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_14, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_13, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_12, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_11, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_10, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_09, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_08, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_07, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_06, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_05, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_04, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_03, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_02, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_01, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_00, \
	__VOXEL_FOREACH_ONE_ARG_COMMA_XX)(Macro, __VA_ARGS__))

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define DECLARE_VOXEL_VERSION(...) \
	struct \
	{ \
		enum Type : int32 \
		{ \
			__VA_ARGS__, \
			__VersionPlusOne, \
			LatestVersion = __VersionPlusOne - 1 \
		}; \
	}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
struct TVoxelUFunctionOverride
{
	struct FFrame : T
	{
		struct FCode
		{
			FORCEINLINE FCode operator+=(FCode) const { return {}; }
			FORCEINLINE FCode operator!() const { return {}; }
		};
		FCode Code;
	};

	using FNativeFuncPtr = void (*)(UObject* Context, FFrame& Stack, void* Result);

	struct FNameNativePtrPair
	{
		const char* NameUTF8;
		FNativeFuncPtr Pointer;
	};

	struct FNativeFunctionRegistrar
	{
		FNativeFunctionRegistrar(UClass* Class, const ANSICHAR* InName, const FNativeFuncPtr InPointer)
		{
			RegisterFunction(Class, InName, InPointer);
		}
		static void RegisterFunction(UClass* Class, const ANSICHAR* InName, const FNativeFuncPtr InPointer)
		{
			::FNativeFunctionRegistrar::RegisterFunction(Class, InName, reinterpret_cast<::FNativeFuncPtr>(InPointer));
		}
		static void RegisterFunction(UClass* Class, const WIDECHAR* InName, const FNativeFuncPtr InPointer)
		{
			::FNativeFunctionRegistrar::RegisterFunction(Class, InName, reinterpret_cast<::FNativeFuncPtr>(InPointer));
		}
		static void RegisterFunctions(UClass* Class, const FNameNativePtrPair* InArray, const int32 NumFunctions)
		{
			::FNativeFunctionRegistrar::RegisterFunctions(Class, reinterpret_cast<const ::FNameNativePtrPair*>(InArray), NumFunctions);
		}
	};
};

#define VOXEL_UFUNCTION_OVERRIDE(NewFrameClass) \
	using FFrame = TVoxelUFunctionOverride<NewFrameClass>::FFrame; \
	using FNativeFuncPtr = TVoxelUFunctionOverride<NewFrameClass>::FNativeFuncPtr; \
	using FNameNativePtrPair = TVoxelUFunctionOverride<NewFrameClass>::FNameNativePtrPair; \
	using FNativeFunctionRegistrar = TVoxelUFunctionOverride<NewFrameClass>::FNativeFunctionRegistrar;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Usage: DEFINE_PRIVATE_ACCESS(FMyClass, MyProperty) in global scope, then PrivateAccess::MyProperty(MyObject) from anywhere
#define DEFINE_PRIVATE_ACCESS(Class, Property) \
	namespace PrivateAccess \
	{ \
		template<typename> \
		struct TClass_ ## Property; \
		\
		template<> \
		struct TClass_ ## Property<Class> \
		{ \
			template<auto PropertyPtr> \
			struct TProperty_ ## Property \
			{ \
				friend auto& Property(Class& Object) \
				{ \
					return Object.*PropertyPtr; \
				} \
				friend auto& Property(const Class& Object) \
				{ \
					return Object.*PropertyPtr; \
				} \
			}; \
		}; \
		template struct TClass_ ## Property<Class>::TProperty_ ## Property<&Class::Property>; \
		\
		auto& Property(Class& Object); \
		auto& Property(const Class& Object); \
	}

// Usage: DEFINE_PRIVATE_ACCESS_FUNCTION(FMyClass, MyFunction) in global scope, then PrivateAccess::MyFunction(MyObject)(MyArgs) from anywhere
#define DEFINE_PRIVATE_ACCESS_FUNCTION(Class, Function) \
	namespace PrivateAccess \
	{ \
		template<typename> \
		struct TClass_ ## Function; \
		\
		template<> \
		struct TClass_ ## Function<Class> \
		{ \
			template<auto FunctionPtr> \
			struct TFunction_ ## Function \
			{ \
				friend auto Function(Class& Object) \
				{ \
					return [&Object]<typename... ArgTypes>(ArgTypes&&... Args) \
					{ \
						return (Object.*FunctionPtr)(Forward<ArgTypes>(Args)...); \
					}; \
				} \
			}; \
		}; \
		template struct TClass_ ## Function<Class>::TFunction_ ## Function<&Class::Function>; \
		\
		auto Function(Class& Object); \
		auto Function(const Class& Object) \
		{ \
			return Function(const_cast<Class&>(Object)); \
		} \
	}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FVoxelAtomicPadding
{
public:
	FORCEINLINE FVoxelAtomicPadding()
	{
	}
	FORCEINLINE FVoxelAtomicPadding(const FVoxelAtomicPadding&)
	{
	}
	FORCEINLINE FVoxelAtomicPadding(FVoxelAtomicPadding&&)
	{
	}
	FORCEINLINE FVoxelAtomicPadding& operator=(const FVoxelAtomicPadding&)
	{
		return *this;
	}
	FORCEINLINE FVoxelAtomicPadding& operator=(FVoxelAtomicPadding&&)
	{
		return *this;
	}

private:
	uint8 Padding[PLATFORM_CACHE_LINE_SIZE * 2];
};

#define VOXEL_ATOMIC_PADDING FVoxelAtomicPadding VOXEL_APPEND_LINE(_Padding)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FVoxelRangeIteratorEnd
{
};

template<typename Type>
struct TVoxelRangeIterator
{
	FORCEINLINE Type begin() const
	{
		return static_cast<const Type&>(*this);
	}
	FORCEINLINE static FVoxelRangeIteratorEnd end()
	{
		return {};
	}
    FORCEINLINE bool operator!=(const FVoxelRangeIteratorEnd&) const
    {
        return bool(static_cast<const Type&>(*this));
    }
};