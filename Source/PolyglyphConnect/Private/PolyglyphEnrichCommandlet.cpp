// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphEnrichCommandlet.h"

#include "HAL/PlatformMisc.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

#include "PolyglyphClient.h"
#include "PolyglyphEnrich.h"
#include "PolyglyphProjectSettings.h"
#include "PolyglyphSettings.h"
#include "PolyglyphTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogPolyglyphEnrich, Log, All);

namespace
{
	/** Apply optional command-line / env overrides to the settings CDOs before enriching. */
	void ApplyOverrides(const FString& InParams)
	{
		FString Value;
		if (FParse::Value(*InParams, TEXT("BaseUrl="), Value))
		{
			GetMutableDefault<UPolyglyphProjectSettings>()->BaseUrl = Value;
		}
		if (FParse::Value(*InParams, TEXT("ProjectSlug="), Value))
		{
			GetMutableDefault<UPolyglyphProjectSettings>()->ProjectSlug = Value;
		}

		FString Key;
		if (!FParse::Value(*InParams, TEXT("ApiKey="), Key))
		{
			Key = FPlatformMisc::GetEnvironmentVariable(TEXT("POLYGLYPH_API_KEY"));
		}
		if (!Key.IsEmpty())
		{
			GetMutableDefault<UPolyglyphSettings>()->ApiKey = Key;
		}
	}
}

UPolyglyphEnrichCommandlet::UPolyglyphEnrichCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UPolyglyphEnrichCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches);
	const bool bStrict = Switches.Contains(TEXT("strict"));

	FString CsvPath;
	if (!FParse::Value(*Params, TEXT("csv="), CsvPath) || CsvPath.IsEmpty())
	{
		UE_LOG(LogPolyglyphEnrich, Error, TEXT("Missing -csv=<path> (binding-map CSV to import)."));
		return 1;
	}
	if (FPaths::IsRelative(CsvPath))
	{
		// Resolve against the project, not the process working directory, so the documented
		// -csv="Saved/..." form works no matter where the editor was launched from.
		CsvPath = FPaths::Combine(FPaths::ProjectDir(), CsvPath);
	}

	ApplyOverrides(Params);

	TArray<FPolyglyphEnrichItem> Items;
	FString BuildError;
	if (!FPolyglyphEnrich::BuildFromCsv(CsvPath, Items, BuildError))
	{
		UE_LOG(LogPolyglyphEnrich, Error, TEXT("Enrich aborted: %s"), *BuildError);
		return 1;
	}

	bool bDone = false;
	bool bOk = false;
	int32 UnmatchedCount = 0;
	FString Summary;
	FPolyglyphEnrich::Push(Items,
		[&bDone, &bOk, &Summary, &UnmatchedCount](bool bSuccess, const FString& InSummary, int32 InUnmatched)
	{
		bOk = bSuccess;
		Summary = InSummary;
		UnmatchedCount = InUnmatched;
		bDone = true;
	});

	// A big binding-map goes out as several sequential requests under the server caps, so scale
	// the wait with the import size instead of using one flat timeout.
	const double TimeoutSeconds = FMath::Max(300.0, Items.Num() / 50.0);
	FPolyglyphClient::PumpHttp(bDone, TimeoutSeconds);

	UE_LOG(LogPolyglyphEnrich, Display, TEXT("%s"), *Summary);
	if (bOk && bStrict && UnmatchedCount > 0)
	{
		UE_LOG(LogPolyglyphEnrich, Error,
			TEXT("-strict: %d row(s) did not match an existing key."), UnmatchedCount);
		return 1;
	}
	return bOk ? 0 : 1;
}
