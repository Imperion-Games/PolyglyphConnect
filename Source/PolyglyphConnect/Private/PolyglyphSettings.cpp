// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphSettings.h"

UPolyglyphSettings::UPolyglyphSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("Polyglyph");
	// Local dev backend by default; point this at the deployed API once it is hosted.
	BaseUrl = TEXT("http://localhost:3000");
}
