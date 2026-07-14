// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FSpawnTabArgs;
class SDockTab;

/** Editor module for PolyglyphConnect: registers the single Polyglyph dashboard tab + menu entry. */
class FPolyglyphConnectModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

private:
	/** Register the single `Tools > Polyglyph` entry that opens the dashboard. */
	void RegisterMenus();

	/** Spawn the dockable Polyglyph dashboard tab. */
	TSharedRef<SDockTab> OnSpawnDashboardTab(const FSpawnTabArgs& InArgs);

	/** Open or focus the Polyglyph dashboard tab. */
	void OpenDashboard();
};
