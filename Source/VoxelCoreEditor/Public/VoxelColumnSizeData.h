// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"

class VOXELCOREEDITOR_API FVoxelColumnSizeData
{
public:
	void SetValueWidth(
		float NewWidth,
		int32 Indentation,
		float CurrentSize);

	float GetNameWidth(
		int32 Indentation,
		float CurrentSize) const;
	float GetValueWidth(
		int32 Indentation,
		float CurrentSize) const;

public:
	int32 GetHoveredSplitterIndex() const
	{
		return HoveredSplitterIndex;
	}
	void SetHoveredSplitterIndex(const int32 HoveredIndex)
	{
		HoveredSplitterIndex = HoveredIndex;
	}

private:
	static constexpr float IndentationValue = 10.f;

	float ValueWidth = 0.4f;
	int32 HoveredSplitterIndex = -1;
};