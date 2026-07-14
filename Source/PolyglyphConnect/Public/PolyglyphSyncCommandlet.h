// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "PolyglyphSyncCommandlet.generated.h"

/**
 * Headless push/pull for CI. Run under UnrealEditor-Cmd:
 *
 *   UnrealEditor-Cmd <Project> -run=PolyglyphSync -push -pull
 *
 * Reuses the same static sync classes the dashboard uses (no editor UI). Connection settings
 * come from config, but can be overridden on the command line, and the API key falls back to
 * the POLYGLYPH_API_KEY environment variable so CI never needs the per-user ini.
 *
 * Switches: -push, -pull (at least one required).
 * Values:   -ApiKey=, -BaseUrl=, -ProjectSlug=, -LocalizationTarget= (all optional).
 * Gather and Compile remain separate GatherText commandlet steps run around this one.
 */
UCLASS()
class UPolyglyphSyncCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UPolyglyphSyncCommandlet();

	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface
};
