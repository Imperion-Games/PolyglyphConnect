// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphClient.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "PolyglyphProjectSettings.h"
#include "PolyglyphSettings.h"
#include "PolyglyphTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> FPolyglyphClient::MakeRequest(
	const FString& Verb,
	const FString& Path,
	FString& OutError)
{
	const UPolyglyphProjectSettings* Project = GetDefault<UPolyglyphProjectSettings>();
	const UPolyglyphSettings* User = GetDefault<UPolyglyphSettings>();
	if (Project->BaseUrl.IsEmpty() || Project->ProjectSlug.IsEmpty() || User->ApiKey.IsEmpty())
	{
		OutError = TEXT("Set the API base URL and project slug in Project Settings > Plugins > "
			"Polyglyph, and your API key in Editor Preferences > Plugins > Polyglyph.");
		return nullptr;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Project->BaseUrl + Path);
	Request->SetVerb(Verb);
	Request->SetHeader(TEXT("X-Polyglyph-Key"), User->ApiKey);
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	return Request;
}

void FPolyglyphClient::Send(
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& Request,
	TFunction<void(const FPolyglyphResponse&)> OnComplete)
{
	Request->OnProcessRequestComplete().BindLambda(
		[OnComplete = MoveTemp(OnComplete)](FHttpRequestPtr, FHttpResponsePtr Resp, bool bConnected)
		{
			FPolyglyphResponse Result;
			if (!bConnected || !Resp.IsValid())
			{
				Result.Error = TEXT("Could not reach the Polyglyph server. "
					"Check the base URL and your network.");
				OnComplete(Result);
				return;
			}

			Result.StatusCode = Resp->GetResponseCode();

			const FString Body = Resp->GetContentAsString();
			if (!Body.IsEmpty())
			{
				const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
				FJsonSerializer::Deserialize(Reader, Result.Json);
			}

			if (Result.StatusCode >= 200 && Result.StatusCode < 300)
			{
				Result.bSuccess = true;
			}
			else
			{
				FString ServerError;
				if (Result.Json.IsValid())
				{
					Result.Json->TryGetStringField(TEXT("error"), ServerError);
				}
				Result.Error = ServerError.IsEmpty()
					? FString::Printf(TEXT("Server returned HTTP %d."), Result.StatusCode)
					: ServerError;
			}

			OnComplete(Result);
		});

	Request->ProcessRequest();
}

void FPolyglyphClient::TestConnection(TFunction<void(const FPolyglyphResponse&)> OnComplete)
{
	const UPolyglyphProjectSettings* Project = GetDefault<UPolyglyphProjectSettings>();
	const FString Path = FString::Printf(
		TEXT("/api/plugin/status?projectSlug=%s"),
		*FGenericPlatformHttp::UrlEncode(Project->ProjectSlug));

	FString Error;
	const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request = MakeRequest(TEXT("GET"), Path, Error);
	if (!Request.IsValid())
	{
		FPolyglyphResponse Result;
		Result.Error = Error;
		OnComplete(Result);
		return;
	}

	Send(Request.ToSharedRef(), MoveTemp(OnComplete));
}

void FPolyglyphClient::PushStrings(
	const TArray<FPolyglyphSourceString>& Strings,
	TFunction<void(const FPolyglyphResponse&)> OnComplete)
{
	const UPolyglyphProjectSettings* Project = GetDefault<UPolyglyphProjectSettings>();

	FString Error;
	const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request =
		MakeRequest(TEXT("POST"), TEXT("/api/plugin/push"), Error);
	if (!Request.IsValid())
	{
		FPolyglyphResponse Result;
		Result.Error = Error;
		OnComplete(Result);
		return;
	}

	TArray<TSharedPtr<FJsonValue>> Items;
	for (const FPolyglyphSourceString& String : Strings)
	{
		const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("namespace"), String.Namespace);
		Item->SetStringField(TEXT("key"), String.Key);
		Item->SetStringField(TEXT("sourceText"), String.SourceText);
		if (!String.Context.IsEmpty())
		{
			Item->SetStringField(TEXT("context"), String.Context);
		}
		if (!String.Format.IsEmpty())
		{
			Item->SetStringField(TEXT("format"), String.Format);
		}
		Items.Add(MakeShared<FJsonValueObject>(Item));
	}

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("projectSlug"), Project->ProjectSlug);
	Root->SetArrayField(TEXT("strings"), Items);

	FString Body;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(Root, Writer);

	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(Body);

	Send(Request.ToSharedRef(), MoveTemp(OnComplete));
}

void FPolyglyphClient::PullTranslations(
	const FString& Culture,
	TFunction<void(const FPolyglyphResponse&)> OnComplete)
{
	const UPolyglyphProjectSettings* Project = GetDefault<UPolyglyphProjectSettings>();
	const FString Path = FString::Printf(
		TEXT("/api/plugin/pull?projectSlug=%s&language=%s&onlyApproved=true"),
		*FGenericPlatformHttp::UrlEncode(Project->ProjectSlug),
		*FGenericPlatformHttp::UrlEncode(Culture));

	FString Error;
	const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request = MakeRequest(TEXT("GET"), Path, Error);
	if (!Request.IsValid())
	{
		FPolyglyphResponse Result;
		Result.Error = Error;
		OnComplete(Result);
		return;
	}

	Send(Request.ToSharedRef(), MoveTemp(OnComplete));
}

void FPolyglyphClient::TriggerTranslate(
	const FString& Language,
	const FString& Mode,
	bool bMock,
	TFunction<void(const FPolyglyphResponse&)> OnComplete)
{
	const UPolyglyphProjectSettings* Project = GetDefault<UPolyglyphProjectSettings>();

	FString Error;
	const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request =
		MakeRequest(TEXT("POST"), TEXT("/api/plugin/translate"), Error);
	if (!Request.IsValid())
	{
		FPolyglyphResponse Result;
		Result.Error = Error;
		OnComplete(Result);
		return;
	}

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("projectSlug"), Project->ProjectSlug);
	Root->SetStringField(TEXT("language"), Language);
	if (!Mode.IsEmpty())
	{
		Root->SetStringField(TEXT("mode"), Mode);
	}
	if (bMock)
	{
		Root->SetBoolField(TEXT("mock"), true);
	}

	FString Body;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(Root, Writer);

	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(Body);

	Send(Request.ToSharedRef(), MoveTemp(OnComplete));
}

void FPolyglyphClient::GetJob(
	const FString& JobId,
	TFunction<void(const FPolyglyphResponse&)> OnComplete)
{
	const FString Path = FString::Printf(
		TEXT("/api/plugin/jobs/%s"),
		*FGenericPlatformHttp::UrlEncode(JobId));

	FString Error;
	const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request = MakeRequest(TEXT("GET"), Path, Error);
	if (!Request.IsValid())
	{
		FPolyglyphResponse Result;
		Result.Error = Error;
		OnComplete(Result);
		return;
	}

	Send(Request.ToSharedRef(), MoveTemp(OnComplete));
}
