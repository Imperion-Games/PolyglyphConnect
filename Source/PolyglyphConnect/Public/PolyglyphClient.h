// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"

class FJsonObject;
struct FPolyglyphSourceString;
struct FPolyglyphEnrichItem;
struct FPolyglyphPullResult;

/** Result of a Polyglyph API call, delivered to the callback on the game thread. */
struct FPolyglyphResponse
{
	/** Sets the response to an unsuccessful request with no HTTP status. */
	FPolyglyphResponse();

	/** True when the request completed with a 2xx status and a parseable body. */
	bool bSuccess;

	/** HTTP status code, or 0 when the request never connected. */
	int32 StatusCode;

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

	/**
	 * POST /api/plugin/push to upsert the given source strings.
	 * Large payloads use sequential requests and return one aggregated response.
	 */
	static void PushStrings(
		const TArray<FPolyglyphSourceString>& Strings,
		TFunction<void(const FPolyglyphResponse&)> OnComplete);

	/** GET /api/plugin/pull for one culture, optionally including unapproved drafts. */
	static void PullTranslations(
		const FString& InCulture,
		bool IncludeUnapprovedDrafts,
		TFunction<void(const FPolyglyphResponse&, const FPolyglyphPullResult&)> OnComplete);

	/**
	 * POST /api/plugin/translate to start a job for one language.
	 * Mode is sync, batch, or auto; Mock fills placeholders with no AI cost.
	 */
	static void TriggerTranslate(
		const FString& InLanguage,
		const FString& InMode,
		bool Mock,
		TFunction<void(const FPolyglyphResponse&)> OnComplete);

	/** GET /api/plugin/jobs/:jobId for a translation job's status. */
	static void GetJob(
		const FString& JobId,
		TFunction<void(const FPolyglyphResponse&)> OnComplete);

	/**
	 * GET /api/plugin/export?format=po for one culture.
	 * Delivers raw PO text on success or an error string on failure.
	 */
	static void ExportCulturePo(
		const FString& InCulture,
		TFunction<void(bool Success, const FString& PoTextOrError)> OnComplete);

	/**
	 * POST /api/plugin/enrich to attach translator-context metadata to existing keys.
	 * Large batches use sequential requests and return one aggregated response.
	 */
	static void EnrichStrings(
		const TArray<FPolyglyphEnrichItem>& Items,
		TFunction<void(const FPolyglyphResponse&)> OnComplete);

	/**
	 * Drive the HTTP manager until InDone flips or the timeout elapses.
	 * Used for synchronous commandlets and localization service operations.
	 */
	static void PumpHttp(const bool& InDone, double TimeoutSeconds);

private:
	/**
	 * Build an authenticated request to BaseUrl plus Path.
	 * Returns null and fills OutError when connection settings are incomplete.
	 */
	static TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> MakeRequest(
		const FString& Verb,
		const FString& Path,
		FString& OutError);

	/** Send Request and route the parsed result to OnComplete. */
	static void Send(
		const TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& Request,
		TFunction<void(const FPolyglyphResponse&)> OnComplete);

	/**
	 * Send one push chunk and fold its counts into the aggregate response.
	 * Chains to the next chunk until the batch is complete.
	 */
	static void SendPushChunk(
		const TSharedRef<TArray<TArray<FPolyglyphSourceString>>>& Chunks,
		int32 ChunkIndex,
		const TSharedRef<FJsonObject>& Aggregate,
		const TSharedRef<TFunction<void(const FPolyglyphResponse&)>>& OnComplete);

	/**
	 * Send one enrich chunk and fold its counts into the aggregate response.
	 * Chains to the next chunk until the batch is complete.
	 */
	static void SendEnrichChunk(
		const TSharedRef<TArray<TArray<FPolyglyphEnrichItem>>>& Chunks,
		int32 ChunkIndex,
		const TSharedRef<FJsonObject>& Aggregate,
		const TSharedRef<TFunction<void(const FPolyglyphResponse&)>>& OnComplete);
};
