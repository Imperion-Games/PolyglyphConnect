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

	/** Build a human-readable summary from an /enrich response body ({ updated, unmatched, total }).
	 *  Enrich only binds keys that already exist, so any unmatched (namespace, key) pairs are the
	 *  keys that were not pushed yet; they are listed so the caller knows to push them first. */
	FString SummariseEnrich(const TSharedPtr<FJsonObject>& InJson)
	{
		if (!InJson.IsValid())
		{
			return TEXT("Enrich succeeded, but the server sent no summary.");
		}

		int32 Total = 0;
		int32 Updated = 0;
		InJson->TryGetNumberField(TEXT("total"), Total);
		InJson->TryGetNumberField(TEXT("updated"), Updated);

		TArray<FString> Unmatched;
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
					Unmatched.Add(FString::Printf(TEXT("%s/%s"), *Namespace, *Key));
				}
			}
		}

		FString Summary = FString::Printf(
			TEXT("Enrich complete: bound %d key(s) from %d item(s)."), Updated, Total);

		if (Unmatched.Num() > 0)
		{
			// List a handful so the log stays readable; the count tells the full story.
			const int32 ShowMax = 10;
			TArray<FString> Shown(Unmatched.GetData(), FMath::Min(Unmatched.Num(), ShowMax));
			FString List = FString::Join(Shown, TEXT(", "));
			if (Unmatched.Num() > ShowMax)
			{
				List += FString::Printf(TEXT(", and %d more"), Unmatched.Num() - ShowMax);
			}
			Summary += FString::Printf(
				TEXT(" %d not in the project yet (push these keys first): %s"),
				Unmatched.Num(), *List);
		}

		return Summary;
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
	FPolyglyphClient::EnrichStrings(InItems,
		[Done = MoveTemp(OnDone)](const FPolyglyphResponse& Response)
		{
			if (Response.bSuccess)
			{
				Done(true, SummariseEnrich(Response.Json));
			}
			else
			{
				Done(false, FString::Printf(TEXT("Enrich failed: %s"), *Response.Error));
			}
		});
}
