// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;

/**
 * One source string to push to Polyglyph (mirrors the service's /api/plugin/push item).
 * Plain struct, serialised to JSON by FPolyglyphClient; promote to a USTRUCT if Blueprint
 * or automation exposure is ever needed.
 */
struct FPolyglyphSourceString
{
	/** Grouping for the string, e.g. the Unreal localization namespace. */
	FString Namespace;

	/** Stable identifier within the namespace (the localization key). */
	FString Key;

	/** Source-language text to translate. */
	FString SourceText;

	/** Optional note that guides the translator/AI (where it appears, tone, etc.). */
	FString Context;

	/** Optional message-format hint: PLAIN, PLURAL, SELECT, or MULTI. Empty lets the
	 *  service auto-detect from the markup. */
	FString Format;
};

/**
 * One approved translation pulled from Polyglyph (mirrors a GET /api/plugin/pull item).
 */
struct FPolyglyphTranslation
{
	/** Localization namespace the string belongs to. */
	FString Namespace;

	/** Localization key within the namespace. */
	FString Key;

	/** Translated text for the requested culture. */
	FString Value;
};

/**
 * A translation job started for one language (from POST /api/plugin/translate).
 */
struct FPolyglyphTriggeredJob
{
	/** Server job id, used to poll GET /api/plugin/jobs/:jobId. */
	FString JobId;

	/** Culture the job translates. */
	FString Language;
};

/**
 * Status of a translation job (from GET /api/plugin/jobs/:jobId `job` object).
 */
struct FPolyglyphJob
{
	/** Job id. */
	FString Id;

	/** Culture being translated. */
	FString Language;

	/** PENDING / RUNNING / COMPLETED / FAILED. */
	FString Status;

	/** sync / batch. */
	FString Mode;

	/** Total strings in the job. */
	int32 Total = 0;

	/** Strings translated so far. */
	int32 Completed = 0;

	/** Error message when the job failed. */
	FString Error;

	/** True once the job reached a terminal state. */
	bool IsFinished() const
	{
		return Status.Equals(TEXT("COMPLETED"), ESearchCase::IgnoreCase)
			|| Status.Equals(TEXT("FAILED"), ESearchCase::IgnoreCase);
	}

	/** Populate OutJob from a GET /jobs body (reads the nested `job` object). */
	static bool FromJson(const TSharedPtr<FJsonObject>& InJson, FPolyglyphJob& OutJob);
};

/**
 * Per-language progress, mirroring one entry of the /api/plugin/status `languages` array.
 */
struct FPolyglyphLanguageStatus
{
	/** BCP-47 culture code, e.g. "pt-BR". */
	FString Code;

	/** Whether the language is enabled for the project. */
	bool bEnabled = false;

	/** Total source strings (the project total, repeated per language). */
	int32 Total = 0;

	/** Strings with any translation (approved or pending review). */
	int32 Translated = 0;

	/** Strings whose translation is approved. */
	int32 Approved = 0;

	/** Strings with no translation yet. */
	int32 Untranslated = 0;

	/** Approved / Total as a 0-100 percentage, as reported by the service. */
	float ApprovedPct = 0.0f;

	/** True when every string is approved. */
	bool bComplete = false;
};

/**
 * Project status from GET /api/plugin/status, parsed into a flat shape for the dashboard.
 */
struct FPolyglyphProjectStatus
{
	/** Project slug echoed by the service. */
	FString Slug;

	/** Source-language code, e.g. "en". */
	FString SourceLanguage;

	/** Optional project-wide translation brief. */
	FString Brief;

	/** Total source strings in the project. */
	int32 TotalStrings = 0;

	/** Per-language progress rows. */
	TArray<FPolyglyphLanguageStatus> Languages;

	/** Populate OutStatus from a parsed /status body. Returns false when InJson is null. */
	static bool FromJson(const TSharedPtr<FJsonObject>& InJson, FPolyglyphProjectStatus& OutStatus);
};
