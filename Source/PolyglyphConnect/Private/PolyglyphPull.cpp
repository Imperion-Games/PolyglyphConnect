// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphPull.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "PolyglyphArchive.h"
#include "PolyglyphClient.h"
#include "PolyglyphTypes.h"

namespace
{
	/** Parse a GET /pull body into translation triples. */
	TArray<FPolyglyphTranslation> ParseTranslations(const TSharedPtr<FJsonObject>& InJson)
	{
		TArray<FPolyglyphTranslation> Translations;
		if (!InJson.IsValid())
		{
			return Translations;
		}

		const TArray<TSharedPtr<FJsonValue>>* Strings = nullptr;
		if (InJson->TryGetArrayField(TEXT("strings"), Strings) && Strings != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& Value : *Strings)
			{
				const TSharedPtr<FJsonObject> Object = Value->AsObject();
				if (Object.IsValid())
				{
					FPolyglyphTranslation Translation;
					Object->TryGetStringField(TEXT("namespace"), Translation.Namespace);
					Object->TryGetStringField(TEXT("key"), Translation.Key);
					Object->TryGetStringField(TEXT("value"), Translation.Value);
					Translations.Add(MoveTemp(Translation));
				}
			}
		}
		return Translations;
	}
}

void FPolyglyphPull::Run(TFunction<void(bool, const FString&)> OnDone)
{
	FPolyglyphClient::TestConnection(
		[Done = MoveTemp(OnDone)](const FPolyglyphResponse& StatusResponse)
		{
			if (!StatusResponse.bSuccess)
			{
				Done(false, StatusResponse.Error);
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
				Done(false, TEXT("No enabled languages to pull."));
				return;
			}

			// Shared across the per-culture callbacks; all fire on the game thread, so the
			// plain counters are safe without atomics.
			const TSharedRef<int32> Remaining = MakeShared<int32>(Cultures.Num());
			const TSharedRef<int32> Imported = MakeShared<int32>(0);
			const TSharedRef<TArray<FString>> Errors = MakeShared<TArray<FString>>();
			const TSharedRef<TFunction<void(bool, const FString&)>> DonePtr =
				MakeShared<TFunction<void(bool, const FString&)>>(Done);

			for (const FString& Culture : Cultures)
			{
				FPolyglyphClient::PullTranslations(Culture,
					[Culture, Remaining, Imported, Errors, DonePtr](const FPolyglyphResponse& PullResponse)
					{
						if (PullResponse.bSuccess)
						{
							const TArray<FPolyglyphTranslation> Translations = ParseTranslations(PullResponse.Json);
							FString StepError;
							if (FPolyglyphArchive::ImportTranslations(Culture, Translations, StepError)
								&& FPolyglyphArchive::CompileCulture(Culture, StepError))
							{
								++(*Imported);
							}
							else
							{
								Errors->Add(FString::Printf(TEXT("%s: %s"), *Culture, *StepError));
							}
						}
						else
						{
							Errors->Add(FString::Printf(TEXT("%s: %s"), *Culture, *PullResponse.Error));
						}

						if (--(*Remaining) == 0)
						{
							const bool bOk = Errors->Num() == 0;
							const FString Summary = bOk
								? FString::Printf(TEXT("Pulled and compiled %d culture(s). .locres updated."), *Imported)
								: FString::Join(*Errors, TEXT("; "));
							(*DonePtr)(bOk, Summary);
						}
					});
			}
		});
}
