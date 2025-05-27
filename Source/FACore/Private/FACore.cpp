#include "FACore.h"

#include "FAPathfindingSettings.h"
#include "ISettingsModule.h"

#define LOCTEXT_NAMESPACE "FFACoreModule"

void FFACoreModule::StartupModule()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "FlyingAIPlugin_Settings",
		                                 LOCTEXT("RuntimeSettingsName", "Flying AI Plugin"),
		                                 LOCTEXT("RuntimeSettingsDescription",
		                                         "Configure my setting"),
		                                 GetMutableDefault<UFAPathfindingSettings>());
	}
}

void FFACoreModule::ShutdownModule()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "FlyingAIPlugin_Settings");
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FFACoreModule, FACore)
