// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FPolyglyphEnrichItem;

/**
 * Builds and sends key-enrichment metadata (character voice, gender, register, max length,
 * context) to Polyglyph. This is the manual/customized feed that the flattened manifest cannot
 * carry: a studio maps keys to speakers in a binding-map CSV, and this pushes them to the
 * planned POST /api/plugin/enrich endpoint. The counterpart to the source-string push.
 */
class FPolyglyphEnrich
{
public:
	/** Parse a binding-map CSV into enrich items. Columns, in order:
	 *  namespace,key,character,gender,register,maxLength,context. The first row is a header and
	 *  is skipped; namespace, key and character are required per row, the rest are optional.
	 *  Simple comma split (no quoted-comma support). Returns false and fills OutError on failure. */
	static bool BuildFromCsv(const FString& InCsvPath, TArray<FPolyglyphEnrichItem>& OutItems, FString& OutError);

	/** Push the enrich items to Polyglyph. OnDone(bSuccess, Summary) fires on the game thread. */
	static void Push(const TArray<FPolyglyphEnrichItem>& InItems, TFunction<void(bool, const FString&)> OnDone);
};
