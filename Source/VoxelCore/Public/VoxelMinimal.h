// Copyright Voxel Plugin SAS. All Rights Reserved.

#ifdef CANNOT_INCLUDE_VOXEL_MINIMAL
#error "VoxelMinimal.h recursively included"
#endif

#ifndef VOXEL_MINIMAL_INCLUDED
#define VOXEL_MINIMAL_INCLUDED
#define CANNOT_INCLUDE_VOXEL_MINIMAL

// Useful when compiling with clang
// Add this to "%appdata%\Unreal Engine\UnrealBuildTool\BuildConfiguration.xml"
/*
<?xml version="1.0" encoding="utf-8" ?>
<Configuration xmlns="https://www.unrealengine.com/BuildConfiguration">
    <WindowsPlatform>
         <Compiler>Clang</Compiler>
    </WindowsPlatform>
	<ParallelExecutor>
		<ProcessorCountMultiplier>2</ProcessorCountMultiplier>
		<MemoryPerActionBytes>0</MemoryPerActionBytes>
	</ParallelExecutor>
</Configuration>
*/
#if 0
#undef DLLEXPORT
#undef DLLIMPORT
#define DLLEXPORT
#define DLLIMPORT
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#include "VoxelCoreMinimal.h"

#include "VoxelMinimal/VoxelArchive.h"
#include "VoxelMinimal/VoxelAsset.h"
#include "VoxelMinimal/VoxelAtomic.h"
#include "VoxelMinimal/VoxelAutoFactoryInterface.h"
#include "VoxelMinimal/VoxelAxis.h"
#include "VoxelMinimal/VoxelBitWriter.h"
#include "VoxelMinimal/VoxelBox.h"
#include "VoxelMinimal/VoxelBox2D.h"
#include "VoxelMinimal/VoxelChunkedRef.h"
#include "VoxelMinimal/VoxelColor3.h"
#include "VoxelMinimal/VoxelCoreCommands.h"
#include "VoxelMinimal/VoxelCriticalSection.h"
#include "VoxelMinimal/VoxelSharedCriticalSection.h"
#include "VoxelMinimal/VoxelDebugDrawer.h"
#include "VoxelMinimal/VoxelDelegateHelpers.h"
#include "VoxelMinimal/VoxelDependencyCollector.h"
#include "VoxelMinimal/VoxelDependencyTracker.h"
#include "VoxelMinimal/VoxelDereferencingIterator.h"
#include "VoxelMinimal/VoxelDuplicateTransient.h"
#include "VoxelMinimal/VoxelFastBox.h"
#include "VoxelMinimal/VoxelFunctionRef.h"
#include "VoxelMinimal/VoxelFuture.h"
#include "VoxelMinimal/VoxelGlobalShader.h"
#include "VoxelMinimal/VoxelGuid.h"
#include "VoxelMinimal/VoxelInstancedStruct.h"
#include "VoxelMinimal/VoxelIntBox.h"
#include "VoxelMinimal/VoxelIntBox2D.h"
#include "VoxelMinimal/VoxelInterval.h"
#include "VoxelMinimal/VoxelIntInterval.h"
#include "VoxelMinimal/VoxelISPC.h"
#include "VoxelMinimal/VoxelIterate.h"
#include "VoxelMinimal/VoxelParallelFor.h"
#include "VoxelMinimal/VoxelMaterialRef.h"
#include "VoxelMinimal/VoxelMessageFactory.h"
#include "VoxelMinimal/VoxelMessageManager.h"
#include "VoxelMinimal/VoxelNotification.h"
#include "VoxelMinimal/VoxelObjectHelpers.h"
#include "VoxelMinimal/VoxelObjectPtr.h"
#include "VoxelMinimal/VoxelOctahedron.h"
#include "VoxelMinimal/VoxelOptional.h"
#include "VoxelMinimal/VoxelRange.h"
#include "VoxelMinimal/VoxelRefCountPtr.h"
#include "VoxelMinimal/VoxelResourceArrayRef.h"
#include "VoxelMinimal/VoxelSerializationGuard.h"
#include "VoxelMinimal/VoxelSharedPtr.h"
#include "VoxelMinimal/VoxelSingleton.h"
#include "VoxelMinimal/VoxelStructView.h"
#include "VoxelMinimal/VoxelSuccess.h"
#include "VoxelMinimal/VoxelTicker.h"
#include "VoxelMinimal/VoxelUniqueFunction.h"
#include "VoxelMinimal/VoxelVirtualStruct.h"
#include "VoxelMinimal/VoxelWeakPropertyPtr.h"
#include "VoxelMinimal/VoxelWorldSubsystem.h"

#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"
#include "VoxelMinimal/Containers/VoxelBitArray.h"
#include "VoxelMinimal/Containers/VoxelBitArrayView.h"
#include "VoxelMinimal/Containers/VoxelChunkedArray.h"
#include "VoxelMinimal/Containers/VoxelChunkedSparseArray.h"
#include "VoxelMinimal/Containers/VoxelLinkedArray.h"
#include "VoxelMinimal/Containers/VoxelMap.h"
#include "VoxelMinimal/Containers/VoxelSet.h"
#include "VoxelMinimal/Containers/VoxelSparseArray.h"
#include "VoxelMinimal/Containers/VoxelStaticArray.h"
#include "VoxelMinimal/Containers/VoxelStaticBitArray.h"

#include "VoxelMinimal/Utilities/VoxelArrayUtilities.h"
#include "VoxelMinimal/Utilities/VoxelDistanceFieldUtilities.h"
#include "VoxelMinimal/Utilities/VoxelGameUtilities.h"
#include "VoxelMinimal/Utilities/VoxelGeometryUtilities.h"
#include "VoxelMinimal/Utilities/VoxelHashUtilities.h"
#include "VoxelMinimal/Utilities/VoxelInterpolationUtilities.h"
#include "VoxelMinimal/Utilities/VoxelIntPointUtilities.h"
#include "VoxelMinimal/Utilities/VoxelIntVectorUtilities.h"
#include "VoxelMinimal/Utilities/VoxelLambdaUtilities.h"
#include "VoxelMinimal/Utilities/VoxelMapUtilities.h"
#include "VoxelMinimal/Utilities/VoxelMaterialUtilities.h"
#include "VoxelMinimal/Utilities/VoxelMathUtilities.h"
#include "VoxelMinimal/Utilities/VoxelObjectUtilities.h"
#include "VoxelMinimal/Utilities/VoxelRenderUtilities.h"
#include "VoxelMinimal/Utilities/VoxelStringUtilities.h"
#include "VoxelMinimal/Utilities/VoxelSystemUtilities.h"
#include "VoxelMinimal/Utilities/VoxelTextureUtilities.h"
#include "VoxelMinimal/Utilities/VoxelThreadingUtilities.h"
#include "VoxelMinimal/Utilities/VoxelTransformUtilities.h"
#include "VoxelMinimal/Utilities/VoxelTypeUtilities.h"
#include "VoxelMinimal/Utilities/VoxelVectorUtilities.h"

#undef CANNOT_INCLUDE_VOXEL_MINIMAL
#endif