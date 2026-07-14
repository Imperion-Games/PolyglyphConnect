// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphSyncCommandlet.h"

#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HttpManager.h"
#include "HttpModule.h"
#include "Misc/Parse.h"

#include "PolyglyphClient.h"
#include "PolyglyphManifest.h"
#include "PolyglyphProjectSettings.h"
#include "PolyglyphPull.h"
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
	void PumpUntil(const bool& bInDone, const double InTimeoutSeconds)
	{
		const double Start = FPlatformTime::Seconds();
		while (!bInDone && (FPlatformTime::Seconds() - Start) < InTimeoutSeconds)
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
		bool bReqOk = true;
		while (!JobState.IsFinished() && bReqOk && (FPlatformTime::Seconds() - Start) < 3600.0)
		{
			bool bGot = false;
			FPolyglyphClient::GetJob(InJob.JobId, [&bGot, &bReqOk, &JobState](const FPolyglyphResponse& Response)
			{
				bReqOk = Response.bSuccess;
				if (Response.bSuccess)
				{
					FPolyglyphJob::FromJson(Response.Json, JobState);
				}
				bGot = true;
			});
			PumpUntil(bGot, 60.0);

			if (bReqOk && !JobState.IsFinished())
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
		TArray<FPolyglyphSourceString> Strings;
		FString GatherError;
		if (!FPolyglyphManifest::GatherSourceStrings(Strings, GatherError))
		{
			UE_LOG(LogPolyglyphSync, Error, TEXT("Push aborted: %s"), *GatherError);
			return 1;
		}

		bool bDone = false;
		bool bOk = false;
		FString Message;
		FPolyglyphClient::PushStrings(Strings, [&bDone, &bOk, &Message](const FPolyglyphResponse& Response)
		{
			bOk = Response.bSuccess;
			Message = Response.Error;
			bDone = true;
		});
		PumpUntil(bDone, 300.0);

		if (bOk)
		{
			UE_LOG(LogPolyglyphSync, Display, TEXT("Pushed %d source string(s)."), Strings.Num());
			return 0;
		}
		UE_LOG(LogPolyglyphSync, Error, TEXT("Push failed: %s"), *Message);
		return 1;
	}

	/** Trigger translation for all enabled languages, optionally waiting for completion. */
	int32 RunTranslate(const FString& InMode, const bool bInMock, const bool bInWait)
	{
		bool bDone = false;
		bool bOk = false;
		FString Summary;
		TArray<FPolyglyphTriggeredJob> Jobs;
		FPolyglyphTranslate::Run(InMode, bInMock,
			[&bDone, &bOk, &Summary, &Jobs](bool bSuccess, const FString& InSummary, const TArray<FPolyglyphTriggeredJob>& InJobs)
		{
			bOk = bSuccess;
			Summary = InSummary;
			Jobs = InJobs;
			bDone = true;
		});
		PumpUntil(bDone, 120.0);
		UE_LOG(LogPolyglyphSync, Display, TEXT("%s"), *Summary);

		if (!bOk)
		{
			return 1;
		}
		if (!bInWait)
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
		bool bDone = false;
		bool bOk = false;
		FString Summary;
		FPolyglyphPull::Run([&bDone, &bOk, &Summary](bool bSuccess, const FString& InSummary)
		{
			bOk = bSuccess;
			Summary = InSummary;
			bDone = true;
		});
		PumpUntil(bDone, 600.0);

		UE_LOG(LogPolyglyphSync, Display, TEXT("%s"), *Summary);
		return bOk ? 0 : 1;
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

	const bool bPush = Switches.Contains(TEXT("push"));
	const bool bTranslate = Switches.Contains(TEXT("translate"));
	const bool bPull = Switches.Contains(TEXT("pull"));
	const bool bWait = Switches.Contains(TEXT("wait"));
	const bool bMock = Switches.Contains(TEXT("mock"));
	if (!bPush && !bTranslate && !bPull)
	{
		UE_LOG(LogPolyglyphSync, Error, TEXT("Nothing to do. Pass -push, -translate and/or -pull."));
		return 1;
	}

	FString TranslateMode;
	FParse::Value(*Params, TEXT("mode="), TranslateMode);

	ApplyOverrides(Params);

	int32 ExitCode = 0;
	if (bPush)
	{
		ExitCode = RunPush();
	}
	if (bTranslate && ExitCode == 0)
	{
		ExitCode = RunTranslate(TranslateMode, bMock, bWait);
	}
	if (bPull && ExitCode == 0)
	{
		ExitCode = RunPull();
	}
	return ExitCode;
}
