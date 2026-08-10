// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphPush.h"

#include "PolyglyphClient.h"
#include "PolyglyphEnrich.h"
#include "PolyglyphManifest.h"
#include "PolyglyphTypes.h"

void FPolyglyphPush::Run(TFunction<void(bool, const FString&)> OnDone)
{
	TArray<FPolyglyphSourceString> Strings;
	TArray<FPolyglyphEnrichItem> EnrichItems;
	FString GatherError;
	if (!FPolyglyphManifest::Gather(Strings, EnrichItems, GatherError))
	{
		OnDone(false, GatherError);
		return;
	}

	const int32 SourceCount = Strings.Num();
	FPolyglyphClient::PushStrings(
		Strings,
		[SourceCount, EnrichItems = MoveTemp(EnrichItems), Done = MoveTemp(OnDone)](
			const FPolyglyphResponse& Response) mutable
		{
			if (!Response.bSuccess)
			{
				Done(false, FString::Printf(TEXT("Push failed: %s"), *Response.Error));
				return;
			}

			if (EnrichItems.Num() == 0)
			{
				Done(true, FString::Printf(TEXT("Pushed %d source string(s) to Polyglyph."), SourceCount));
				return;
			}

			const int32 EnrichCount = EnrichItems.Num();
			FPolyglyphEnrich::Push(
				EnrichItems,
				[SourceCount, EnrichCount, Done = MoveTemp(Done)](
					bool InSuccess,
					const FString& InSummary,
					int32 InUnmatchedCount) mutable
				{
					if (!InSuccess)
					{
						Done(false, FString::Printf(
							TEXT("Pushed %d source string(s), but automatic dialogue enrichment failed: %s"),
							SourceCount,
							*InSummary));
						return;
					}

					const int32 EnrichedCount = EnrichCount - InUnmatchedCount;
					Done(true, FString::Printf(
						TEXT("Pushed %d source string(s) and enriched %d native dialogue key(s). %s"),
						SourceCount,
						EnrichedCount,
						*InSummary));
				});
		});
}
