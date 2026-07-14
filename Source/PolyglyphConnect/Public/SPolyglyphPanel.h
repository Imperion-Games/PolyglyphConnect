// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PolyglyphTypes.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class SVerticalBox;
class SWidget;
class SWidgetSwitcher;
struct FPolyglyphResponse;

/**
 * Single dockable home for PolyglyphConnect: a status strip + sync actions (Dashboard screen)
 * and an inline connection editor (Settings screen) that writes straight into the settings
 * config. Rich translation views live in the Polyglyph web dashboard; this is a bridge.
 */
class SPolyglyphPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPolyglyphPanel) {}
	SLATE_END_ARGS()

	/** Build both screens and show the right one for the current config. */
	void Construct(const FArguments& InArgs);

private:
	/** Connection state reflected in the header badge. */
	enum class EConnectionState : uint8
	{
		Unconfigured,
		Checking,
		Connected,
		Failed,
	};

	/** Assemble the Dashboard and Settings screens. */
	TSharedRef<SWidget> BuildDashboardView();
	TSharedRef<SWidget> BuildSettingsView();

	/** Dashboard sub-sections. */
	TSharedRef<SWidget> BuildHeaderRow();
	TSharedRef<SWidget> BuildActionBar();
	TSharedRef<SWidget> BuildLanguagesCard();

	/** Settings sub-sections. */
	TSharedRef<SWidget> BuildConnectionCard();
	TSharedRef<SWidget> BuildKeyCard();
	TSharedRef<SWidget> BuildSettingsButtons();

	/** A titled, bordered section. */
	TSharedRef<SWidget> MakeCard(const FText& InTitle, TSharedRef<SWidget> InContent) const;

	/** A label + input row for the settings form. */
	TSharedRef<SWidget> MakeLabeledRow(const FText& InLabel, TSharedRef<SWidget> InInput) const;

	/** One language progress row for the board. */
	TSharedRef<SWidget> MakeLanguageRow(const FPolyglyphLanguageStatus& InLanguage) const;

	/** Re-fetch GET /status and rebuild the board. Backs the refresh and test actions. */
	void RefreshStatus();

	/** Apply a completed status response to the panel (runs on the game thread). */
	void ApplyStatus(const FPolyglyphResponse& InResponse);

	/** Rebuild the language rows from the cached status. */
	void RebuildLanguageBoard();

	/** Gather the localization manifest and push every source string to Polyglyph. */
	FReply OnPushClicked();

	/** Start AI translation for every enabled language (fire-and-forget). */
	FReply OnTranslateClicked();

	/** Pull approved translations and import them into the localization archives. */
	FReply OnPullClicked();

	/** Open the Polyglyph web dashboard in a browser. */
	FReply OnOpenInPolyglyphClicked();

	/** Switch the visible screen. */
	void ShowDashboard();
	void ShowSettings();

	/** Copy the saved settings into the editable fields. */
	void LoadSettingsIntoFields();

	/** Write the editable fields back to config (committed project + per-user key). */
	FReply OnSaveSettingsClicked();

	/** Header badge text/colour for the current state. */
	FText GetStateText() const;
	FSlateColor GetStateColor() const;

	/** Project summary line text, bound to the cached status. */
	FText GetProjectLineText() const;

	/** True when base URL, slug, and API key are all set. */
	bool IsConfigured() const;

private:
	/** Swaps between the Dashboard (0) and Settings (1) screens. */
	TSharedPtr<SWidgetSwitcher> ViewSwitcher;

	/** Container the language rows are rebuilt into. */
	TSharedPtr<SVerticalBox> LanguageBox;

	/** Settings fields (committed connection + per-user key). */
	TSharedPtr<SEditableTextBox> BaseUrlBox;
	TSharedPtr<SEditableTextBox> SlugBox;
	TSharedPtr<SEditableTextBox> TargetBox;
	TSharedPtr<SEditableTextBox> ApiKeyBox;

	/** Last parsed project status; empty until the first successful fetch. */
	FPolyglyphProjectStatus Status;

	/** Bottom status/message line (shared by both screens). */
	FText LastMessage;

	/** Current connection state. */
	EConnectionState State = EConnectionState::Unconfigured;

	/** True while a request is in flight; disables the action buttons. */
	bool bIsBusy = false;
};
