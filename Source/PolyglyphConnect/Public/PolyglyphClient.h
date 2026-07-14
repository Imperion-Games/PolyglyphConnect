// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"

class FJsonObject;
struct FPolyglyphSourceString;

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

	/** POST /api/plugin/push to upsert the given source strings. */
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
};
