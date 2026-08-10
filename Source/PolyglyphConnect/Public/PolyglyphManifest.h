// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FLocMetadataObject;
struct FPolyglyphEnrichItem;
struct FPolyglyphSourceString;

/**
 * Reads source strings and native dialogue metadata from the project's UE localization manifest.
 * The manifest is the single source of truth produced by the Localization Dashboard's gather;
 * this never crawls individual assets.
 */
class FPolyglyphManifest
{
public:
	/** Load source strings and automatic DialogueWave enrichment from the configured manifest. */
	static bool Gather(
		TArray<FPolyglyphSourceString>& OutStrings,
		TArray<FPolyglyphEnrichItem>& OutEnrichItems,
		FString& OutError);

	/** Convert one Unreal DialogueWave manifest context into a Polyglyph enrichment item. */
	static bool BuildDialogueEnrichment(
		const FString& InNamespace,
		const FString& InKey,
		const TSharedPtr<FLocMetadataObject>& InInfoMetadata,
		const TSharedPtr<FLocMetadataObject>& InKeyMetadata,
		FPolyglyphEnrichItem& OutItem);
};
