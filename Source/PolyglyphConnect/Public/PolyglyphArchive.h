// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FPolyglyphTranslation;

/**
 * Writes approved translations into the project's UE localization archives and compiles them
 * into the .locres the packaged game loads. The write counterpart to FPolyglyphManifest.
 */
class FPolyglyphArchive
{
public:
	/** Import the given culture's approved translations into its archive. Returns false and
	 *  fills OutError on failure. */
	static bool ImportTranslations(
		const FString& Culture,
		const TArray<FPolyglyphTranslation>& Translations,
		FString& OutError);

	/** Compile the culture's archive into its .locres (what the packaged game loads).
	 *  Returns false and fills OutError on failure. */
	static bool CompileCulture(const FString& Culture, FString& OutError);
};
