// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "PolyglyphEnrichCommandlet.generated.h"

/**
 * Headless key enrichment for CI or manual runs. Imports a binding-map CSV of key -> character
 * (and optional gender / register / max length / context) and pushes it to Polyglyph, so
 * translations honour each speaker's voice. Run under UnrealEditor-Cmd:
 *
 *   UnrealEditor-Cmd <Project> -run=PolyglyphEnrich -csv="Path/To/bindings.csv"
 *
 * A relative -csv path resolves against the project directory. Run it after a source push:
 * enrich never creates keys, and any rows whose keys are not in the project yet are reported
 * back as unmatched.
 *
 * Connection settings come from config, overridable on the command line; the API key falls back
 * to the POLYGLYPH_API_KEY environment variable so CI never needs the per-user ini.
 *
 * Switches: -strict (exit non-zero when any row is unmatched; default tolerates them).
 * Values:   -csv= (required), -ApiKey=, -BaseUrl=, -ProjectSlug= (all optional).
 */
UCLASS()
class UPolyglyphEnrichCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UPolyglyphEnrichCommandlet();

	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface
};
