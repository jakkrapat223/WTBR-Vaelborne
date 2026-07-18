#include "NiagaraJsonSpikeImporter.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "Components/SceneCaptureComponent2D.h"
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraTypes.h"
#include "NiagaraUserRedirectionParameterStore.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Actors/WTBRProjectileBase.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Subsystems/AssetEditorSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogNiagaraJsonSpike, Log, All);

namespace
{
	// Supported production parameter types, mapped to Niagara type definitions.
	// Sizes must match SetParameterValue<T>'s sizeof check:
	//   float(4) / int32(4) / FNiagaraBool(4) / FLinearColor(16) / FVector3f(12)
	const FNiagaraTypeDefinition* ResolveTypeDef(const FString& TypeName)
	{
		const FString Lower = TypeName.ToLower();
		if (Lower == TEXT("float"))                            { return &FNiagaraTypeDefinition::GetFloatDef(); }
		if (Lower == TEXT("int") || Lower == TEXT("int32"))    { return &FNiagaraTypeDefinition::GetIntDef(); }
		if (Lower == TEXT("bool"))                             { return &FNiagaraTypeDefinition::GetBoolDef(); }
		if (Lower == TEXT("color") || Lower == TEXT("linearcolor")) { return &FNiagaraTypeDefinition::GetColorDef(); }
		if (Lower == TEXT("vector2") || Lower == TEXT("vec2")) { return &FNiagaraTypeDefinition::GetVec2Def(); }
		if (Lower == TEXT("vector3") || Lower == TEXT("vec3") || Lower == TEXT("vector"))
		{
			return &FNiagaraTypeDefinition::GetVec3Def();
		}
		if (Lower == TEXT("vector4") || Lower == TEXT("vec4")) { return &FNiagaraTypeDefinition::GetVec4Def(); }
		return nullptr;
	}

	// Phase A path whitelists (see memory scope locks / README).
	const TCHAR* TemplateRoot = TEXT("/Game/VFX/Templates/");
	const TCHAR* OutputRoot   = TEXT("/Game/VFX/");

	template <typename ValueType>
	bool SetUserParameterDefault(
		UNiagaraSystem* System,
		const TCHAR* ParamName,
		const FNiagaraTypeDefinition& TypeDef,
		const ValueType& Value)
	{
		if (!IsValid(System))
		{
			return false;
		}

		FNiagaraVariable Variable(TypeDef, FName(ParamName));
		FNiagaraUserRedirectionParameterStore::MakeUserVariable(Variable);
		return System->GetExposedParameters().SetParameterValue(Value, Variable, true);
	}

	bool EnsureSlashTrailUserParameterContract(UNiagaraSystem* System)
	{
		if (!IsValid(System))
		{
			return false;
		}

		System->Modify();
		const bool bColorSet = SetUserParameterDefault(
			System,
			TEXT("User.Color"),
			FNiagaraTypeDefinition::GetColorDef(),
			FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("27D8FF"))));
		const bool bHotCoreSet = SetUserParameterDefault(
			System,
			TEXT("User.HotCoreColor"),
			FNiagaraTypeDefinition::GetColorDef(),
			FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("F2F5FF"))));
		const bool bWidthSet = SetUserParameterDefault(
			System,
			TEXT("User.SlashWidth"),
			FNiagaraTypeDefinition::GetFloatDef(),
			26.0f);
		const bool bLifetimeSet = SetUserParameterDefault(
			System,
			TEXT("User.TrailLifetime"),
			FNiagaraTypeDefinition::GetFloatDef(),
			0.18f);

		const bool bSucceeded = bColorSet && bHotCoreSet && bWidthSet && bLifetimeSet;
		if (bSucceeded)
		{
			System->MarkPackageDirty();
			UE_LOG(LogNiagaraJsonSpike, Display,
				TEXT("Slash Trail parameter contract ready on %s: User.Color, User.HotCoreColor, User.SlashWidth, User.TrailLifetime"),
				*System->GetPathName());
		}
		else
		{
			UE_LOG(LogNiagaraJsonSpike, Error,
				TEXT("MT006: Failed to apply Slash Trail parameter contract on %s"),
				*System->GetPathName());
		}
		return bSucceeded;
	}

	bool IsNumberValue(const TSharedPtr<FJsonValue>& Value)
	{
		double Unused = 0.0;
		return Value.IsValid() && Value->TryGetNumber(Unused);
	}

	// Pre-flight shape check for a param's "value" against its declared type.
	// Mirrors what ApplyParam() will accept at runtime, without touching assets.
	bool IsValidValueShape(const FNiagaraTypeDefinition& TypeDef,
	                       const TSharedPtr<FJsonValue>& Value,
	                       FString& OutExpected)
	{
		if (TypeDef == FNiagaraTypeDefinition::GetFloatDef()
			|| TypeDef == FNiagaraTypeDefinition::GetIntDef())
		{
			OutExpected = TEXT("a number");
			return IsNumberValue(Value);
		}
		if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())
		{
			OutExpected = TEXT("true/false");
			bool Unused = false;
			return Value.IsValid() && Value->TryGetBool(Unused);
		}

		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		const bool bIsArray = Value.IsValid() && Value->TryGetArray(Arr);

		auto AllNumbers = [](const TArray<TSharedPtr<FJsonValue>>& Elems)
		{
			for (const TSharedPtr<FJsonValue>& Elem : Elems)
			{
				double Unused = 0.0;
				if (!Elem.IsValid() || !Elem->TryGetNumber(Unused)) { return false; }
			}
			return true;
		};

		if (TypeDef == FNiagaraTypeDefinition::GetColorDef())
		{
			OutExpected = TEXT("an array [r,g,b] or [r,g,b,a] of numbers");
			return bIsArray && (Arr->Num() == 3 || Arr->Num() == 4) && AllNumbers(*Arr);
		}
		if (TypeDef == FNiagaraTypeDefinition::GetVec3Def())
		{
			OutExpected = TEXT("an array [x,y,z] of exactly 3 numbers");
			return bIsArray && Arr->Num() == 3 && AllNumbers(*Arr);
		}
		if (TypeDef == FNiagaraTypeDefinition::GetVec2Def())
		{
			OutExpected = TEXT("an array [x,y] of exactly 2 numbers");
			return bIsArray && Arr->Num() == 2 && AllNumbers(*Arr);
		}
		if (TypeDef == FNiagaraTypeDefinition::GetVec4Def())
		{
			OutExpected = TEXT("an array [x,y,z,w] of exactly 4 numbers");
			return bIsArray && Arr->Num() == 4 && AllNumbers(*Arr);
		}
		OutExpected = TEXT("(unknown)");
		return false;
	}

	bool ReadNumberArray(const TSharedPtr<FJsonObject>& ParamObj, int32 MinCount,
	                     TArray<double>& OutValues)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!ParamObj->TryGetArrayField(TEXT("value"), Arr) || !Arr)
		{
			return false;
		}
		OutValues.Reset();
		for (const TSharedPtr<FJsonValue>& Elem : *Arr)
		{
			double Num = 0.0;
			if (!Elem.IsValid() || !Elem->TryGetNumber(Num))
			{
				return false;
			}
			OutValues.Add(Num);
		}
		return OutValues.Num() >= MinCount;
	}
}

// Phase A pre-flight — collects ALL schema problems (not fail-fast) so one run
// surfaces every fix needed. E-codes are errors (abort before asset access),
// W-codes are warnings (import continues). Codes are documented in README.
bool FNiagaraJsonSpikeImporter::ValidateSpec(const TSharedPtr<FJsonObject>& Root, FImportStats& Stats)
{
	int32 Errors = 0;
	auto Error = [&Errors](const FString& Code, const FString& Msg)
	{
		UE_LOG(LogNiagaraJsonSpike, Error, TEXT("%s: %s"), *Code, *Msg);
		Errors++;
	};
	auto Warn = [&Stats](const FString& Code, const FString& Msg)
	{
		LogWarning(FString::Printf(TEXT("%s: %s"), *Code, *Msg), &Stats);
	};

	// W001 — unknown top-level fields (warn, keep importing)
	static const TSet<FString> KnownTopLevelFields =
		{ TEXT("schemaVersion"), TEXT("template"), TEXT("outputPath"),
		  TEXT("strict"), TEXT("addMissingParams"), TEXT("params") };
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Root->Values)
	{
		if (!KnownTopLevelFields.Contains(Pair.Key))
		{
			Warn(TEXT("W001"), FString::Printf(
				TEXT("Unknown top-level field \"%s\" — ignored"), *Pair.Key));
		}
	}

	// schemaVersion is optional for backwards compatibility with v0.1 specs.
	// New production specs should declare version 1 explicitly.
	if (Root->HasField(TEXT("schemaVersion")))
	{
		double SchemaVersion = 0.0;
		if (!Root->TryGetNumberField(TEXT("schemaVersion"), SchemaVersion)
			|| SchemaVersion != 1.0)
		{
			Error(TEXT("E014"), TEXT("\"schemaVersion\" must be the number 1"));
		}
	}

	if (Root->HasField(TEXT("strict")))
	{
		bool bUnused = false;
		if (!Root->TryGetBoolField(TEXT("strict"), bUnused))
		{
			Error(TEXT("E015"), TEXT("\"strict\" must be true or false"));
		}
	}

	// template: required string, must live under the template whitelist root
	FString TemplatePath;
	if (!Root->TryGetStringField(TEXT("template"), TemplatePath) || TemplatePath.IsEmpty())
	{
		Error(TEXT("E003"), TEXT("Missing or empty required string field \"template\""));
	}
	else if (!TemplatePath.StartsWith(TemplateRoot))
	{
		Error(TEXT("E004"), FString::Printf(
			TEXT("\"template\" must be under %s (whitelist) — got: %s"),
			TemplateRoot, *TemplatePath));
	}

	// outputPath: required string, under /Game/VFX/, never under the template root
	FString OutputPath;
	if (!Root->TryGetStringField(TEXT("outputPath"), OutputPath) || OutputPath.IsEmpty())
	{
		Error(TEXT("E005"), TEXT("Missing or empty required string field \"outputPath\""));
	}
	else
	{
		if (!OutputPath.StartsWith(OutputRoot))
		{
			Error(TEXT("E006"), FString::Printf(
				TEXT("\"outputPath\" must be under %s — got: %s"), OutputRoot, *OutputPath));
		}
		else if (OutputPath.StartsWith(TemplateRoot))
		{
			Error(TEXT("E007"), FString::Printf(
				TEXT("\"outputPath\" must not be under %s (templates are read-only inputs) — got: %s"),
				TemplateRoot, *OutputPath));
		}
	}

	// addMissingParams: optional, but if present it must be a bool (default stays false)
	if (Root->HasField(TEXT("addMissingParams")))
	{
		bool bUnused = false;
		if (!Root->TryGetBoolField(TEXT("addMissingParams"), bUnused))
		{
			Warn(TEXT("W005"),
				TEXT("\"addMissingParams\" is not a boolean — using default false"));
		}
	}

	// params: required object; every entry must be { "type": <allowed>, "value": <shape> }
	const TSharedPtr<FJsonObject>* ParamsObj = nullptr;
	if (!Root->TryGetObjectField(TEXT("params"), ParamsObj) || !ParamsObj->IsValid())
	{
		Error(TEXT("E008"), TEXT("Missing required object field \"params\""));
	}
	else
	{
		if ((*ParamsObj)->Values.Num() == 0)
		{
			Warn(TEXT("W004"), TEXT("\"params\" is empty — asset will be duplicated/updated with no parameter changes"));
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ParamsObj)->Values)
		{
			const FString EntryLabel = FString::Printf(TEXT("Param \"%s\""), *Pair.Key);

			if (Pair.Key.TrimStartAndEnd().IsEmpty())
			{
				Error(TEXT("E013"), TEXT("Param key must not be empty"));
				continue;
			}

			const TSharedPtr<FJsonObject>* Entry = nullptr;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(Entry) || !Entry->IsValid())
			{
				Error(TEXT("E009"), EntryLabel
					+ TEXT(": entry must be an object { \"type\": ..., \"value\": ... }"));
				continue;
			}

			FString TypeName;
			if (!(*Entry)->TryGetStringField(TEXT("type"), TypeName) || TypeName.IsEmpty())
			{
				Error(TEXT("E010"), EntryLabel + TEXT(": missing required string field \"type\""));
				continue;
			}

			const FNiagaraTypeDefinition* TypeDef = ResolveTypeDef(TypeName);
			if (!TypeDef)
			{
				Error(TEXT("E011"), FString::Printf(
					TEXT("%s: unsupported type \"%s\" (allowed: float, int, bool, color, vector2, vector3, vector4)"),
					*EntryLabel, *TypeName));
				continue;
			}

			if (!(*Entry)->HasField(TEXT("value")))
			{
				Error(TEXT("E012"), EntryLabel + TEXT(": missing required field \"value\""));
				continue;
			}
			FString Expected;
			if (!IsValidValueShape(*TypeDef, (*Entry)->Values[TEXT("value")], Expected))
			{
				Error(TEXT("E012"), FString::Printf(
					TEXT("%s: \"value\" must be %s for type \"%s\""),
					*EntryLabel, *Expected, *TypeName));
			}
		}
	}

	if (Errors > 0)
	{
		UE_LOG(LogNiagaraJsonSpike, Error,
			TEXT("Pre-flight validation found %d error(s) — see E-codes above."), Errors);
		return false;
	}
	return true;
}

// Existence-detection + safe load/duplicate — see header comment for why this
// cannot use UEditorAssetLibrary::DoesAssetExist alone (Asset Registry cache,
// stale for a package a *different* prior process just wrote to disk).
bool FNiagaraJsonSpikeImporter::ValidateTemplateContract(
	const TSharedPtr<FJsonObject>& Root, UNiagaraSystem* TemplateSystem,
	bool bLogAsErrors, FImportStats& Stats)
{
	if (!Root.IsValid() || !TemplateSystem)
	{
		return false;
	}

	FNiagaraUserRedirectionParameterStore& Store = TemplateSystem->GetExposedParameters();
	TArray<FNiagaraVariable> ExistingParams;
	Store.GetParameters(ExistingParams);

	const TSharedPtr<FJsonObject>* ParamsObj = nullptr;
	if (!Root->TryGetObjectField(TEXT("params"), ParamsObj) || !ParamsObj->IsValid())
	{
		return false;
	}

	int32 Issues = 0;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ParamsObj)->Values)
	{
		const TSharedPtr<FJsonObject>* Entry = nullptr;
		FString TypeName;
		if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(Entry)
			|| !Entry->IsValid() || !(*Entry)->TryGetStringField(TEXT("type"), TypeName))
		{
			continue;
		}

		const FNiagaraTypeDefinition* TypeDef = ResolveTypeDef(TypeName);
		if (!TypeDef)
		{
			continue;
		}

		FNiagaraVariable Requested(*TypeDef, FName(*Pair.Key));
		FNiagaraUserRedirectionParameterStore::MakeUserVariable(Requested);
		const FNiagaraVariable* Existing = ExistingParams.FindByPredicate(
			[&Requested](const FNiagaraVariable& Param)
			{
				return Param.GetName() == Requested.GetName();
			});

		FString Message;
		if (!Existing)
		{
			Message = FString::Printf(
				TEXT("Template contract: %s is not exposed by %s"),
				*Requested.GetName().ToString(), *TemplateSystem->GetPathName());
		}
		else if (Existing->GetType() != *TypeDef)
		{
			Message = FString::Printf(
				TEXT("Template contract: %s type mismatch (template=%s, JSON=%s)"),
				*Requested.GetName().ToString(), *Existing->GetType().GetName(), *TypeName);
		}

		if (!Message.IsEmpty())
		{
			Issues++;
			if (bLogAsErrors)
			{
				UE_LOG(LogNiagaraJsonSpike, Error, TEXT("E301: %s"), *Message);
			}
			else
			{
				LogWarning(FString::Printf(TEXT("W301: %s"), *Message), &Stats);
			}
		}
	}

	return Issues == 0;
}

bool FNiagaraJsonSpikeImporter::ValidateOutputTarget(const FString& OutputPath)
{
	if (!FPackageName::DoesPackageExist(OutputPath))
	{
		return true;
	}

	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.ScanPathsSynchronous(
		TArray<FString>{ FPackageName::GetLongPackagePath(OutputPath) }, true);

	if (!Cast<UNiagaraSystem>(UEditorAssetLibrary::LoadAsset(OutputPath)))
	{
		LogError(FString::Printf(
			TEXT("E302: Existing output is not a NiagaraSystem: %s"), *OutputPath));
		return false;
	}
	return true;
}

UNiagaraSystem* FNiagaraJsonSpikeImporter::ResolveOutputAsset(const FString& TemplatePath,
                                                              const FString& OutputPath,
                                                              bool& bOutIsNewAsset)
{
	bOutIsNewAsset = false;

	// FPackageName::DoesPackageExist resolves directly against the mounted
	// content roots on disk — it does not consult the Asset Registry, so it
	// cannot be fooled by a registry that hasn't scanned this package yet.
	if (FPackageName::DoesPackageExist(OutputPath))
	{
		if (!UEditorAssetLibrary::DoesAssetExist(OutputPath))
		{
			LogWarning(FString::Printf(
				TEXT("W201: Asset Registry has no record of package \"%s\" even though it exists on disk (stale in this process) — forcing a targeted rescan before loading."),
				*OutputPath), nullptr);
		}

		// Targeted, synchronous, forced rescan of just this package's directory —
		// resyncs the registry before anything downstream trusts it.
		IAssetRegistry& AssetRegistry =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		AssetRegistry.ScanPathsSynchronous(
			TArray<FString>{ FPackageName::GetLongPackagePath(OutputPath) }, /*bForceRescan*/ true);

		UNiagaraSystem* Existing = Cast<UNiagaraSystem>(UEditorAssetLibrary::LoadAsset(OutputPath));
		if (!Existing)
		{
			LogError(FString::Printf(
				TEXT("E201: Package \"%s\" exists on disk but could not be loaded as a UNiagaraSystem — aborting (refusing to fall back to DuplicateAsset, which would overwrite it)."),
				*OutputPath));
			return nullptr;
		}

		UE_LOG(LogNiagaraJsonSpike, Log,
			TEXT("Output asset exists on disk — updating User Parameters in place: %s"), *OutputPath);
		return Existing;
	}

	// No package on disk as of the check above. Re-check immediately before
	// duplicating to close the race window: if something wrote the destination
	// in between, hard-abort rather than silently overwriting it.
	if (FPackageName::DoesPackageExist(OutputPath))
	{
		LogError(FString::Printf(
			TEXT("E202: DuplicateAsset blocked — destination package \"%s\" appeared on disk between existence checks."),
			*OutputPath));
		return nullptr;
	}

	UNiagaraSystem* Duplicated = Cast<UNiagaraSystem>(UEditorAssetLibrary::DuplicateAsset(TemplatePath, OutputPath));
	if (!Duplicated)
	{
		LogError(FString::Printf(TEXT("E104: DuplicateAsset failed: %s -> %s"),
			*TemplatePath, *OutputPath));
		return nullptr;
	}

	bOutIsNewAsset = true;
	UE_LOG(LogNiagaraJsonSpike, Log, TEXT("Duplicated template %s -> %s"),
		*TemplatePath, *OutputPath);
	return Duplicated;
}

UNiagaraSystem* FNiagaraJsonSpikeImporter::ImportFromFile(const FString& FilePath)
{
	// ── 1. Read + parse JSON ──────────────────────────────────────────────────
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *FilePath))
	{
		LogError(FString::Printf(TEXT("E001: Cannot read file: %s"), *FilePath));
		return nullptr;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		LogError(FString::Printf(TEXT("E002: Invalid JSON in file: %s"), *FilePath));
		return nullptr;
	}

	// ── 2. Pre-flight schema validation — MUST pass before any asset access ──
	FImportStats Stats;
	if (!ValidateSpec(Root, Stats))
	{
		LogError(FString::Printf(
			TEXT("E000: Pre-flight validation failed for %s — no assets were touched."),
			*FilePath));
		return nullptr;
	}

	// Guaranteed present/valid by ValidateSpec.
	FString TemplatePath;
	Root->TryGetStringField(TEXT("template"), TemplatePath);
	FString OutputPath;
	Root->TryGetStringField(TEXT("outputPath"), OutputPath);
	bool bAddMissing = false;
	Root->TryGetBoolField(TEXT("addMissingParams"), bAddMissing);

	// ── 3. Load template ──────────────────────────────────────────────────────
	if (!UEditorAssetLibrary::DoesAssetExist(TemplatePath))
	{
		LogError(FString::Printf(TEXT("E101: Template asset not found: %s"), *TemplatePath));
		return nullptr;
	}

	UNiagaraSystem* TemplateSystem = Cast<UNiagaraSystem>(UEditorAssetLibrary::LoadAsset(TemplatePath));
	if (!TemplateSystem)
	{
		LogError(FString::Printf(TEXT("E102: Template asset is not a NiagaraSystem: %s"), *TemplatePath));
		return nullptr;
	}

	// ── 4. Duplicate template, or update the output asset in place ───────────
	bool bStrict = false;
	Root->TryGetBoolField(TEXT("strict"), bStrict);
	if (bStrict && !ValidateTemplateContract(Root, TemplateSystem, true, Stats))
	{
		LogError(FString::Printf(
			TEXT("E300: Strict template-contract validation failed for %s - no output asset was touched."),
			*FilePath));
		return nullptr;
	}

	if (!ValidateOutputTarget(OutputPath))
	{
		return nullptr;
	}

	bool bIsNewAsset = false;
	UNiagaraSystem* System = ResolveOutputAsset(TemplatePath, OutputPath, bIsNewAsset);
	if (!System)
	{
		return nullptr;
	}

	// ── 5. Apply User Parameter values (spec already validated) ──────────────
	System->Modify();

	const TSharedPtr<FJsonObject>* ParamsObj = nullptr;
	if (Root->TryGetObjectField(TEXT("params"), ParamsObj) && ParamsObj->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ParamsObj)->Values)
		{
			const TSharedPtr<FJsonObject>* Entry = nullptr;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(Entry) || !Entry->IsValid())
			{
				// Unreachable after ValidateSpec; kept as a runtime backstop.
				LogWarning(FString::Printf(
					TEXT("W104: Param \"%s\": entry must be an object { \"type\": ..., \"value\": ... } — skipped"),
					*Pair.Key), &Stats);
				Stats.Skipped++;
				continue;
			}
			if (!ApplyParam(System, Pair.Key, *Entry, bAddMissing, Stats))
			{
				Stats.Skipped++;
			}
		}
	}

	// ── 6. Save ───────────────────────────────────────────────────────────────
	System->MarkPackageDirty();
	const bool bSaved = UEditorAssetLibrary::SaveLoadedAsset(System, /*bOnlyIfIsDirty*/ false);
	if (!bSaved)
	{
		LogError(FString::Printf(TEXT("E105: SaveLoadedAsset failed for: %s"), *OutputPath));
		return nullptr;
	}

	const FString Summary = FString::Printf(
		TEXT("%s %s | params set: %d, added: %d, skipped: %d, warnings: %d"),
		bIsNewAsset ? TEXT("Created") : TEXT("Updated"),
		*OutputPath, Stats.ParamsSet, Stats.ParamsAdded, Stats.Skipped, Stats.Warnings);
	UE_LOG(LogNiagaraJsonSpike, Log, TEXT("%s"), *Summary);
	Notify(Summary, Stats.Skipped == 0);

	return System;
}

bool FNiagaraJsonSpikeImporter::ValidateFile(const FString& FilePath)
{
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *FilePath))
	{
		LogError(FString::Printf(TEXT("E001: Cannot read file: %s"), *FilePath));
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		LogError(FString::Printf(TEXT("E002: Invalid JSON in file: %s"), *FilePath));
		return false;
	}

	FImportStats Stats;
	if (!ValidateSpec(Root, Stats))
	{
		LogError(FString::Printf(
			TEXT("E000: Validation failed for %s - no assets were touched."), *FilePath));
		return false;
	}

	FString TemplatePath;
	FString OutputPath;
	Root->TryGetStringField(TEXT("template"), TemplatePath);
	Root->TryGetStringField(TEXT("outputPath"), OutputPath);

	if (!UEditorAssetLibrary::DoesAssetExist(TemplatePath))
	{
		LogError(FString::Printf(TEXT("E101: Template asset not found: %s"), *TemplatePath));
		return false;
	}

	UNiagaraSystem* TemplateSystem = Cast<UNiagaraSystem>(
		UEditorAssetLibrary::LoadAsset(TemplatePath));
	if (!TemplateSystem)
	{
		LogError(FString::Printf(TEXT("E102: Template asset is not a NiagaraSystem: %s"), *TemplatePath));
		return false;
	}

	const bool bContractValid = ValidateTemplateContract(
		Root, TemplateSystem, false, Stats);
	const bool bOutputValid = ValidateOutputTarget(OutputPath);
	const bool bValid = bContractValid && bOutputValid;

	const FString Summary = FString::Printf(
		TEXT("Validation %s: %s | template contract: %s, warnings: %d, no assets modified"),
		bValid ? TEXT("PASSED") : TEXT("FAILED"), *FilePath,
		bContractValid ? TEXT("valid") : TEXT("invalid"), Stats.Warnings);
	UE_LOG(LogNiagaraJsonSpike, Display, TEXT("%s"), *Summary);
	Notify(Summary, bValid);
	return bValid;
}

bool FNiagaraJsonSpikeImporter::ImportBatchFromFile(const FString& FilePath)
{
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *FilePath))
	{
		LogError(FString::Printf(TEXT("B001: Cannot read batch manifest: %s"), *FilePath));
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		LogError(FString::Printf(TEXT("B002: Invalid batch JSON: %s"), *FilePath));
		return false;
	}

	double SchemaVersion = 0.0;
	if (!Root->TryGetNumberField(TEXT("schemaVersion"), SchemaVersion) || SchemaVersion != 1.0)
	{
		LogError(TEXT("B003: Batch manifest requires \"schemaVersion\": 1"));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Files = nullptr;
	if (!Root->TryGetArrayField(TEXT("files"), Files) || !Files || Files->Num() == 0)
	{
		LogError(TEXT("B004: Batch manifest requires a non-empty \"files\" array"));
		return false;
	}

	bool bStopOnError = false;
	bool bValidateOnly = false;
	Root->TryGetBoolField(TEXT("stopOnError"), bStopOnError);
	Root->TryGetBoolField(TEXT("validateOnly"), bValidateOnly);

	const FString ManifestDir = FPaths::GetPath(FPaths::ConvertRelativePathToFull(FilePath));
	int32 Succeeded = 0;
	int32 Failed = 0;
	for (const TSharedPtr<FJsonValue>& FileValue : *Files)
	{
		FString SpecPath;
		if (!FileValue.IsValid() || !FileValue->TryGetString(SpecPath) || SpecPath.IsEmpty())
		{
			UE_LOG(LogNiagaraJsonSpike, Error,
				TEXT("B005: Every batch \"files\" entry must be a non-empty string"));
			Failed++;
			if (bStopOnError) { break; }
			continue;
		}

		if (FPaths::IsRelative(SpecPath))
		{
			SpecPath = FPaths::Combine(ManifestDir, SpecPath);
		}
		SpecPath = FPaths::ConvertRelativePathToFull(SpecPath);
		FPaths::NormalizeFilename(SpecPath);

		const bool bSucceeded = bValidateOnly
			? ValidateFile(SpecPath)
			: ImportFromFile(SpecPath) != nullptr;
		if (bSucceeded)
		{
			Succeeded++;
		}
		else
		{
			Failed++;
			if (bStopOnError) { break; }
		}
	}

	const FString Summary = FString::Printf(
		TEXT("Batch %s complete | succeeded: %d, failed: %d, manifest: %s"),
		bValidateOnly ? TEXT("validation") : TEXT("import"),
		Succeeded, Failed, *FilePath);
	UE_LOG(LogNiagaraJsonSpike, Display, TEXT("%s"), *Summary);
	Notify(Summary, Failed == 0);
	return Failed == 0;
}

UNiagaraSystem* FNiagaraJsonSpikeImporter::GeneratePreset(
    const FString& TemplatePath, const FString& OutputPath,
    const FLinearColor& Color, float Energy, float Speed, int32 SparkCount)
{
    // Keep the importer as the single write path. The preset dialog simply
    // produces this short-lived strict spec in Saved/, rather than requiring
    // artists to author or maintain a JSON file.
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("schemaVersion"), 1.0);
    Root->SetStringField(TEXT("template"), TemplatePath);
    Root->SetStringField(TEXT("outputPath"), OutputPath);
    Root->SetBoolField(TEXT("strict"), true);

    TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    auto AddFloat = [&Params](const TCHAR* Name, float Value)
    {
        TSharedPtr<FJsonObject> Param = MakeShared<FJsonObject>();
        Param->SetStringField(TEXT("type"), TEXT("float"));
        Param->SetNumberField(TEXT("value"), Value);
        Params->SetObjectField(Name, Param);
    };
    auto AddInt = [&Params](const TCHAR* Name, int32 Value)
    {
        TSharedPtr<FJsonObject> Param = MakeShared<FJsonObject>();
        Param->SetStringField(TEXT("type"), TEXT("int"));
        Param->SetNumberField(TEXT("value"), Value);
        Params->SetObjectField(Name, Param);
    };
    TArray<TSharedPtr<FJsonValue>> ColorValues;
    ColorValues.Add(MakeShared<FJsonValueNumber>(Color.R));
    ColorValues.Add(MakeShared<FJsonValueNumber>(Color.G));
    ColorValues.Add(MakeShared<FJsonValueNumber>(Color.B));
    ColorValues.Add(MakeShared<FJsonValueNumber>(Color.A));
    TSharedPtr<FJsonObject> ColorParam = MakeShared<FJsonObject>();
    ColorParam->SetStringField(TEXT("type"), TEXT("color"));
    ColorParam->SetArrayField(TEXT("value"), ColorValues);
    Params->SetObjectField(TEXT("User.Color"), ColorParam);
    AddFloat(TEXT("User.Energy"), Energy);
    AddFloat(TEXT("User.Speed"), Speed);
    AddInt(TEXT("User.SparkCount"), FMath::Max(0, SparkCount));
    Root->SetObjectField(TEXT("params"), Params);

    const FString PresetDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VFXPresets"));
    IFileManager::Get().MakeDirectory(*PresetDir, true);
    const FString PresetFile = FPaths::Combine(PresetDir, FString::Printf(
        TEXT("Preset_%s.json"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    FString Serialized;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
    if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer)
        || !FFileHelper::SaveStringToFile(Serialized, *PresetFile))
    {
        LogError(FString::Printf(TEXT("PR001: Could not create internal preset spec: %s"), *PresetFile));
        return nullptr;
    }
    UNiagaraSystem* GeneratedSystem = ImportFromFile(PresetFile);
    if (GeneratedSystem && GEditor && !IsRunningCommandlet() && !FApp::IsUnattended())
    {
        // Opens Niagara's built-in preview scene immediately after generation.
        // The Content Browser then renders its normal asset thumbnail from the
        // same system, so the artist can inspect it before binding gameplay.
        if (UAssetEditorSubsystem* AssetEditor =
            GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
        {
            AssetEditor->OpenEditorForAsset(GeneratedSystem);
        }
    }

    FString PreviewPath;
    if (GeneratedSystem && !FApp::IsUnattended())
    {
        ExportPreviewPng(GeneratedSystem, PreviewPath);
    }

    return GeneratedSystem;
}

bool FNiagaraJsonSpikeImporter::InstallMeleeTemplateLibrary()
{
    struct FTemplateInstallEntry
    {
        const TCHAR* Source;
        const TCHAR* Destination;
        const TCHAR* Label;
    };
    const FTemplateInstallEntry Entries[] =
    {
        { TEXT("/Game/VFX/Lacern/NS_Lacern_Slash"),
          TEXT("/Game/VFX/Templates/NS_Base_SlashTrail"), TEXT("Slash Trail") },
        { TEXT("/Game/VFX/Lacern/NS_Lacern_Hit_Impact_01"),
          TEXT("/Game/VFX/Templates/NS_Base_HitBurst"), TEXT("Hit Burst") },
        { TEXT("/Game/VFX/Lacern/NS_Lacern_Extend"),
          TEXT("/Game/VFX/Templates/NS_Base_LacernExtend"), TEXT("Lacern Extend") },
    };

    bool bSucceeded = true;
    int32 InstalledCount = 0;
    for (const FTemplateInstallEntry& Entry : Entries)
    {
        if (!UEditorAssetLibrary::DoesAssetExist(Entry.Source))
        {
            LogError(FString::Printf(TEXT("MT001: Missing curated melee source for %s: %s"),
                Entry.Label, Entry.Source));
            bSucceeded = false;
            continue;
        }

        UNiagaraSystem* Template = nullptr;
        if (UEditorAssetLibrary::DoesAssetExist(Entry.Destination))
        {
            Template = Cast<UNiagaraSystem>(UEditorAssetLibrary::LoadAsset(Entry.Destination));
            if (!Template)
            {
                LogError(FString::Printf(TEXT("MT002: Existing melee template has wrong type: %s"),
                    Entry.Destination));
                bSucceeded = false;
                continue;
            }
        }
        else
        {
            Template = Cast<UNiagaraSystem>(UEditorAssetLibrary::DuplicateAsset(
                Entry.Source, Entry.Destination));
            if (!Template)
            {
                LogError(FString::Printf(TEXT("MT003: Failed to install melee template %s -> %s"),
                    Entry.Source, Entry.Destination));
                bSucceeded = false;
                continue;
            }
            if (!UEditorAssetLibrary::SaveLoadedAsset(Template, false))
            {
                LogError(FString::Printf(TEXT("MT004: Failed to save installed melee template: %s"),
                    Entry.Destination));
                bSucceeded = false;
                continue;
            }
            ++InstalledCount;
        }

        TArray<FNiagaraVariable> Parameters;
        Template->GetExposedParameters().GetParameters(Parameters);
        if (FString(Entry.Destination) == TEXT("/Game/VFX/Templates/NS_Base_SlashTrail"))
        {
            if (!EnsureSlashTrailUserParameterContract(Template)
                || !UEditorAssetLibrary::SaveLoadedAsset(Template, false))
            {
                LogError(FString::Printf(TEXT("MT007: Failed to save slash trail parameter contract: %s"),
                    Entry.Destination));
                bSucceeded = false;
                continue;
            }
        }
        else if (Parameters.Num() == 0)
        {
            LogWarning(FString::Printf(
                TEXT("MT101: %s installed as a fixed visual template (no exposed User parameters). "
                     "It can be generated and bound now; expose Color/Width/Lifetime in Niagara to enable preset tuning."),
                Entry.Label));
        }
    }

    Notify(bSucceeded
        ? FString::Printf(TEXT("Melee VFX Template Library ready (%d new asset(s) installed)."), InstalledCount)
        : TEXT("Melee VFX Template Library installation completed with errors."), bSucceeded);
    return bSucceeded;
}

UNiagaraSystem* FNiagaraJsonSpikeImporter::GenerateLacernSlashVariant(
    const FString& OutputPath)
{
    constexpr const TCHAR* SlashTemplatePath = TEXT("/Game/VFX/Templates/NS_Base_SlashTrail");
    if (!InstallMeleeTemplateLibrary() || !ValidateOutputTarget(OutputPath))
    {
        return nullptr;
    }

    bool bIsNewAsset = false;
    UNiagaraSystem* Generated = ResolveOutputAsset(SlashTemplatePath, OutputPath, bIsNewAsset);
    if (!Generated)
    {
        return nullptr;
    }

    if (!EnsureSlashTrailUserParameterContract(Generated))
    {
        return nullptr;
    }

    if (!UEditorAssetLibrary::SaveLoadedAsset(Generated, false))
    {
        LogError(FString::Printf(TEXT("MT005: Failed to save Lacern slash variant: %s"), *OutputPath));
        return nullptr;
    }
    if (GEditor && !IsRunningCommandlet() && !FApp::IsUnattended())
    {
        if (UAssetEditorSubsystem* AssetEditor = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
        {
            AssetEditor->OpenEditorForAsset(Generated);
        }
    }
    if (!FApp::IsUnattended())
    {
        FString PreviewPath;
        ExportPreviewPng(Generated, PreviewPath);
    }
    Notify(FString::Printf(TEXT("Lacern slash variant ready: %s"), *OutputPath), true);
    return Generated;
}

bool FNiagaraJsonSpikeImporter::ExportPreviewPng(
    UNiagaraSystem* System, FString& OutFilePath)
{
    OutFilePath.Reset();
    if (!IsValid(System) || !GEditor)
    {
        return false;
    }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        LogWarning(TEXT("PR002: No editor world is available for VFX preview rendering."));
        return false;
    }

    constexpr int32 PreviewSize = 512;
    const FVector PreviewOrigin(0.0f, 0.0f, 200.0f);
    const FVector CameraLocation = PreviewOrigin + FVector(-350.0f, 0.0f, 80.0f);
    FActorSpawnParameters SpawnParams;
    SpawnParams.ObjectFlags = RF_Transient;
    SpawnParams.bTemporaryEditorActor = true;

    ANiagaraActor* PreviewActor = World->SpawnActor<ANiagaraActor>(
        PreviewOrigin, FRotator::ZeroRotator, SpawnParams);
    ASceneCapture2D* CaptureActor = World->SpawnActor<ASceneCapture2D>(
        CameraLocation, (PreviewOrigin - CameraLocation).Rotation(), SpawnParams);
    UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(
        GetTransientPackage(), NAME_None, RF_Transient);
    if (!PreviewActor || !CaptureActor || !RenderTarget)
    {
        if (CaptureActor) { CaptureActor->Destroy(); }
        if (PreviewActor) { PreviewActor->Destroy(); }
        LogWarning(TEXT("PR003: Could not allocate temporary VFX preview actors."));
        return false;
    }

    RenderTarget->InitAutoFormat(PreviewSize, PreviewSize);
    RenderTarget->ClearColor = FLinearColor::Transparent;
    RenderTarget->UpdateResourceImmediate(true);
    UNiagaraComponent* NiagaraComponent = PreviewActor->GetNiagaraComponent();
    NiagaraComponent->SetForceSolo(true);
    NiagaraComponent->SetAsset(System);
    NiagaraComponent->ReinitializeSystem();
    NiagaraComponent->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
    // Capture near the beginning of the effect.  Impact systems are often
    // shorter than 0.2 seconds, so a long warm-up would export a blank frame.
    NiagaraComponent->AdvanceSimulation(2, 1.0f / 60.0f);

    USceneCaptureComponent2D* Capture = CaptureActor->GetCaptureComponent2D();
    Capture->TextureTarget = RenderTarget;
    Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    Capture->bCaptureEveryFrame = false;
    Capture->bCaptureOnMovement = false;
    Capture->CaptureScene();
    // CaptureScene submits work asynchronously.  The readback below must wait
    // for it, especially when invoked by the headless preview command.
    FlushRenderingCommands();

    TArray<FColor> Pixels;
    FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
    const bool bReadSucceeded = Resource && Resource->ReadPixels(Pixels);
    CaptureActor->Destroy();
    PreviewActor->Destroy();
    if (!bReadSucceeded || Pixels.Num() != PreviewSize * PreviewSize)
    {
        LogWarning(FString::Printf(TEXT("PR004: Failed to read preview pixels for %s"), *System->GetPathName()));
        return false;
    }

    TArray64<uint8> PngData;
    FImageUtils::PNGCompressImageArray(PreviewSize, PreviewSize, Pixels, PngData);
    const FString PreviewDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VFXPreviews"));
    IFileManager::Get().MakeDirectory(*PreviewDir, true);
    OutFilePath = FPaths::Combine(PreviewDir, System->GetName() + TEXT(".png"));
    if (!FFileHelper::SaveArrayToFile(PngData, *OutFilePath))
    {
        LogWarning(FString::Printf(TEXT("PR005: Failed to save preview PNG: %s"), *OutFilePath));
        OutFilePath.Reset();
        return false;
    }
    UE_LOG(LogNiagaraJsonSpike, Display, TEXT("VFX preview exported: %s"), *OutFilePath);
    return true;
}

bool FNiagaraJsonSpikeImporter::BindPresetToProjectile(
    const FString& BlueprintPath, UNiagaraSystem* ImpactSystem,
    const TArray<FWTBRNiagaraAssetParameter>& AssetParameters)
{
    UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
    AWTBRProjectileBase* ProjectileCDO = Blueprint && Blueprint->GeneratedClass
        ? Cast<AWTBRProjectileBase>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (!Blueprint || !ProjectileCDO || !IsValid(ImpactSystem))
    {
        LogError(FString::Printf(TEXT("PR006: Invalid projectile Blueprint or generated Niagara asset: %s"), *BlueprintPath));
        return false;
    }
    ProjectileCDO->Modify();
    ProjectileCDO->DefaultImpactEffect = ImpactSystem;
    ProjectileCDO->ImpactAssetParameters = AssetParameters;
    ProjectileCDO->bUseBuiltInImpactVFX = true;
    Blueprint->Modify();
    Blueprint->MarkPackageDirty();
    const bool bSaved = UEditorAssetLibrary::SaveLoadedAsset(Blueprint, false);
    if (!bSaved)
    {
        LogError(FString::Printf(TEXT("PR007: Failed to save projectile Blueprint: %s"), *BlueprintPath));
    }
    return bSaved;
}

bool FNiagaraJsonSpikeImporter::AutoBindSniperFromFile(const FString& FilePath)
{
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *FilePath))
	{
		LogError(FString::Printf(TEXT("A001: Cannot read Sniper binding manifest: %s"), *FilePath));
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		LogError(FString::Printf(TEXT("A002: Invalid Sniper binding JSON: %s"), *FilePath));
		return false;
	}

	double SchemaVersion = 0.0;
	if (!Root->TryGetNumberField(TEXT("schemaVersion"), SchemaVersion) || SchemaVersion != 1.0)
	{
		LogError(TEXT("A003: Sniper binding manifest requires \"schemaVersion\": 1"));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Bindings = nullptr;
	if (!Root->TryGetArrayField(TEXT("bindings"), Bindings) || !Bindings || Bindings->Num() == 0)
	{
		LogError(TEXT("A004: Sniper binding manifest requires a non-empty \"bindings\" array"));
		return false;
	}

	int32 Bound = 0;
	int32 Failed = 0;
	for (const TSharedPtr<FJsonValue>& BindingValue : *Bindings)
	{
		const TSharedPtr<FJsonObject>* Binding = nullptr;
		if (!BindingValue.IsValid() || !BindingValue->TryGetObject(Binding) || !Binding->IsValid())
		{
			UE_LOG(LogNiagaraJsonSpike, Error, TEXT("A005: Every binding entry must be an object"));
			Failed++;
			continue;
		}

		FString SniperName;
		FString DataAssetPath;
		FString ImpactPath;
		FString TrailPath;
		if (!(*Binding)->TryGetStringField(TEXT("sniper"), SniperName)
			|| !(*Binding)->TryGetStringField(TEXT("dataAsset"), DataAssetPath)
			|| !(*Binding)->TryGetStringField(TEXT("impact"), ImpactPath))
		{
			UE_LOG(LogNiagaraJsonSpike, Error,
				TEXT("A006: Binding requires string fields \"sniper\", \"dataAsset\", and \"impact\""));
			Failed++;
			continue;
		}
		(*Binding)->TryGetStringField(TEXT("trail"), TrailPath);

		UWTBRTriggerDataAsset* DataAsset = Cast<UWTBRTriggerDataAsset>(
			UEditorAssetLibrary::LoadAsset(DataAssetPath));
		UNiagaraSystem* Impact = Cast<UNiagaraSystem>(UEditorAssetLibrary::LoadAsset(ImpactPath));
		UNiagaraSystem* Trail = TrailPath.IsEmpty()
			? nullptr : Cast<UNiagaraSystem>(UEditorAssetLibrary::LoadAsset(TrailPath));
		if (!DataAsset || !Impact || (!TrailPath.IsEmpty() && !Trail))
		{
			UE_LOG(LogNiagaraJsonSpike, Error, TEXT("A007: Invalid DataAsset or Niagara asset in binding for %s"), *SniperName);
			Failed++;
			continue;
		}

		FWTBRProjectileVFXConfig* VFXConfig = nullptr;
		const FString SniperLower = SniperName.ToLower();
		if (SniperLower == TEXT("telorn"))
		{
			VFXConfig = &DataAsset->TelornParams.VFX;
		}
		else if (SniperLower == TEXT("piercex"))
		{
			VFXConfig = &DataAsset->PiercexParams.VFX;
		}
		else if (SniperLower == TEXT("fulgris"))
		{
			VFXConfig = &DataAsset->FulgrisParams.VFX;
		}
		else
		{
			UE_LOG(LogNiagaraJsonSpike, Error, TEXT("A008: Unknown Sniper \"%s\" (allowed: Telorn, Piercex, Fulgris)"), *SniperName);
			Failed++;
			continue;
		}

		float MaxDistance = 20000.0f;
		double MaxDistanceJson = 0.0;
		if ((*Binding)->TryGetNumberField(TEXT("maxImpactVFXDistance"), MaxDistanceJson)
			&& MaxDistanceJson >= 0.0)
		{
			MaxDistance = static_cast<float>(MaxDistanceJson);
		}
		if (MaxDistance <= 0.0f)
		{
			LogWarning(FString::Printf(
				TEXT("P103: %s impact VFX has distance culling disabled; confirm this is intentional."),
				*SniperName));
		}
		else if (MaxDistance > 50000.0f)
		{
			LogWarning(FString::Printf(
				TEXT("P104: %s impact VFX cull distance is %.0f cm; review Niagara scalability/LOD."),
				*SniperName, MaxDistance));
		}

		TArray<FWTBRSurfaceImpactVFX> SurfaceOverrides;
		const TArray<TSharedPtr<FJsonValue>>* Overrides = nullptr;
		if ((*Binding)->TryGetArrayField(TEXT("surfaceOverrides"), Overrides) && Overrides)
		{
			bool bOverridesValid = true;
			for (const TSharedPtr<FJsonValue>& OverrideValue : *Overrides)
			{
				const TSharedPtr<FJsonObject>* Override = nullptr;
				double SurfaceNumber = 0.0;
				FString EffectPath;
				if (!OverrideValue.IsValid() || !OverrideValue->TryGetObject(Override)
					|| !Override->IsValid()
					|| !(*Override)->TryGetNumberField(TEXT("surfaceType"), SurfaceNumber)
					|| !(*Override)->TryGetStringField(TEXT("effect"), EffectPath)
					|| SurfaceNumber < 0.0 || SurfaceNumber >= static_cast<double>(SurfaceType_Max))
				{
					bOverridesValid = false;
					break;
				}

				UNiagaraSystem* SurfaceEffect = Cast<UNiagaraSystem>(
					UEditorAssetLibrary::LoadAsset(EffectPath));
				if (!SurfaceEffect)
				{
					bOverridesValid = false;
					break;
				}

				FWTBRSurfaceImpactVFX& NewOverride = SurfaceOverrides.AddDefaulted_GetRef();
				NewOverride.SurfaceType = static_cast<EPhysicalSurface>(static_cast<uint8>(SurfaceNumber));
				NewOverride.Effect = SurfaceEffect;
			}

			if (!bOverridesValid)
			{
				UE_LOG(LogNiagaraJsonSpike, Error, TEXT("A009: Invalid surfaceOverrides for %s"), *SniperName);
				Failed++;
				continue;
			}
		}

		// Asset parameters are deliberately generic: Niagara templates own the
		// exact compatible type, while the manifest supplies the selected
		// Material/Texture/Curve/Mesh/Ribbon-profile content asset.
		TArray<FWTBRNiagaraAssetParameter> AssetOverrides;
		const TArray<TSharedPtr<FJsonValue>>* AssetOverrideValues = nullptr;
		if ((*Binding)->TryGetArrayField(TEXT("assetOverrides"), AssetOverrideValues)
			&& AssetOverrideValues)
		{
			bool bAssetOverridesValid = true;
			for (const TSharedPtr<FJsonValue>& OverrideValue : *AssetOverrideValues)
			{
				const TSharedPtr<FJsonObject>* Override = nullptr;
				FString ParameterName;
				FString AssetPath;
				if (!OverrideValue.IsValid() || !OverrideValue->TryGetObject(Override)
					|| !Override->IsValid()
					|| !(*Override)->TryGetStringField(TEXT("parameter"), ParameterName)
					|| !(*Override)->TryGetStringField(TEXT("asset"), AssetPath)
					|| ParameterName.IsEmpty())
				{
					bAssetOverridesValid = false;
					break;
				}

				UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
				if (!IsValid(Asset))
				{
					bAssetOverridesValid = false;
					break;
				}

				FWTBRNiagaraAssetParameter& NewOverride = AssetOverrides.AddDefaulted_GetRef();
				NewOverride.ParameterName = FName(*ParameterName);
				NewOverride.Asset = Asset;
			}

			if (!bAssetOverridesValid)
			{
				UE_LOG(LogNiagaraJsonSpike, Error,
					TEXT("A010: Invalid assetOverrides for %s (each entry needs parameter + valid asset)"),
					*SniperName);
				Failed++;
				continue;
			}
		}

		DataAsset->Modify();
		VFXConfig->TrailEffect = Trail;
		VFXConfig->DefaultImpactEffect = Impact;
		VFXConfig->SurfaceImpactOverrides = MoveTemp(SurfaceOverrides);
		VFXConfig->ImpactAssetParameters = MoveTemp(AssetOverrides);
		VFXConfig->bUseBuiltInImpactVFX = true;
		VFXConfig->MaxImpactVFXDistance = MaxDistance;
		DataAsset->MarkPackageDirty();
		if (!UEditorAssetLibrary::SaveLoadedAsset(DataAsset, false))
		{
			UE_LOG(LogNiagaraJsonSpike, Error, TEXT("A011: Failed to save DataAsset: %s"), *DataAssetPath);
			Failed++;
			continue;
		}

		UE_LOG(LogNiagaraJsonSpike, Display,
			TEXT("Auto-bound %s | DataAsset=%s | Trail=%s | Impact=%s | SurfaceOverrides=%d | AssetOverrides=%d | MaxDistance=%.0f"),
			*SniperName, *DataAssetPath, *GetNameSafe(Trail), *GetNameSafe(Impact),
			VFXConfig->SurfaceImpactOverrides.Num(), VFXConfig->ImpactAssetParameters.Num(),
			VFXConfig->MaxImpactVFXDistance);
		AuditSystemPerformance(ImpactPath);
		if (!TrailPath.IsEmpty())
		{
			AuditSystemPerformance(TrailPath);
		}
		Bound++;
	}

	const FString Summary = FString::Printf(
		TEXT("Sniper VFX auto-bind complete | bound: %d, failed: %d, manifest: %s"),
		Bound, Failed, *FilePath);
	UE_LOG(LogNiagaraJsonSpike, Display, TEXT("%s"), *Summary);
	Notify(Summary, Failed == 0);
	return Failed == 0;
}

bool FNiagaraJsonSpikeImporter::AutoBindProjectileBlueprintsFromFile(const FString& FilePath)
{
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *FilePath))
	{
		LogError(FString::Printf(TEXT("PB001: Cannot read projectile binding manifest: %s"), *FilePath));
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		LogError(FString::Printf(TEXT("PB002: Invalid projectile binding JSON: %s"), *FilePath));
		return false;
	}

	double SchemaVersion = 0.0;
	const TArray<TSharedPtr<FJsonValue>>* Bindings = nullptr;
	if (!Root->TryGetNumberField(TEXT("schemaVersion"), SchemaVersion) || SchemaVersion != 1.0
		|| !Root->TryGetArrayField(TEXT("bindings"), Bindings) || !Bindings || Bindings->Num() == 0)
	{
		LogError(TEXT("PB003: Manifest requires schemaVersion 1 and a non-empty bindings array"));
		return false;
	}

	int32 Bound = 0;
	int32 Failed = 0;
	for (const TSharedPtr<FJsonValue>& BindingValue : *Bindings)
	{
		const TSharedPtr<FJsonObject>* Binding = nullptr;
		FString BlueprintPath;
		FString ImpactPath;
		FString TrailPath;
		if (!BindingValue.IsValid() || !BindingValue->TryGetObject(Binding) || !Binding->IsValid()
			|| !(*Binding)->TryGetStringField(TEXT("projectileBlueprint"), BlueprintPath)
			|| !(*Binding)->TryGetStringField(TEXT("impact"), ImpactPath))
		{
			UE_LOG(LogNiagaraJsonSpike, Error,
				TEXT("PB004: Each binding requires projectileBlueprint and impact paths"));
			Failed++;
			continue;
		}
		(*Binding)->TryGetStringField(TEXT("trail"), TrailPath);

		UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
		UNiagaraSystem* Impact = Cast<UNiagaraSystem>(UEditorAssetLibrary::LoadAsset(ImpactPath));
		UNiagaraSystem* Trail = TrailPath.IsEmpty()
			? nullptr : Cast<UNiagaraSystem>(UEditorAssetLibrary::LoadAsset(TrailPath));
		AWTBRProjectileBase* ProjectileCDO = Blueprint && Blueprint->GeneratedClass
			? Cast<AWTBRProjectileBase>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
		if (!Blueprint || !ProjectileCDO || !Impact || (!TrailPath.IsEmpty() && !Trail))
		{
			UE_LOG(LogNiagaraJsonSpike, Error,
				TEXT("PB005: Blueprint must derive from AWTBRProjectileBase and all Niagara paths must be valid: %s"),
				*BlueprintPath);
			Failed++;
			continue;
		}

		float MaxDistance = 20000.0f;
		double MaxDistanceJson = 0.0;
		if ((*Binding)->TryGetNumberField(TEXT("maxImpactVFXDistance"), MaxDistanceJson)
			&& MaxDistanceJson >= 0.0)
		{
			MaxDistance = static_cast<float>(MaxDistanceJson);
		}

		TArray<FWTBRSurfaceImpactVFX> SurfaceOverrides;
		const TArray<TSharedPtr<FJsonValue>>* Overrides = nullptr;
		if ((*Binding)->TryGetArrayField(TEXT("surfaceOverrides"), Overrides) && Overrides)
		{
			bool bValid = true;
			for (const TSharedPtr<FJsonValue>& OverrideValue : *Overrides)
			{
				const TSharedPtr<FJsonObject>* Override = nullptr;
				double SurfaceNumber = 0.0;
				FString EffectPath;
				if (!OverrideValue.IsValid() || !OverrideValue->TryGetObject(Override)
					|| !Override->IsValid()
					|| !(*Override)->TryGetNumberField(TEXT("surfaceType"), SurfaceNumber)
					|| !(*Override)->TryGetStringField(TEXT("effect"), EffectPath)
					|| SurfaceNumber < 0.0 || SurfaceNumber >= static_cast<double>(SurfaceType_Max))
				{
					bValid = false;
					break;
				}
				UNiagaraSystem* Effect = Cast<UNiagaraSystem>(UEditorAssetLibrary::LoadAsset(EffectPath));
				if (!Effect)
				{
					bValid = false;
					break;
				}
				FWTBRSurfaceImpactVFX& NewOverride = SurfaceOverrides.AddDefaulted_GetRef();
				NewOverride.SurfaceType = static_cast<EPhysicalSurface>(static_cast<uint8>(SurfaceNumber));
				NewOverride.Effect = Effect;
			}
			if (!bValid)
			{
				UE_LOG(LogNiagaraJsonSpike, Error, TEXT("PB006: Invalid surfaceOverrides for %s"), *BlueprintPath);
				Failed++;
				continue;
			}
		}

		TArray<FWTBRNiagaraAssetParameter> AssetOverrides;
		const TArray<TSharedPtr<FJsonValue>>* AssetOverrideValues = nullptr;
		if ((*Binding)->TryGetArrayField(TEXT("assetOverrides"), AssetOverrideValues)
			&& AssetOverrideValues)
		{
			bool bValid = true;
			for (const TSharedPtr<FJsonValue>& OverrideValue : *AssetOverrideValues)
			{
				const TSharedPtr<FJsonObject>* Override = nullptr;
				FString ParameterName;
				FString AssetPath;
				if (!OverrideValue.IsValid() || !OverrideValue->TryGetObject(Override)
					|| !Override->IsValid()
					|| !(*Override)->TryGetStringField(TEXT("parameter"), ParameterName)
					|| !(*Override)->TryGetStringField(TEXT("asset"), AssetPath)
					|| ParameterName.IsEmpty())
				{
					bValid = false;
					break;
				}
				UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
				if (!IsValid(Asset))
				{
					bValid = false;
					break;
				}
				FWTBRNiagaraAssetParameter& NewOverride = AssetOverrides.AddDefaulted_GetRef();
				NewOverride.ParameterName = FName(*ParameterName);
				NewOverride.Asset = Asset;
			}
			if (!bValid)
			{
				UE_LOG(LogNiagaraJsonSpike, Error, TEXT("PB007: Invalid assetOverrides for %s"), *BlueprintPath);
				Failed++;
				continue;
			}
		}

		ProjectileCDO->Modify();
		ProjectileCDO->TrailEffect = Trail;
		ProjectileCDO->DefaultImpactEffect = Impact;
		ProjectileCDO->SurfaceImpactOverrides = MoveTemp(SurfaceOverrides);
		ProjectileCDO->ImpactAssetParameters = MoveTemp(AssetOverrides);
		ProjectileCDO->bUseBuiltInImpactVFX = true;
		ProjectileCDO->MaxImpactVFXDistance = MaxDistance;
		Blueprint->Modify();
		Blueprint->MarkPackageDirty();
		if (!UEditorAssetLibrary::SaveLoadedAsset(Blueprint, false))
		{
			UE_LOG(LogNiagaraJsonSpike, Error, TEXT("PB008: Failed to save Blueprint: %s"), *BlueprintPath);
			Failed++;
			continue;
		}

		UE_LOG(LogNiagaraJsonSpike, Display,
			TEXT("Auto-bound projectile Blueprint | Blueprint=%s | Trail=%s | Impact=%s | SurfaceOverrides=%d | AssetOverrides=%d"),
			*BlueprintPath, *GetNameSafe(Trail), *GetNameSafe(Impact),
			ProjectileCDO->SurfaceImpactOverrides.Num(), ProjectileCDO->ImpactAssetParameters.Num());
		AuditSystemPerformance(ImpactPath);
		if (!TrailPath.IsEmpty())
		{
			AuditSystemPerformance(TrailPath);
		}
		Bound++;
	}

	const FString Summary = FString::Printf(
		TEXT("Projectile Blueprint VFX auto-bind complete | bound: %d, failed: %d, manifest: %s"),
		Bound, Failed, *FilePath);
	UE_LOG(LogNiagaraJsonSpike, Display, TEXT("%s"), *Summary);
	Notify(Summary, Failed == 0);
	return Failed == 0;
}

bool FNiagaraJsonSpikeImporter::AuditSystemPerformance(const FString& AssetPath)
{
	UNiagaraSystem* System = Cast<UNiagaraSystem>(UEditorAssetLibrary::LoadAsset(AssetPath));
	if (!System)
	{
		LogError(FString::Printf(TEXT("P001: NiagaraSystem not found: %s"), *AssetPath));
		return false;
	}

	int32 EnabledEmitters = 0;
	int32 CpuEmitters = 0;
	int32 GpuEmitters = 0;
	int32 RendererCount = 0;
	int32 PreAllocationTotal = 0;

	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		const FVersionedNiagaraEmitterData* EmitterData = Handle.GetInstance().GetEmitterData();
		if (!EmitterData || !Handle.GetIsEnabled())
		{
			continue;
		}

		EnabledEmitters++;
		const bool bGpu = EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim;
		if (bGpu)
		{
			GpuEmitters++;
		}
		else
		{
			CpuEmitters++;
		}
		const int32 EmitterRenderers = EmitterData->GetRenderers().Num();
		RendererCount += EmitterRenderers;
		PreAllocationTotal += EmitterData->PreAllocationCount;

		UE_LOG(LogNiagaraJsonSpike, Display,
			TEXT("VFX audit emitter | %s | Sim=%s | Renderers=%d | PreAllocation=%d"),
			*Handle.GetUniqueInstanceName(),
			bGpu ? TEXT("GPU") : TEXT("CPU"),
			EmitterRenderers, EmitterData->PreAllocationCount);
	}

	const FString Summary = FString::Printf(
		TEXT("VFX audit | %s | Emitters=%d (CPU=%d GPU=%d) | Renderers=%d | PreAllocation=%d"),
		*AssetPath, EnabledEmitters, CpuEmitters, GpuEmitters, RendererCount,
		PreAllocationTotal);
	UE_LOG(LogNiagaraJsonSpike, Display, TEXT("%s"), *Summary);

	if (EnabledEmitters > 4 || RendererCount > 6 || PreAllocationTotal > 2000)
	{
		LogWarning(TEXT("P101: High static complexity - review Niagara scalability, cull distance, and burst counts before shipping."));
	}
	if (GpuEmitters > 0)
	{
		LogWarning(TEXT("P102: GPU emitter detected - confirm it is appropriate for this gameplay effect and target hardware."));
	}
	return true;
}

bool FNiagaraJsonSpikeImporter::DumpUserParams(const FString& AssetPath)
{
	if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
	{
		LogError(FString::Printf(TEXT("DumpUserParams: asset not found: %s"), *AssetPath));
		return false;
	}

	UNiagaraSystem* System = Cast<UNiagaraSystem>(UEditorAssetLibrary::LoadAsset(AssetPath));
	if (!System)
	{
		LogError(FString::Printf(TEXT("DumpUserParams: asset is not a NiagaraSystem: %s"), *AssetPath));
		return false;
	}

	FNiagaraUserRedirectionParameterStore& Store = System->GetExposedParameters();
	TArray<FNiagaraVariable> Params;
	Store.GetParameters(Params);
	Params.Sort([](const FNiagaraVariable& A, const FNiagaraVariable& B)
	{
		return A.GetName().LexicalLess(B.GetName());
	});

	UE_LOG(LogNiagaraJsonSpike, Display, TEXT("UserParams of %s (%d exposed):"),
		*AssetPath, Params.Num());

	for (const FNiagaraVariable& Param : Params)
	{
		const FNiagaraTypeDefinition& Type = Param.GetType();
		const uint8* Data = Store.GetParameterData(Param);
		FString ValueText = TEXT("(no data)");

		if (Data)
		{
			if (Type == FNiagaraTypeDefinition::GetFloatDef())
			{
				ValueText = FString::SanitizeFloat(*reinterpret_cast<const float*>(Data));
			}
			else if (Type == FNiagaraTypeDefinition::GetIntDef())
			{
				ValueText = FString::FromInt(*reinterpret_cast<const int32*>(Data));
			}
			else if (Type == FNiagaraTypeDefinition::GetBoolDef())
			{
				ValueText = reinterpret_cast<const FNiagaraBool*>(Data)->GetValue()
					? TEXT("true") : TEXT("false");
			}
			else if (Type == FNiagaraTypeDefinition::GetColorDef())
			{
				ValueText = reinterpret_cast<const FLinearColor*>(Data)->ToString();
			}
			else if (Type == FNiagaraTypeDefinition::GetVec3Def())
			{
				ValueText = reinterpret_cast<const FVector3f*>(Data)->ToString();
			}
			else if (Type == FNiagaraTypeDefinition::GetVec2Def())
			{
				ValueText = reinterpret_cast<const FVector2f*>(Data)->ToString();
			}
			else if (Type == FNiagaraTypeDefinition::GetVec4Def())
			{
				ValueText = reinterpret_cast<const FVector4f*>(Data)->ToString();
			}
			else
			{
				ValueText = TEXT("(type not supported by parameter dump)");
			}
		}

		UE_LOG(LogNiagaraJsonSpike, Display, TEXT("  %s : %s = %s"),
			*Param.GetName().ToString(), *Type.GetName(), *ValueText);
	}
	return true;
}

bool FNiagaraJsonSpikeImporter::ApplyParam(UNiagaraSystem* System, const FString& ParamName,
                                           const TSharedPtr<FJsonObject>& ParamObj,
                                           bool bAddMissing, FImportStats& Stats)
{
	FString TypeName;
	if (!ParamObj->TryGetStringField(TEXT("type"), TypeName))
	{
		LogWarning(FString::Printf(TEXT("W105: Param \"%s\": missing \"type\" field — skipped"),
			*ParamName), &Stats);
		return false;
	}

	const FNiagaraTypeDefinition* TypeDef = ResolveTypeDef(TypeName);
	if (!TypeDef)
	{
		LogWarning(FString::Printf(
			TEXT("W106: Param \"%s\": unsupported type \"%s\" (allowed: float, int, bool, color, vector2, vector3, vector4) — skipped"),
			*ParamName, *TypeName), &Stats);
		return false;
	}

	// Normalize to the "User." namespace the exposed store uses internally,
	// so both "Color" and "User.Color" JSON keys resolve to the same parameter.
	FNiagaraVariable Var(*TypeDef, FName(*ParamName));
	FNiagaraUserRedirectionParameterStore::MakeUserVariable(Var);

	FNiagaraUserRedirectionParameterStore& Store = System->GetExposedParameters();

	TArray<FNiagaraVariable> ExistingParams;
	Store.GetParameters(ExistingParams);

	const FNiagaraVariable* Existing = ExistingParams.FindByPredicate(
		[&Var](const FNiagaraVariable& P) { return P.GetName() == Var.GetName(); });

	if (Existing && Existing->GetType() != *TypeDef)
	{
		LogWarning(FString::Printf(
			TEXT("W102: Param \"%s\": type mismatch — template exposes \"%s\", JSON says \"%s\" — skipped"),
			*ParamName, *Existing->GetType().GetName(), *TypeName), &Stats);
		return false;
	}

	if (!Existing && !bAddMissing)
	{
		LogWarning(FString::Printf(
			TEXT("W101: Param \"%s\": not exposed on template (set \"addMissingParams\": true to add) — skipped"),
			*ParamName), &Stats);
		return false;
	}

	const bool bAdd = (Existing == nullptr);
	bool bSetOk = false;

	if (*TypeDef == FNiagaraTypeDefinition::GetFloatDef())
	{
		double Num = 0.0;
		if (!ParamObj->TryGetNumberField(TEXT("value"), Num))
		{
			LogWarning(FString::Printf(TEXT("W107: Param \"%s\": \"value\" must be a number — skipped"), *ParamName), &Stats);
			return false;
		}
		bSetOk = Store.SetParameterValue(static_cast<float>(Num), Var, bAdd);
	}
	else if (*TypeDef == FNiagaraTypeDefinition::GetIntDef())
	{
		double Num = 0.0;
		if (!ParamObj->TryGetNumberField(TEXT("value"), Num))
		{
			LogWarning(FString::Printf(TEXT("W107: Param \"%s\": \"value\" must be a number — skipped"), *ParamName), &Stats);
			return false;
		}
		bSetOk = Store.SetParameterValue(static_cast<int32>(Num), Var, bAdd);
	}
	else if (*TypeDef == FNiagaraTypeDefinition::GetBoolDef())
	{
		bool bValue = false;
		if (!ParamObj->TryGetBoolField(TEXT("value"), bValue))
		{
			LogWarning(FString::Printf(TEXT("W107: Param \"%s\": \"value\" must be true/false — skipped"), *ParamName), &Stats);
			return false;
		}
		FNiagaraBool NiagaraBool;
		NiagaraBool.SetValue(bValue);
		bSetOk = Store.SetParameterValue(NiagaraBool, Var, bAdd);
	}
	else if (*TypeDef == FNiagaraTypeDefinition::GetColorDef())
	{
		TArray<double> Values;
		if (!ReadNumberArray(ParamObj, 3, Values))
		{
			LogWarning(FString::Printf(
				TEXT("W107: Param \"%s\": \"value\" must be an array [r,g,b] or [r,g,b,a] (0-1 floats) — skipped"),
				*ParamName), &Stats);
			return false;
		}
		const FLinearColor Color(
			static_cast<float>(Values[0]),
			static_cast<float>(Values[1]),
			static_cast<float>(Values[2]),
			Values.Num() >= 4 ? static_cast<float>(Values[3]) : 1.0f);
		bSetOk = Store.SetParameterValue(Color, Var, bAdd);
	}
	else if (*TypeDef == FNiagaraTypeDefinition::GetVec3Def())
	{
		TArray<double> Values;
		if (!ReadNumberArray(ParamObj, 3, Values))
		{
			LogWarning(FString::Printf(
				TEXT("W107: Param \"%s\": \"value\" must be an array [x,y,z] — skipped"), *ParamName), &Stats);
			return false;
		}
		const FVector3f Vec(
			static_cast<float>(Values[0]),
			static_cast<float>(Values[1]),
			static_cast<float>(Values[2]));
		bSetOk = Store.SetParameterValue(Vec, Var, bAdd);
	}
	else if (*TypeDef == FNiagaraTypeDefinition::GetVec2Def())
	{
		TArray<double> Values;
		if (!ReadNumberArray(ParamObj, 2, Values))
		{
			LogWarning(FString::Printf(
				TEXT("W107: Param \"%s\": \"value\" must be an array [x,y] - skipped"), *ParamName), &Stats);
			return false;
		}
		const FVector2f Vec(static_cast<float>(Values[0]), static_cast<float>(Values[1]));
		bSetOk = Store.SetParameterValue(Vec, Var, bAdd);
	}
	else if (*TypeDef == FNiagaraTypeDefinition::GetVec4Def())
	{
		TArray<double> Values;
		if (!ReadNumberArray(ParamObj, 4, Values))
		{
			LogWarning(FString::Printf(
				TEXT("W107: Param \"%s\": \"value\" must be an array [x,y,z,w] - skipped"), *ParamName), &Stats);
			return false;
		}
		const FVector4f Vec(
			static_cast<float>(Values[0]), static_cast<float>(Values[1]),
			static_cast<float>(Values[2]), static_cast<float>(Values[3]));
		bSetOk = Store.SetParameterValue(Vec, Var, bAdd);
	}

	if (!bSetOk)
	{
		LogWarning(FString::Printf(TEXT("W103: Param \"%s\": SetParameterValue failed — skipped"), *ParamName), &Stats);
		return false;
	}

	if (bAdd)
	{
		Stats.ParamsAdded++;
		UE_LOG(LogNiagaraJsonSpike, Log, TEXT("Added + set User Parameter: %s (%s)"),
			*Var.GetName().ToString(), *TypeName);
	}
	else
	{
		Stats.ParamsSet++;
		UE_LOG(LogNiagaraJsonSpike, Log, TEXT("Set User Parameter: %s (%s)"),
			*Var.GetName().ToString(), *TypeName);
	}
	return true;
}

void FNiagaraJsonSpikeImporter::LogError(const FString& Msg)
{
	UE_LOG(LogNiagaraJsonSpike, Error, TEXT("%s"), *Msg);
	Notify(Msg, false);
}

void FNiagaraJsonSpikeImporter::LogWarning(const FString& Msg, FImportStats* Stats)
{
	UE_LOG(LogNiagaraJsonSpike, Warning, TEXT("%s"), *Msg);
	if (Stats)
	{
		Stats->Warnings++;
	}
}

void FNiagaraJsonSpikeImporter::Notify(const FString& Msg, bool bSuccess)
{
	// Headless runs (commandlet / -ExecCmds automation) have no Slate application.
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	FNotificationInfo Info(FText::FromString(Msg));
	Info.ExpireDuration = 6.0f;
	Info.bUseSuccessFailIcons = true;
	if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
	{
		Item->SetCompletionState(bSuccess
			? SNotificationItem::CS_Success
			: SNotificationItem::CS_Fail);
	}
}
