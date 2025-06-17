// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

namespace Voxel::Tests
{

struct FMyStruct
{
private:
	int32 MyValue = 0;
	int32 MyValue2 = 0;
};

struct FMyStruct2
{
private:
	int32 MyValue = 0;
};

DEFINE_PRIVATE_ACCESS(FMyStruct, MyValue);
DEFINE_PRIVATE_ACCESS(FMyStruct, MyValue2);
DEFINE_PRIVATE_ACCESS(FMyStruct2, MyValue);


void Test()
{
	FMyStruct MyStruct;
	PrivateAccess::MyValue(MyStruct) = 0;
}

}