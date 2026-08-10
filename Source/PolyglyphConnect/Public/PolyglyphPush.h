// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Coordinates source push followed by automatic native Unreal dialogue enrichment. */
class FPolyglyphPush
{
public:
	/** Gather the configured target, push its source, then enrich any DialogueWave keys. */
	static void Run(TFunction<void(bool, const FString&)> OnDone);
};
