// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PolyglyphProjectSettings.generated.h"

/**
 * Project-wide connection settings for PolyglyphConnect (Project Settings > Plugins > Polyglyph).
 *
 * Stored in DefaultGame.ini and committed to source control, so the whole team shares the API
 * base URL, project slug, and localization target. The per-user API key lives separately in
 * UPolyglyphSettings so it is never committed.
 */
UCLASS(Config = Game, DefaultConfig, Meta = (DisplayName = "Polyglyph"))
class POLYGLYPHCONNECT_API UPolyglyphProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Sets the Project Settings category/section and the default base URL and target. */
	UPolyglyphProjectSettings();

	/** Base URL of the Polyglyph API, e.g. https://api.polyglyph.app (no trailing slash). */
	UPROPERTY(Config, EditAnywhere, Category = "Connection", Meta = (DisplayName = "API Base URL"))
	FString BaseUrl;

	/** Project slug exactly as shown in the Polyglyph dashboard. */
	UPROPERTY(Config, EditAnywhere, Category = "Connection", Meta = (DisplayName = "Project Slug"))
	FString ProjectSlug;

	/** Name of the UE localization target to gather source strings from (defaults to "Game"). */
	UPROPERTY(Config, EditAnywhere, Category = "Localization", Meta = (DisplayName = "Localization Target"))
	FString LocalizationTarget;

	/** Optional project-relative JSON file that adds or replaces shipped Unreal locale mappings. */
	UPROPERTY(Config, EditAnywhere, Category = "Localization", Meta = (DisplayName = "Locale Mapping Overrides File"))
	FString LocaleMappingFile;
};
