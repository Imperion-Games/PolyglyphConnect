// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Orchestrates a full pull cycle: read the project's languages from the service, pull each
 * culture under the configured approval policy, and import results into localization archives.
 */
class FPolyglyphPull
{
public:
	/**
	 * Run the pull for every enabled language.
	 * OnDone fires on the game thread with imported, skipped, and failed culture counts.
	 */
	static void Run(TFunction<void(bool, const FString&)> OnDone);
};
