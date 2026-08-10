// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphArchive.h"

#include "Internationalization/InternationalizationManifest.h"
#include "Internationalization/TextKey.h"
#include "Internationalization/TextLocalizationResource.h"
#include "LocalizationConfigurationScript.h"
#include "LocalizationTargetTypes.h"
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

	/** Get every configured culture from the localization target. */
	bool GetConfiguredCultures(
		const ULocalizationTarget& InLocalizationTarget,
		TArray<FString>& OutCultures,
		FString& OutError)
	{
		const TArray<FCultureStatistics>& SupportedCultures =
			InLocalizationTarget.Settings.SupportedCulturesStatistics;

		OutCultures.Reset(SupportedCultures.Num());
		for (const FCultureStatistics& Culture : SupportedCultures)
		{
			if (!Culture.CultureName.IsEmpty())
			{
				OutCultures.AddUnique(Culture.CultureName);
			}
		}

		if (OutCultures.Num() == 0)
		{
			OutError = TEXT("The localization target has no configured cultures.");
			return false;
		}

		return true;
	}
}

bool FPolyglyphArchive::ImportTranslations(
	const FString& InCulture,
	const TArray<FPolyglyphTranslation>& InTranslations,
	FString& OutError)
{
	if (InTranslations.Num() == 0)
	{
		OutError = TEXT("Polyglyph returned no translations to import.");
		return false;
	}

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
	Cultures.Add(InCulture);
	FLocTextHelper LocTextHelper(TargetPath, ManifestName, ArchiveName, TEXT("en"), Cultures, nullptr);

	FText LoadError;
	if (!LocTextHelper.LoadManifest(ELocTextHelperLoadFlags::Load, &LoadError))
	{
		OutError = LoadError.ToString();
		return false;
	}
	if (!LocTextHelper.LoadArchive(InCulture, ELocTextHelperLoadFlags::LoadOrCreate, &LoadError))
	{
		OutError = LoadError.ToString();
		return false;
	}

	const TMap<FString, FString> SourceByKey = BuildSourceByKey(LocTextHelper);

	int32 Matched = 0;
	int32 Applied = 0;
	for (const FPolyglyphTranslation& Translation : InTranslations)
	{
		const FString* SourceText = SourceByKey.Find(Translation.Namespace / Translation.Key);
		if (SourceText == nullptr)
		{
			continue;
		}
		++Matched;

		// ImportTranslation updates the existing (empty) entry the gather pre-created, or adds a
		// new one; AddTranslation would be rejected as a duplicate and drop the value silently.
		const FLocItem Source(*SourceText);
		const FLocItem Translated(Translation.Value);
		if (LocTextHelper.ImportTranslation(
			InCulture, Translation.Namespace, Translation.Key, nullptr, Source, Translated, false))
		{
			++Applied;
		}
	}

	if (Matched == 0)
	{
		OutError = FString::Printf(
			TEXT("No matching strings to import: %d translation(s) arrived from Polyglyph and %d matched "
				"the manifest (gather may be out of date)."),
			InTranslations.Num(),
			Matched);
		return false;
	}

	if (Applied == 0)
	{
		OutError = FString::Printf(
			TEXT("Polyglyph sent %d translation(s) and %d matched the manifest, but none could be applied "
				"to the archive."),
			InTranslations.Num(),
			Matched);
		return false;
	}

	if (!LocTextHelper.SaveArchive(InCulture, &LoadError))
	{
		OutError = LoadError.ToString();
		return false;
	}

	return true;
}

bool FPolyglyphArchive::CompileCulture(const FString& InCulture, FString& OutError)
{
	const FString TargetName = GetTargetName();
	const FString TargetPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Localization"), TargetName);
	const FString ManifestName = TargetName + TEXT(".manifest");
	const FString ArchiveName = TargetName + TEXT(".archive");
	const FString ResourceName = TargetName + TEXT(".locres");

	// GenerateLocRes consults the native culture's archive while resolving translations, so load
	// it alongside the culture being compiled (they are the same archive when compiling "en").
	TArray<FString> Cultures;
	Cultures.Add(TEXT("en"));
	Cultures.AddUnique(InCulture);
	FLocTextHelper LocTextHelper(TargetPath, ManifestName, ArchiveName, TEXT("en"), Cultures, nullptr);

	FText LoadError;
	if (!LocTextHelper.LoadManifest(ELocTextHelperLoadFlags::Load, &LoadError)
		|| !LocTextHelper.LoadAllArchives(ELocTextHelperLoadFlags::Load, &LoadError))
	{
		OutError = LoadError.ToString();
		return false;
	}

	const FTextKey LocResId = TargetPath / InCulture / ResourceName;
	FTextLocalizationResource LocRes;
	TMap<FName, TSharedRef<FTextLocalizationResource>> PerPlatformLocRes;
	if (!FTextLocalizationResourceGenerator::GenerateLocRes(
		LocTextHelper, InCulture, EGenerateLocResFlags::None, LocResId, LocRes, PerPlatformLocRes))
	{
		OutError = FString::Printf(TEXT("Failed to generate .locres for %s."), *InCulture);
		return false;
	}

	const FString LocResPath = FPaths::Combine(TargetPath, InCulture, ResourceName);
	if (!LocRes.SaveToFile(LocResPath))
	{
		OutError = FString::Printf(TEXT("Failed to write %s."), *LocResPath);
		return false;
	}

	return true;
}

bool FPolyglyphArchive::UpdateWordCountReport(ULocalizationTarget* InLocalizationTarget, FString& OutError)
{
	if (InLocalizationTarget == nullptr)
	{
		OutError = TEXT("No localization target is selected.");
		return false;
	}

	TArray<FString> Cultures;
	if (!GetConfiguredCultures(*InLocalizationTarget, Cultures, OutError))
	{
		return false;
	}

	const FString TargetPath = LocalizationConfigurationScript::GetDataDirectory(InLocalizationTarget);
	const FString ManifestName = LocalizationConfigurationScript::GetManifestFileName(InLocalizationTarget);
	const FString ArchiveName = LocalizationConfigurationScript::GetArchiveFileName(InLocalizationTarget);
	FLocTextHelper LocTextHelper(TargetPath, ManifestName, ArchiveName, FString(), Cultures, nullptr);

	FText LoadError;
	if (!LocTextHelper.LoadAll(ELocTextHelperLoadFlags::LoadOrCreate, &LoadError))
	{
		OutError = FString::Printf(
			TEXT("Could not load localization data while refreshing word counts: %s"),
			*LoadError.ToString());
		return false;
	}

	const FString ReportPath = LocalizationConfigurationScript::GetWordCountCSVPath(InLocalizationTarget);
	if (!LocTextHelper.SaveWordCountReport(FDateTime::Now(), ReportPath, &LoadError))
	{
		OutError = LoadError.ToString();
		return false;
	}

	if (!InLocalizationTarget->UpdateWordCountsFromCSV())
	{
		OutError = FString::Printf(TEXT("Could not read the generated word-count report at %s."), *ReportPath);
		return false;
	}

	InLocalizationTarget->PostEditChange();
	return true;
}
