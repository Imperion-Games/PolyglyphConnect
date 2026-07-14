// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphProjectSettings.h"

UPolyglyphProjectSettings::UPolyglyphProjectSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("Polyglyph");
	// Local dev backend by default; point this at the deployed API once it is hosted.
	BaseUrl = TEXT("http://localhost:3000");
	// Default UE localization target; override if the project uses a different target name.
	LocalizationTarget = TEXT("Game");
}
