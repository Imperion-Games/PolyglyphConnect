// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphConnect.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Modules/ModuleManager.h"
#include "PolyglyphClient.h"
#include "PolyglyphTypes.h"
#include "ToolMenus.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "PolyglyphConnect"

IMPLEMENT_MODULE(FPolyglyphConnectModule, PolyglyphConnect)

namespace
{
	/** Non-blocking editor toast, green on success and red on failure. */
	void NotifyResult(const FText& Message, const bool bSuccess)
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = 5.0f;
		Info.bUseSuccessFailIcons = true;
		const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
		if (Item.IsValid())
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}
}

void FPolyglyphConnectModule::StartupModule()
{
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FPolyglyphConnectModule::RegisterMenus));
}

void FPolyglyphConnectModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

void FPolyglyphConnectModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	FToolMenuSection& Section = ToolsMenu->FindOrAddSection(
		"Polyglyph", LOCTEXT("PolyglyphSection", "Polyglyph"));

	Section.AddSubMenu(
		"PolyglyphSubmenu",
		LOCTEXT("PolyglyphLabel", "Polyglyph"),
		LOCTEXT("PolyglyphTooltip", "Localization sync with the Polyglyph service"),
		FNewToolMenuDelegate::CreateLambda([this](UToolMenu* SubMenu)
		{
			FToolMenuSection& Sub = SubMenu->FindOrAddSection("PolyglyphItems");

			Sub.AddMenuEntry("TestConnection",
				LOCTEXT("TestConnectionLabel", "Test Connection"),
				LOCTEXT("TestConnectionTooltip", "Check the API key and project slug against the Polyglyph service."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateRaw(this, &FPolyglyphConnectModule::OnTestConnection)));

			Sub.AddMenuEntry("PushSourceStrings",
				LOCTEXT("PushSourceStringsLabel", "Push Source Strings"),
				LOCTEXT("PushSourceStringsTooltip", "Send source strings to Polyglyph (probe payload until the gather is wired in)."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateRaw(this, &FPolyglyphConnectModule::OnPushSourceStrings)));
		}));
}

void FPolyglyphConnectModule::OnTestConnection()
{
	FPolyglyphClient::TestConnection([](const FPolyglyphResponse& Response)
	{
		if (Response.bSuccess && Response.Json.IsValid())
		{
			const int32 Total = static_cast<int32>(Response.Json->GetNumberField(TEXT("totalStrings")));
			NotifyResult(
				FText::Format(
					LOCTEXT("TestConnectionOk", "Connected to Polyglyph. Project has {0} source strings."),
					FText::AsNumber(Total)),
				true);
		}
		else
		{
			NotifyResult(
				FText::Format(
					LOCTEXT("TestConnectionFail", "Polyglyph connection failed: {0}"),
					FText::FromString(Response.Error)),
				false);
		}
	});
}

void FPolyglyphConnectModule::OnPushSourceStrings()
{
	// Probe payload. Real source text will come from the UE localization gather; see the
	// Brain notes (Plugins/PolyglyphConnect/Reference.md) for the planned mapping.
	TArray<FPolyglyphSourceString> Strings;
	FPolyglyphSourceString Probe;
	Probe.Namespace = TEXT("PolyglyphConnect");
	Probe.Key = TEXT("ConnectionProbe");
	Probe.SourceText = TEXT("Hello from Unreal");
	Probe.Context = TEXT("Connectivity probe pushed by PolyglyphConnect");
	Strings.Add(Probe);

	FPolyglyphClient::PushStrings(Strings, [](const FPolyglyphResponse& Response)
	{
		if (Response.bSuccess && Response.Json.IsValid())
		{
			const int32 Total = static_cast<int32>(Response.Json->GetNumberField(TEXT("total")));
			NotifyResult(
				FText::Format(
					LOCTEXT("PushOk", "Pushed {0} string(s) to Polyglyph."),
					FText::AsNumber(Total)),
				true);
		}
		else
		{
			NotifyResult(
				FText::Format(
					LOCTEXT("PushFail", "Polyglyph push failed: {0}"),
					FText::FromString(Response.Error)),
				false);
		}
	});
}

#undef LOCTEXT_NAMESPACE
