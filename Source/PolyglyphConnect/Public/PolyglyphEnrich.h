// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FPolyglyphEnrichItem;

/**
 * Builds and sends key-enrichment metadata (character voice, gender, register, max length,
 * context) to Polyglyph. Native DialogueWave metadata and manual binding-map CSV imports both
 * feed this single POST /api/plugin/enrich channel.
 */
class FPolyglyphEnrich
{
public:
	/** Parse a binding-map CSV into enrich items. Columns, in order:
	 *  namespace,key,character,gender,register,maxLength,context. The first row is a header and
	 *  is skipped; namespace and key are required per row, the rest are optional. Cells may be
	 *  double-quoted to carry commas ("" inside a quoted cell is a literal quote). Returns false
	 *  and fills OutError on failure. */
	static bool BuildFromCsv(const FString& InCsvPath, TArray<FPolyglyphEnrichItem>& OutItems, FString& OutError);

	/** Push the enrich items to Polyglyph (the client splits large batches under the server
	 *  caps). OnDone(bSuccess, Summary, UnmatchedCount) fires on the game thread; UnmatchedCount
	 *  is how many items matched no existing key server-side (they need a source push first). */
	static void Push(
		const TArray<FPolyglyphEnrichItem>& InItems,
		TFunction<void(bool, const FString&, int32)> OnDone);
};
