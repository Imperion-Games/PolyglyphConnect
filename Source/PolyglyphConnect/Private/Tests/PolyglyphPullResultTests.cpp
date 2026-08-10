// Copyright © ToaGames. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "PolyglyphTypes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"

namespace
{
	/** Build a valid pull body with one draft translation. */
	TSharedRef<FJsonObject> MakePullBody()
	{
		const TSharedRef<FJsonObject> Translation = MakeShared<FJsonObject>();
		Translation->SetStringField(TEXT("namespace"), TEXT("Game"));
		Translation->SetStringField(TEXT("key"), TEXT("Greeting"));
		Translation->SetStringField(TEXT("value"), TEXT("Bonjour"));

		TArray<TSharedPtr<FJsonValue>> Strings;
		Strings.Add(MakeShared<FJsonValueObject>(Translation));

		const TSharedRef<FJsonObject> Counts = MakeShared<FJsonObject>();
		Counts->SetNumberField(TEXT("totalStrings"), 13);
		Counts->SetNumberField(TEXT("returned"), 1);
		Counts->SetNumberField(TEXT("approved"), 0);
		Counts->SetNumberField(TEXT("needsReview"), 13);
		Counts->SetNumberField(TEXT("untranslated"), 0);

		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetArrayField(TEXT("strings"), Strings);
		Root->SetObjectField(TEXT("counts"), Counts);
		return Root;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPolyglyphPullResultTest,
	"PolyglyphConnect.Pull.ResponseContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPolyglyphPullResultTest::RunTest(const FString& InParameters)
{
	static_cast<void>(InParameters);

	FPolyglyphPullResult Result;
	FString Error;
	const TSharedRef<FJsonObject> ValidBody = MakePullBody();
	TestTrue(TEXT("Parses translations and required counts"),
		FPolyglyphPullResult::ParsePullResponse(ValidBody, Result, Error));
	TestEqual(TEXT("Parses the returned translation count"), Result.Translations.Num(), 1);
	TestEqual(TEXT("Parses total source strings"), Result.Counts.TotalStrings, 13);
	TestEqual(TEXT("Parses drafts waiting for review"), Result.Counts.NeedsReview, 13);

	const TSharedRef<FJsonObject> MissingCounts = MakeShared<FJsonObject>();
	MissingCounts->SetArrayField(TEXT("strings"), TArray<TSharedPtr<FJsonValue>>());
	TestFalse(TEXT("Rejects a pull body without counts"),
		FPolyglyphPullResult::ParsePullResponse(MissingCounts, Result, Error));

	const TSharedRef<FJsonObject> MismatchedBody = MakePullBody();
	MismatchedBody->GetObjectField(TEXT("counts"))->SetNumberField(TEXT("returned"), 0);
	TestFalse(TEXT("Rejects a returned count that disagrees with strings"),
		FPolyglyphPullResult::ParsePullResponse(MismatchedBody, Result, Error));

	return true;
}

#endif
