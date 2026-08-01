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
	/** Split one CSV line into cells. Cells may be double-quoted to carry commas; inside a quoted
	 *  cell, "" is a literal quote. Quotes elsewhere in an unquoted cell are kept as-is. */
	TArray<FString> ParseCsvLine(const FString& InLine)
	{
		TArray<FString> Cells;
		FString Current;
		bool bInQuotes = false;

		for (int32 CharIndex = 0; CharIndex < InLine.Len(); ++CharIndex)
		{
			const TCHAR Char = InLine[CharIndex];
			if (bInQuotes)
			{
				if (Char == TEXT('"'))
				{
					if (CharIndex + 1 < InLine.Len() && InLine[CharIndex + 1] == TEXT('"'))
					{
						Current.AppendChar(TEXT('"'));
						++CharIndex;
					}
					else
					{
						bInQuotes = false;
					}
				}
				else
				{
					Current.AppendChar(Char);
				}
			}
			else if (Char == TEXT('"') && Current.IsEmpty())
			{
				bInQuotes = true;
			}
			else if (Char == TEXT(','))
			{
				Cells.Add(MoveTemp(Current));
				Current.Reset();
			}
			else
			{
				Current.AppendChar(Char);
			}
		}

		Cells.Add(MoveTemp(Current));
		return Cells;
	}

	/** Read a cell if present, trimmed of surrounding whitespace. */
	FString Cell(const TArray<FString>& InCells, int32 InIndex)
	{
		return InCells.IsValidIndex(InIndex) ? InCells[InIndex].TrimStartAndEnd() : FString();
	}

	/** Collect the "namespace/key" pairs of an aggregated /enrich response's unmatched array. */
	TArray<FString> CollectUnmatched(const TSharedPtr<FJsonObject>& InJson)
	{
		TArray<FString> Unmatched;
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (InJson.IsValid() && InJson->TryGetArrayField(TEXT("unmatched"), Values) && Values != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& Value : *Values)
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
		return Unmatched;
	}

	/** Build the human-readable summary. Enrich only binds keys that already exist, so unmatched
	 *  (namespace, key) pairs are keys not pushed yet; list a capped sample so the caller knows
	 *  what to push first without flooding the log. */
	FString SummariseEnrich(const TSharedPtr<FJsonObject>& InJson, const TArray<FString>& InUnmatched)
	{
		int32 Total = 0;
		int32 Updated = 0;
		if (InJson.IsValid())
		{
			InJson->TryGetNumberField(TEXT("total"), Total);
			InJson->TryGetNumberField(TEXT("updated"), Updated);
		}

		FString Summary = FString::Printf(
			TEXT("Enrich complete: bound %d key(s) from %d item(s)."), Updated, Total);

		if (InUnmatched.Num() > 0)
		{
			const int32 ShowMax = 10;
			const TArray<FString> Shown(InUnmatched.GetData(), FMath::Min(InUnmatched.Num(), ShowMax));
			FString List = FString::Join(Shown, TEXT(", "));
			if (InUnmatched.Num() > ShowMax)
			{
				List += FString::Printf(TEXT(", and %d more"), InUnmatched.Num() - ShowMax);
			}
			Summary += FString::Printf(
				TEXT(" %d not in the project yet (push these keys first): %s"),
				InUnmatched.Num(), *List);
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

		const TArray<FString> Cells = ParseCsvLine(Line);

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

void FPolyglyphEnrich::Push(
	const TArray<FPolyglyphEnrichItem>& InItems,
	TFunction<void(bool, const FString&, int32)> OnDone)
{
	if (InItems.Num() == 0)
	{
		OnDone(false, TEXT("No enrichment items to send."), 0);
		return;
	}

	FPolyglyphClient::EnrichStrings(InItems,
		[Done = MoveTemp(OnDone)](const FPolyglyphResponse& Response)
		{
			if (!Response.bSuccess)
			{
				Done(false, FString::Printf(TEXT("Enrich failed: %s"), *Response.Error), 0);
				return;
			}

			const TArray<FString> Unmatched = CollectUnmatched(Response.Json);
			Done(true, SummariseEnrich(Response.Json, Unmatched), Unmatched.Num());
		});
}
