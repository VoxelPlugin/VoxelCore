// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelDeveloperSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ConfigUtilities.h"

UVoxelDeveloperSettings::UVoxelDeveloperSettings()
{
	CategoryName = "Plugins";
}

FName UVoxelDeveloperSettings::GetContainerName() const
{
	return "Project";
}

void UVoxelDeveloperSettings::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	if (IsTemplate())
	{
		ImportConsoleVariableValues();
	}
#endif
}

void UVoxelDeveloperSettings::PostCDOContruct()
{
	Super::PostCDOContruct();

	UE::ConfigUtilities::ApplyCVarSettingsFromIni(*GetClass()->GetPathName(), *GEngineIni, ECVF_SetByProjectSetting);

#if WITH_EDITOR
	ImportConsoleVariableValues();
#endif
}

#if WITH_EDITOR
void UVoxelDeveloperSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		ExportValuesToConsoleVariables(PropertyChangedEvent.Property);
	}

	SaveConfig(CPF_Config, *GetDefaultConfigFilename());
}
#endif