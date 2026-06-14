// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/** Editor module for PolyglyphConnect: registers the Polyglyph menu and its actions. */
class FPolyglyphConnectModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

private:
	/** Register the Polyglyph submenu under the level editor's Tools menu. */
	void RegisterMenus();

	/** Ping GET /api/plugin/status with the configured key/slug and report the result. */
	void OnTestConnection();

	/** Push source strings to Polyglyph. Currently sends a probe payload until the
	 *  Unreal localization gather is wired in. */
	void OnPushSourceStrings();
};
