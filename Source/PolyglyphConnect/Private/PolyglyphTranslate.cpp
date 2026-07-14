// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphTranslate.h"

#include "Dom/JsonObject.h"

#include "PolyglyphClient.h"
#include "PolyglyphTypes.h"

void FPolyglyphTranslate::Run(
	const FString& Mode,
	bool bMock,
	TFunction<void(bool, const FString&, const TArray<FPolyglyphTriggeredJob>&)> OnDone)
{
	FPolyglyphClient::TestConnection(
		[Mode, bMock, Done = MoveTemp(OnDone)](const FPolyglyphResponse& StatusResponse)
		{
			if (!StatusResponse.bSuccess)
			{
				Done(false, StatusResponse.Error, TArray<FPolyglyphTriggeredJob>());
				return;
			}

			FPolyglyphProjectStatus Status;
			FPolyglyphProjectStatus::FromJson(StatusResponse.Json, Status);

			TArray<FString> Cultures;
			for (const FPolyglyphLanguageStatus& Language : Status.Languages)
			{
				if (Language.bEnabled)
				{
					Cultures.Add(Language.Code);
				}
			}

			if (Cultures.Num() == 0)
			{
				Done(false, TEXT("No enabled languages to translate."), TArray<FPolyglyphTriggeredJob>());
				return;
			}

			const TSharedRef<int32> Remaining = MakeShared<int32>(Cultures.Num());
			const TSharedRef<TArray<FPolyglyphTriggeredJob>> Jobs = MakeShared<TArray<FPolyglyphTriggeredJob>>();
			const TSharedRef<TArray<FString>> Errors = MakeShared<TArray<FString>>();
			const TSharedRef<TFunction<void(bool, const FString&, const TArray<FPolyglyphTriggeredJob>&)>> DonePtr =
				MakeShared<TFunction<void(bool, const FString&, const TArray<FPolyglyphTriggeredJob>&)>>(Done);

			for (const FString& Culture : Cultures)
			{
				FPolyglyphClient::TriggerTranslate(Culture, Mode, bMock,
					[Culture, Remaining, Jobs, Errors, DonePtr](const FPolyglyphResponse& Response)
					{
						if (Response.bSuccess && Response.Json.IsValid())
						{
							FPolyglyphTriggeredJob Job;
							Response.Json->TryGetStringField(TEXT("jobId"), Job.JobId);
							Job.Language = Culture;
							Jobs->Add(Job);
						}
						else
						{
							Errors->Add(FString::Printf(TEXT("%s: %s"), *Culture, *Response.Error));
						}

						if (--(*Remaining) == 0)
						{
							const bool bOk = Errors->Num() == 0;
							const FString Summary = bOk
								? FString::Printf(TEXT("Started %d translation job(s)."), Jobs->Num())
								: FString::Join(*Errors, TEXT("; "));
							(*DonePtr)(bOk, Summary, *Jobs);
						}
					});
			}
		});
}
