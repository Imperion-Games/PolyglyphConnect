// Copyright © ToaGames. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "PolyglyphLocaleMapping.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPolyglyphUnrealLocaleMapperTest,
	"PolyglyphConnect.Locales.UnrealLocaleMapper",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPolyglyphUnrealLocaleMapperTest::RunTest(const FString& InParameters)
{
	static_cast<void>(InParameters);

	FString Error;
	TMap<FString, FPolyglyphLocaleMapping> Mappings;
	TestTrue(TEXT("Loads the shipped Unreal catalog"), FPolyglyphUnrealLocaleCatalog::Load(FString(), Mappings, Error));
	TestTrue(TEXT("Ships a substantial Unreal culture catalog"), Mappings.Num() > 300);

	const TArray<FString> GameTargetCultures = {
		TEXT("en"), TEXT("pt-PT"), TEXT("uk-UA"), TEXT("es-ES"),
		TEXT("fr-FR"), TEXT("it-IT"), TEXT("ja-JP")
	};
	for (const FString& Culture : GameTargetCultures)
	{
		const FPolyglyphLocaleMapping* const Mapping = FPolyglyphUnrealLocaleCatalog::Find(Culture, Mappings);
		TestNotNull(*FString::Printf(TEXT("Finds Game target culture %s"), *Culture), Mapping);
		if (Mapping != nullptr)
		{
			TestEqual(*FString::Printf(TEXT("Keeps canonical Game target culture %s"), *Culture),
				Mapping->LocaleTag,
				Culture);
			TestFalse(*FString::Printf(TEXT("Provides a display name for %s"), *Culture), Mapping->DisplayName.IsEmpty());
		}
	}

	TestNull(TEXT("Does not infer a missing external code"),
		FPolyglyphUnrealLocaleCatalog::Find(TEXT("fr_FR"), Mappings));

	const FString OverrideFile = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PolyglyphLocaleMappingTest.json"));
	const FString OverrideJson = TEXT("{\n"
		"  \"schemaVersion\": 1,\n"
		"  \"integration\": \"unreal\",\n"
		"  \"mappings\": [\n"
		"    {\n"
		"      \"externalCode\": \"STUDIO_ELVISH\",\n"
		"      \"localeTag\": \"x-studio-elvish\",\n"
		"      \"displayName\": \"Studio Elvish\"\n"
		"    }\n"
		"  ]\n"
		"}\n");
	TestTrue(TEXT("Writes a partial project mapping file"), FFileHelper::SaveStringToFile(OverrideJson, *OverrideFile));

	TMap<FString, FPolyglyphLocaleMapping> MappingsWithOverride;
	TestTrue(TEXT("Loads a partial project mapping file"),
		FPolyglyphUnrealLocaleCatalog::Load(OverrideFile, MappingsWithOverride, Error));
	const FPolyglyphLocaleMapping* const CustomMapping =
		FPolyglyphUnrealLocaleCatalog::Find(TEXT("STUDIO_ELVISH"), MappingsWithOverride);
	TestNotNull(TEXT("Finds an entry from the partial project mapping file"), CustomMapping);
	if (CustomMapping != nullptr)
	{
		TestEqual(TEXT("Preserves the private-use locale tag"),
			CustomMapping->LocaleTag,
			FString(TEXT("x-studio-elvish")));
	}

	TestTrue(TEXT("Removes the temporary project mapping file"), IFileManager::Get().Delete(*OverrideFile));

	return true;
}

#endif
