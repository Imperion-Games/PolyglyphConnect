// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FPolyglyphTriggeredJob;

/**
 * Polls triggered translation jobs to completion without blocking the game thread.
 *
 * Starting a translation only queues work on the service, so the editor needs to know when the
 * jobs actually finished before a pull is worth running. Commandlets use their own blocking pump
 * instead (see UPolyglyphSyncCommandlet's -wait).
 */
class POLYGLYPHCONNECT_API FPolyglyphJobWatcher
{
public:
	/**
	 * Poll InJobs on the core ticker until each reaches a terminal state or the watch times out.
	 * OnDone fires once on the game thread: true only when every job COMPLETED, with a summary
	 * either way. Jobs without an id cannot be polled and are ignored; returns false when that
	 * leaves nothing to watch, in which case OnDone never fires.
	 */
	static bool Watch(
		const TArray<FPolyglyphTriggeredJob>& InJobs,
		TFunction<void(bool, const FString&)> OnDone);
};
