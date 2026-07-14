// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FPolyglyphSourceString;

/**
 * Reads source strings from the project's UE localization manifest so they can be pushed to
 * Polyglyph. The manifest is the single source of truth produced by the Localization
 * Dashboard's gather; this never crawls individual assets.
 */
class FPolyglyphManifest
{
public:
	/** Load the configured localization target's manifest and map every entry to a source
	 *  string. Returns false and fills OutError when no manifest exists or it is empty. */
	static bool GatherSourceStrings(TArray<FPolyglyphSourceString>& OutStrings, FString& OutError);
};
