#pragma once

#include "Modules/ModuleManager.h"

class FShangCloudMMOModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
