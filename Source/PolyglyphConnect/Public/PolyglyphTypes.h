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

	/**
	 * Optional message-format hint: PLAIN, PLURAL, SELECT, or MULTI.
	 * Empty lets the service auto-detect from the markup.
	 */
	FString Format;
};

/** One translation pulled from Polyglyph (mirrors a GET /api/plugin/pull item). */
struct FPolyglyphTranslation
{
	/** Localization namespace the string belongs to. */
	FString Namespace;

	/** Localization key within the namespace. */
	FString Key;

	/** Translated text for the requested culture. */
	FString Value;
};

/** Translation counts returned with one GET /api/plugin/pull response. */
struct FPolyglyphPullCounts
{
	/** Sets every count to zero. */
	FPolyglyphPullCounts();

	/** Total source strings in the Polyglyph project. */
	int32 TotalStrings;

	/** Translations included in this response. */
	int32 Returned;

	/** Approved translations available for the culture. */
	int32 Approved;

	/** Draft translations waiting for review. */
	int32 NeedsReview;

	/** Source strings without a translation. */
	int32 Untranslated;
};

/** Parsed translations and counts from one GET /api/plugin/pull response. */
struct FPolyglyphPullResult
{
	/** Translations returned under the request's approval policy. */
	TArray<FPolyglyphTranslation> Translations;

	/** Server-side counts for the requested culture. */
	FPolyglyphPullCounts Counts;

	/** Parse a pull body and require its strings and counts contract. */
	static bool ParsePullResponse(
		const TSharedPtr<FJsonObject>& InJson,
		FPolyglyphPullResult& OutResult,
		FString& OutError);
};

/**
 * One enrichment record attaching translator-context metadata to an existing key (mirrors a
 * POST /api/plugin/enrich item). Character voice, grammatical gender, register, and a
 * max display length all ride this single channel, keyed by namespace + key.
 */
struct FPolyglyphEnrichItem
{
	/** Sets the optional maximum length to zero. */
	FPolyglyphEnrichItem();

	/** Localization namespace of the key to enrich. */
	FString Namespace;

	/** Localization key to enrich. */
	FString Key;

	/** Speaker/character name whose voice profile the translator should honour. */
	FString Character;

	/** Grammatical gender hint, e.g. "masculine" / "feminine" / "neuter". */
	FString Gender;

	/** Register hint, e.g. "formal" / "casual" (drives the T-V choice). */
	FString Register;

	/** Optional maximum display length in characters; 0 means unset. */
	int32 MaxLength;

	/** Free-text note appended to the string's translation context. */
	FString Context;
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
	/** Sets numeric progress fields to zero. */
	FPolyglyphJob();

	/** Job id. */
	FString Id;

	/** Culture being translated. */
	FString Language;

	/** PENDING / RUNNING / COMPLETED / FAILED. */
	FString Status;

	/** sync / batch. */
	FString Mode;

	/** Total strings in the job. */
	int32 Total;

	/** Strings translated so far. */
	int32 Completed;

	/** Error message when the job failed. */
	FString Error;

	/** True once the job reached a terminal state. */
	bool IsFinished() const
	{
		return Status.Equals(TEXT("COMPLETED"), ESearchCase::IgnoreCase)
			|| Status.Equals(TEXT("FAILED"), ESearchCase::IgnoreCase);
	}

	/** Populate OutJob from a GET /jobs body (reads the nested `job` object). */
	static bool ParseJobResponse(const TSharedPtr<FJsonObject>& InJson, FPolyglyphJob& OutJob);
};

/**
 * Per-language progress, mirroring one entry of the /api/plugin/status `languages` array.
 */
struct FPolyglyphLanguageStatus
{
	/** Sets progress fields to their disabled zero state. */
	FPolyglyphLanguageStatus();

	/** BCP-47 culture code, e.g. "pt-BR". */
	FString Code;

	/** Whether the language is enabled for the project. */
	bool bEnabled;

	/** Total source strings (the project total, repeated per language). */
	int32 Total;

	/** Strings with any translation (approved or pending review). */
	int32 Translated;

	/** Strings whose translation is approved. */
	int32 Approved;

	/** Strings with no translation yet. */
	int32 Untranslated;

	/** Approved / Total as a 0-100 percentage, as reported by the service. */
	float ApprovedPct;

	/** True when every string is approved. */
	bool bComplete;
};

/**
 * Project status from GET /api/plugin/status, parsed into a flat shape for the dashboard.
 */
struct FPolyglyphProjectStatus
{
	/** Sets the project string count to zero. */
	FPolyglyphProjectStatus();

	/** Project slug echoed by the service. */
	FString Slug;

	/** Source-language code, e.g. "en". */
	FString SourceLanguage;

	/** Optional project-wide translation brief. */
	FString Brief;

	/** Total source strings in the project. */
	int32 TotalStrings;

	/** Per-language progress rows. */
	TArray<FPolyglyphLanguageStatus> Languages;

	/** Populate OutStatus from a parsed /status body. Returns false when InJson is null. */
	static bool ParseStatusResponse(
		const TSharedPtr<FJsonObject>& InJson,
		FPolyglyphProjectStatus& OutStatus);
};
