// Copyright Voxel Plugin SAS. All Rights Reserved.

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
		InitializeAs(Other.GetScriptStruct(), Other.GetStructMemory());
	}

	FORCEINLINE FVoxelInstancedStruct(FVoxelInstancedStruct&& Other)
		: PrivateScriptStruct(MoveTemp(Other.PrivateScriptStruct))
		, PrivateStructMemory(MoveTemp(Other.PrivateStructMemory))
	{
		Other.PrivateScriptStruct = nullptr;
		Other.PrivateStructMemory = nullptr;
	}

public:
	FVoxelInstancedStruct& operator=(const FVoxelInstancedStruct& Other)
	{
		if (!ensure(this != &Other))
		{
			return *this;
		}

		InitializeAs(Other.GetScriptStruct(), Other.GetStructMemory());
		return *this;
	}

	FORCEINLINE FVoxelInstancedStruct& operator=(FVoxelInstancedStruct&& Other)
	{
		checkVoxelSlow(this != &Other);

		PrivateScriptStruct = MoveTemp(Other.PrivateScriptStruct);
		PrivateStructMemory = MoveTemp(Other.PrivateStructMemory);

		Other.PrivateScriptStruct = nullptr;
		Other.PrivateStructMemory = nullptr;

		return *this;
	}

	void InitializeAs(UScriptStruct* NewScriptStruct, const void* NewStructMemory = nullptr);

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
		if constexpr (std::derived_from<T, FVoxelVirtualStruct>)
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
	void Reset();
	TSharedRef<FStructOnScope> MakeStructOnScope();
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);

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
		if (PrivateScriptStruct &&
			PrivateScriptStruct->IsChildOf(StaticStructFast<FVoxelVirtualStruct>()))
		{
			checkVoxelSlow(PrivateScriptStruct == reinterpret_cast<const FVoxelVirtualStruct*>(PrivateStructMemory.Get())->GetStruct());
		}
#endif
		return PrivateScriptStruct;
	}

	FORCEINLINE void* GetStructMemory()
	{
		return PrivateStructMemory.Get();
	}
	FORCEINLINE const void* GetStructMemory() const
	{
		return PrivateStructMemory.Get();
	}

	FORCEINLINE TVoxelArrayView<uint8> GetStructView()
	{
		checkVoxelSlow(IsValid());
		return TVoxelArrayView<uint8>(static_cast<uint8*>(GetStructMemory()), GetScriptStruct()->GetStructureSize());
	}
	FORCEINLINE TConstVoxelArrayView<uint8> GetStructView() const
	{
		checkVoxelSlow(IsValid());
		return TConstVoxelArrayView<uint8>(static_cast<const uint8*>(GetStructMemory()), GetScriptStruct()->GetStructureSize());
	}

	FORCEINLINE bool IsValid() const
	{
		checkVoxelSlow((GetScriptStruct() != nullptr) == (GetStructMemory() != nullptr));
		return GetScriptStruct() != nullptr;
	}

public:
	FORCEINLINE bool IsA(const UScriptStruct* Struct) const
	{
		return
			IsValid() &&
			GetScriptStruct()->IsChildOf(Struct);
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

		return static_cast<T*>(GetStructMemory());
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
		return *static_cast<T*>(GetStructMemory());
	}
	template<typename T>
	FORCEINLINE const T& Get() const
	{
		return ConstCast(this)->Get<T>();
	}

	template<typename T>
	TSharedRef<T> ToSharedRef()
	{
		checkVoxelSlow(IsA<T>())
		return ReinterpretCastRef<TSharedRef<T>>(PrivateStructMemory);
	}
	template<typename T>
	TSharedRef<const T> ToSharedRef() const
	{
		return ConstCast(this)->ToSharedRef<const T>();
	}

	template<typename T>
	TSharedPtr<T> ToSharedPtr()
	{
		if (!IsA<T>())
		{
			return nullptr;
		}

		return ReinterpretCastRef<TSharedPtr<T>>(PrivateStructMemory);
	}
	template<typename T>
	TSharedPtr<const T> ToSharedPtr() const
	{
		return ConstCast(this)->ToSharedPtr<const T>();
	}

	template<typename T>
	TSharedRef<T> MakeSharedCopy() const
	{
		return MakeSharedStruct<T>(GetScriptStruct(), &Get<T>());
	}

	FORCEINLINE void CopyFrom(const void* Data)
	{
		checkVoxelSlow(IsValid());
		GetScriptStruct()->CopyScriptStruct(GetStructMemory(), Data);
	}
	FORCEINLINE void CopyTo(void* Data) const
	{
		checkVoxelSlow(IsValid());
		GetScriptStruct()->CopyScriptStruct(Data, GetStructMemory());
	}

public:
	bool operator==(const FVoxelInstancedStruct& Other) const
	{
		return Identical(&Other, PPF_None);
	}

protected:
	UScriptStruct* PrivateScriptStruct = nullptr;
	FSharedVoidPtr PrivateStructMemory;
};

template<>
struct TStructOpsTypeTraits<FVoxelInstancedStruct> : TStructOpsTypeTraitsBase2<FVoxelInstancedStruct>
{
	enum
	{
		WithSerializer = true,
		WithNetSerializer = true,
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

	template<typename OtherType>
	requires std::derived_from<OtherType, T>
	FORCEINLINE TVoxelInstancedStruct(const TVoxelInstancedStruct<OtherType>& Other)
		: FVoxelInstancedStruct(Other)
	{
	}
	template<typename OtherType>
	requires std::derived_from<OtherType, T>
	FORCEINLINE TVoxelInstancedStruct(TVoxelInstancedStruct<OtherType>&& Other)
		: FVoxelInstancedStruct(MoveTemp(Other))
	{
	}

	template<typename OtherType>
	requires std::derived_from<OtherType, T>
	FORCEINLINE TVoxelInstancedStruct(const OtherType& Other)
		: FVoxelInstancedStruct(FVoxelInstancedStruct::Make(Other))
	{
	}

	FORCEINLINE explicit TVoxelInstancedStruct(const FVoxelInstancedStruct& Other)
		: FVoxelInstancedStruct(Other)
	{
		CheckType();
	}
	FORCEINLINE explicit TVoxelInstancedStruct(FVoxelInstancedStruct&& Other)
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
	template<typename OtherType>
	requires std::derived_from<OtherType, T>
	FORCEINLINE TVoxelInstancedStruct& operator=(const TVoxelInstancedStruct<OtherType>& Other)
	{
		FVoxelInstancedStruct::operator=(Other);
		return *this;
	}
	template<typename OtherType>
	requires std::derived_from<OtherType, T>
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

	template<typename OtherType>
	requires std::derived_from<OtherType, T>
	FORCEINLINE TVoxelInstancedStruct& operator=(const TSharedPtr<OtherType>& Other)
	{
		if (Other)
		{
			*this = Other.ToSharedRef();
		}
		else
		{
			Reset();
		}
		return *this;
	}
	template<typename OtherType>
	requires std::derived_from<OtherType, T>
	FORCEINLINE TVoxelInstancedStruct& operator=(const TSharedRef<OtherType>& Other)
	{
		if constexpr (std::derived_from<OtherType, FVoxelVirtualStruct>)
		{
			PrivateScriptStruct = Other->GetStruct();
		}
		else
		{
			PrivateScriptStruct = StaticStructFast<OtherType>();
		}

		PrivateStructMemory = MakeSharedVoidRef(Other);
		return *this;
	}

public:
	using FVoxelInstancedStruct::GetScriptStruct;
	using FVoxelInstancedStruct::IsValid;
	using FVoxelInstancedStruct::Reset;
	using FVoxelInstancedStruct::MakeStructOnScope;
	using FVoxelInstancedStruct::NetSerialize;
	using FVoxelInstancedStruct::Serialize;
	using FVoxelInstancedStruct::Identical;
	using FVoxelInstancedStruct::ExportTextItem;
	using FVoxelInstancedStruct::ImportTextItem;
	using FVoxelInstancedStruct::AddStructReferencedObjects;
	using FVoxelInstancedStruct::GetPreloadDependencies;

public:
	template<typename OtherType = T>
	requires std::derived_from<OtherType, T>
	FORCEINLINE TSharedRef<OtherType> ToSharedRef()
	{
		return FVoxelInstancedStruct::ToSharedRef<OtherType>();
	}
	template<typename OtherType = T>
	requires std::derived_from<OtherType, T>
	FORCEINLINE TSharedRef<const OtherType> ToSharedRef() const
	{
		return FVoxelInstancedStruct::ToSharedRef<const OtherType>();
	}

	template<typename OtherType = T>
	requires std::derived_from<OtherType, T>
	FORCEINLINE TSharedPtr<OtherType> ToSharedPtr()
	{
		return FVoxelInstancedStruct::ToSharedPtr<OtherType>();
	}
	template<typename OtherType = T>
	requires std::derived_from<OtherType, T>
	FORCEINLINE TSharedPtr<const OtherType> ToSharedPtr() const
	{
		return FVoxelInstancedStruct::ToSharedPtr<const OtherType>();
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

	template<typename OtherType>
	requires
	(
		std::derived_from<OtherType, T> &&
		!std::is_same_v<OtherType, T>
	)
	FORCEINLINE OtherType& Get()
	{
		CheckType();
		checkVoxelSlow(IsValid());
		checkVoxelSlow(IsA<OtherType>());
		return *static_cast<OtherType*>(GetStructMemory());
	}
	template<typename OtherType>
	requires
	(
		std::derived_from<OtherType, T> &&
		!std::is_same_v<OtherType, T>
	)
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

	template<typename OtherType>
	requires
	(
		std::derived_from<OtherType, T> &&
		!std::is_same_v<OtherType, T>
	)
	FORCEINLINE OtherType* GetPtr()
	{
		return FVoxelInstancedStruct::GetPtr<OtherType>();
	}
	template<typename OtherType>
	requires
	(
		std::derived_from<OtherType, T> &&
		!std::is_same_v<OtherType, T>
	)
	FORCEINLINE const OtherType* GetPtr() const
	{
		return FVoxelInstancedStruct::GetPtr<OtherType>();
	}

public:
	template<typename OtherType>
	requires
	(
		std::derived_from<OtherType, T> &&
		!std::is_same_v<OtherType, T>
	)
	FORCEINLINE bool IsA() const
	{
		return FVoxelInstancedStruct::IsA<OtherType>();
	}
	FORCEINLINE bool IsA(const UScriptStruct* Struct) const
	{
		return FVoxelInstancedStruct::IsA(Struct);
	}
	FORCEINLINE bool IsValid() const
	{
		return
			FVoxelInstancedStruct::IsValid() &&
			ensureVoxelSlow(GetScriptStruct()->IsChildOf(StaticStructFast<T>()));
	}

	template<typename OtherType = T>
	requires std::derived_from<OtherType, T>
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

private:
	FORCEINLINE void CheckType() const
	{
		checkVoxelSlow(!FVoxelInstancedStruct::IsValid() || FVoxelInstancedStruct::IsA<T>());
	}

	template<typename>
	friend struct TVoxelInstancedStruct;
};