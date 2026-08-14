// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphProjectSettings.h"

UPolyglyphProjectSettings::UPolyglyphProjectSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("Polyglyph");
	// Hosted Polyglyph API. Override for a self-hosted backend, or point at a local
	// dev server (typically http://localhost:3000) when working on the service itself.
	BaseUrl = TEXT("https://api.polyglyph.app");
	// Default UE localization target; override if the project uses a different target name.
	LocalizationTarget = TEXT("Game");
	bIncludeUnapprovedDrafts = false;
}
