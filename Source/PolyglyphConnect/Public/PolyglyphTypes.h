// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * One source string to push to Polyglyph (mirrors the service's /api/plugin/push item).
 * Plain struct, serialised to JSON by FPolyglyphClient; promote to a USTRUCT if Blueprint
 * or automation exposure is ever needed.
 */
struct FPolyglyphSourceString
{
	/** Grouping for the string, e.g. the Unreal localization namespace. */
	FString Namespace;

	/** Stable identifier within the namespace (the localization key). */
	FString Key;

	/** Source-language text to translate. */
	FString SourceText;

	/** Optional note that guides the translator/AI (where it appears, tone, etc.). */
	FString Context;

	/** Optional message-format hint: PLAIN, PLURAL, SELECT, or MULTI. Empty lets the
	 *  service auto-detect from the markup. */
	FString Format;
};
