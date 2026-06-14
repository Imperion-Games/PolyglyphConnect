// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PolyglyphSettings.generated.h"

/**
 * Per-user editor settings for PolyglyphConnect (Editor Preferences > Plugins > Polyglyph).
 *
 * Stored in EditorPerProjectUserSettings (under Saved/, gitignored), so the API key is
 * never committed to source control. Base URL and project slug live here too, so each
 * developer configures their own connection; a shared, committed project config can be
 * split out later if a team wants the slug checked in.
 */
UCLASS(Config = EditorPerProjectUserSettings, Meta = (DisplayName = "Polyglyph"))
class POLYGLYPHCONNECT_API UPolyglyphSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Sets the Editor Preferences category/section and the default base URL. */
	UPolyglyphSettings();

	/** Base URL of the Polyglyph API, e.g. https://api.polyglyph.app (no trailing slash). */
	UPROPERTY(Config, EditAnywhere, Category = "Connection", Meta = (DisplayName = "API Base URL"))
	FString BaseUrl;

	/** Project slug exactly as shown in the Polyglyph dashboard. */
	UPROPERTY(Config, EditAnywhere, Category = "Connection", Meta = (DisplayName = "Project Slug"))
	FString ProjectSlug;

	/** API key sent as the X-Polyglyph-Key header. Generate one in the dashboard; stored
	 *  per-user and never committed. */
	UPROPERTY(Config, EditAnywhere, Category = "Connection",
		Meta = (DisplayName = "API Key", PasswordField = true))
	FString ApiKey;
};
