// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FPolyglyphTranslation;
class ULocalizationTarget;

/**
 * Writes pulled translations into the project's UE localization archives and compiles them
 * into the .locres the packaged game loads. The write counterpart to FPolyglyphManifest.
 */
class FPolyglyphArchive
{
public:
	/**
	 * Import the given culture's pulled translations into its archive.
	 * Returns false and fills OutError on failure.
	 */
	static bool ImportTranslations(
		const FString& InCulture,
		const TArray<FPolyglyphTranslation>& InTranslations,
		FString& OutError);

	/**
	 * Compile the culture's archive into its .locres (what the packaged game loads).
	 * Returns false and fills OutError on failure.
	 */
	static bool CompileCulture(const FString& InCulture, FString& OutError);

	/** Regenerate the target's word-count report from its manifest and archives. */
	static bool UpdateWordCountReport(ULocalizationTarget* InLocalizationTarget, FString& OutError);
};
