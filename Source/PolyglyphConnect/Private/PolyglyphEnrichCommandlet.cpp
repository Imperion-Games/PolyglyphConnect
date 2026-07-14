// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphEnrichCommandlet.h"

#include "HAL/PlatformMisc.h"
#include "Misc/Parse.h"

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
	FString CsvPath;
	if (!FParse::Value(*Params, TEXT("csv="), CsvPath) || CsvPath.IsEmpty())
	{
		UE_LOG(LogPolyglyphEnrich, Error, TEXT("Missing -csv=<path> (binding-map CSV to import)."));
		return 1;
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
	FString Summary;
	FPolyglyphEnrich::Push(Items, [&bDone, &bOk, &Summary](bool bSuccess, const FString& InSummary)
	{
		bOk = bSuccess;
		Summary = InSummary;
		bDone = true;
	});

	// Push sends one request per 5000 items (the server cap), sequentially, so give the wait
	// headroom for every chunk rather than a single flat timeout.
	const double TimeoutSeconds = FMath::Max(300.0, FMath::DivideAndRoundUp(Items.Num(), 5000) * 120.0);
	FPolyglyphClient::PumpHttp(bDone, TimeoutSeconds);

	UE_LOG(LogPolyglyphEnrich, Display, TEXT("%s"), *Summary);
	return bOk ? 0 : 1;
}
