// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphPull.h"

#include "PolyglyphArchive.h"
#include "PolyglyphClient.h"
#include "PolyglyphProjectSettings.h"
#include "PolyglyphTypes.h"

namespace
{
	/** Shared completion state for asynchronous per-culture pull requests. */
	struct FPolyglyphPullProgress
	{
		explicit FPolyglyphPullProgress(
			int32 InCultureCount,
			TFunction<void(bool, const FString&)> InDone)
			: Remaining(InCultureCount)
			, Imported(0)
			, Skipped(0)
			, Failed(0)
			, Done(MoveTemp(InDone))
		{
		}

		/** Complete one culture and deliver the aggregate summary after the final callback. */
		void CompleteCulture()
		{
			--Remaining;
			if (Remaining > 0)
			{
				return;
			}

			FString Summary = FString::Printf(
				TEXT("Imported %d culture(s), skipped %d, failed %d."),
				Imported,
				Skipped,
				Failed);
			if (Imported > 0)
			{
				Summary += TEXT(" .locres updated.");
			}
			if (Messages.Num() > 0)
			{
				Summary += TEXT(" ") + FString::Join(Messages, TEXT("; "));
			}
			Done(Failed == 0, Summary);
		}

		int32 Remaining;
		int32 Imported;
		int32 Skipped;
		int32 Failed;
		TArray<FString> Messages;
		TFunction<void(bool, const FString&)> Done;
	};

	/** Describe why a valid pull response contained no importable translations. */
	FString BuildSkipMessage(
		const FString& InCulture,
		const FPolyglyphPullCounts& InCounts,
		bool IncludeUnapprovedDrafts)
	{
		if (!IncludeUnapprovedDrafts && InCounts.NeedsReview > 0 && InCounts.Approved == 0)
		{
			return FString::Printf(
				TEXT("%s: %d translation(s) are waiting for approval in Polyglyph. Approve them, or enable "
					"Include unapproved drafts."),
				*InCulture,
				InCounts.NeedsReview);
		}

		if (InCounts.Untranslated > 0)
		{
			return FString::Printf(
				TEXT("%s: No translations are available yet; %d source string(s) are untranslated in Polyglyph."),
				*InCulture,
				InCounts.Untranslated);
		}

		return FString::Printf(TEXT("%s: Polyglyph returned no translations for this culture."), *InCulture);
	}
}

void FPolyglyphPull::Run(TFunction<void(bool, const FString&)> OnDone)
{
	FPolyglyphClient::TestConnection(
		[Done = MoveTemp(OnDone)](const FPolyglyphResponse& StatusResponse)
		{
			if (!StatusResponse.bSuccess)
			{
				Done(false, StatusResponse.Error);
				return;
			}

			FPolyglyphProjectStatus Status;
			FPolyglyphProjectStatus::ParseStatusResponse(StatusResponse.Json, Status);

			TArray<FString> Cultures;
			for (const FPolyglyphLanguageStatus& Language : Status.Languages)
			{
				if (Language.bEnabled)
				{
					Cultures.Add(Language.Code);
				}
			}

			if (Cultures.Num() == 0)
			{
				Done(false, TEXT("No enabled languages to pull."));
				return;
			}

			const UPolyglyphProjectSettings* const Project = GetDefault<UPolyglyphProjectSettings>();
			const bool IncludeUnapprovedDrafts = Project->bIncludeUnapprovedDrafts;
			const TSharedRef<FPolyglyphPullProgress> Progress =
				MakeShared<FPolyglyphPullProgress>(Cultures.Num(), Done);

			for (const FString& Culture : Cultures)
			{
				FPolyglyphClient::PullTranslations(
					Culture,
					IncludeUnapprovedDrafts,
					[Culture, IncludeUnapprovedDrafts, Progress](
						const FPolyglyphResponse& PullResponse,
						const FPolyglyphPullResult& PullResult)
					{
						if (!PullResponse.bSuccess)
						{
							++Progress->Failed;
							Progress->Messages.Add(
								FString::Printf(TEXT("%s: %s"), *Culture, *PullResponse.Error));
						}
						else if (PullResult.Translations.Num() == 0)
						{
							++Progress->Skipped;
							Progress->Messages.Add(
								BuildSkipMessage(Culture, PullResult.Counts, IncludeUnapprovedDrafts));
						}
						else
						{
							FString StepError;
							if (FPolyglyphArchive::ImportTranslations(
								Culture,
								PullResult.Translations,
								StepError)
								&& FPolyglyphArchive::CompileCulture(Culture, StepError))
							{
								++Progress->Imported;
							}
							else
							{
								++Progress->Failed;
								Progress->Messages.Add(
									FString::Printf(TEXT("%s: %s"), *Culture, *StepError));
							}
						}

						Progress->CompleteCulture();
					});
			}
		});
}
