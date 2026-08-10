// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphLocalizationServiceProvider.h"

#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformProcess.h"
#include "LocalizationServiceOperations.h"
#include "LocalizationTargetTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "PolyglyphClient.h"
#include "PolyglyphManifest.h"
#include "PolyglyphProjectSettings.h"
#include "PolyglyphPull.h"
#include "PolyglyphSettings.h"
#include "PolyglyphTranslate.h"
#include "PolyglyphTypes.h"

#define LOCTEXT_NAMESPACE "PolyglyphConnect"

namespace
{
	/** Raise a fire-and-forget editor notification for a completed Polyglyph action. */
	void Notify(const FString& InMessage, bool bInSuccess)
	{
		FNotificationInfo Info(FText::FromString(InMessage));
		Info.bFireAndForget = true;
		Info.ExpireDuration = bInSuccess ? 4.0f : 8.0f;

		const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
		if (Item.IsValid())
		{
			Item->SetCompletionState(bInSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}
}

FPolyglyphLocalizationServiceProvider::FPolyglyphLocalizationServiceProvider()
	: ProviderName(TEXT("Polyglyph"))
	, RefreshTickerHandle()
	, bRefreshPending(false)
{
}

void FPolyglyphLocalizationServiceProvider::Init(bool InForceConnection)
{
	// Stateless HTTP client; nothing to establish up front. The connection is validated
	// on demand by FConnectToProvider and by each toolbar action.
	static_cast<void>(InForceConnection);

	if (!bRefreshPending && FModuleManager::Get().IsModuleLoaded(TEXT("LocalizationDashboard")))
	{
		bRefreshPending = true;
		RefreshTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			TEXT("PolyglyphConnect.RefreshLocalizationDashboard"),
			0.0f,
			[this](float InDeltaSeconds)
			{
				return RefreshDashboardDetails(InDeltaSeconds);
			});
	}
}

void FPolyglyphLocalizationServiceProvider::Close()
{
	if (RefreshTickerHandle.IsValid())
	{
		FTSTicker::RemoveTicker(RefreshTickerHandle);
		RefreshTickerHandle.Reset();
	}
	bRefreshPending = false;
}

const FName& FPolyglyphLocalizationServiceProvider::GetName() const
{
	return ProviderName;
}

const FText FPolyglyphLocalizationServiceProvider::GetDisplayName() const
{
	return LOCTEXT("PolyglyphProviderDisplayName", "Polyglyph");
}

FText FPolyglyphLocalizationServiceProvider::GetStatusText() const
{
	return IsAvailable()
		? LOCTEXT("PolyglyphStatusReady", "Polyglyph connection settings are configured.")
		: LOCTEXT("PolyglyphStatusUnconfigured", "Set the Polyglyph base URL, project slug and API key in Project Settings and Editor Preferences > Plugins > Polyglyph.");
}

bool FPolyglyphLocalizationServiceProvider::IsEnabled() const
{
	return true;
}

bool FPolyglyphLocalizationServiceProvider::IsAvailable() const
{
	const UPolyglyphProjectSettings* Project = GetDefault<UPolyglyphProjectSettings>();
	const UPolyglyphSettings* User = GetDefault<UPolyglyphSettings>();
	return !Project->BaseUrl.IsEmpty() && !Project->ProjectSlug.IsEmpty() && !User->ApiKey.IsEmpty();
}

ELocalizationServiceOperationCommandResult::Type FPolyglyphLocalizationServiceProvider::GetState(
	const TArray<FLocalizationServiceTranslationIdentifier>& InTranslationIds,
	TArray<TSharedRef<ILocalizationServiceState, ESPMode::ThreadSafe>>& OutState,
	ELocalizationServiceCacheUsage::Type InStateCacheUsage)
{
	// No per-string state cache: Polyglyph tracks review/approval in its own dashboard. Report
	// failure rather than an empty success, because the interface's single-state helper indexes
	// OutState[0] whenever this returns Succeeded.
	OutState.Reset();
	return ELocalizationServiceOperationCommandResult::Failed;
}

ELocalizationServiceOperationCommandResult::Type FPolyglyphLocalizationServiceProvider::Execute(
	const TSharedRef<ILocalizationServiceOperation, ESPMode::ThreadSafe>& InOperation,
	const TArray<FLocalizationServiceTranslationIdentifier>& InTranslationIds,
	ELocalizationServiceOperationConcurrency::Type InConcurrency,
	const FLocalizationServiceOperationComplete& InOperationCompleteDelegate)
{
	// Every operation runs synchronously; concurrency hints are ignored.
	ELocalizationServiceOperationCommandResult::Type Result = ELocalizationServiceOperationCommandResult::Failed;
	const FName OperationName = InOperation->GetName();

	if (OperationName == "Connect")
	{
		Result = ExecuteConnect(InOperation);
	}
	else if (OperationName == "DownloadLocalizationTargetFile")
	{
		Result = ExecuteDownload(InOperation);
	}
	else if (OperationName == "UploadLocalizationTargetFile")
	{
		Result = ExecuteUpload(InOperation);
	}

	InOperationCompleteDelegate.ExecuteIfBound(InOperation, Result);
	return Result;
}

bool FPolyglyphLocalizationServiceProvider::CanCancelOperation(
	const TSharedRef<ILocalizationServiceOperation, ESPMode::ThreadSafe>& InOperation) const
{
	return false;
}

void FPolyglyphLocalizationServiceProvider::CancelOperation(
	const TSharedRef<ILocalizationServiceOperation, ESPMode::ThreadSafe>& InOperation)
{
}

void FPolyglyphLocalizationServiceProvider::Tick()
{
}

#if LOCALIZATION_SERVICES_WITH_SLATE

void FPolyglyphLocalizationServiceProvider::CustomizeSettingsDetails(IDetailCategoryBuilder& DetailCategoryBuilder) const
{
	// Connection settings live in Project Settings / Editor Preferences > Plugins > Polyglyph,
	// so the dashboard's provider settings panel stays empty.
}

void FPolyglyphLocalizationServiceProvider::CustomizeTargetDetails(
	IDetailCategoryBuilder& DetailCategoryBuilder,
	TWeakObjectPtr<ULocalizationTarget> LocalizationTarget) const
{
}

void FPolyglyphLocalizationServiceProvider::CustomizeTargetToolbar(
	TSharedRef<FExtender>& MenuExtender,
	TWeakObjectPtr<ULocalizationTarget> LocalizationTarget) const
{
	const TSharedRef<FUICommandList> CommandList = MakeShareable(new FUICommandList);
	MenuExtender->AddToolBarExtension(
		"LocalizationService",
		EExtensionHook::First,
		CommandList,
		FToolBarExtensionDelegate::CreateRaw(
			const_cast<FPolyglyphLocalizationServiceProvider*>(this),
			&FPolyglyphLocalizationServiceProvider::AddTargetToolbarButtons,
			LocalizationTarget));
}

void FPolyglyphLocalizationServiceProvider::CustomizeTargetSetToolbar(
	TSharedRef<FExtender>& MenuExtender,
	TWeakObjectPtr<ULocalizationTargetSet> LocalizationTargetSet) const
{
	// Actions are per-target; the target-set toolbar is left to the engine.
}

#endif // LOCALIZATION_SERVICES_WITH_SLATE

bool FPolyglyphLocalizationServiceProvider::RefreshDashboardDetails(float InDeltaSeconds)
{
	static_cast<void>(InDeltaSeconds);

	bRefreshPending = false;
	RefreshTickerHandle.Reset();

	if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyEditorModule =
			FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyEditorModule.NotifyCustomizationModuleChanged();
	}

	return false;
}

void FPolyglyphLocalizationServiceProvider::AddTargetToolbarButtons(
	FToolBarBuilder& ToolbarBuilder,
	TWeakObjectPtr<ULocalizationTarget> InLocalizationTarget)
{
	ToolbarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateRaw(this, &FPolyglyphLocalizationServiceProvider::PushSourceForTarget, InLocalizationTarget)),
		NAME_None,
		LOCTEXT("PolyglyphPushLabel", "Push Source"),
		LOCTEXT("PolyglyphPushTip", "Gather this target's source strings and push them to Polyglyph."),
		FSlateIcon());

	ToolbarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateRaw(this, &FPolyglyphLocalizationServiceProvider::PullApprovedForTarget, InLocalizationTarget)),
		NAME_None,
		LOCTEXT("PolyglyphPullLabel", "Pull Approved"),
		LOCTEXT("PolyglyphPullTip", "Pull approved translations for every enabled culture, then import and compile them."),
		FSlateIcon());

	ToolbarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateRaw(this, &FPolyglyphLocalizationServiceProvider::TranslateForTarget, InLocalizationTarget)),
		NAME_None,
		LOCTEXT("PolyglyphTranslateLabel", "Translate"),
		LOCTEXT("PolyglyphTranslateTip", "Start an AI translation job for every enabled language (review and approval happen in Polyglyph)."),
		FSlateIcon());

	ToolbarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateRaw(this, &FPolyglyphLocalizationServiceProvider::OpenInPolyglyph)),
		NAME_None,
		LOCTEXT("PolyglyphOpenLabel", "Open in Polyglyph"),
		LOCTEXT("PolyglyphOpenTip", "Open the Polyglyph web dashboard in a browser."),
		FSlateIcon());
}

void FPolyglyphLocalizationServiceProvider::SelectTarget(TWeakObjectPtr<ULocalizationTarget> InLocalizationTarget) const
{
	if (InLocalizationTarget.IsValid())
	{
		GetMutableDefault<UPolyglyphProjectSettings>()->LocalizationTarget = InLocalizationTarget->Settings.Name;
	}
}

void FPolyglyphLocalizationServiceProvider::PushSourceForTarget(TWeakObjectPtr<ULocalizationTarget> InLocalizationTarget)
{
	SelectTarget(InLocalizationTarget);

	TArray<FPolyglyphSourceString> Strings;
	FString GatherError;
	if (!FPolyglyphManifest::GatherSourceStrings(Strings, GatherError))
	{
		Notify(GatherError, false);
		return;
	}

	const int32 Count = Strings.Num();
	FPolyglyphClient::PushStrings(Strings, [Count](const FPolyglyphResponse& Response)
	{
		if (Response.bSuccess)
		{
			Notify(FString::Printf(TEXT("Pushed %d source string(s) to Polyglyph."), Count), true);
		}
		else
		{
			Notify(FString::Printf(TEXT("Push failed: %s"), *Response.Error), false);
		}
	});
}

void FPolyglyphLocalizationServiceProvider::PullApprovedForTarget(TWeakObjectPtr<ULocalizationTarget> InLocalizationTarget)
{
	SelectTarget(InLocalizationTarget);

	FPolyglyphPull::Run([](bool bSuccess, const FString& Summary)
	{
		Notify(Summary, bSuccess);
	});
}

void FPolyglyphLocalizationServiceProvider::TranslateForTarget(TWeakObjectPtr<ULocalizationTarget> InLocalizationTarget)
{
	SelectTarget(InLocalizationTarget);

	FPolyglyphTranslate::Run(FString(), false,
		[](bool bSuccess, const FString& Summary, const TArray<FPolyglyphTriggeredJob>& Jobs)
		{
			Notify(Summary, bSuccess);
		});
}

void FPolyglyphLocalizationServiceProvider::OpenInPolyglyph() const
{
	const UPolyglyphProjectSettings* Project = GetDefault<UPolyglyphProjectSettings>();
	if (Project->BaseUrl.IsEmpty())
	{
		Notify(TEXT("Set the Polyglyph base URL in Project Settings > Plugins > Polyglyph first."), false);
		return;
	}

	FPlatformProcess::LaunchURL(*Project->BaseUrl, nullptr, nullptr);
}

ELocalizationServiceOperationCommandResult::Type FPolyglyphLocalizationServiceProvider::ExecuteConnect(
	const TSharedRef<ILocalizationServiceOperation, ESPMode::ThreadSafe>& InOperation) const
{
	bool bDone = false;
	FPolyglyphResponse Response;
	FPolyglyphClient::TestConnection([&bDone, &Response](const FPolyglyphResponse& InResponse)
	{
		Response = InResponse;
		bDone = true;
	});
	FPolyglyphClient::PumpHttp(bDone, 30.0);

	if (Response.bSuccess)
	{
		return ELocalizationServiceOperationCommandResult::Succeeded;
	}

	const TSharedRef<FConnectToProvider, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FConnectToProvider>(InOperation);
	Operation->SetErrorText(FText::FromString(Response.Error));
	return ELocalizationServiceOperationCommandResult::Failed;
}

ELocalizationServiceOperationCommandResult::Type FPolyglyphLocalizationServiceProvider::ExecuteDownload(
	const TSharedRef<ILocalizationServiceOperation, ESPMode::ThreadSafe>& InOperation) const
{
	const TSharedRef<FDownloadLocalizationTargetFile, ESPMode::ThreadSafe> Operation =
		StaticCastSharedRef<FDownloadLocalizationTargetFile>(InOperation);

	bool bDone = false;
	bool bOk = false;
	FString PoTextOrError;
	FPolyglyphClient::ExportCulturePo(Operation->GetInLocale(),
		[&bDone, &bOk, &PoTextOrError](bool bSuccess, const FString& InBody)
		{
			bOk = bSuccess;
			PoTextOrError = InBody;
			bDone = true;
		});
	FPolyglyphClient::PumpHttp(bDone, 120.0);

	if (!bOk)
	{
		Operation->SetOutErrorText(FText::FromString(PoTextOrError));
		return ELocalizationServiceOperationCommandResult::Failed;
	}

	const FString AbsolutePath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectDir() / Operation->GetInRelativeOutputFilePathAndName());
	if (!FFileHelper::SaveStringToFile(PoTextOrError, *AbsolutePath))
	{
		Operation->SetOutErrorText(FText::Format(
			LOCTEXT("PolyglyphWriteFailed", "Could not write {0}."), FText::FromString(AbsolutePath)));
		return ELocalizationServiceOperationCommandResult::Failed;
	}

	return ELocalizationServiceOperationCommandResult::Succeeded;
}

ELocalizationServiceOperationCommandResult::Type FPolyglyphLocalizationServiceProvider::ExecuteUpload(
	const TSharedRef<ILocalizationServiceOperation, ESPMode::ThreadSafe>& InOperation) const
{
	// The only caller is the Translation Editor's "save translations to the localization service",
	// which hands us a culture's edited TRANSLATIONS. Polyglyph owns translations (they are drafted,
	// reviewed and approved there, and the next pull overwrites the archive), so there is nothing
	// honest to do with them. Refuse with an explanation instead of silently pushing source strings
	// and reporting success, which would look like the edits were saved.
	const TSharedRef<FUploadLocalizationTargetFile, ESPMode::ThreadSafe> Operation =
		StaticCastSharedRef<FUploadLocalizationTargetFile>(InOperation);

	Operation->SetOutErrorText(LOCTEXT("PolyglyphUploadUnsupported",
		"Polyglyph owns translations: edit and approve them in the Polyglyph dashboard, then use "
		"Pull Approved. Uploading translations from the Translation Editor is not supported, "
		"because the next pull would overwrite them. Use Push Source to send source text."));
	return ELocalizationServiceOperationCommandResult::Failed;
}

#undef LOCTEXT_NAMESPACE
