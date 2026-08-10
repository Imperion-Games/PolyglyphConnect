// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ULocalizationTarget;

/** Maps one Unreal culture to Polyglyph's canonical locale identity. */
struct FPolyglyphLocaleMapping
{
	/** Culture code exactly as configured on the Unreal localization target. */
	FString ExternalCode;

	/** Canonical BCP 47 locale tag sent to Polyglyph. */
	FString LocaleTag;

	/** English locale name suitable for translators and model prompts. */
	FString DisplayName;
};

/** Locale declaration produced from one Unreal localization target. */
struct FPolyglyphLocaleManifest
{
	/** Integration type understood by Polyglyph. */
	FString IntegrationKind;

	/** Stable Unreal localization target identifier. */
	FString IntegrationId;

	/** Unreal localization target name shown in the dashboard. */
	FString IntegrationName;

	/** Native culture used as the source locale. */
	FPolyglyphLocaleMapping SourceLocale;

	/** Supported non-native cultures used as target locales. */
	TArray<FPolyglyphLocaleMapping> TargetLocales;
};

/** Converts Unreal culture identities into the locale manifest expected by Polyglyph. */
class POLYGLYPHCONNECT_API FPolyglyphUnrealLocaleCatalog
{
public:
	/** Load the shipped Unreal catalog and overlay an optional partial project mapping file. */
	static bool Load(
		const FString& InProjectMappingFile,
		TMap<FString, FPolyglyphLocaleMapping>& OutMappings,
		FString& OutError);

	/** Find a mapping by its exact external culture code. */
	static const FPolyglyphLocaleMapping* Find(
		const FString& InExternalCode,
		const TMap<FString, FPolyglyphLocaleMapping>& InMappings);

	/** Generate the complete catalog supported by the running Unreal Engine. */
	static bool GenerateDefaultCatalog(
		TArray<FPolyglyphLocaleMapping>& OutMappings,
		FString& OutError);

	/** Write a generated catalog to a JSON file for a specific Unreal Engine version. */
	static bool WriteCatalog(
		const TArray<FPolyglyphLocaleMapping>& InMappings,
		const FString& InOutputFile,
		FString& OutError);
};

/** Selects the exact locale mappings required by one Unreal localization target. */
class POLYGLYPHCONNECT_API FPolyglyphUnrealLocaleMapper
{
public:
	/** Build the authoritative locale declaration for an Unreal localization target. */
	static bool BuildManifest(
		const ULocalizationTarget* InLocalizationTarget,
		const TMap<FString, FPolyglyphLocaleMapping>& InMappings,
		FPolyglyphLocaleManifest& OutManifest,
		FString& OutError);
};
