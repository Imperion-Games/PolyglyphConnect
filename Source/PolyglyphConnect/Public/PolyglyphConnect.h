// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FPolyglyphLocalizationServiceProvider;

/**
 * Editor module for PolyglyphConnect. Owns the single Polyglyph localization service provider
 * and registers it with the "LocalizationService" modular feature so it appears in the native
 * Localization Dashboard's service picker.
 */
class FPolyglyphConnectModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	/** Load-on-demand access to the module. */
	static FPolyglyphConnectModule& Get();

	/** The one provider instance owned by this module. */
	FPolyglyphLocalizationServiceProvider& GetProvider() const;

private:
	/** The single provider registered with the LocalizationService feature registry. */
	TUniquePtr<FPolyglyphLocalizationServiceProvider> Provider;
};
