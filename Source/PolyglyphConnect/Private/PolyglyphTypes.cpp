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

bool FPolyglyphJob::FromJson(const TSharedPtr<FJsonObject>& InJson, FPolyglyphJob& OutJob)
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

bool FPolyglyphProjectStatus::FromJson(const TSharedPtr<FJsonObject>& InJson, FPolyglyphProjectStatus& OutStatus)
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
