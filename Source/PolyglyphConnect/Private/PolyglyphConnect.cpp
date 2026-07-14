// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphConnect.h"

#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

#include "PolyglyphLocalizationServiceProvider.h"

IMPLEMENT_MODULE(FPolyglyphConnectModule, PolyglyphConnect)

namespace
{
	/** Modular feature name every localization service provider registers under. */
	const FName LocalizationServiceFeatureName(TEXT("LocalizationService"));
}

void FPolyglyphConnectModule::StartupModule()
{
	Provider = MakeUnique<FPolyglyphLocalizationServiceProvider>();
	IModularFeatures::Get().RegisterModularFeature(LocalizationServiceFeatureName, Provider.Get());
}

void FPolyglyphConnectModule::ShutdownModule()
{
	if (Provider.IsValid())
	{
		Provider->Close();
		IModularFeatures::Get().UnregisterModularFeature(LocalizationServiceFeatureName, Provider.Get());
		Provider.Reset();
	}
}

FPolyglyphConnectModule& FPolyglyphConnectModule::Get()
{
	return FModuleManager::LoadModuleChecked<FPolyglyphConnectModule>("PolyglyphConnect");
}

FPolyglyphLocalizationServiceProvider& FPolyglyphConnectModule::GetProvider() const
{
	check(Provider.IsValid());
	return *Provider;
}
