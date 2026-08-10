// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphSyncCommandlet.h"

#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HttpManager.h"
#include "HttpModule.h"
#include "Misc/Parse.h"

#include "PolyglyphClient.h"
#include "PolyglyphProjectSettings.h"
#include "PolyglyphPull.h"
#include "PolyglyphPush.h"
#include "PolyglyphSettings.h"
#include "PolyglyphTranslate.h"
#include "PolyglyphTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogPolyglyphSync, Log, All);

namespace
{
	/** Apply optional command-line / env overrides to the settings CDOs before syncing. */
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
		if (FParse::Value(*InParams, TEXT("LocalizationTarget="), Value))
		{
			GetMutableDefault<UPolyglyphProjectSettings>()->LocalizationTarget = Value;
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

	/** Drive the HTTP manager on this thread until the request completes or times out. */
	void PumpUntil(const bool& InDone, const double InTimeoutSeconds)
	{
		const double Start = FPlatformTime::Seconds();
		while (!InDone && (FPlatformTime::Seconds() - Start) < InTimeoutSeconds)
		{
			FHttpModule::Get().GetHttpManager().Tick(0.05f);
			FPlatformProcess::Sleep(0.05f);
		}
	}

	/** Poll one job to a terminal state. Returns true only when it COMPLETED. */
	bool WaitForJob(const FPolyglyphTriggeredJob& InJob)
	{
		const double Start = FPlatformTime::Seconds();
		FPolyglyphJob JobState;
		bool RequestSucceeded = true;
		while (!JobState.IsFinished() && RequestSucceeded && (FPlatformTime::Seconds() - Start) < 3600.0)
		{
			bool Received = false;
			FPolyglyphClient::GetJob(InJob.JobId, [&Received, &RequestSucceeded, &JobState](const FPolyglyphResponse& Response)
			{
				RequestSucceeded = Response.bSuccess;
				if (Response.bSuccess)
				{
					FPolyglyphJob::ParseJobResponse(Response.Json, JobState);
				}
				Received = true;
			});
			PumpUntil(Received, 60.0);

			if (RequestSucceeded && !JobState.IsFinished())
			{
				FPlatformProcess::Sleep(3.0f);
			}
		}

		const FString Outcome = JobState.IsFinished() ? JobState.Status : TEXT("did not finish");
		UE_LOG(LogPolyglyphSync, Display, TEXT("Job %s (%s): %s"), *InJob.JobId, *InJob.Language, *Outcome);
		return JobState.Status.Equals(TEXT("COMPLETED"), ESearchCase::IgnoreCase);
	}

	/** Gather the manifest and push it. Returns 0 on success, 1 on failure. */
	int32 RunPush()
	{
		bool Completed = false;
		bool Succeeded = false;
		FString Message;
		FPolyglyphPush::Run([&Completed, &Succeeded, &Message](bool InSuccess, const FString& InMessage)
		{
			Succeeded = InSuccess;
			Message = InMessage;
			Completed = true;
		});
		PumpUntil(Completed, 300.0);

		if (Succeeded)
		{
			UE_LOG(LogPolyglyphSync, Display, TEXT("%s"), *Message);
			return 0;
		}

		UE_LOG(LogPolyglyphSync, Error, TEXT("%s"), *Message);
		return 1;
	}

	/** Trigger translation for all enabled languages, optionally waiting for completion. */
	int32 RunTranslate(const FString& InMode, const bool Mock, const bool Wait)
	{
		bool Completed = false;
		bool Succeeded = false;
		FString Summary;
		TArray<FPolyglyphTriggeredJob> Jobs;
		FPolyglyphTranslate::Run(InMode, Mock,
			[&Completed, &Succeeded, &Summary, &Jobs](bool Success, const FString& InSummary, const TArray<FPolyglyphTriggeredJob>& InJobs)
		{
			Succeeded = Success;
			Summary = InSummary;
			Jobs = InJobs;
			Completed = true;
		});
		PumpUntil(Completed, 120.0);
		UE_LOG(LogPolyglyphSync, Display, TEXT("%s"), *Summary);

		if (!Succeeded)
		{
			return 1;
		}
		if (!Wait)
		{
			return 0;
		}

		int32 ExitCode = 0;
		for (const FPolyglyphTriggeredJob& Job : Jobs)
		{
			if (!Job.JobId.IsEmpty() && !WaitForJob(Job))
			{
				ExitCode = 1;
			}
		}
		return ExitCode;
	}

	/** Pull approved translations (imports + compiles). Returns 0 on success, 1 on failure. */
	int32 RunPull()
	{
		bool Completed = false;
		bool Succeeded = false;
		FString Summary;
		FPolyglyphPull::Run([&Completed, &Succeeded, &Summary](bool Success, const FString& InSummary)
		{
			Succeeded = Success;
			Summary = InSummary;
			Completed = true;
		});
		PumpUntil(Completed, 600.0);

		UE_LOG(LogPolyglyphSync, Display, TEXT("%s"), *Summary);
		return Succeeded ? 0 : 1;
	}
}

UPolyglyphSyncCommandlet::UPolyglyphSyncCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UPolyglyphSyncCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches);

	const bool Push = Switches.Contains(TEXT("push"));
	const bool Translate = Switches.Contains(TEXT("translate"));
	const bool Pull = Switches.Contains(TEXT("pull"));
	const bool Wait = Switches.Contains(TEXT("wait"));
	const bool Mock = Switches.Contains(TEXT("mock"));
	if (!Push && !Translate && !Pull)
	{
		UE_LOG(LogPolyglyphSync, Error, TEXT("Nothing to do. Pass -push, -translate and/or -pull."));
		return 1;
	}

	FString TranslateMode;
	FParse::Value(*Params, TEXT("mode="), TranslateMode);

	ApplyOverrides(Params);

	int32 ExitCode = 0;
	if (Push)
	{
		ExitCode = RunPush();
	}
	if (Translate && ExitCode == 0)
	{
		ExitCode = RunTranslate(TranslateMode, Mock, Wait);
	}
	if (Pull && ExitCode == 0)
	{
		ExitCode = RunPull();
	}
	return ExitCode;
}
