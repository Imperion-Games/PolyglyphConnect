// Copyright © ToaGames. All Rights Reserved.

#include "SPolyglyphPanel.h"

#include "HAL/PlatformProcess.h"
#include "PolyglyphClient.h"
#include "PolyglyphManifest.h"
#include "PolyglyphProjectSettings.h"
#include "PolyglyphPull.h"
#include "PolyglyphSettings.h"
#include "PolyglyphTranslate.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PolyglyphConnect"

namespace
{
	constexpr int32 DashboardViewIndex = 0;
	constexpr int32 SettingsViewIndex = 1;
}

void SPolyglyphPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SAssignNew(ViewSwitcher, SWidgetSwitcher)
		+ SWidgetSwitcher::Slot()[ BuildDashboardView() ]
		+ SWidgetSwitcher::Slot()[ BuildSettingsView() ]
	];

	LoadSettingsIntoFields();
	RebuildLanguageBoard();

	if (IsConfigured())
	{
		ShowDashboard();
		RefreshStatus();
	}
	else
	{
		LastMessage = LOCTEXT("FirstRun", "Welcome. Enter your connection details below to begin.");
		ShowSettings();
	}
}

TSharedRef<SWidget> SPolyglyphPanel::BuildDashboardView()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 12.0f, 12.0f, 6.0f)
		[ BuildHeaderRow() ]
		+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 0.0f, 12.0f, 8.0f)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return GetProjectLineText(); })
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 0.0f, 12.0f, 8.0f)
		[ BuildActionBar() ]
		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(12.0f, 0.0f)
		[ BuildLanguagesCard() ]
		+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 8.0f, 12.0f, 12.0f)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return LastMessage; })
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		];
}

TSharedRef<SWidget> SPolyglyphPanel::BuildHeaderRow()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PanelTitle", "Polyglyph"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.0f, 2.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return GetStateText(); })
			.ColorAndOpacity_Lambda([this]() { return GetStateColor(); })
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f)[ SNullWidget::NullWidget ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Refresh", "Refresh"))
			.ToolTipText(LOCTEXT("RefreshTip", "Re-fetch the project status."))
			.IsEnabled_Lambda([this]() { return !bIsBusy; })
			.OnClicked(FOnClicked::CreateLambda([this]() { RefreshStatus(); return FReply::Handled(); }))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("SettingsBtn", "Settings"))
			.ToolTipText(LOCTEXT("SettingsBtnTip", "Edit your connection and API key."))
			.OnClicked(FOnClicked::CreateLambda([this]() { ShowSettings(); return FReply::Handled(); }))
		];
}

TSharedRef<SWidget> SPolyglyphPanel::BuildActionBar()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Push", "Push source"))
			.ToolTipText(LOCTEXT("PushTip", "Gather the localization manifest and push every source string."))
			.IsEnabled_Lambda([this]() { return !bIsBusy; })
			.OnClicked(FOnClicked::CreateSP(this, &SPolyglyphPanel::OnPushClicked))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Translate", "Translate"))
			.ToolTipText(LOCTEXT("TranslateTip", "Start AI translation for all enabled languages. Review and approve in Polyglyph, then Pull."))
			.IsEnabled_Lambda([this]() { return !bIsBusy; })
			.OnClicked(FOnClicked::CreateSP(this, &SPolyglyphPanel::OnTranslateClicked))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Pull", "Pull approved"))
			.ToolTipText(LOCTEXT("PullTip", "Import approved translations and compile .locres."))
			.IsEnabled_Lambda([this]() { return !bIsBusy; })
			.OnClicked(FOnClicked::CreateSP(this, &SPolyglyphPanel::OnPullClicked))
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f)[ SNullWidget::NullWidget ]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("OpenWeb", "Open in Polyglyph"))
			.ToolTipText(LOCTEXT("OpenWebTip", "Open the Polyglyph web dashboard to review, approve, and manage."))
			.OnClicked(FOnClicked::CreateSP(this, &SPolyglyphPanel::OnOpenInPolyglyphClicked))
		];
}

TSharedRef<SWidget> SPolyglyphPanel::BuildLanguagesCard()
{
	return MakeCard(
		LOCTEXT("LanguagesTitle", "Languages"),
		SNew(SScrollBox)
		+ SScrollBox::Slot()[ SAssignNew(LanguageBox, SVerticalBox) ]);
}

TSharedRef<SWidget> SPolyglyphPanel::BuildSettingsView()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 12.0f, 12.0f, 8.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SettingsTitle", "Polyglyph settings"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 0.0f, 12.0f, 8.0f)
		[ BuildConnectionCard() ]
		+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 0.0f, 12.0f, 8.0f)
		[ BuildKeyCard() ]
		+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 0.0f, 12.0f, 8.0f)
		[ BuildSettingsButtons() ]
		+ SVerticalBox::Slot().FillHeight(1.0f)[ SNullWidget::NullWidget ]
		+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 0.0f, 12.0f, 12.0f)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return LastMessage; })
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		];
}

TSharedRef<SWidget> SPolyglyphPanel::BuildConnectionCard()
{
	return MakeCard(
		LOCTEXT("ConnCard", "Connection (shared with your team, committed)"),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[ MakeLabeledRow(LOCTEXT("BaseUrl", "API Base URL"),
			SAssignNew(BaseUrlBox, SEditableTextBox).HintText(LOCTEXT("BaseUrlHint", "https://api.polyglyph.app"))) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[ MakeLabeledRow(LOCTEXT("Slug", "Project Slug"),
			SAssignNew(SlugBox, SEditableTextBox).HintText(LOCTEXT("SlugHint", "as shown in the dashboard"))) ]
		+ SVerticalBox::Slot().AutoHeight()
		[ MakeLabeledRow(LOCTEXT("Target", "Localization Target"),
			SAssignNew(TargetBox, SEditableTextBox).HintText(LOCTEXT("TargetHint", "Game"))) ]);
}

TSharedRef<SWidget> SPolyglyphPanel::BuildKeyCard()
{
	return MakeCard(
		LOCTEXT("KeyCard", "Your API key (private, never committed)"),
		MakeLabeledRow(LOCTEXT("ApiKey", "API Key"),
			SAssignNew(ApiKeyBox, SEditableTextBox).IsPassword(true)));
}

TSharedRef<SWidget> SPolyglyphPanel::BuildSettingsButtons()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Save", "Save and connect"))
			.OnClicked(FOnClicked::CreateSP(this, &SPolyglyphPanel::OnSaveSettingsClicked))
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("Cancel", "Cancel"))
			.OnClicked(FOnClicked::CreateLambda([this]() { LoadSettingsIntoFields(); ShowDashboard(); return FReply::Handled(); }))
		];
}

TSharedRef<SWidget> SPolyglyphPanel::MakeCard(const FText& InTitle, TSharedRef<SWidget> InContent) const
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Padding(12.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.Text(InTitle)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)[ InContent ]
		];
}

TSharedRef<SWidget> SPolyglyphPanel::MakeLabeledRow(const FText& InLabel, TSharedRef<SWidget> InInput) const
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(135.0f)
			[ SNew(STextBlock).Text(InLabel) ]
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[ InInput ];
}

TSharedRef<SWidget> SPolyglyphPanel::MakeLanguageRow(const FPolyglyphLanguageStatus& InLanguage) const
{
	const float Fraction = FMath::Clamp(InLanguage.ApprovedPct / 100.0f, 0.0f, 1.0f);
	const FText Counts = InLanguage.bComplete
		? LOCTEXT("LangComplete", "Complete")
		: FText::Format(
			LOCTEXT("LangCounts", "{0}% approved, {1} strings"),
			FText::AsNumber(FMath::RoundToInt(InLanguage.ApprovedPct)),
			FText::AsNumber(InLanguage.Approved));

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(60.0f)
			[ SNew(STextBlock).Text(FText::FromString(InLanguage.Code)) ]
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(8.0f, 0.0f)
		[
			SNew(SBox).HeightOverride(8.0f)
			[ SNew(SProgressBar).Percent(Fraction) ]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).MinDesiredWidth(150.0f).HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Text(Counts)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		];
}

void SPolyglyphPanel::RefreshStatus()
{
	if (bIsBusy)
	{
		return;
	}

	bIsBusy = true;
	State = EConnectionState::Checking;
	LastMessage = LOCTEXT("Refreshing", "Contacting Polyglyph...");

	TWeakPtr<SPolyglyphPanel> WeakSelf = SharedThis(this);
	FPolyglyphClient::TestConnection([WeakSelf](const FPolyglyphResponse& Response)
	{
		if (const TSharedPtr<SPolyglyphPanel> Self = WeakSelf.Pin())
		{
			Self->ApplyStatus(Response);
		}
	});
}

void SPolyglyphPanel::ApplyStatus(const FPolyglyphResponse& InResponse)
{
	bIsBusy = false;

	if (InResponse.bSuccess)
	{
		FPolyglyphProjectStatus::FromJson(InResponse.Json, Status);
		State = EConnectionState::Connected;
		LastMessage = FText::Format(
			LOCTEXT("Connected", "Connected. {0} source strings."),
			FText::AsNumber(Status.TotalStrings));
	}
	else
	{
		State = EConnectionState::Failed;
		LastMessage = InResponse.Error.IsEmpty()
			? LOCTEXT("FailedGeneric", "Could not reach Polyglyph.")
			: FText::FromString(InResponse.Error);
	}

	RebuildLanguageBoard();
}

void SPolyglyphPanel::RebuildLanguageBoard()
{
	if (!LanguageBox.IsValid())
	{
		return;
	}

	LanguageBox->ClearChildren();

	if (Status.Languages.Num() == 0)
	{
		LanguageBox->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoLanguages", "No languages to show yet."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
		return;
	}

	for (const FPolyglyphLanguageStatus& Language : Status.Languages)
	{
		LanguageBox->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
		[ MakeLanguageRow(Language) ];
	}
}

FReply SPolyglyphPanel::OnPushClicked()
{
	if (bIsBusy)
	{
		return FReply::Handled();
	}

	TArray<FPolyglyphSourceString> Strings;
	FString GatherError;
	if (!FPolyglyphManifest::GatherSourceStrings(Strings, GatherError))
	{
		LastMessage = FText::FromString(GatherError);
		return FReply::Handled();
	}

	bIsBusy = true;
	LastMessage = FText::Format(
		LOCTEXT("Pushing", "Pushing {0} source strings..."),
		FText::AsNumber(Strings.Num()));

	TWeakPtr<SPolyglyphPanel> WeakSelf = SharedThis(this);
	FPolyglyphClient::PushStrings(Strings, [WeakSelf](const FPolyglyphResponse& Response)
	{
		if (const TSharedPtr<SPolyglyphPanel> Self = WeakSelf.Pin())
		{
			Self->bIsBusy = false;
			if (Response.bSuccess)
			{
				Self->LastMessage = LOCTEXT("PushOk", "Push complete. Refreshing status...");
				Self->RefreshStatus();
			}
			else
			{
				Self->LastMessage = FText::FromString(Response.Error);
			}
		}
	});

	return FReply::Handled();
}

FReply SPolyglyphPanel::OnTranslateClicked()
{
	if (bIsBusy)
	{
		return FReply::Handled();
	}

	bIsBusy = true;
	LastMessage = LOCTEXT("Translating", "Starting translation...");

	TWeakPtr<SPolyglyphPanel> WeakSelf = SharedThis(this);
	FPolyglyphTranslate::Run(FString(), false,
		[WeakSelf](bool bSuccess, const FString& Summary, const TArray<FPolyglyphTriggeredJob>&)
	{
		if (const TSharedPtr<SPolyglyphPanel> Self = WeakSelf.Pin())
		{
			Self->bIsBusy = false;
			Self->LastMessage = FText::FromString(Summary);
			if (bSuccess)
			{
				Self->RefreshStatus();
			}
		}
	});

	return FReply::Handled();
}

FReply SPolyglyphPanel::OnPullClicked()
{
	if (bIsBusy)
	{
		return FReply::Handled();
	}

	bIsBusy = true;
	LastMessage = LOCTEXT("Pulling", "Pulling approved translations...");

	TWeakPtr<SPolyglyphPanel> WeakSelf = SharedThis(this);
	FPolyglyphPull::Run([WeakSelf](bool bSuccess, const FString& Summary)
	{
		if (const TSharedPtr<SPolyglyphPanel> Self = WeakSelf.Pin())
		{
			Self->bIsBusy = false;
			Self->LastMessage = FText::FromString(Summary);
			if (bSuccess)
			{
				Self->RefreshStatus();
			}
		}
	});

	return FReply::Handled();
}

FReply SPolyglyphPanel::OnOpenInPolyglyphClicked()
{
	const UPolyglyphProjectSettings* Project = GetDefault<UPolyglyphProjectSettings>();
	if (!Project->BaseUrl.IsEmpty())
	{
		FPlatformProcess::LaunchURL(*Project->BaseUrl, nullptr, nullptr);
	}
	return FReply::Handled();
}

void SPolyglyphPanel::ShowDashboard()
{
	if (ViewSwitcher.IsValid())
	{
		ViewSwitcher->SetActiveWidgetIndex(DashboardViewIndex);
	}
}

void SPolyglyphPanel::ShowSettings()
{
	LoadSettingsIntoFields();
	if (ViewSwitcher.IsValid())
	{
		ViewSwitcher->SetActiveWidgetIndex(SettingsViewIndex);
	}
}

void SPolyglyphPanel::LoadSettingsIntoFields()
{
	const UPolyglyphProjectSettings* Project = GetDefault<UPolyglyphProjectSettings>();
	const UPolyglyphSettings* User = GetDefault<UPolyglyphSettings>();

	if (BaseUrlBox.IsValid()) { BaseUrlBox->SetText(FText::FromString(Project->BaseUrl)); }
	if (SlugBox.IsValid())    { SlugBox->SetText(FText::FromString(Project->ProjectSlug)); }
	if (TargetBox.IsValid())  { TargetBox->SetText(FText::FromString(Project->LocalizationTarget)); }
	if (ApiKeyBox.IsValid())  { ApiKeyBox->SetText(FText::FromString(User->ApiKey)); }
}

FReply SPolyglyphPanel::OnSaveSettingsClicked()
{
	UPolyglyphProjectSettings* Project = GetMutableDefault<UPolyglyphProjectSettings>();
	Project->BaseUrl = BaseUrlBox->GetText().ToString().TrimStartAndEnd();
	Project->ProjectSlug = SlugBox->GetText().ToString().TrimStartAndEnd();
	Project->LocalizationTarget = TargetBox->GetText().ToString().TrimStartAndEnd();
	Project->TryUpdateDefaultConfigFile();

	UPolyglyphSettings* User = GetMutableDefault<UPolyglyphSettings>();
	User->ApiKey = ApiKeyBox->GetText().ToString().TrimStartAndEnd();
	User->SaveConfig();

	LastMessage = LOCTEXT("SettingsSaved", "Settings saved.");
	ShowDashboard();

	if (IsConfigured())
	{
		RefreshStatus();
	}

	return FReply::Handled();
}

FText SPolyglyphPanel::GetStateText() const
{
	switch (State)
	{
	case EConnectionState::Checking:  return LOCTEXT("StateChecking", "Checking...");
	case EConnectionState::Connected: return LOCTEXT("StateConnected", "Connected");
	case EConnectionState::Failed:    return LOCTEXT("StateFailed", "Not connected");
	default:                          return LOCTEXT("StateUnconfigured", "Not configured");
	}
}

FSlateColor SPolyglyphPanel::GetStateColor() const
{
	switch (State)
	{
	case EConnectionState::Connected: return FSlateColor(FLinearColor(0.20f, 0.70f, 0.30f));
	case EConnectionState::Failed:    return FSlateColor(FLinearColor(0.85f, 0.25f, 0.25f));
	default:                          return FSlateColor::UseSubduedForeground();
	}
}

FText SPolyglyphPanel::GetProjectLineText() const
{
	if (State != EConnectionState::Connected || Status.Slug.IsEmpty())
	{
		return LOCTEXT("ProjectLineIdle", "Not connected yet. Open Settings to enter your details.");
	}

	return FText::Format(
		LOCTEXT("ProjectLine", "{0}, source {1}, {2} strings"),
		FText::FromString(Status.Slug),
		FText::FromString(Status.SourceLanguage),
		FText::AsNumber(Status.TotalStrings));
}

bool SPolyglyphPanel::IsConfigured() const
{
	const UPolyglyphProjectSettings* Project = GetDefault<UPolyglyphProjectSettings>();
	const UPolyglyphSettings* User = GetDefault<UPolyglyphSettings>();
	return !Project->BaseUrl.IsEmpty() && !Project->ProjectSlug.IsEmpty() && !User->ApiKey.IsEmpty();
}

#undef LOCTEXT_NAMESPACE
