// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelVirtualStruct.h"
#include "VoxelMinimal/Utilities/VoxelObjectUtilities.h"
#include "VoxelInstancedStruct.generated.h"

template<typename>
struct TVoxelInstancedStruct;

USTRUCT(BlueprintType)
struct VOXELCORE_API FVoxelInstancedStruct
{
	GENERATED_BODY()

public:
	FVoxelInstancedStruct() = default;
	explicit FVoxelInstancedStruct(UScriptStruct* ScriptStruct)
	{
		InitializeAs(ScriptStruct);
	}

	FVoxelInstancedStruct(const FVoxelInstancedStruct& Other)
	{
		InitializeAs(Other.ScriptStruct, Other.StructMemory);
	}

	FORCEINLINE FVoxelInstancedStruct(FVoxelInstancedStruct&& Other)
		: ScriptStruct(Other.ScriptStruct)
		, StructMemory(Other.StructMemory)
	{
		Other.ScriptStruct = nullptr;
		Other.StructMemory = nullptr;
	}
	FORCEINLINE ~FVoxelInstancedStruct()
	{
		Reset();
	}

public:
	FVoxelInstancedStruct& operator=(const FVoxelInstancedStruct& Other)
	{
		if (!ensure(this != &Other))
		{
			return *this;
		}

		InitializeAs(Other.ScriptStruct, Other.StructMemory);
		return *this;
	}

	FORCEINLINE FVoxelInstancedStruct& operator=(FVoxelInstancedStruct&& Other)
	{
		checkVoxelSlow(this != &Other);

		Reset();

		ScriptStruct = Other.ScriptStruct;
		StructMemory = Other.StructMemory;

		Other.ScriptStruct = nullptr;
		Other.StructMemory = nullptr;

		return *this;
	}

	// Called in move operators so FORCEINLINE
	FORCEINLINE void Reset()
	{
		if (!ScriptStruct)
		{
			checkVoxelSlow(!ScriptStruct && !StructMemory);
			return;
		}

		Free();
	}

	void Free();
	void* Release();
	void InitializeAs(UScriptStruct* NewScriptStruct, const void* NewStructMemory = nullptr);

	template<typename T>
	T* Release()
	{
		checkVoxelSlow(IsA<T>());
		return static_cast<T*>(Release());
	}
	template<typename T>
	TVoxelUniquePtr<T> ReleaseUnique()
	{
		return TVoxelUniquePtr<T>(Release<T>());
	}
	template<typename T>
	TSharedPtr<T> ReleaseShared()
	{
		return MakeVoxelShareable(Release<T>());
	}

	template<typename T>
	static FVoxelInstancedStruct Make()
	{
		FVoxelInstancedStruct InstancedStruct;
		InstancedStruct.InitializeAs(StaticStructFast<T>());
		return InstancedStruct;
	}

	template<typename T>
	static FVoxelInstancedStruct Make(const T& Struct)
	{
		checkVoxelSlow(&Struct);

		FVoxelInstancedStruct InstancedStruct;
		if constexpr (TIsDerivedFrom<T, FVoxelVirtualStruct>::Value)
		{
			InstancedStruct.InitializeAs(Struct.GetStruct(), &Struct);
		}
		else
		{
			InstancedStruct.InitializeAs(StaticStructFast<T>(), &Struct);
		}
		return InstancedStruct;
	}

public:
	uint64 GetPropertyHash() const;
	bool NetSerialize(FArchive& Ar, UPackageMap& Map);

	//~ Begin TStructOpsTypeTraits Interface
	bool Serialize(FArchive& Ar);
	bool Identical(const FVoxelInstancedStruct* Other, uint32 PortFlags) const;
	bool ExportTextItem(FString& ValueStr, const FVoxelInstancedStruct& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const;
	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText, FArchive* InSerializingArchive = nullptr);
	void AddStructReferencedObjects(FReferenceCollector& Collector);
	void GetPreloadDependencies(TArray<UObject*>& OutDependencies) const;
	//~ End TStructOpsTypeTraits Interface

public:
	FORCEINLINE UScriptStruct* GetScriptStruct() const
	{
#if VOXEL_DEBUG
		if (ScriptStruct &&
			ScriptStruct->IsChildOf(StaticStructFast<FVoxelVirtualStruct>()))
		{
			checkVoxelSlow(ScriptStruct == Get<FVoxelVirtualStruct>().GetStruct());
		}
#endif
		return ScriptStruct;
	}

	FORCEINLINE void* GetStructMemory()
	{
		return StructMemory;
	}
	FORCEINLINE const void* GetStructMemory() const
	{
		return StructMemory;
	}

	FORCEINLINE TVoxelArrayView<uint8> GetStructView()
	{
		checkVoxelSlow(IsValid());
		return TVoxelArrayView<uint8>(static_cast<uint8*>(StructMemory), ScriptStruct->GetStructureSize());
	}
	FORCEINLINE TConstVoxelArrayView<uint8> GetStructView() const
	{
		checkVoxelSlow(IsValid());
		return TConstVoxelArrayView<uint8>(static_cast<const uint8*>(StructMemory), ScriptStruct->GetStructureSize());
	}

	FORCEINLINE bool IsValid() const
	{
		checkVoxelSlow((ScriptStruct != nullptr) == (StructMemory != nullptr));
		return ScriptStruct != nullptr;
	}

public:
	FORCEINLINE bool IsA(const UScriptStruct* Struct) const
	{
		return
			IsValid() &&
			ScriptStruct->IsChildOf(Struct);
	}
	template<typename T>
	FORCEINLINE bool IsA() const
	{
		return this->IsA(StaticStructFast<T>());
	}

	template<typename T>
	FORCEINLINE T* GetPtr()
	{
		if (!IsA<T>())
		{
			return nullptr;
		}

		return static_cast<T*>(StructMemory);
	}
	template<typename T>
	FORCEINLINE const T* GetPtr() const
	{
		return ConstCast(this)->GetPtr<T>();
	}

	template<typename T>
	FORCEINLINE T& Get()
	{
		checkVoxelSlow(IsA<T>());
		return *static_cast<T*>(StructMemory);
	}
	template<typename T>
	FORCEINLINE const T& Get() const
	{
		return ConstCast(this)->Get<T>();
	}

	template<typename T>
	TSharedRef<T> MakeSharedCopy() const
	{
		return MakeSharedStruct<T>(GetScriptStruct(), &Get<T>());
	}

	FORCEINLINE void CopyFrom(const void* Data)
	{
		checkVoxelSlow(IsValid());
		ScriptStruct->CopyScriptStruct(GetStructMemory(), Data);
	}
	FORCEINLINE void CopyTo(void* Data) const
	{
		checkVoxelSlow(IsValid());
		ScriptStruct->CopyScriptStruct(Data, GetStructMemory());
	}

public:
	bool operator==(const FVoxelInstancedStruct& Other) const
	{
		return Identical(&Other, PPF_None);
	}
	bool operator!=(const FVoxelInstancedStruct& Other) const
	{
		return !Identical(&Other, PPF_None);
	}

private:
	UScriptStruct* ScriptStruct = nullptr;
	void* StructMemory = nullptr;
};

template<>
struct TStructOpsTypeTraits<FVoxelInstancedStruct> : TStructOpsTypeTraitsBase2<FVoxelInstancedStruct>
{
	enum
	{
		WithSerializer = true,
		WithIdentical = true,
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithAddStructReferencedObjects = true,
		WithGetPreloadDependencies = true,
	};
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
struct TVoxelInstancedStruct : private FVoxelInstancedStruct
{
public:
	TVoxelInstancedStruct() = default;

	template<typename OtherType, typename = typename TEnableIf<TIsDerivedFrom<OtherType, T>::Value>::Type>
	FORCEINLINE TVoxelInstancedStruct(const TVoxelInstancedStruct<OtherType>& Other)
		: FVoxelInstancedStruct(Other)
	{
	}
	template<typename OtherType, typename = typename TEnableIf<TIsDerivedFrom<OtherType, T>::Value>::Type>
	FORCEINLINE TVoxelInstancedStruct(TVoxelInstancedStruct<OtherType>&& Other)
		: FVoxelInstancedStruct(MoveTemp(Other))
	{
	}

	template<typename OtherType, typename = typename TEnableIf<TIsDerivedFrom<OtherType, T>::Value>::Type>
	FORCEINLINE TVoxelInstancedStruct(const OtherType& Other)
		: FVoxelInstancedStruct(FVoxelInstancedStruct::Make(Other))
	{
	}

	FORCEINLINE TVoxelInstancedStruct(const FVoxelInstancedStruct& Other)
		: FVoxelInstancedStruct(Other)
	{
		CheckType();
	}
	FORCEINLINE TVoxelInstancedStruct(FVoxelInstancedStruct&& Other)
		: FVoxelInstancedStruct(MoveTemp(Other))
	{
		CheckType();
	}

	FORCEINLINE TVoxelInstancedStruct(UScriptStruct* ScriptStruct)
		: FVoxelInstancedStruct(ScriptStruct)
	{
		CheckType();
	}

public:
	template<typename OtherType, typename = typename TEnableIf<TIsDerivedFrom<OtherType, T>::Value>::Type>
	FORCEINLINE TVoxelInstancedStruct& operator=(const TVoxelInstancedStruct<OtherType>& Other)
	{
		FVoxelInstancedStruct::operator=(Other);
		return *this;
	}
	template<typename OtherType, typename = typename TEnableIf<TIsDerivedFrom<OtherType, T>::Value>::Type>
	FORCEINLINE TVoxelInstancedStruct& operator=(TVoxelInstancedStruct<OtherType>&& Other)
	{
		FVoxelInstancedStruct::operator=(MoveTemp(Other));
		return *this;
	}

	FORCEINLINE TVoxelInstancedStruct& operator=(const FVoxelInstancedStruct& Other)
	{
		FVoxelInstancedStruct::operator=(Other);
		CheckType();
		return *this;
	}
	FORCEINLINE TVoxelInstancedStruct& operator=(FVoxelInstancedStruct&& Other)
	{
		FVoxelInstancedStruct::operator=(MoveTemp(Other));
		CheckType();
		return *this;
	}

public:
	using FVoxelInstancedStruct::Reset;
	using FVoxelInstancedStruct::GetScriptStruct;
	using FVoxelInstancedStruct::IsValid;
	using FVoxelInstancedStruct::IsA;
	using FVoxelInstancedStruct::AddStructReferencedObjects;

	FORCEINLINE T* Release()
	{
		return static_cast<T*>(FVoxelInstancedStruct::Release());
	}

public:
	FORCEINLINE T& Get()
	{
		CheckType();
		checkVoxelSlow(IsValid());
		return *static_cast<T*>(GetStructMemory());
	}
	FORCEINLINE const T& Get() const
	{
		return ConstCast(*this).Get();
	}

	template<typename OtherType, typename = typename TEnableIf<TIsDerivedFrom<OtherType, T>::Value && !std::is_same_v<OtherType, T>>::Type>
	FORCEINLINE OtherType& Get()
	{
		CheckType();
		checkVoxelSlow(IsValid());
		checkVoxelSlow(IsA<OtherType>());
		return *static_cast<OtherType*>(GetStructMemory());
	}
	template<typename OtherType, typename = typename TEnableIf<TIsDerivedFrom<OtherType, T>::Value && !std::is_same_v<OtherType, T>>::Type>
	FORCEINLINE const OtherType& Get() const
	{
		return ConstCast(*this).template Get<OtherType>();
	}

public:
	FORCEINLINE T* GetPtr()
	{
		CheckType();
		return static_cast<T*>(GetStructMemory());
	}
	FORCEINLINE const T* GetPtr() const
	{
		CheckType();
		return static_cast<const T*>(GetStructMemory());
	}

	template<typename OtherType, typename = typename TEnableIf<TIsDerivedFrom<OtherType, T>::Value && !std::is_same_v<OtherType, T>>::Type>
	FORCEINLINE OtherType* GetPtr()
	{
		return FVoxelInstancedStruct::GetPtr<OtherType>();
	}
	template<typename OtherType, typename = typename TEnableIf<TIsDerivedFrom<OtherType, T>::Value && !std::is_same_v<OtherType, T>>::Type>
	FORCEINLINE const OtherType* GetPtr() const
	{
		return FVoxelInstancedStruct::GetPtr<OtherType>();
	}

public:
	template<typename OtherType, typename = typename TEnableIf<TIsDerivedFrom<OtherType, T>::Value && !std::is_same_v<OtherType, T>>::Type>
	FORCEINLINE bool IsA() const
	{
		return FVoxelInstancedStruct::IsA<OtherType>();
	}
	FORCEINLINE bool IsValid() const
	{
		return
			FVoxelInstancedStruct::IsValid() &&
			ensureVoxelSlow(GetScriptStruct()->IsChildOf(StaticStructFast<T>()));
	}

	template<typename OtherType = T, typename = typename TEnableIf<TIsDerivedFrom<OtherType, T>::Value>::Type>
	FORCEINLINE TSharedRef<OtherType> MakeSharedCopy() const
	{
		return FVoxelInstancedStruct::MakeSharedCopy<OtherType>();
	}

	FORCEINLINE T& operator*()
	{
		return Get();
	}
	FORCEINLINE const T& operator*() const
	{
		return Get();
	}

	FORCEINLINE T* operator->()
	{
		return &Get();
	}
	FORCEINLINE const T* operator->() const
	{
		return &Get();
	}

	FORCEINLINE operator bool() const
	{
		return IsValid();
	}

	FORCEINLINE bool operator==(const TVoxelInstancedStruct& Other) const
	{
		return FVoxelInstancedStruct::operator==(Other);
	}
	FORCEINLINE bool operator!=(const TVoxelInstancedStruct& Other) const
	{
		return FVoxelInstancedStruct::operator!=(Other);
	}

private:
	FORCEINLINE void CheckType() const
	{
		checkVoxelSlow(!FVoxelInstancedStruct::IsValid() || FVoxelInstancedStruct::IsA<T>());
	}

	template<typename>
	friend struct TVoxelInstancedStruct;
};