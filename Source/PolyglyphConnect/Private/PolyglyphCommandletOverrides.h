// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Command-line override handling shared by the PolyglyphConnect commandlets.
 *
 * This previously lived in a per-file anonymous namespace in both
 * PolyglyphSyncCommandlet.cpp and PolyglyphEnrichCommandlet.cpp. Each .cpp is its own
 * translation unit in a non-unity build, so that compiled, but Unreal's default unity
 * build concatenates several .cpp files into one, merging those anonymous namespaces
 * into a single scope and producing a redefinition error. Anything shared lives here.
 */
namespace PolyglyphCommandletOverrides
{
    /**
     * Apply optional command-line overrides to the settings CDOs before a commandlet runs.
     *
     * Recognises -BaseUrl=, -ProjectSlug=, -LocalizationTarget= and -ApiKey=. When -ApiKey=
     * is absent the POLYGLYPH_API_KEY environment variable is used instead, which keeps the
     * key out of shell history and CI logs.
     */
    void Apply(const FString& InParams);
}
