# Voxel Core

Voxel Core is an open-source plugin with the Core module of [Voxel Plugin](https://voxelplugin.com/).
It's essentially a layer on top Unreal to make it easier to write high-performance code & customize the editor.
Unlike what the name might imply, there's barely any voxel-specific code included here - all of that lives in the other Voxel Plugin modules.

This plugin is intented to be used in C++: if your project is blueprint-only, this will be of no use to you.

If you have questions or suggestions about the code, feel free to ask in #cpp in the plugin's Discord: [discord.voxelplugin.com](https://discord.voxelplugin.com)

## Compatibility

Voxel Core currently compiles on **5.4**. Check branches for older engine versions.

## Getting started

Download this repo, put it in the Plugins folder of your project.
You can then add the VoxelCore dependency (or VoxelCoreEditor for editor features) to your module.

For runtime features, use `#include "VoxelMinimal.h"`.
For editor features, use `#include "VoxelEditorMinimal.h"`.

If you have a custom PCH, you should add these includes to it.

## Asserts

VoxelCore uses its own assertion macros (`checkVoxelSlow` etc).
This allows disabling them in Development Editor builds, which is important for performance-heavy editor features to be fast.

You can change this behavior in `VoxelCore.Build.cs`.

## Overview

### Containers

A variety of high-performance containers are included:

* `TVoxelAddOnlySet`
* `TVoxelArray`
* `TVoxelArrayView`
* `TVoxelBitArray`
* `TVoxelChunkedArray`
* `TVoxelMap`
* `TVoxelSparseArray`
* `TVoxelChunkedSparseArray`
* `TVoxelStaticArray`
* `TVoxelStaticBitArray`

Some of them, like TVoxelArray, are mainly there to disable range checking when `VOXEL_DEBUG` is 0.
Others, like `TVoxelMap`, are full replacement of engine containers to be faster.

Here's a few benchmarks in a shipping build with checks disabled (see `FVoxelCoreBenchmark::Run()`), comparing voxel containers against engine ones.

```cpp
Map::FindChecked               1.2x faster    Engine: 1.39ns    Voxel: 1.21ns    std:  5%  7%
Map::Remove                    1.2x faster    Engine: 13.98ns   Voxel: 11.6ns    std:  4%  4%
Map::Reserve(1M)              66.6x faster    Engine: 1.22ms    Voxel: 18.32us   std:  5% 14%
Map::FindOrAdd                 2.2x faster    Engine: 1.99ns    Voxel: 0.89ns    std:  8%  5%
Map::FindOrAdd<FIntVector>     5.9x faster    Engine: 8.82ns    Voxel: 1.5ns     std: 12%  4%
Map::Add_CheckNew              4.5x faster    Engine: 9.5ns     Voxel: 2.12ns    std: 22%  2%
SparseArray::Reserve(1M)       2.5x faster    Engine: 1.09ms    Voxel: 443.1us   std: 54%  5%
SparseArray::Add               1.4x faster    Engine: 2.99ns    Voxel: 2.11ns    std:  8% 14%
BitArray::Add                  2.1x faster    Engine: 3.02ns    Voxel: 1.45ns    std: 14% 17%
BitArray::CountSetBits         9.1x faster    Engine: 56.56us   Voxel: 6.25us    std:  3% 16%
ConstSetBitIterator            3.4x faster    Engine: 1.03ms    Voxel: 306.48us  std:  6%  5%
Array::RemoveAtSwap            3.2x faster    Engine: 2.06ns    Voxel: 0.65ns    std: 15%  6%
AActor::StaticClass            2.9x faster    Engine: 1.25ns    Voxel: 0.43ns    std:  3%  4%
```


### Messages

You can use `VOXEL_MESSAGE` to log a message from any thread.
The message will be displayed in the editor as a notification & will be logged.

A lot of types can be directly printed by default (strings, names, all numetic types etc) without having to use printf specifiers.
You can override the behavior of your own types by adding a `TSharedRef<FVoxelMessageToken> CreateMessageToken() const;` function to them or defining a new `TVoxelMessageTokenFactory`.

Calling `VOXEL_MESSAGE` inside a UFunction call will automatically append the blueprint callstack to it.

```cpp
// This will print "Actor: NameA is not the same as NameB. NameA should be NameB"
// Actor will be clickable and will take you to the relevant actor
VOXEL_MESSAGE(Error, "{0}: {1} is not the same as {2}. {1} should be {2}",
    MyActor,
    MyActor->GetName(),
    MyActor->NameOverride);

int32 Int = 0;
float Float = 0;
FString String = "String";
FName Name = "Name";
UObject* Object = nullptr;
TArray<int32> Array = { 0, 1, 2, 3 };

// VOXEL_MESSAGE can accept a lot of different types by default, including objects & arrays
VOXEL_MESSAGE(Info, "Test: {0} {1} {2} {3} {4} {5} {6}",
    Int,
    Float,
    String,
    Name,
    Object,
    Array);
```

### Virtual Structs

By inheriting from `FVoxelVirtualStruct`, you can add runtime type information to a struct.
This is especially useful to build thread-safe systems using UStructs instead of UObjects.

`FVoxelVirtualStruct` supports `Cast`, `TSharedFromThis` and many other features.

```cpp
USTRUCT()
struct FVoxelMessageToken
    : public FVoxelVirtualStruct
    , public TSharedFromThis<FVoxelMessageToken>
{
    GENERATED_BODY()
    GENERATED_VIRTUAL_STRUCT_BODY()
};

USTRUCT()
struct FVoxelMessageToken_Group : public FVoxelMessageToken
{
    GENERATED_BODY()
    GENERATED_VIRTUAL_STRUCT_BODY()
};

void AddGroup(const TSharedPtr<FVoxelMessageToken>& Token)
{
    // Cast work on TSharedPtr
    // You can also manually check IsA<FVoxelMessageToken_Group>, or call Token->As<FVoxelMessageToken_Group>
    const TSharedPtr<FVoxelMessageToken_Group> Group = Cast<FVoxelMessageToken_Group>(Token);
    if (!Group)
    {
        return;
    }
    
    Group->DoSmthg();
}

UScriptStruct* Struct = FVoxelMessageToken_Group::StaticStruct();

// You can make shared structs from their UStruct*, in a similar fashion as NewObject
const TSharedRef<FVoxelMessageToken> Token = MakeSharedStruct<FVoxelMessageToken>(Struct)
CastChecked<FVoxelMessageToken_Group>(Token)->DoSmthg();
```

### Editor

#### Detail customizations

To customize a class, simply add the code below in any .cpp file.
No need to bother registering customization globally - it's all automatically done by the macro.

```cpp
VOXEL_CUSTOMIZE_CLASS(AVoxelActor)(IDetailLayoutBuilder& DetailLayout)
{
    const TSharedRef<IPropertyHandle> GraphHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_STATIC(AVoxelActor, Graph_NewProperty));
    GraphHandle->MarkHiddenByCustomization();
}
```

For structs, use `VOXEL_CUSTOMIZE_STRUCT_HEADER`.
You can also use `DEFINE_VOXEL_STRUCT_LAYOUT` or `DEFINE_VOXEL_STRUCT_LAYOUT_RECURSIVE` if you wish to have a complex customization.

#### Thumbnails

The following defines a custom thumbnail for asset `UVoxelVoxelizedMeshAsset`, by rendering its `Mesh`.

```cpp
UCLASS()
class UVoxelVoxelizedMeshAssetThumbnailRenderer : public UVoxelStaticMeshThumbnailRenderer
{
    GENERATED_BODY()

public:
    virtual UStaticMesh* GetStaticMesh(UObject* Object, TArray<UMaterialInterface*>& OutMaterialOverrides) const override
    {
        return CastChecked<UVoxelVoxelizedMeshAsset>(Object)->Mesh.LoadSynchronous();
    }
};

// In cpp
DEFINE_VOXEL_THUMBNAIL_RENDERER(UVoxelVoxelizedMeshAssetThumbnailRenderer, UVoxelVoxelizedMeshAsset);
```

#### Asset type

To define a new asset type, you can simply do the following:

```cpp
// VoxelAssetType will automatically register the asset type actions
// You can customize the thumbnail color using AssetColor
UCLASS(meta = (VoxelAssetType, AssetColor=Blue))
class UVoxelAsset : public UObject
{
    GENERATED_BODY()
};

// In cpp, will add the asset to the Voxel category of the content browser context menu
// This automatically defines a new UFactory for that asset
DEFINE_VOXEL_FACTORY(UVoxelAsset);
```

#### Custom toolkit

To create a custom toolkit for an asset (ie, to open a custom window when you open an asset), you can use `FVoxelSimpleAssetToolkit`:

```cpp
USTRUCT()
struct FVoxelVoxelizedMeshAssetToolkit : public FVoxelSimpleAssetToolkit
{
    GENERATED_BODY()
    GENERATED_VIRTUAL_STRUCT_BODY()

    UPROPERTY()
    TObjectPtr<UVoxelVoxelizedMeshAsset> Asset;

public:
    //~ Begin FVoxelSimpleAssetToolkit Interface
    virtual void Tick() override;
    virtual void SetupPreview() override;
    //~ End FVoxelSimpleAssetToolkit Interface

private:
    UPROPERTY()
    TObjectPtr<AStaticMeshActor> Actor;
};
```

This will automatically define a new toolkit for `UVoxelVoxelizedMeshAsset`.
SimpleAssetToolkit comes with a detail panel and a preview scene.

You can setup the preview scene in `SetupPreview`, typically by spawning actors into it:

```cpp
void FVoxelVoxelizedMeshAssetToolkit::SetupPreview()
{
    VOXEL_FUNCTION_COUNTER();

    Super::SetupPreview();

    Actor = SpawnActor<AStaticMeshActor>();
    Actor->SetStatucMesh(Asset->Mesh);
    
    UVoxelInvokerComponent* Component = CreateComponent<UVoxelInvokerComponent>();
    Component->Radius = 10000;
}
```

### Profiling
#### Scope counters

A collection of QoL macros used to record trace data for Insights, similar to `TRACE_CPUPROFILER_EVENT_SCOPE`.
They are all pretty much free if tracing of the `voxel` channel is not enabled through `-trace=voxel` or `voxel.StartInsights`.

```cpp
void MyFunction()
{
    // Will show up as MyClass::MyFunction in Insights
    VOXEL_FUNCTION_COUNTER();

    {
        // Will show up as MyClass::MyFunction.DoWork1 in Insights
        VOXEL_SCOPE_COUNTER("DoWork1");
        
        for (int32 Index : Indices)
        {
            DoWork1(Index);
        }
    }

    {
        // Cheaper than a Printf
        VOXEL_SCOPE_COUNTER_FNAME(Actor->GetFName());
        Actor->Destroy();
    }

    {
        // Will have 1-2us of overhead when tracing due to the Printf, use sparingly
        VOXEL_SCOPE_COUNTER_FORMAT("Iterate Num=%d", Indices.Num());

        for (int32& Index : Indices)
        {
            Index++;
        }
    }
}
```

#### Memory tracker

Automatically track the amount of memory used by instances of a class:

```cpp
// Will show up in stat voxelmemory, otherwise use the WITH_CATEGORY variant
DECLARE_VOXEL_MEMORY_STAT(MY_API, STAT_MyStructMemory, "MyStruct Memory");

// in .cpp
DEFINE_VOXEL_MEMORY_STAT(STAT_MyStructMemory);

struct FMyStruct
{
    TArray<int32> LargeArray;

    VOXEL_ALLOCATED_SIZE_TRACKER(STAT_MyStructMemory);

    int64 GetAllocatedSize() const
    {
        return LargeArray.GetAllocatedSize();
    }
};

void MyFunction()
{
    FMyStruct MyStruct = MakeMyStruct();
    // Call GetAllocatedSize and update stats accordingly
    MyStruct.UpdateStats();
}
```

#### Instance tracker

Track the number of instances of a class:

```cpp
struct FMyStruct
{
    // Extremely cheap (atomic add on construct, atomic sub on destroy)
    VOXEL_COUNT_INSTANCES()
};

// in .cpp, will show up in stat voxelcounters
DEFINE_VOXEL_INSTANCE_COUNTER(FMyStruct);
```

### Misc

#### STATIC_FNAME

`STATIC_FNAME("MyName")` will cache a FName into a static variable.

```cpp
if (Name == STATIC_FNAME("MyActor"))
{
    return;
}
```

#### INLINE_LAMBDA

Create a local scope for easier flow management. Since we don't store the lambda in any TFunction, this is usually completely optimized out & close to zero overhead.
This is especially useful when you want strong const-correctness.

```cpp
const int32 MyValue = INLINE_LAMBDA
{
    if (bCondition0)
    {
        return 0;
    }
    
    if (bCondition1)
    {
        return 1;
    }
    
    int32 Result = 0;
    for (int32 Index : Indices)
    {
        Result += Index;
    }
    return Result;
};
```

#### VOXEL_CONSOLE_VARIABLE

Easily declare a console variable & access it through a global.

```cpp
// in cpp
VOXEL_CONSOLE_VARIABLE(
    VOXELGRAPHCORE_API, int32, GVoxelNumThreads, 2, // API Type Name Default
    "voxel.NumThreads", // Name
    "The number of threads to use to process voxel tasks"); // Description
    
// Optional, in header
extern VOXELGRAPHCORE_API int32 GVoxelNumThreads;


// In code
if (GVoxelNumThreads > 4)
{
    return;
}
```

#### VOXEL_CONSOLE_COMMAND

Easily declare a new console command.
Use `VOXEL_CONSOLE_WORLD_COMMAND` if you need a UWorld.

```cpp
VOXEL_CONSOLE_COMMAND(
    LogAllBrushes, // Unique name, doesn't really matter
    "voxel.LogAllBrushes", // Command
    "Log all brushes") // Description
{
    // Code goes here
    GVoxelChannelManager->LogAllBrushes_GameThread();
}
```

#### VOXEL_PURE_VIRTUAL

Similar to PURE_VIRTUAL, but easier to use:

```cpp
virtual void NoReturn() const VOXEL_PURE_VIRTUAL();
virtual uint32 WithReturn() const VOXEL_PURE_VIRTUAL({});
```

#### VOXEL_RUN_ON_STARTUP

Run code on app startup. See `VOXEL_RUN_ON_STARTUP_GAME`, `VOXEL_RUN_ON_STARTUP_EDITOR` and `VOXEL_RUN_ON_STARTUP_EDITOR_COMMANDLET`.

```cpp

VOXEL_RUN_ON_STARTUP_GAME()
{
    // Initialize something
}
```

#### ReinterpretCastXXX

"type safe" reinterpret_cast: will check that the type size is the same.
Useful to ensure some amount of type safety when using reinterpret_cast.

```cpp
bool bValue = true;
check(ReinterpretCastRef<uint8>(bValue) == 1);
```

#### ConstCast

Easily const-cast anything:

```cpp
const int32* Ptr;
ConstCast(Ptr); // Will be int32*

const int32& Ref;
ConstCast(Ref); // Will be int32&

TSharedPtr<const int32> SharedPtr;
ConstCast(SharedPtr); // Will be TSharedPtr<int32>&
```

#### VOXEL_FOREACH

For-loop for macros:

```cpp
#define GET_TYPES_IMPL(Value) , decltype(Value)

#define GET_TYPES(...) \
    VOXEL_FOREACH(GET_TYPES_IMPL, ##__VA_ARGS__)


GET_TYPES(A, B, C, D) // will expand to , decltype(A), decltype(B), decltype(C), decltype(D)
```

#### FVoxelSingleton

Easily create global ticking singletons. Do prefer subsystems to this for gameplay features - mainly useful for backends.

```cpp
class FVoxelMaterialManager : public FVoxelSingleton
{
public:
    virtual void Initialize() override;
    virtual void Tick() override;
    virtual void Tick_Async() override; // Tick called as a background task for convenience
    virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
};
extern FVoxelMaterialManager* GVoxelMaterialManager;

// In cpp
FVoxelMaterialManager* GVoxelMaterialManager = new FVoxelMaterialManager();

// In code
GVoxelMaterialManager->DoSomething();
```

#### IVoxelWorldSubsystem

Thread-safe world subsystems, can be accessed from any thread & created on demand.
If you do not need the thread safety aspect, do use UWorldSubsystem instead.

```cpp
class VOXELGRAPHCORE_API FVoxelWorldChannelManager : public IVoxelWorldSubsystem
{
public:
    GENERATED_VOXEL_WORLD_SUBSYSTEM_BODY(FVoxelWorldChannelManager);

    virtual void Tick() override;
};

// In code, from any thread
FObjectKey MyWorld;
const TSharedRef<FVoxelWorldChannelManager> ChannelManager = FVoxelWorldChannelManager::Get(MyWorld);
```

#### Zip reader/writer

`FVoxelZipReader` and `FVoxelZipWriter` are easy-to-use zip wrappers.

### Credits

[miniz](https://github.com/richgel999/miniz)
[Transvoxel](https://transvoxel.org/)
