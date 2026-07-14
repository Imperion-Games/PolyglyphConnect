// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PolyglyphSettings.generated.h"

/**
 * Per-user editor settings for PolyglyphConnect (Editor Preferences > Plugins > Polyglyph).
 *
 * Holds only the API key, stored in EditorPerProjectUserSettings (under Saved/, gitignored),
 * so the key is never committed. Shared connection fields (base URL, project slug, target)
 * live in UPolyglyphProjectSettings and are committed for the whole team.
 */
UCLASS(Config = EditorPerProjectUserSettings, Meta = (DisplayName = "Polyglyph"))
class POLYGLYPHCONNECT_API UPolyglyphSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Sets the Editor Preferences category/section. */
	UPolyglyphSettings();

	/** API key sent as the X-Polyglyph-Key header. Generate one in the dashboard; stored
	 *  per-user and never committed. */
	UPROPERTY(Config, EditAnywhere, Category = "Connection",
		Meta = (DisplayName = "API Key", PasswordField = true))
	FString ApiKey;
};
