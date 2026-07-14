// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphEnrich.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "PolyglyphClient.h"
#include "PolyglyphTypes.h"

namespace
{
	/** Read a cell if present, trimmed of surrounding whitespace. */
	FString Cell(const TArray<FString>& InCells, int32 InIndex)
	{
		return InCells.IsValidIndex(InIndex) ? InCells[InIndex].TrimStartAndEnd() : FString();
	}

	/** The server caps /api/plugin/enrich at 5000 items per request, so a larger binding-map is
	 *  sent as sequential chunks of this size. */
	constexpr int32 GEnrichChunkSize = 5000;

	/** Running totals across all chunks of one enrich push. */
	struct FEnrichAggregate
	{
		/** Items sent, summed over chunks. */
		int32 Total = 0;

		/** Keys that received at least one metadata change, summed over chunks. */
		int32 Updated = 0;

		/** "namespace/key" pairs that matched no existing key (need a push first). */
		TArray<FString> Unmatched;
	};

	/** Fold one /enrich response body ({ updated, unmatched, total }) into the running totals. */
	void AccumulateEnrich(const TSharedPtr<FJsonObject>& InJson, FEnrichAggregate& InOutAggregate)
	{
		if (!InJson.IsValid())
		{
			return;
		}

		int32 Total = 0;
		int32 Updated = 0;
		InJson->TryGetNumberField(TEXT("total"), Total);
		InJson->TryGetNumberField(TEXT("updated"), Updated);
		InOutAggregate.Total += Total;
		InOutAggregate.Updated += Updated;

		const TArray<TSharedPtr<FJsonValue>>* UnmatchedValues = nullptr;
		if (InJson->TryGetArrayField(TEXT("unmatched"), UnmatchedValues) && UnmatchedValues != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& Value : *UnmatchedValues)
			{
				const TSharedPtr<FJsonObject> Object = Value->AsObject();
				if (Object.IsValid())
				{
					FString Namespace, Key;
					Object->TryGetStringField(TEXT("namespace"), Namespace);
					Object->TryGetStringField(TEXT("key"), Key);
					InOutAggregate.Unmatched.Add(FString::Printf(TEXT("%s/%s"), *Namespace, *Key));
				}
			}
		}
	}

	/** Build the final human-readable summary. Enrich only binds keys that already exist, so any
	 *  unmatched (namespace, key) pairs are keys that were not pushed yet; they are listed (capped)
	 *  so the caller knows to push them first. */
	FString SummariseAggregate(const FEnrichAggregate& InAggregate)
	{
		FString Summary = FString::Printf(
			TEXT("Enrich complete: bound %d key(s) from %d item(s)."),
			InAggregate.Updated, InAggregate.Total);

		if (InAggregate.Unmatched.Num() > 0)
		{
			// List a handful so the log stays readable; the count tells the full story.
			const int32 ShowMax = 10;
			const TArray<FString> Shown(
				InAggregate.Unmatched.GetData(), FMath::Min(InAggregate.Unmatched.Num(), ShowMax));
			FString List = FString::Join(Shown, TEXT(", "));
			if (InAggregate.Unmatched.Num() > ShowMax)
			{
				List += FString::Printf(TEXT(", and %d more"), InAggregate.Unmatched.Num() - ShowMax);
			}
			Summary += FString::Printf(
				TEXT(" %d not in the project yet (push these keys first): %s"),
				InAggregate.Unmatched.Num(), *List);
		}

		return Summary;
	}

	/** Send the chunk starting at InStart, then recurse to the next chunk on success. All state is
	 *  shared so it survives the async callbacks (which fire on the game thread, so no atomics). */
	void SendEnrichChunk(
		const TSharedRef<TArray<FPolyglyphEnrichItem>>& InItems,
		int32 InStart,
		const TSharedRef<FEnrichAggregate>& InAggregate,
		const TSharedRef<TFunction<void(bool, const FString&)>>& InDone)
	{
		const int32 ChunkCount = FMath::Min(GEnrichChunkSize, InItems->Num() - InStart);
		const TArray<FPolyglyphEnrichItem> Chunk(InItems->GetData() + InStart, ChunkCount);

		FPolyglyphClient::EnrichStrings(Chunk,
			[InItems, InStart, InAggregate, InDone](const FPolyglyphResponse& Response)
			{
				if (!Response.bSuccess)
				{
					// Earlier chunks already committed server-side; say how many bound before the failure.
					const FString Message = InAggregate->Updated > 0
						? FString::Printf(TEXT("Enrich failed after binding %d key(s): %s"),
							InAggregate->Updated, *Response.Error)
						: FString::Printf(TEXT("Enrich failed: %s"), *Response.Error);
					(*InDone)(false, Message);
					return;
				}

				AccumulateEnrich(Response.Json, *InAggregate);

				const int32 Next = InStart + GEnrichChunkSize;
				if (Next < InItems->Num())
				{
					SendEnrichChunk(InItems, Next, InAggregate, InDone);
				}
				else
				{
					(*InDone)(true, SummariseAggregate(*InAggregate));
				}
			});
	}
}

bool FPolyglyphEnrich::BuildFromCsv(const FString& InCsvPath, TArray<FPolyglyphEnrichItem>& OutItems, FString& OutError)
{
	OutItems.Reset();

	const FString CsvPath = FPaths::ConvertRelativePathToFull(InCsvPath);
	if (!FPaths::FileExists(CsvPath))
	{
		OutError = FString::Printf(TEXT("Binding-map CSV not found: %s"), *CsvPath);
		return false;
	}

	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *CsvPath))
	{
		OutError = FString::Printf(TEXT("Could not read %s"), *CsvPath);
		return false;
	}

	// Skip the header row (index 0); every later non-empty line is a binding.
	for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
	{
		const FString& Line = Lines[LineIndex];
		if (Line.TrimStartAndEnd().IsEmpty())
		{
			continue;
		}

		TArray<FString> Cells;
		Line.ParseIntoArray(Cells, TEXT(","), false);

		FPolyglyphEnrichItem Item;
		Item.Namespace = Cell(Cells, 0);
		Item.Key = Cell(Cells, 1);
		Item.Character = Cell(Cells, 2);
		Item.Gender = Cell(Cells, 3);
		Item.Register = Cell(Cells, 4);

		const FString MaxLengthCell = Cell(Cells, 5);
		if (!MaxLengthCell.IsEmpty())
		{
			Item.MaxLength = FCString::Atoi(*MaxLengthCell);
		}

		Item.Context = Cell(Cells, 6);

		if (Item.Namespace.IsEmpty() || Item.Key.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Row %d is missing a namespace or key."), LineIndex + 1);
			return false;
		}

		OutItems.Add(MoveTemp(Item));
	}

	if (OutItems.Num() == 0)
	{
		OutError = TEXT("The binding-map CSV has no data rows.");
		return false;
	}

	return true;
}

void FPolyglyphEnrich::Push(const TArray<FPolyglyphEnrichItem>& InItems, TFunction<void(bool, const FString&)> OnDone)
{
	if (InItems.Num() == 0)
	{
		OnDone(false, TEXT("No enrichment items to send."));
		return;
	}

	// Send in sequential chunks (the server caps one request at GEnrichChunkSize). Shared storage
	// keeps the items and running totals alive across the async chunk callbacks.
	const TSharedRef<TArray<FPolyglyphEnrichItem>> Items = MakeShared<TArray<FPolyglyphEnrichItem>>(InItems);
	const TSharedRef<FEnrichAggregate> Aggregate = MakeShared<FEnrichAggregate>();
	const TSharedRef<TFunction<void(bool, const FString&)>> Done =
		MakeShared<TFunction<void(bool, const FString&)>>(MoveTemp(OnDone));

	SendEnrichChunk(Items, 0, Aggregate, Done);
}
