// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "ILocalizationServiceProvider.h"

class FToolBarBuilder;
class IDetailCategoryBuilder;
class ULocalizationTarget;
class ULocalizationTargetSet;

/**
 * Localization service provider that plugs Polyglyph into the native Localization Dashboard.
 *
 * It adds per-target toolbar actions (push source, pull approved, translate, open in Polyglyph)
 * that reuse the structured push/pull helpers, and implements the standard connect / download /
 * upload operations so the dashboard's own service flows work too. All network calls go through
 * FPolyglyphClient. The module registers one instance under the "LocalizationService" feature.
 */
class FPolyglyphLocalizationServiceProvider : public ILocalizationServiceProvider
{
public:
	FPolyglyphLocalizationServiceProvider();

	//~ Begin ILocalizationServiceProvider Interface
	virtual void Init(bool InForceConnection = true) override;
	virtual void Close() override;
	virtual const FName& GetName() const override;
	virtual const FText GetDisplayName() const override;
	virtual FText GetStatusText() const override;
	virtual bool IsEnabled() const override;
	virtual bool IsAvailable() const override;
	virtual ELocalizationServiceOperationCommandResult::Type GetState(
		const TArray<FLocalizationServiceTranslationIdentifier>& InTranslationIds,
		TArray<TSharedRef<ILocalizationServiceState, ESPMode::ThreadSafe>>& OutState,
		ELocalizationServiceCacheUsage::Type InStateCacheUsage) override;
	virtual ELocalizationServiceOperationCommandResult::Type Execute(
		const TSharedRef<ILocalizationServiceOperation, ESPMode::ThreadSafe>& InOperation,
		const TArray<FLocalizationServiceTranslationIdentifier>& InTranslationIds,
		ELocalizationServiceOperationConcurrency::Type InConcurrency = ELocalizationServiceOperationConcurrency::Synchronous,
		const FLocalizationServiceOperationComplete& InOperationCompleteDelegate = FLocalizationServiceOperationComplete()) override;
	virtual bool CanCancelOperation(const TSharedRef<ILocalizationServiceOperation, ESPMode::ThreadSafe>& InOperation) const override;
	virtual void CancelOperation(const TSharedRef<ILocalizationServiceOperation, ESPMode::ThreadSafe>& InOperation) override;
	virtual void Tick() override;
#if LOCALIZATION_SERVICES_WITH_SLATE
	virtual void CustomizeSettingsDetails(IDetailCategoryBuilder& DetailCategoryBuilder) const override;
	virtual void CustomizeTargetDetails(IDetailCategoryBuilder& DetailCategoryBuilder, TWeakObjectPtr<ULocalizationTarget> LocalizationTarget) const override;
	virtual void CustomizeTargetToolbar(TSharedRef<FExtender>& MenuExtender, TWeakObjectPtr<ULocalizationTarget> LocalizationTarget) const override;
	virtual void CustomizeTargetSetToolbar(TSharedRef<FExtender>& MenuExtender, TWeakObjectPtr<ULocalizationTargetSet> LocalizationTargetSet) const override;
#endif // LOCALIZATION_SERVICES_WITH_SLATE
	//~ End ILocalizationServiceProvider Interface

private:
	/** Rebuild open Details panels after Polyglyph becomes the active provider. */
	bool RefreshDashboardDetails(float InDeltaSeconds);

	/** Add the Polyglyph action buttons to a localization target's toolbar. */
	void AddTargetToolbarButtons(FToolBarBuilder& ToolbarBuilder, TWeakObjectPtr<ULocalizationTarget> InLocalizationTarget);

	/** Point the connection settings at the clicked target so the sync helpers act on it. */
	void SelectTarget(TWeakObjectPtr<ULocalizationTarget> InLocalizationTarget) const;

	/** Toolbar action: gather the target manifest and push its source strings (async). */
	void PushSourceForTarget(TWeakObjectPtr<ULocalizationTarget> InLocalizationTarget);

	/** Toolbar action: pull approved translations for every enabled culture, import + compile. */
	void PullApprovedForTarget(TWeakObjectPtr<ULocalizationTarget> InLocalizationTarget);

	/** Toolbar action: trigger AI translation for every enabled language (fire-and-forget). */
	void TranslateForTarget(TWeakObjectPtr<ULocalizationTarget> InLocalizationTarget);

	/** Toolbar action: open the Polyglyph web dashboard in a browser. */
	void OpenInPolyglyph() const;

	/** Synchronous FConnectToProvider: test the connection, store any error on the operation. */
	ELocalizationServiceOperationCommandResult::Type ExecuteConnect(
		const TSharedRef<ILocalizationServiceOperation, ESPMode::ThreadSafe>& InOperation) const;

	/** Synchronous FDownloadLocalizationTargetFile: export a culture's approved PO to the path. */
	ELocalizationServiceOperationCommandResult::Type ExecuteDownload(
		const TSharedRef<ILocalizationServiceOperation, ESPMode::ThreadSafe>& InOperation) const;

	/** Synchronous FUploadLocalizationTargetFile: always fails with an explanation. The caller
	 *  (the Translation Editor) is trying to upload edited translations, which Polyglyph owns and
	 *  the next pull would overwrite; source text goes up via Push Source instead. */
	ELocalizationServiceOperationCommandResult::Type ExecuteUpload(
		const TSharedRef<ILocalizationServiceOperation, ESPMode::ThreadSafe>& InOperation) const;

private:
	/** Provider name used by the LocalizationService feature registry. */
	FName ProviderName;

	/** One-shot ticker that defers the dashboard refresh until provider selection completes. */
	FTSTicker::FDelegateHandle RefreshTickerHandle;

	/** Whether a dashboard Details-panel refresh has already been queued. */
	bool bRefreshPending;
};
