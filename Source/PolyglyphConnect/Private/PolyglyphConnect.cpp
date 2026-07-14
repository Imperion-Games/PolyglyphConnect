// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphConnect.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#include "SPolyglyphPanel.h"

#define LOCTEXT_NAMESPACE "PolyglyphConnect"

IMPLEMENT_MODULE(FPolyglyphConnectModule, PolyglyphConnect)

namespace
{
	/** Nomad tab id for the Polyglyph dashboard. */
	const FName PolyglyphDashboardTabName(TEXT("PolyglyphDashboard"));
}

void FPolyglyphConnectModule::StartupModule()
{
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FPolyglyphConnectModule::RegisterMenus));

	FGlobalTabmanager::Get()
		->RegisterNomadTabSpawner(
			PolyglyphDashboardTabName,
			FOnSpawnTab::CreateRaw(this, &FPolyglyphConnectModule::OnSpawnDashboardTab))
		.SetDisplayName(LOCTEXT("PolyglyphTabTitle", "Polyglyph"))
		.SetTooltipText(LOCTEXT("PolyglyphTabTooltipText", "Open the Polyglyph localization dashboard."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());
}

void FPolyglyphConnectModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PolyglyphDashboardTabName);
	}
}

void FPolyglyphConnectModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	FToolMenuSection& Section = ToolsMenu->FindOrAddSection(
		"Polyglyph", LOCTEXT("PolyglyphSection", "Polyglyph"));

	Section.AddMenuEntry("PolyglyphDashboard",
		LOCTEXT("PolyglyphLabel", "Polyglyph"),
		LOCTEXT("PolyglyphTooltip", "Open the Polyglyph localization dashboard."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FPolyglyphConnectModule::OpenDashboard)));
}

TSharedRef<SDockTab> FPolyglyphConnectModule::OnSpawnDashboardTab(const FSpawnTabArgs& InArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SPolyglyphPanel)
		];
}

void FPolyglyphConnectModule::OpenDashboard()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FTabId(PolyglyphDashboardTabName));
}

#undef LOCTEXT_NAMESPACE
