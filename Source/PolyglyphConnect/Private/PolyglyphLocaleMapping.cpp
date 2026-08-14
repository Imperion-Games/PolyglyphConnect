// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphLocaleMapping.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Interfaces/IPluginManager.h"
#include "LocalizationTargetTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	constexpr int32 CatalogSchemaVersion = 1;

	/** Convert an external culture code into the case-insensitive catalog key. */
	FString MakeMappingKey(const FString& InExternalCode)
	{
		FString Key = InExternalCode;
		Key.TrimStartAndEndInline();
		Key.ToLowerInline();
		return Key;
	}

	/**
	 * Major.Minor of the engine this plugin is compiled against, e.g. "5.7".
	 *
	 * Reported to the Polyglyph API so the backend sees the engine actually in use.
	 * This was previously hardcoded to 5.7, which meant every project reported 5.7
	 * regardless of the engine it ran on.
	 */
	FString GetEngineVersionString()
	{
		return FString::Printf(TEXT("%d.%d"), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION);
	}

	/**
	 * Locate the catalog resource that ships with this plugin.
	 *
	 * The catalog is a list of language codes, which does not vary between engine
	 * versions, so one version-agnostic file serves every supported engine. A
	 * legacy 5.7-specific name is still accepted so an existing install keeps working.
	 */
	bool GetShippedCatalogPath(FString& OutPath, FString& OutError)
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("PolyglyphConnect"));
		if (!Plugin.IsValid())
		{
			OutError = TEXT("Could not find the PolyglyphConnect plugin directory.");
			return false;
		}

		const FString MappingsDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("LocaleMappings"));

		OutPath = FPaths::Combine(MappingsDir, TEXT("Unreal.json"));
		if (FPaths::FileExists(OutPath))
		{
			return true;
		}

		// Fall back to the pre-multi-version filename.
		const FString LegacyPath = FPaths::Combine(MappingsDir, TEXT("Unreal-5.7.json"));
		if (FPaths::FileExists(LegacyPath))
		{
			OutPath = LegacyPath;
			return true;
		}

		OutError = FString::Printf(TEXT("No shipped locale catalog found in '%s'."), *MappingsDir);
		return false;
	}

	/** Read one JSON mapping record. */
	bool ParseMapping(
		const FJsonObject& InObject,
		FPolyglyphLocaleMapping& OutMapping,
		FString& OutError)
	{
		OutMapping = FPolyglyphLocaleMapping();
		InObject.TryGetStringField(TEXT("externalCode"), OutMapping.ExternalCode);
		InObject.TryGetStringField(TEXT("localeTag"), OutMapping.LocaleTag);
		InObject.TryGetStringField(TEXT("displayName"), OutMapping.DisplayName);

		OutMapping.ExternalCode.TrimStartAndEndInline();
		OutMapping.LocaleTag.TrimStartAndEndInline();
		OutMapping.DisplayName.TrimStartAndEndInline();
		if (OutMapping.ExternalCode.IsEmpty() || OutMapping.LocaleTag.IsEmpty())
		{
			OutError = TEXT("Each locale mapping requires externalCode and localeTag.");
			return false;
		}
		if (OutMapping.DisplayName.IsEmpty())
		{
			OutMapping.DisplayName = OutMapping.LocaleTag;
		}

		return true;
	}

	/** Load a JSON catalog, adding its mappings after previously loaded entries. */
	bool LoadCatalogFile(
		const FString& InPath,
		TMap<FString, FPolyglyphLocaleMapping>& InOutMappings,
		FString& OutError)
	{
		FString JsonText;
		if (!FFileHelper::LoadFileToString(JsonText, *InPath))
		{
			OutError = FString::Printf(TEXT("Could not read locale mapping file '%s'."), *InPath);
			return false;
		}

		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutError = FString::Printf(TEXT("Locale mapping file '%s' is not valid JSON."), *InPath);
			return false;
		}

		int32 SchemaVersion = 0;
		Root->TryGetNumberField(TEXT("schemaVersion"), SchemaVersion);
		if (SchemaVersion != CatalogSchemaVersion)
		{
			OutError = FString::Printf(
				TEXT("Locale mapping file '%s' uses schema version %d. Expected %d."),
				*InPath,
				SchemaVersion,
				CatalogSchemaVersion);
			return false;
		}

		FString Integration;
		Root->TryGetStringField(TEXT("integration"), Integration);
		if (!Integration.Equals(TEXT("unreal"), ESearchCase::IgnoreCase))
		{
			OutError = FString::Printf(TEXT("Locale mapping file '%s' is not an Unreal mapping catalog."), *InPath);
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Mappings = nullptr;
		if (!Root->TryGetArrayField(TEXT("mappings"), Mappings) || Mappings == nullptr)
		{
			OutError = FString::Printf(TEXT("Locale mapping file '%s' is missing its mappings array."), *InPath);
			return false;
		}

		TSet<FString> FileKeys;
		for (const TSharedPtr<FJsonValue>& Value : *Mappings)
		{
			const TSharedPtr<FJsonObject>& Object = Value->AsObject();
			if (!Object.IsValid())
			{
				OutError = FString::Printf(TEXT("Locale mapping file '%s' contains a non-object mapping."), *InPath);
				return false;
			}

			FPolyglyphLocaleMapping Mapping;
			if (!ParseMapping(*Object, Mapping, OutError))
			{
				OutError = FString::Printf(TEXT("Locale mapping file '%s': %s"), *InPath, *OutError);
				return false;
			}

			const FString Key = MakeMappingKey(Mapping.ExternalCode);
			if (FileKeys.Contains(Key))
			{
				OutError = FString::Printf(TEXT("Locale mapping file '%s' maps '%s' more than once."), *InPath, *Mapping.ExternalCode);
				return false;
			}
			FileKeys.Add(Key);
			InOutMappings.Add(Key, MoveTemp(Mapping));
		}

		return true;
	}

	/** Resolve a culture from Unreal's ICU catalog while generating the shipped resource. */
	bool BuildEngineMapping(
		const FString& InCultureName,
		FPolyglyphLocaleMapping& OutMapping,
		FString& OutError)
	{
		const FString CanonicalName = FCulture::GetCanonicalName(InCultureName);
		const FCulturePtr Culture = FInternationalization::Get().GetCulture(CanonicalName);
		if (!Culture.IsValid())
		{
			OutError = FString::Printf(TEXT("Unreal could not resolve culture '%s'."), *InCultureName);
			return false;
		}

		OutMapping.ExternalCode = InCultureName;
		OutMapping.LocaleTag = Culture->GetName();
		OutMapping.LocaleTag.ReplaceInline(TEXT("_"), TEXT("-"), ESearchCase::CaseSensitive);
		OutMapping.DisplayName = Culture->GetEnglishName();
		if (OutMapping.DisplayName.IsEmpty())
		{
			OutMapping.DisplayName = OutMapping.LocaleTag;
		}

		return true;
	}
}

bool FPolyglyphUnrealLocaleCatalog::Load(
	const FString& InProjectMappingFile,
	TMap<FString, FPolyglyphLocaleMapping>& OutMappings,
	FString& OutError)
{
	OutMappings.Reset();
	OutError.Reset();

	FString ShippedCatalogPath;
	if (!GetShippedCatalogPath(ShippedCatalogPath, OutError)
		|| !LoadCatalogFile(ShippedCatalogPath, OutMappings, OutError))
	{
		return false;
	}

	FString ProjectMappingFile = InProjectMappingFile;
	ProjectMappingFile.TrimStartAndEndInline();
	if (ProjectMappingFile.IsEmpty())
	{
		return true;
	}

	if (FPaths::IsRelative(ProjectMappingFile))
	{
		ProjectMappingFile = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), ProjectMappingFile);
	}

	return LoadCatalogFile(ProjectMappingFile, OutMappings, OutError);
}

const FPolyglyphLocaleMapping* FPolyglyphUnrealLocaleCatalog::Find(
	const FString& InExternalCode,
	const TMap<FString, FPolyglyphLocaleMapping>& InMappings)
{
	return InMappings.Find(MakeMappingKey(InExternalCode));
}

bool FPolyglyphUnrealLocaleCatalog::GenerateDefaultCatalog(
	TArray<FPolyglyphLocaleMapping>& OutMappings,
	FString& OutError)
{
	OutMappings.Reset();
	OutError.Reset();

	TArray<FString> CultureNames;
	FInternationalization::Get().GetCultureNames(CultureNames);
	CultureNames.Sort();

	TSet<FString> MappingKeys;
	for (const FString& CultureName : CultureNames)
	{
		if (CultureName.IsEmpty())
		{
			continue;
		}

		FPolyglyphLocaleMapping Mapping;
		if (!BuildEngineMapping(CultureName, Mapping, OutError))
		{
			return false;
		}

		const FString MappingKey = MakeMappingKey(Mapping.ExternalCode);
		if (!MappingKeys.Contains(MappingKey))
		{
			MappingKeys.Add(MappingKey);
			OutMappings.Add(MoveTemp(Mapping));
		}
	}

	return true;
}

bool FPolyglyphUnrealLocaleCatalog::WriteCatalog(
	const TArray<FPolyglyphLocaleMapping>& InMappings,
	const FString& InOutputFile,
	FString& OutError)
{
	OutError.Reset();

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schemaVersion"), CatalogSchemaVersion);
	Root->SetStringField(TEXT("integration"), TEXT("unreal"));
	Root->SetStringField(TEXT("engineVersion"), GetEngineVersionString());

	TArray<TSharedPtr<FJsonValue>> JsonMappings;
	JsonMappings.Reserve(InMappings.Num());
	for (const FPolyglyphLocaleMapping& Mapping : InMappings)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("externalCode"), Mapping.ExternalCode);
		Object->SetStringField(TEXT("localeTag"), Mapping.LocaleTag);
		Object->SetStringField(TEXT("displayName"), Mapping.DisplayName);
		JsonMappings.Add(MakeShared<FJsonValueObject>(Object));
	}
	Root->SetArrayField(TEXT("mappings"), JsonMappings);

	FString JsonText;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		OutError = TEXT("Could not serialize the Unreal locale catalog.");
		return false;
	}

	if (!FFileHelper::SaveStringToFile(JsonText, *InOutputFile))
	{
		OutError = FString::Printf(TEXT("Could not write locale catalog '%s'."), *InOutputFile);
		return false;
	}

	return true;
}

bool FPolyglyphUnrealLocaleMapper::BuildManifest(
	const ULocalizationTarget* InLocalizationTarget,
	const TMap<FString, FPolyglyphLocaleMapping>& InMappings,
	FPolyglyphLocaleManifest& OutManifest,
	FString& OutError)
{
	OutManifest = FPolyglyphLocaleManifest();
	OutError.Reset();

	if (InLocalizationTarget == nullptr)
	{
		OutError = TEXT("The Unreal localization target is unavailable.");
		return false;
	}

	const FLocalizationTargetSettings& Settings = InLocalizationTarget->Settings;
	if (!Settings.SupportedCulturesStatistics.IsValidIndex(Settings.NativeCultureIndex))
	{
		OutError = FString::Printf(
			TEXT("Localization target '%s' does not have a valid native culture."),
			*Settings.Name);
		return false;
	}

	OutManifest.IntegrationKind = TEXT("unreal");
	OutManifest.IntegrationId = Settings.Guid.ToString(EGuidFormats::DigitsWithHyphensLower);
	OutManifest.IntegrationName = Settings.Name;

	TSet<FString> LocaleTags;
	for (int32 CultureIndex = 0; CultureIndex < Settings.SupportedCulturesStatistics.Num(); ++CultureIndex)
	{
		const FString& CultureName = Settings.SupportedCulturesStatistics[CultureIndex].CultureName;
		const FPolyglyphLocaleMapping* const Mapping = FPolyglyphUnrealLocaleCatalog::Find(CultureName, InMappings);
		if (Mapping == nullptr)
		{
			OutError = FString::Printf(
				TEXT("Localization target '%s' uses culture '%s', which is not in the shipped Unreal catalog or the project override file."),
				*Settings.Name,
				*CultureName);
			return false;
		}

		const FString LocaleTagKey = Mapping->LocaleTag.ToLower();
		if (LocaleTags.Contains(LocaleTagKey))
		{
			OutError = FString::Printf(
				TEXT("Localization target '%s' maps more than one Unreal culture to '%s'."),
				*Settings.Name,
				*Mapping->LocaleTag);
			return false;
		}
		LocaleTags.Add(LocaleTagKey);

		if (CultureIndex == Settings.NativeCultureIndex)
		{
			OutManifest.SourceLocale = *Mapping;
		}
		else
		{
			OutManifest.TargetLocales.Add(*Mapping);
		}
	}

	return true;
}
