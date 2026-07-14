// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Orchestrates a full "pull approved" cycle: read the project's languages from the service,
 * pull each culture's approved translations, and import them into the localization archives.
 */
class FPolyglyphPull
{
public:
	/** Run the pull for every enabled language. OnDone(bSuccess, Summary) fires on the game
	 *  thread once all cultures have been imported (or failed). */
	static void Run(TFunction<void(bool, const FString&)> OnDone);
};
