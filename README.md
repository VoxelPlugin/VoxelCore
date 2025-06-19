# Voxel Core

Voxel Core is an open-source plugin with the Core module of [Voxel Plugin](https://voxelplugin.com/).
It contains a bunch of high performance containers to replace the default Unreal ones, and a few helper macros & tools.

This plugin is intented to be used in C++: if your project is blueprint-only, this will be of no use to you.

If you have questions or suggestions about the code, feel free to ask on this discord: [discord.victorcareil.com](https://discord.victorcareil.com)

## Performance

### Map

- `TVoxelMap::FindChecked`: 1.4x faster than TMap, 1.2x faster than `TRobinHoodHashMap`
- `TVoxelMap::Remove`: 1.4x faster than TMap, 2.4x faster than `TRobinHoodHashMap`
- `TVoxelMap::FindOrAdd`: 1.2x faster than TMap, 3.7x faster than `TRobinHoodHashMap`
- `TVoxelMap::Reserve`: 17.4x faster than TMap, 4.5x faster than `TRobinHoodHashMap`

`TVoxelMap` is also quite a bit smaller than `TMap` and `TRobinHoodHashMap`:
- uint16 -> uint16 with 1M elements: `TVoxelMap` 9.6MB, `TMap` 13.6MB, `TRobinHoodHashMap` 19.8MB
- uint32 -> uint32 with 1M elements: `TVoxelMap` 13.4MB, `TMap` 17.4MB, `TRobinHoodHashMap` 23.6MB

Note that TVoxelSet performs similarly.

### Bit array

- `FVoxelBitArray::Add`: 1.9x faster than `TBitArray`
- `FVoxelBitArray::CountSetBits`: 6.6x faster than `TBitArray`

### Sparse array

- `TVoxelSparseArray::Add`: up to 1.7x faster than `TSparseArray`
- `TVoxelSparseArray::RemoveAt`: 1.5x faster than `TSparseArray`
- `Iterating TVoxelSparseArray`: up to 7.2x faster than `TSparseArray`
- `TVoxelSparseArray::Reserve`: 345x faster than `TSparseArray`

### Misc

- `TVoxelUniqueFunction`: 1.8x faster than `TUniqueFunction`
- `TVoxelArray::RemoveAtSwap`: 2.9x faster than `TArray` (size [here](https://github.com/EpicGames/UnrealEngine/blob/bee4336c7af93976a152b932bdff9a23cd564ed2/Engine/Source/Runtime/Core/Public/Containers/Array.h#L2038C26-L2038C27) isn't compile-time)

## Compatibility

Voxel Core usually compiles on the last few Unreal releases. Check branches for older engine versions.

## Getting started

Download this repo, put it in the Plugins folder of your project.
You can then add the VoxelCore dependency (or VoxelCoreEditor for editor features) to your module.

For runtime features, use `#include "VoxelMinimal.h"`.
For editor features, use `#include "VoxelEditorMinimal.h"`.

If you have a custom PCH, you should add these includes to it.

## Asserts

VoxelCore uses its own assertion macros (`checkVoxelSlow` etc). By default, they follow `DO_CHECK`. You can manually disable them by defining `VOXEL_DEBUG` to 0 before including `VoxelMinimal.h`

## Performance details

Here's the full output of [VoxelCoreBenchmark.cpp](https://github.com/VoxelPlugin/VoxelCore/blob/master/Source/VoxelCore/Private/VoxelCoreBenchmark.cpp), comparing voxel containers against engine ones in shipping with checks disabled.
This capture was done on a 7945HX.

```
Calling TUniqueFunction                            took 1.96ns    +- 0       and Calling TVoxelUniqueFunction                       took 1.09ns    +- 0       ====>  1.8x faster
TMap::FindChecked                                  took 1.45ns    +- 0.2     and TVoxelMap::FindChecked                             took 1.29ns    +- 0.2     ====>  1.1x faster
TMap::Remove                                       took 34.93ns   +- 3.1     and TVoxelMap::Remove                                  took 24.1ns    +- 2.3     ====>  1.4x faster
  Note: TVoxelMap::Remove does a RemoveAtSwap and doesn't keep order. TMap::Remove keeps order as long as no new element is added afterwards
TMap::FindOrAdd                                    took 0.95ns    +- 0.1     and TVoxelMap::FindOrAdd                               took 0.81ns    +- 0.1     ====>  1.2x faster
TMap::Add                                          took 7.51ns    +- 0.1     and TVoxelMap::Add_CheckNew                            took 2.14ns    +- 0.1     ====>  3.5x faster
  Note: TVoxelMap::Add_CheckNew can only be used when the element is not already in the map
TMap::Reserve(1M)                                  took 1.16ms    +- 0       and TVoxelMap::Reserve(1M)                             took 66.8us    +- 5.3     ====> 17.4x faster
  Note: TMap::Reserve builds a linked list on reserve, TVoxelMap::Reserve is a memset
TMap<uint16, uint16> with 1M elements: 13.6MB TVoxelMap<uint16, uint16> with 1M elements: 9.6MB
TMap<uint32, uint32> with 1M elements: 17.4MB TVoxelMap<uint32, uint32> with 1M elements: 13.4MB
TRobinHoodHashMap::Find                            took 1.42ns    +- 0.1     and TVoxelMap::FindChecked                             took 1.14ns    +- 0.1     ====>  1.2x faster
TRobinHoodHashMap::Remove + Add                    took 71.3ns    +- 6.3     and TVoxelMap::Remove + Add                            took 30.05ns   +- 1.5     ====>  2.4x faster
  Note: TRobinHoodHashMap::Remove can shrink the table. We add back the element we removed to avoid that.
TRobinHoodHashMap::FindOrAdd                       took 3.68ns    +- 0.1     and TVoxelMap::FindOrAdd                               took 0.99ns    +- 0.1     ====>  3.7x faster
TRobinHoodHashMap::Reserve(1M)                     took 298.26us  +- 6.7     and TVoxelMap::Reserve(1M)                             took 66.9us    +- 7       ====>  4.5x faster
TRobinHoodHashMap<uint16, uint16> with 1M elements: 19.8MB TVoxelMap<uint16, uint16> with 1M elements: 9.6MB
TRobinHoodHashMap<uint32, uint32> with 1M elements: 23.6MB TVoxelMap<uint32, uint32> with 1M elements: 13.4MB
TArray::RemoveAtSwap                               took 1.68ns    +- 0.1     and TVoxelArray::RemoveAtSwap                          took 0.58ns    +- 0       ====>  2.9x faster
  Note: TArray::RemoveAtSwap of one element does a memcpy with a non-compile-time size
TBitArray::Add                                     took 2.14ns    +- 0       and FVoxelBitArray::Add                                took 1.14ns    +- 0.1     ====>  1.9x faster
TBitArray::CountSetBits(1M)                        took 37.2us    +- 2.7     and FVoxelBitArray::CountSetBits(1M)                   took 5.64us    +- 0.6     ====>  6.6x faster
  Note: FVoxelBitArray::CountSetBits makes use of the popcount intrinsics
TSparseArray::Add (empty array)                    took 2.77ns    +- 0.1     and TVoxelSparseArray::Add (empty array)               took 1.64ns    +- 0       ====>  1.7x faster
TSparseArray::Add (full array, all removed)        took 2.76ns    +- 0.1     and TVoxelSparseArray::Add (full array, all removed)   took 2.75ns    +- 0       ====>  1.0x faster
TSparseArray::RemoveAt                             took 1.6ns     +- 0       and TVoxelSparseArray::RemoveAt                        took 1.04ns    +- 0.1     ====>  1.5x faster
Iterating TSparseArray of size 1M 0% empty         took 2.59ms    +- 0.1     and Iterating TVoxelSparseArray of size 1M 0% empty    took 359.0us   +- 8.1     ====>  7.2x faster
Iterating TSparseArray of size 1M 25% empty        took 1.85ms    +- 0       and Iterating TVoxelSparseArray of size 1M 25% empty   took 1.16ms    +- 0       ====>  1.6x faster
Iterating TSparseArray of size 1M 50% empty        took 1.35ms    +- 0.1     and Iterating TVoxelSparseArray of size 1M 50% empty   took 1.31ms    +- 0.1     ====>  1.0x faster
Iterating TSparseArray of size 1M 75% empty        took 803.95us  +- 18.6    and Iterating TVoxelSparseArray of size 1M 75% empty   took 544.82us  +- 12.1    ====>  1.5x faster
Iterating TSparseArray of size 1M 99% empty        took 292.23us  +- 15.4    and Iterating TVoxelSparseArray of size 1M 99% empty   took 181.23us  +- 6.8     ====>  1.6x faster
TSparseArray::Reserve(1M)                          took 848.0us   +- 20.8    and TVoxelSparseArray::Reserve(1M)                     took 2.45us    +- 0.7     ====> 345.6x faster
  Note: TSparseArray::Reserve creates a linked list, TVoxelSparseArray::Reserve is a memset
```


# Feature overview

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
