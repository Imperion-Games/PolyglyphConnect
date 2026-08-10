// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FPolyglyphTriggeredJob;

/**
 * Starts AI translation for every enabled language (fire-and-forget). Waiting for the jobs
 * to finish is the caller's job (the commandlet's -wait does it); the interactive panel just
 * starts the jobs and lets the status board reflect progress on refresh.
 */
class FPolyglyphTranslate
{
public:
	/**
	 * Trigger a translation job per enabled language.
	 * Mock is a no-cost dry run; OnDone fires after every language has been triggered.
	 */
	static void Run(
		const FString& InMode,
		bool Mock,
		TFunction<void(bool, const FString&, const TArray<FPolyglyphTriggeredJob>&)> OnDone);
};
