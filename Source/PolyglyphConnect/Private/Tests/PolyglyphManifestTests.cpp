// Copyright © ToaGames. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Internationalization/InternationalizationMetadata.h"
#include "Misc/AutomationTest.h"

#include "PolyglyphManifest.h"
#include "PolyglyphTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPolyglyphDialogueManifestMetadataTest,
	"PolyglyphConnect.Manifest.DialogueMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPolyglyphDialogueManifestMetadataTest::RunTest(const FString& InParameters)
{
	static_cast<void>(InParameters);

	const TSharedRef<FLocMetadataObject> InfoMetadata = MakeShared<FLocMetadataObject>();
	InfoMetadata->SetStringField(TEXT("Context"), TEXT("DV_CaptainRooke -> DV_SisterWren"));
	InfoMetadata->SetStringField(
		TEXT("VoiceActorDirection"),
		TEXT("Gruff and guarded. First words when the player meets Captain Rooke."));

	const TSharedRef<FLocMetadataObject> KeyMetadata = MakeShared<FLocMetadataObject>();
	KeyMetadata->SetStringField(TEXT("Speaker"), TEXT("F52AE19A4487C5AA99CDBFB63714FA9A"));
	KeyMetadata->SetStringField(TEXT("Gender"), TEXT("Masculine"));
	KeyMetadata->SetStringField(TEXT("Plurality"), TEXT("Singular"));
	KeyMetadata->SetStringField(TEXT("TargetGender"), TEXT("Feminine"));
	KeyMetadata->SetStringField(TEXT("TargetPlurality"), TEXT("Singular"));

	FPolyglyphEnrichItem Item;
	TestTrue(
		TEXT("Builds enrichment from standard DialogueWave metadata"),
		FPolyglyphManifest::BuildDialogueEnrichment(
			TEXT("Dialogue"),
			TEXT("WaveGuid_ContextHash"),
			InfoMetadata,
			KeyMetadata,
			Item));
	TestEqual(TEXT("Keeps the manifest namespace"), Item.Namespace, FString(TEXT("Dialogue")));
	TestEqual(TEXT("Keeps the context-specific key"), Item.Key, FString(TEXT("WaveGuid_ContextHash")));
	TestEqual(TEXT("Converts the DialogueVoice asset name"), Item.Character, FString(TEXT("Captain Rooke")));
	TestEqual(TEXT("Reads the speaker gender"), Item.Gender, FString(TEXT("Masculine")));
	TestTrue(TEXT("Includes the target character"), Item.Context.Contains(TEXT("Sister Wren")));
	TestTrue(TEXT("Includes voice direction"), Item.Context.Contains(TEXT("Gruff and guarded")));
	TestTrue(TEXT("Includes grammatical context"), Item.Context.Contains(TEXT("Target gender: Feminine")));

	const TSharedRef<FLocMetadataObject> PlainMetadata = MakeShared<FLocMetadataObject>();
	PlainMetadata->SetStringField(TEXT("Context"), TEXT("A regular translator note"));
	TestFalse(
		TEXT("Ignores non-dialogue manifest metadata without a speaker identity"),
		FPolyglyphManifest::BuildDialogueEnrichment(
			TEXT("UI"),
			TEXT("PlayButton"),
			PlainMetadata,
			TSharedPtr<FLocMetadataObject>(),
			Item));

	return true;
}

#endif
