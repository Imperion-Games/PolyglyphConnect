// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphClient.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HttpManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "PolyglyphProjectSettings.h"
#include "PolyglyphSettings.h"
#include "PolyglyphTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	/** The backend rejects request bodies over ~1 MB, so chunks flush well before that to leave
	 *  headroom for JSON overhead and string escaping. */
	constexpr int32 GPolyglyphChunkByteBudget = 400 * 1024;

	/** The backend also caps one /enrich request at this many items. */
	constexpr int32 GPolyglyphEnrichMaxItems = 5000;

	/** Split items into chunks that stay under the byte budget and (when InMaxItems > 0) the item
	 *  cap. Estimate must approximate one item's serialised size in bytes. */
	template <typename ItemType, typename EstimateType>
	TArray<TArray<ItemType>> ChunkForTransport(
		const TArray<ItemType>& InItems, int32 InMaxItems, EstimateType InEstimate)
	{
		TArray<TArray<ItemType>> Chunks;
		TArray<ItemType> Current;
		int32 CurrentBytes = 0;

		for (const ItemType& Item : InItems)
		{
			const int32 ItemBytes = InEstimate(Item);
			const bool OverBytes = Current.Num() > 0 && (CurrentBytes + ItemBytes) > GPolyglyphChunkByteBudget;
			const bool OverCount = InMaxItems > 0 && Current.Num() >= InMaxItems;
			if (OverBytes || OverCount)
			{
				Chunks.Add(MoveTemp(Current));
				Current.Reset();
				CurrentBytes = 0;
			}
			Current.Add(Item);
			CurrentBytes += ItemBytes;
		}

		if (Current.Num() > 0)
		{
			Chunks.Add(MoveTemp(Current));
		}
		return Chunks;
	}

	/** Rough serialised size of one source string (field text plus JSON key/quote overhead). */
	int32 EstimatePushItemBytes(const FPolyglyphSourceString& InString)
	{
		return InString.Namespace.Len() + InString.Key.Len() + InString.SourceText.Len()
			+ InString.Context.Len() + InString.Format.Len() + 80;
	}

	/** Rough serialised size of one enrich item (field text plus JSON key/quote overhead). */
	int32 EstimateEnrichItemBytes(const FPolyglyphEnrichItem& InItem)
	{
		return InItem.Namespace.Len() + InItem.Key.Len() + InItem.Character.Len()
			+ InItem.Gender.Len() + InItem.Register.Len() + InItem.Context.Len() + 100;
	}

	/** Immediate all-zero success for an empty batch, so callers never hit the server's
	 *  non-empty-array validation with a benign no-op. */
	FPolyglyphResponse MakeEmptyBatchResponse()
	{
		FPolyglyphResponse Response;
		Response.bSuccess = true;
		Response.StatusCode = 200;
		Response.Json = MakeShared<FJsonObject>();
		Response.Json->SetNumberField(TEXT("created"), 0);
		Response.Json->SetNumberField(TEXT("updated"), 0);
		Response.Json->SetNumberField(TEXT("total"), 0);
		Response.Json->SetArrayField(TEXT("unmatched"), TArray<TSharedPtr<FJsonValue>>());
		return Response;
	}
}

FPolyglyphResponse::FPolyglyphResponse()
	: bSuccess(false)
	, StatusCode(0)
{
}

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
		[OnComplete = MoveTemp(OnComplete)](FHttpRequestPtr, FHttpResponsePtr Resp, bool Connected)
		{
			FPolyglyphResponse Result;
			if (!Connected || !Resp.IsValid())
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
	if (Strings.Num() == 0)
	{
		OnComplete(MakeEmptyBatchResponse());
		return;
	}

	// One request per chunk, chained sequentially; the shared state survives the async callbacks
	// (all on the game thread, so plain counters are safe).
	const TSharedRef<TArray<TArray<FPolyglyphSourceString>>> Chunks =
		MakeShared<TArray<TArray<FPolyglyphSourceString>>>(
			ChunkForTransport(Strings, 0, &EstimatePushItemBytes));
	const TSharedRef<FJsonObject> Aggregate = MakeShared<FJsonObject>();
	Aggregate->SetNumberField(TEXT("created"), 0);
	Aggregate->SetNumberField(TEXT("updated"), 0);
	Aggregate->SetNumberField(TEXT("total"), 0);
	const TSharedRef<TFunction<void(const FPolyglyphResponse&)>> Done =
		MakeShared<TFunction<void(const FPolyglyphResponse&)>>(MoveTemp(OnComplete));

	SendPushChunk(Chunks, 0, Aggregate, Done);
}

void FPolyglyphClient::SendPushChunk(
	const TSharedRef<TArray<TArray<FPolyglyphSourceString>>>& Chunks,
	int32 ChunkIndex,
	const TSharedRef<FJsonObject>& Aggregate,
	const TSharedRef<TFunction<void(const FPolyglyphResponse&)>>& OnComplete)
{
	const UPolyglyphProjectSettings* Project = GetDefault<UPolyglyphProjectSettings>();

	FString Error;
	const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request =
		MakeRequest(TEXT("POST"), TEXT("/api/plugin/push"), Error);
	if (!Request.IsValid())
	{
		FPolyglyphResponse Result;
		Result.Error = Error;
		(*OnComplete)(Result);
		return;
	}

	TArray<TSharedPtr<FJsonValue>> Items;
	for (const FPolyglyphSourceString& String : (*Chunks)[ChunkIndex])
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

	Send(Request.ToSharedRef(),
		[Chunks, ChunkIndex, Aggregate, OnComplete](const FPolyglyphResponse& Response)
		{
			if (!Response.bSuccess)
			{
				// Earlier chunks already upserted server-side; push is idempotent, so the caller
				// can simply push again after fixing the cause.
				FPolyglyphResponse Result = Response;
				const int32 Upserted = static_cast<int32>(Aggregate->GetNumberField(TEXT("total")));
				Result.Error = FString::Printf(
					TEXT("Push request %d of %d failed after upserting %d string(s): %s"),
					ChunkIndex + 1, Chunks->Num(), Upserted, *Response.Error);
				(*OnComplete)(Result);
				return;
			}

			if (Response.Json.IsValid())
			{
				for (const TCHAR* Field : { TEXT("created"), TEXT("updated"), TEXT("total") })
				{
					double Value = 0.0;
					if (Response.Json->TryGetNumberField(Field, Value))
					{
						Aggregate->SetNumberField(Field, Aggregate->GetNumberField(Field) + Value);
					}
				}
			}

			if (ChunkIndex + 1 < Chunks->Num())
			{
				SendPushChunk(Chunks, ChunkIndex + 1, Aggregate, OnComplete);
				return;
			}

			FPolyglyphResponse Result = Response;
			Result.Json = Aggregate;
			(*OnComplete)(Result);
		});
}

void FPolyglyphClient::PullTranslations(
	const FString& InCulture,
	bool IncludeUnapprovedDrafts,
	TFunction<void(const FPolyglyphResponse&, const FPolyglyphPullResult&)> OnComplete)
{
	const UPolyglyphProjectSettings* Project = GetDefault<UPolyglyphProjectSettings>();
	const FString Path = FString::Printf(
		TEXT("/api/plugin/pull?projectSlug=%s&language=%s&onlyApproved=%s"),
		*FGenericPlatformHttp::UrlEncode(Project->ProjectSlug),
		*FGenericPlatformHttp::UrlEncode(InCulture),
		IncludeUnapprovedDrafts ? TEXT("false") : TEXT("true"));

	FString Error;
	const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request = MakeRequest(TEXT("GET"), Path, Error);
	if (!Request.IsValid())
	{
		FPolyglyphResponse Result;
		Result.Error = Error;
		OnComplete(Result, FPolyglyphPullResult());
		return;
	}

	Send(Request.ToSharedRef(),
		[OnComplete = MoveTemp(OnComplete)](const FPolyglyphResponse& Response)
		{
			FPolyglyphPullResult PullResult;
			if (!Response.bSuccess)
			{
				OnComplete(Response, PullResult);
				return;
			}

			FString ParseError;
			if (!FPolyglyphPullResult::ParsePullResponse(Response.Json, PullResult, ParseError))
			{
				FPolyglyphResponse InvalidResponse = Response;
				InvalidResponse.bSuccess = false;
				InvalidResponse.Error = ParseError;
				OnComplete(InvalidResponse, PullResult);
				return;
			}

			OnComplete(Response, PullResult);
		});
}

void FPolyglyphClient::TriggerTranslate(
	const FString& InLanguage,
	const FString& InMode,
	bool Mock,
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
	Root->SetStringField(TEXT("language"), InLanguage);
	if (!InMode.IsEmpty())
	{
		Root->SetStringField(TEXT("mode"), InMode);
	}
	if (Mock)
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

void FPolyglyphClient::ExportCulturePo(
	const FString& InCulture,
	TFunction<void(bool Success, const FString& PoTextOrError)> OnComplete)
{
	const UPolyglyphProjectSettings* Project = GetDefault<UPolyglyphProjectSettings>();
	const FString Path = FString::Printf(
		TEXT("/api/plugin/export?projectSlug=%s&language=%s&format=po&onlyApproved=true"),
		*FGenericPlatformHttp::UrlEncode(Project->ProjectSlug),
		*FGenericPlatformHttp::UrlEncode(InCulture));

	FString Error;
	const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request = MakeRequest(TEXT("GET"), Path, Error);
	if (!Request.IsValid())
	{
		OnComplete(false, Error);
		return;
	}

	// The export body is a PO file, not JSON, so read the raw content instead of using Send.
	Request->OnProcessRequestComplete().BindLambda(
		[OnComplete = MoveTemp(OnComplete)](FHttpRequestPtr, FHttpResponsePtr Resp, bool Connected)
		{
			if (!Connected || !Resp.IsValid())
			{
				OnComplete(false, TEXT("Could not reach the Polyglyph server. "
					"Check the base URL and your network."));
				return;
			}

			const int32 StatusCode = Resp->GetResponseCode();
			if (StatusCode >= 200 && StatusCode < 300)
			{
				OnComplete(true, Resp->GetContentAsString());
			}
			else
			{
				OnComplete(false, FString::Printf(TEXT("Server returned HTTP %d."), StatusCode));
			}
		});

	Request->ProcessRequest();
}

void FPolyglyphClient::EnrichStrings(
	const TArray<FPolyglyphEnrichItem>& Items,
	TFunction<void(const FPolyglyphResponse&)> OnComplete)
{
	if (Items.Num() == 0)
	{
		OnComplete(MakeEmptyBatchResponse());
		return;
	}

	// One request per chunk (the server caps both the item count and the body size), chained
	// sequentially with the results merged into one aggregated response.
	const TSharedRef<TArray<TArray<FPolyglyphEnrichItem>>> Chunks =
		MakeShared<TArray<TArray<FPolyglyphEnrichItem>>>(
			ChunkForTransport(Items, GPolyglyphEnrichMaxItems, &EstimateEnrichItemBytes));
	const TSharedRef<FJsonObject> Aggregate = MakeShared<FJsonObject>();
	Aggregate->SetNumberField(TEXT("updated"), 0);
	Aggregate->SetNumberField(TEXT("total"), 0);
	Aggregate->SetArrayField(TEXT("unmatched"), TArray<TSharedPtr<FJsonValue>>());
	const TSharedRef<TFunction<void(const FPolyglyphResponse&)>> Done =
		MakeShared<TFunction<void(const FPolyglyphResponse&)>>(MoveTemp(OnComplete));

	SendEnrichChunk(Chunks, 0, Aggregate, Done);
}

void FPolyglyphClient::SendEnrichChunk(
	const TSharedRef<TArray<TArray<FPolyglyphEnrichItem>>>& Chunks,
	int32 ChunkIndex,
	const TSharedRef<FJsonObject>& Aggregate,
	const TSharedRef<TFunction<void(const FPolyglyphResponse&)>>& OnComplete)
{
	const UPolyglyphProjectSettings* Project = GetDefault<UPolyglyphProjectSettings>();

	FString Error;
	const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request =
		MakeRequest(TEXT("POST"), TEXT("/api/plugin/enrich"), Error);
	if (!Request.IsValid())
	{
		FPolyglyphResponse Result;
		Result.Error = Error;
		(*OnComplete)(Result);
		return;
	}
	TArray<TSharedPtr<FJsonValue>> ItemValues;
	for (const FPolyglyphEnrichItem& Item : (*Chunks)[ChunkIndex])
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("namespace"), Item.Namespace);
		Object->SetStringField(TEXT("key"), Item.Key);
		if (!Item.Character.IsEmpty())
		{
			Object->SetStringField(TEXT("character"), Item.Character);
		}
		if (!Item.Gender.IsEmpty())
		{
			Object->SetStringField(TEXT("gender"), Item.Gender);
		}
		if (!Item.Register.IsEmpty())
		{
			Object->SetStringField(TEXT("register"), Item.Register);
		}
		if (Item.MaxLength > 0)
		{
			Object->SetNumberField(TEXT("maxLength"), Item.MaxLength);
		}
		if (!Item.Context.IsEmpty())
		{
			Object->SetStringField(TEXT("context"), Item.Context);
		}
		ItemValues.Add(MakeShared<FJsonValueObject>(Object));
	}

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("projectSlug"), Project->ProjectSlug);
	Root->SetArrayField(TEXT("items"), ItemValues);

	FString Body;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(Root, Writer);

	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(Body);

	Send(Request.ToSharedRef(),
		[Chunks, ChunkIndex, Aggregate, OnComplete](const FPolyglyphResponse& Response)
		{
			if (!Response.bSuccess)
			{
				// Earlier chunks already bound server-side; report progress so a partial run
				// is not mistaken for a no-op.
				FPolyglyphResponse Result = Response;
				const int32 Bound = static_cast<int32>(Aggregate->GetNumberField(TEXT("updated")));
				Result.Error = Bound > 0
					? FString::Printf(TEXT("Enrich request %d of %d failed after binding %d key(s): %s"),
						ChunkIndex + 1, Chunks->Num(), Bound, *Response.Error)
					: Response.Error;
				(*OnComplete)(Result);
				return;
			}

			if (Response.Json.IsValid())
			{
				for (const TCHAR* Field : { TEXT("updated"), TEXT("total") })
				{
					double Value = 0.0;
					if (Response.Json->TryGetNumberField(Field, Value))
					{
						Aggregate->SetNumberField(Field, Aggregate->GetNumberField(Field) + Value);
					}
				}

				const TArray<TSharedPtr<FJsonValue>>* ChunkUnmatched = nullptr;
				if (Response.Json->TryGetArrayField(TEXT("unmatched"), ChunkUnmatched) && ChunkUnmatched != nullptr)
				{
					TArray<TSharedPtr<FJsonValue>> Merged = Aggregate->GetArrayField(TEXT("unmatched"));
					Merged.Append(*ChunkUnmatched);
					Aggregate->SetArrayField(TEXT("unmatched"), Merged);
				}
			}

			if (ChunkIndex + 1 < Chunks->Num())
			{
				SendEnrichChunk(Chunks, ChunkIndex + 1, Aggregate, OnComplete);
				return;
			}

			FPolyglyphResponse Result = Response;
			Result.Json = Aggregate;
			(*OnComplete)(Result);
		});
}

void FPolyglyphClient::PumpHttp(const bool& InDone, double TimeoutSeconds)
{
	const double Start = FPlatformTime::Seconds();
	while (!InDone && (FPlatformTime::Seconds() - Start) < TimeoutSeconds)
	{
		FHttpModule::Get().GetHttpManager().Tick(0.05f);
		FPlatformProcess::Sleep(0.05f);
	}
}
