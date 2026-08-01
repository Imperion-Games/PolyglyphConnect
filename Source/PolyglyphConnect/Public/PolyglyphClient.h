// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"

class FJsonObject;
struct FPolyglyphSourceString;
struct FPolyglyphEnrichItem;

/** Result of a Polyglyph API call, delivered to the callback on the game thread. */
struct FPolyglyphResponse
{
	/** True when the request completed with a 2xx status and a parseable body. */
	bool bSuccess = false;

	/** HTTP status code, or 0 when the request never connected. */
	int32 StatusCode = 0;

	/** Human-readable error when bSuccess is false. */
	FString Error;

	/** Parsed JSON body, or null when there was none. */
	TSharedPtr<FJsonObject> Json;
};

/**
 * HTTP client for the Polyglyph plugin API. Stateless: each call reads the current
 * connection settings, fires one async request, and delivers the parsed result to the
 * callback on the game thread. Authentication is the X-Polyglyph-Key header.
 */
class POLYGLYPHCONNECT_API FPolyglyphClient
{
public:
	/** GET /api/plugin/status for the configured project (the connection check). */
	static void TestConnection(TFunction<void(const FPolyglyphResponse&)> OnComplete);

	/** POST /api/plugin/push to upsert the given source strings. Large payloads are sent as
	 *  several sequential requests under the server's body limit; OnComplete fires once with an
	 *  aggregated response ({ created, updated, total } summed across requests). Push is an
	 *  upsert, so a mid-run failure is safe to retry by pushing again. */
	static void PushStrings(
		const TArray<FPolyglyphSourceString>& Strings,
		TFunction<void(const FPolyglyphResponse&)> OnComplete);

	/** GET /api/plugin/pull for one culture's approved translations. */
	static void PullTranslations(
		const FString& Culture,
		TFunction<void(const FPolyglyphResponse&)> OnComplete);

	/** POST /api/plugin/translate to start a job for one language. Mode is sync/batch/auto
	 *  (empty lets the server choose); bMock fills placeholders with no AI cost. */
	static void TriggerTranslate(
		const FString& Language,
		const FString& Mode,
		bool bMock,
		TFunction<void(const FPolyglyphResponse&)> OnComplete);

	/** GET /api/plugin/jobs/:jobId for a translation job's status. */
	static void GetJob(
		const FString& JobId,
		TFunction<void(const FPolyglyphResponse&)> OnComplete);

	/** GET /api/plugin/export?format=po for one culture. Delivers the raw PO text (not JSON)
	 *  on success, or an error string, so callers can write the file the dashboard imports. */
	static void ExportCulturePo(
		const FString& Culture,
		TFunction<void(bool bSuccess, const FString& PoTextOrError)> OnComplete);

	/** POST /api/plugin/enrich to attach translator-context metadata (character, gender,
	 *  register, max length, context) to existing keys. Large batches are sent as several
	 *  sequential requests under the server's item and body caps; OnComplete fires once with an
	 *  aggregated response ({ updated, total, unmatched[] } merged across requests). */
	static void EnrichStrings(
		const TArray<FPolyglyphEnrichItem>& Items,
		TFunction<void(const FPolyglyphResponse&)> OnComplete);

	/** Drive the HTTP manager on the calling thread until bDone flips or the timeout elapses,
	 *  so a request can be run synchronously (commandlets, blocking service operations). */
	static void PumpHttp(const bool& bDone, double TimeoutSeconds);

private:
	/** Build an authenticated request to BaseUrl + Path. Returns null and fills OutError
	 *  when the connection settings are incomplete. */
	static TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> MakeRequest(
		const FString& Verb,
		const FString& Path,
		FString& OutError);

	/** Send Request and route the parsed result to OnComplete. */
	static void Send(
		const TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& Request,
		TFunction<void(const FPolyglyphResponse&)> OnComplete);

	/** Send push chunk ChunkIndex, fold its counts into Aggregate, then chain to the next chunk
	 *  or deliver the aggregated response. */
	static void SendPushChunk(
		const TSharedRef<TArray<TArray<FPolyglyphSourceString>>>& Chunks,
		int32 ChunkIndex,
		const TSharedRef<FJsonObject>& Aggregate,
		const TSharedRef<TFunction<void(const FPolyglyphResponse&)>>& OnComplete);

	/** Send enrich chunk ChunkIndex, fold its counts and unmatched list into Aggregate, then
	 *  chain to the next chunk or deliver the aggregated response. */
	static void SendEnrichChunk(
		const TSharedRef<TArray<TArray<FPolyglyphEnrichItem>>>& Chunks,
		int32 ChunkIndex,
		const TSharedRef<FJsonObject>& Aggregate,
		const TSharedRef<TFunction<void(const FPolyglyphResponse&)>>& OnComplete);
};
