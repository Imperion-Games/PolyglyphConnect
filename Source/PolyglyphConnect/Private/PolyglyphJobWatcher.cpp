// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphJobWatcher.h"

#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"

#include "PolyglyphClient.h"
#include "PolyglyphTypes.h"

namespace
{
	/** Seconds between status polls, matching the sync commandlet's cadence. */
	constexpr float PollIntervalSeconds = 3.0f;

	/** Stop watching a job that has not reached a terminal state within an hour. */
	constexpr double WatchTimeoutSeconds = 3600.0;

	/** One job being polled, plus the last status read back for it. */
	struct FPolyglyphWatchedJob
	{
		FPolyglyphWatchedJob()
			: bFinished(false)
			, bPolling(false)
		{
		}

		/** Job id and culture as returned by the translate call. */
		FPolyglyphTriggeredJob Job;

		/** Last status seen, e.g. RUNNING, COMPLETED, FAILED. Empty before the first poll. */
		FString Status;

		/** True once the job reached a terminal state, timed out, or its status request failed. */
		bool bFinished;

		/** True while a status request for this job is in flight. */
		bool bPolling;
	};

	/** Shared state for one watch, kept alive by the ticker and by any in-flight request. */
	struct FPolyglyphWatch
	{
		FPolyglyphWatch()
			: StartSeconds(FPlatformTime::Seconds())
		{
		}

		/** Every job this watch is polling. Never resized after Watch() builds it. */
		TArray<FPolyglyphWatchedJob> Jobs;

		/** Platform time the watch began, used for the overall timeout. */
		double StartSeconds;

		/** Called once when every job has settled. */
		TFunction<void(bool, const FString&)> OnDone;
	};

	/** Report a settled watch: success only when every job reached COMPLETED. */
	void ReportWatch(const TSharedRef<FPolyglyphWatch>& InWatch)
	{
		TArray<FString> Unfinished;
		for (const FPolyglyphWatchedJob& Watched : InWatch->Jobs)
		{
			if (!Watched.Status.Equals(TEXT("COMPLETED"), ESearchCase::IgnoreCase))
			{
				Unfinished.Add(FString::Printf(
					TEXT("%s (%s)"),
					*Watched.Job.Language,
					Watched.Status.IsEmpty() ? TEXT("no status") : *Watched.Status));
			}
		}

		if (Unfinished.Num() == 0)
		{
			InWatch->OnDone(true, FString::Printf(
				TEXT("%d translation job(s) completed."), InWatch->Jobs.Num()));
			return;
		}

		InWatch->OnDone(false, FString::Printf(
			TEXT("%d of %d translation job(s) did not complete: %s."),
			Unfinished.Num(),
			InWatch->Jobs.Num(),
			*FString::Join(Unfinished, TEXT(", "))));
	}

	/** One poll round: request status for every job that is unsettled and not already in flight. */
	bool PollWatch(const TSharedRef<FPolyglyphWatch>& InWatch)
	{
		const bool bTimedOut = (FPlatformTime::Seconds() - InWatch->StartSeconds) > WatchTimeoutSeconds;
		bool bAnyPending = false;

		for (int32 Index = 0; Index < InWatch->Jobs.Num(); ++Index)
		{
			FPolyglyphWatchedJob& Watched = InWatch->Jobs[Index];
			if (Watched.bFinished)
			{
				continue;
			}
			if (bTimedOut)
			{
				// Leave the last status in place so the summary reports what it was doing.
				Watched.bFinished = true;
				continue;
			}

			bAnyPending = true;
			if (Watched.bPolling)
			{
				continue;
			}

			Watched.bPolling = true;
			FPolyglyphClient::GetJob(Watched.Job.JobId, [InWatch, Index](const FPolyglyphResponse& InResponse)
			{
				FPolyglyphWatchedJob& Polled = InWatch->Jobs[Index];
				Polled.bPolling = false;

				FPolyglyphJob JobState;
				if (!InResponse.bSuccess || !FPolyglyphJob::ParseJobResponse(InResponse.Json, JobState))
				{
					// A failed status request is not a failed job, but there is nothing left to poll
					// against, so stop here and let the summary report it as unfinished.
					Polled.bFinished = true;
					return;
				}

				Polled.Status = JobState.Status;
				Polled.bFinished = JobState.IsFinished();
			});
		}

		if (bAnyPending && !bTimedOut)
		{
			return true;
		}

		ReportWatch(InWatch);
		return false;
	}
}

bool FPolyglyphJobWatcher::Watch(
	const TArray<FPolyglyphTriggeredJob>& InJobs,
	TFunction<void(bool, const FString&)> OnDone)
{
	const TSharedRef<FPolyglyphWatch> Watch = MakeShared<FPolyglyphWatch>();
	Watch->OnDone = MoveTemp(OnDone);

	for (const FPolyglyphTriggeredJob& Job : InJobs)
	{
		// A job the service reported without an id cannot be polled. That is how a synchronous
		// run answers, so treat it as nothing to wait for rather than as a failure.
		if (Job.JobId.IsEmpty())
		{
			continue;
		}

		FPolyglyphWatchedJob Watched;
		Watched.Job = Job;
		Watch->Jobs.Add(Watched);
	}

	if (Watch->Jobs.Num() == 0)
	{
		return false;
	}

	FTSTicker::GetCoreTicker().AddTicker(
		TEXT("PolyglyphConnect.WatchTranslationJobs"),
		PollIntervalSeconds,
		[Watch](float InDeltaSeconds)
		{
			static_cast<void>(InDeltaSeconds);
			return PollWatch(Watch);
		});

	return true;
}
