// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphManifest.h"

#include "Internationalization/InternationalizationManifest.h"
#include "LocTextHelper.h"
#include "Misc/Paths.h"

#include "PolyglyphProjectSettings.h"
#include "PolyglyphTypes.h"

bool FPolyglyphManifest::GatherSourceStrings(TArray<FPolyglyphSourceString>& OutStrings, FString& OutError)
{
	OutStrings.Reset();

	const UPolyglyphProjectSettings* Project = GetDefault<UPolyglyphProjectSettings>();
	const FString TargetName = Project->LocalizationTarget.IsEmpty() ? TEXT("Game") : Project->LocalizationTarget;

	// The Localization Dashboard writes the target's data under Content/Localization/<Target>/.
	const FString TargetPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Localization"), TargetName);
	const FString ManifestName = TargetName + TEXT(".manifest");

	if (!FPaths::FileExists(FPaths::Combine(TargetPath, ManifestName)))
	{
		OutError = FString::Printf(
			TEXT("No manifest for localization target '%s'. Create the target in the "
				"Localization Dashboard and run Gather Text first."),
			*TargetName);
		return false;
	}

	// Read the source side of the target only; cultures/archives are not needed for a push.
	const TArray<FString> NoCultures;
	FLocTextHelper LocTextHelper(TargetPath, ManifestName, FString(), TEXT("en"), NoCultures, nullptr);

	FText LoadError;
	if (!LocTextHelper.LoadManifest(ELocTextHelperLoadFlags::Load, &LoadError))
	{
		OutError = LoadError.ToString();
		return false;
	}

	LocTextHelper.EnumerateSourceTexts(
		[&OutStrings](TSharedRef<FManifestEntry> InEntry) -> bool
		{
			for (const FManifestContext& Context : InEntry->Contexts)
			{
				FPolyglyphSourceString Item;
				Item.Namespace = InEntry->Namespace.GetString();
				Item.Key = Context.Key.GetString();
				Item.SourceText = InEntry->Source.Text;
				OutStrings.Add(MoveTemp(Item));
			}
			return true;
		},
		true);

	if (OutStrings.Num() == 0)
	{
		OutError = TEXT("The localization manifest has no source strings. Add localizable FText "
			"and run Gather Text, then try again.");
		return false;
	}

	return true;
}
