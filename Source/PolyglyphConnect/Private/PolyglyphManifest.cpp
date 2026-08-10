// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphManifest.h"

#include "Internationalization/InternationalizationManifest.h"
#include "Internationalization/InternationalizationMetadata.h"
#include "LocTextHelper.h"
#include "Misc/Paths.h"

#include "PolyglyphProjectSettings.h"
#include "PolyglyphTypes.h"

namespace
{
	/** Read an optional string metadata field without triggering the metadata API's ensures. */
	FString ReadStringField(const TSharedPtr<FLocMetadataObject>& InMetadata, const FString& InFieldName)
	{
		if (!InMetadata.IsValid() || !InMetadata->HasTypedField<ELocMetadataType::String>(InFieldName))
		{
			return FString();
		}

		return InMetadata->GetStringField(InFieldName);
	}

	/** Convert a DialogueVoice object name into the character name shown in Polyglyph. */
	FString DisplayVoiceName(const FString& InObjectName)
	{
		FString ObjectName = InObjectName.TrimStartAndEnd();
		for (const FString& Prefix : { TEXT("DialogueVoice_"), TEXT("DV_"), TEXT("Voice_") })
		{
			if (ObjectName.StartsWith(Prefix, ESearchCase::IgnoreCase))
			{
				ObjectName.RightChopInline(Prefix.Len());
				break;
			}
		}

		return FName::NameToDisplayString(ObjectName, false).TrimStartAndEnd();
	}

	/** Convert Unreal's compact "speaker -> target,target" context to readable character names. */
	bool ParseDialogueContext(
		const FString& InContext,
		FString& OutSpeaker,
		TArray<FString>& OutTargets)
	{
		OutSpeaker.Reset();
		OutTargets.Reset();

		FString SpeakerObjectName;
		FString TargetObjectNames;
		if (!InContext.Split(TEXT(" -> "), &SpeakerObjectName, &TargetObjectNames))
		{
			return false;
		}

		OutSpeaker = DisplayVoiceName(SpeakerObjectName);
		TArray<FString> TargetNames;
		TargetObjectNames.ParseIntoArray(TargetNames, TEXT(","), true);
		for (const FString& TargetName : TargetNames)
		{
			const FString DisplayName = DisplayVoiceName(TargetName);
			if (!DisplayName.IsEmpty())
			{
				OutTargets.Add(DisplayName);
			}
		}

		return !OutSpeaker.IsEmpty();
	}

	/** Build a translator-facing note from the standard DialogueWave metadata fields. */
	FString BuildDialogueNote(
		const FString& InSpeaker,
		const TArray<FString>& InTargets,
		const FString& InDirection,
		const FString& InPlurality,
		const FString& InTargetGender,
		const FString& InTargetPlurality)
	{
		TArray<FString> Notes;
		if (InTargets.Num() > 0)
		{
			Notes.Add(FString::Printf(
				TEXT("Dialogue context: %s speaks to %s."),
				*InSpeaker,
				*FString::Join(InTargets, TEXT(", "))));
		}
		else
		{
			Notes.Add(FString::Printf(TEXT("Dialogue speaker: %s."), *InSpeaker));
		}
		if (!InDirection.IsEmpty())
		{
			Notes.Add(FString::Printf(TEXT("Voice direction: %s"), *InDirection));
		}
		if (!InPlurality.IsEmpty())
		{
			Notes.Add(FString::Printf(TEXT("Speaker plurality: %s."), *InPlurality));
		}
		if (!InTargetGender.IsEmpty())
		{
			Notes.Add(FString::Printf(TEXT("Target gender: %s."), *InTargetGender));
		}
		if (!InTargetPlurality.IsEmpty())
		{
			Notes.Add(FString::Printf(TEXT("Target plurality: %s."), *InTargetPlurality));
		}

		return FString::Join(Notes, TEXT(" "));
	}
}

bool FPolyglyphManifest::Gather(
	TArray<FPolyglyphSourceString>& OutStrings,
	TArray<FPolyglyphEnrichItem>& OutEnrichItems,
	FString& OutError)
{
	OutStrings.Reset();
	OutEnrichItems.Reset();

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

	// Read the source side of the target only; no archives are loaded for a push, but the helper
	// asserts on an empty archive name, so pass the target's real one.
	const TArray<FString> NoCultures;
	FLocTextHelper LocTextHelper(
		TargetPath, ManifestName, TargetName + TEXT(".archive"), TEXT("en"), NoCultures, nullptr);

	FText LoadError;
	if (!LocTextHelper.LoadManifest(ELocTextHelperLoadFlags::Load, &LoadError))
	{
		OutError = LoadError.ToString();
		return false;
	}

	LocTextHelper.EnumerateSourceTexts(
		[&OutStrings, &OutEnrichItems](TSharedRef<FManifestEntry> InEntry) -> bool
		{
			for (const FManifestContext& Context : InEntry->Contexts)
			{
				FPolyglyphSourceString Item;
				Item.Namespace = InEntry->Namespace.GetString();
				Item.Key = Context.Key.GetString();
				Item.SourceText = InEntry->Source.Text;

				FPolyglyphEnrichItem EnrichItem;
				if (BuildDialogueEnrichment(
					Item.Namespace,
					Item.Key,
					Context.InfoMetadataObj,
					Context.KeyMetadataObj,
					EnrichItem))
				{
					Item.Context = EnrichItem.Context;
					OutEnrichItems.Add(MoveTemp(EnrichItem));
				}
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

bool FPolyglyphManifest::BuildDialogueEnrichment(
	const FString& InNamespace,
	const FString& InKey,
	const TSharedPtr<FLocMetadataObject>& InInfoMetadata,
	const TSharedPtr<FLocMetadataObject>& InKeyMetadata,
	FPolyglyphEnrichItem& OutItem)
{
	OutItem = FPolyglyphEnrichItem();

	const FString SpeakerGuid = ReadStringField(InKeyMetadata, TEXT("Speaker"));
	const FString CompactContext = ReadStringField(InInfoMetadata, TEXT("Context"));
	if (SpeakerGuid.IsEmpty() || CompactContext.IsEmpty())
	{
		return false;
	}

	FString Speaker;
	TArray<FString> Targets;
	if (!ParseDialogueContext(CompactContext, Speaker, Targets))
	{
		return false;
	}

	OutItem.Namespace = InNamespace;
	OutItem.Key = InKey;
	OutItem.Character = Speaker;
	OutItem.Gender = ReadStringField(InKeyMetadata, TEXT("Gender"));
	OutItem.Context = BuildDialogueNote(
		Speaker,
		Targets,
		ReadStringField(InInfoMetadata, TEXT("VoiceActorDirection")),
		ReadStringField(InKeyMetadata, TEXT("Plurality")),
		ReadStringField(InKeyMetadata, TEXT("TargetGender")),
		ReadStringField(InKeyMetadata, TEXT("TargetPlurality")));
	return true;
}
