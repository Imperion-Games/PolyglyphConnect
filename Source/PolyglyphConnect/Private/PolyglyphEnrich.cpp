// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphEnrich.h"

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
	const int32 Count = InItems.Num();
	FPolyglyphClient::EnrichStrings(InItems,
		[Count, Done = MoveTemp(OnDone)](const FPolyglyphResponse& Response)
		{
			if (Response.bSuccess)
			{
				Done(true, FString::Printf(TEXT("Enriched %d key(s)."), Count));
			}
			else
			{
				Done(false, FString::Printf(TEXT("Enrich failed: %s"), *Response.Error));
			}
		});
}
