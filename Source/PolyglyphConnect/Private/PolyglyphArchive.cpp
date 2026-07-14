// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphArchive.h"

#include "Internationalization/InternationalizationManifest.h"
#include "Internationalization/TextKey.h"
#include "Internationalization/TextLocalizationResource.h"
#include "LocTextHelper.h"
#include "Misc/Paths.h"
#include "TextLocalizationResourceGenerator.h"

#include "PolyglyphProjectSettings.h"
#include "PolyglyphTypes.h"

namespace
{
	/** Resolve the configured localization target name (defaults to "Game"). */
	FString GetTargetName()
	{
		const UPolyglyphProjectSettings* Project = GetDefault<UPolyglyphProjectSettings>();
		return Project->LocalizationTarget.IsEmpty() ? TEXT("Game") : Project->LocalizationTarget;
	}

	/** Build a namespace+key -> source text map from the loaded manifest. */
	TMap<FString, FString> BuildSourceByKey(FLocTextHelper& InHelper)
	{
		TMap<FString, FString> SourceByKey;
		InHelper.EnumerateSourceTexts(
			[&SourceByKey](TSharedRef<FManifestEntry> InEntry) -> bool
			{
				for (const FManifestContext& Context : InEntry->Contexts)
				{
					SourceByKey.Add(InEntry->Namespace.GetString() / Context.Key.GetString(), InEntry->Source.Text);
				}
				return true;
			},
			true);
		return SourceByKey;
	}
}

bool FPolyglyphArchive::ImportTranslations(
	const FString& Culture,
	const TArray<FPolyglyphTranslation>& Translations,
	FString& OutError)
{
	const FString TargetName = GetTargetName();
	const FString TargetPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Localization"), TargetName);
	const FString ManifestName = TargetName + TEXT(".manifest");
	const FString ArchiveName = TargetName + TEXT(".archive");

	if (!FPaths::FileExists(FPaths::Combine(TargetPath, ManifestName)))
	{
		OutError = FString::Printf(TEXT("No manifest for target '%s'. Gather Text first."), *TargetName);
		return false;
	}

	TArray<FString> Cultures;
	Cultures.Add(Culture);
	FLocTextHelper LocTextHelper(TargetPath, ManifestName, ArchiveName, TEXT("en"), Cultures, nullptr);

	FText LoadError;
	if (!LocTextHelper.LoadManifest(ELocTextHelperLoadFlags::Load, &LoadError))
	{
		OutError = LoadError.ToString();
		return false;
	}
	if (!LocTextHelper.LoadArchive(Culture, ELocTextHelperLoadFlags::LoadOrCreate, &LoadError))
	{
		OutError = LoadError.ToString();
		return false;
	}

	const TMap<FString, FString> SourceByKey = BuildSourceByKey(LocTextHelper);

	int32 Applied = 0;
	for (const FPolyglyphTranslation& Translation : Translations)
	{
		const FString* SourceText = SourceByKey.Find(Translation.Namespace / Translation.Key);
		if (SourceText == nullptr)
		{
			continue;
		}

		const FLocItem Source(*SourceText);
		const FLocItem Translated(Translation.Value);
		LocTextHelper.AddTranslation(Culture, Translation.Namespace, Translation.Key, nullptr, Source, Translated, false);
		++Applied;
	}

	if (!LocTextHelper.SaveArchive(Culture, &LoadError))
	{
		OutError = LoadError.ToString();
		return false;
	}

	if (Applied == 0)
	{
		OutError = TEXT("No matching strings to import (gather may be out of date).");
		return false;
	}

	return true;
}

bool FPolyglyphArchive::CompileCulture(const FString& Culture, FString& OutError)
{
	const FString TargetName = GetTargetName();
	const FString TargetPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Localization"), TargetName);
	const FString ManifestName = TargetName + TEXT(".manifest");
	const FString ArchiveName = TargetName + TEXT(".archive");
	const FString ResourceName = TargetName + TEXT(".locres");

	TArray<FString> Cultures;
	Cultures.Add(Culture);
	FLocTextHelper LocTextHelper(TargetPath, ManifestName, ArchiveName, TEXT("en"), Cultures, nullptr);

	FText LoadError;
	if (!LocTextHelper.LoadManifest(ELocTextHelperLoadFlags::Load, &LoadError)
		|| !LocTextHelper.LoadArchive(Culture, ELocTextHelperLoadFlags::Load, &LoadError))
	{
		OutError = LoadError.ToString();
		return false;
	}

	const FTextKey LocResId = TargetPath / Culture / ResourceName;
	FTextLocalizationResource LocRes;
	TMap<FName, TSharedRef<FTextLocalizationResource>> PerPlatformLocRes;
	if (!FTextLocalizationResourceGenerator::GenerateLocRes(
		LocTextHelper, Culture, EGenerateLocResFlags::None, LocResId, LocRes, PerPlatformLocRes))
	{
		OutError = FString::Printf(TEXT("Failed to generate .locres for %s."), *Culture);
		return false;
	}

	const FString LocResPath = FPaths::Combine(TargetPath, Culture, ResourceName);
	if (!LocRes.SaveToFile(LocResPath))
	{
		OutError = FString::Printf(TEXT("Failed to write %s."), *LocResPath);
		return false;
	}

	return true;
}
