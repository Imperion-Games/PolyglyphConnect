// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphTypes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace
{
	/** Read one language entry of the /status `languages` array. */
	FPolyglyphLanguageStatus ParseLanguage(const FJsonObject& InObject)
	{
		FPolyglyphLanguageStatus Language;
		InObject.TryGetStringField(TEXT("code"), Language.Code);
		InObject.TryGetBoolField(TEXT("enabled"), Language.bEnabled);
		InObject.TryGetNumberField(TEXT("total"), Language.Total);
		InObject.TryGetNumberField(TEXT("translated"), Language.Translated);
		InObject.TryGetNumberField(TEXT("approved"), Language.Approved);
		InObject.TryGetNumberField(TEXT("untranslated"), Language.Untranslated);

		double ApprovedPct = 0.0;
		InObject.TryGetNumberField(TEXT("approvedPct"), ApprovedPct);
		Language.ApprovedPct = static_cast<float>(ApprovedPct);

		InObject.TryGetBoolField(TEXT("complete"), Language.bComplete);
		return Language;
	}
}

FPolyglyphPullCounts::FPolyglyphPullCounts()
	: TotalStrings(0)
	, Returned(0)
	, Approved(0)
	, NeedsReview(0)
	, Untranslated(0)
{
}

bool FPolyglyphPullResult::ParsePullResponse(
	const TSharedPtr<FJsonObject>& InJson,
	FPolyglyphPullResult& OutResult,
	FString& OutError)
{
	if (!InJson.IsValid())
	{
		OutError = TEXT("Polyglyph returned an invalid pull response.");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Strings = nullptr;
	const TSharedPtr<FJsonObject>* Counts = nullptr;
	if (!InJson->TryGetArrayField(TEXT("strings"), Strings) || Strings == nullptr
		|| !InJson->TryGetObjectField(TEXT("counts"), Counts) || Counts == nullptr)
	{
		OutError = TEXT("Polyglyph pull response is missing strings or counts.");
		return false;
	}

	FPolyglyphPullResult Result;
	for (const TSharedPtr<FJsonValue>& Value : *Strings)
	{
		const TSharedPtr<FJsonObject>& Object = Value->AsObject();
		if (!Object.IsValid())
		{
			OutError = TEXT("Polyglyph pull response contains an invalid translation.");
			return false;
		}

		FPolyglyphTranslation Translation;
		if (!Object->TryGetStringField(TEXT("namespace"), Translation.Namespace)
			|| !Object->TryGetStringField(TEXT("key"), Translation.Key)
			|| !Object->TryGetStringField(TEXT("value"), Translation.Value))
		{
			OutError = TEXT("Polyglyph pull response contains an incomplete translation.");
			return false;
		}
		Result.Translations.Add(MoveTemp(Translation));
	}

	if (!(*Counts)->TryGetNumberField(TEXT("totalStrings"), Result.Counts.TotalStrings)
		|| !(*Counts)->TryGetNumberField(TEXT("returned"), Result.Counts.Returned)
		|| !(*Counts)->TryGetNumberField(TEXT("approved"), Result.Counts.Approved)
		|| !(*Counts)->TryGetNumberField(TEXT("needsReview"), Result.Counts.NeedsReview)
		|| !(*Counts)->TryGetNumberField(TEXT("untranslated"), Result.Counts.Untranslated))
	{
		OutError = TEXT("Polyglyph pull response contains incomplete counts.");
		return false;
	}

	if (Result.Counts.Returned != Result.Translations.Num())
	{
		OutError = FString::Printf(
			TEXT("Polyglyph pull response count mismatch: expected %d returned translation(s), received %d."),
			Result.Counts.Returned,
			Result.Translations.Num());
		return false;
	}

	OutResult = MoveTemp(Result);
	return true;
}

FPolyglyphEnrichItem::FPolyglyphEnrichItem()
	: MaxLength(0)
{
}

FPolyglyphJob::FPolyglyphJob()
	: Total(0)
	, Completed(0)
{
}

bool FPolyglyphJob::ParseJobResponse(const TSharedPtr<FJsonObject>& InJson, FPolyglyphJob& OutJob)
{
	if (!InJson.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* Job = nullptr;
	if (!InJson->TryGetObjectField(TEXT("job"), Job) || Job == nullptr)
	{
		return false;
	}

	OutJob = FPolyglyphJob();
	(*Job)->TryGetStringField(TEXT("id"), OutJob.Id);
	(*Job)->TryGetStringField(TEXT("language"), OutJob.Language);
	(*Job)->TryGetStringField(TEXT("status"), OutJob.Status);
	(*Job)->TryGetStringField(TEXT("mode"), OutJob.Mode);
	(*Job)->TryGetNumberField(TEXT("total"), OutJob.Total);
	(*Job)->TryGetNumberField(TEXT("completed"), OutJob.Completed);
	(*Job)->TryGetStringField(TEXT("error"), OutJob.Error);
	return true;
}

FPolyglyphLanguageStatus::FPolyglyphLanguageStatus()
	: bEnabled(false)
	, Total(0)
	, Translated(0)
	, Approved(0)
	, Untranslated(0)
	, ApprovedPct(0.0f)
	, bComplete(false)
{
}

FPolyglyphProjectStatus::FPolyglyphProjectStatus()
	: TotalStrings(0)
{
}

bool FPolyglyphProjectStatus::ParseStatusResponse(
	const TSharedPtr<FJsonObject>& InJson,
	FPolyglyphProjectStatus& OutStatus)
{
	if (!InJson.IsValid())
	{
		return false;
	}

	OutStatus = FPolyglyphProjectStatus();

	const TSharedPtr<FJsonObject>* Project = nullptr;
	if (InJson->TryGetObjectField(TEXT("project"), Project) && Project != nullptr)
	{
		(*Project)->TryGetStringField(TEXT("slug"), OutStatus.Slug);
		(*Project)->TryGetStringField(TEXT("sourceLanguage"), OutStatus.SourceLanguage);
		(*Project)->TryGetStringField(TEXT("brief"), OutStatus.Brief);
	}

	InJson->TryGetNumberField(TEXT("totalStrings"), OutStatus.TotalStrings);

	const TArray<TSharedPtr<FJsonValue>>* Languages = nullptr;
	if (InJson->TryGetArrayField(TEXT("languages"), Languages) && Languages != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Languages)
		{
			const TSharedPtr<FJsonObject>& Object = Value->AsObject();
			if (Object.IsValid())
			{
				OutStatus.Languages.Add(ParseLanguage(*Object));
			}
		}
	}

	return true;
}
