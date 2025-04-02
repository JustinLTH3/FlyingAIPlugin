// Copyright Epic Games, Inc. All Rights Reserved.
//Test Change
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FFlyingAIPluginModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
