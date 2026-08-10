// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphLocaleMapCommandlet.h"

#include "Misc/Parse.h"
#include "Misc/Paths.h"

#include "PolyglyphLocaleMapping.h"

DEFINE_LOG_CATEGORY_STATIC(LogPolyglyphLocaleMap, Log, All);

UPolyglyphLocaleMapCommandlet::UPolyglyphLocaleMapCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UPolyglyphLocaleMapCommandlet::Main(const FString& InParams)
{
	FString OutputFile;
	if (!FParse::Value(*InParams, TEXT("Output="), OutputFile))
	{
		UE_LOG(LogPolyglyphLocaleMap, Error, TEXT("Missing Output=<path> parameter."));
		return 1;
	}

	OutputFile = FPaths::ConvertRelativePathToFull(OutputFile);
	TArray<FPolyglyphLocaleMapping> Mappings;
	FString Error;
	if (!FPolyglyphUnrealLocaleCatalog::GenerateDefaultCatalog(Mappings, Error)
		|| !FPolyglyphUnrealLocaleCatalog::WriteCatalog(Mappings, OutputFile, Error))
	{
		UE_LOG(LogPolyglyphLocaleMap, Error, TEXT("Could not generate locale catalog: %s"), *Error);
		return 1;
	}

	UE_LOG(LogPolyglyphLocaleMap, Display, TEXT("Wrote %d Unreal locale mappings to %s."), Mappings.Num(), *OutputFile);
	return 0;
}
