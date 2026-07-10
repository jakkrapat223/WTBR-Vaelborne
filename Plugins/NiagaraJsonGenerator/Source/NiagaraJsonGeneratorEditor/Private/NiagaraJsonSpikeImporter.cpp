#include "NiagaraJsonSpikeImporter.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "NiagaraUserRedirectionParameterStore.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY_STATIC(LogNiagaraJsonSpike, Log, All);

namespace
{
	// Supported spike param types, mapped to Niagara type definitions.
	// Sizes must match SetParameterValue<T>'s sizeof check:
	//   float(4) / int32(4) / FNiagaraBool(4) / FLinearColor(16) / FVector3f(12)
	const FNiagaraTypeDefinition* ResolveTypeDef(const FString& TypeName)
	{
		const FString Lower = TypeName.ToLower();
		if (Lower == TEXT("float"))                            { return &FNiagaraTypeDefinition::GetFloatDef(); }
		if (Lower == TEXT("int") || Lower == TEXT("int32"))    { return &FNiagaraTypeDefinition::GetIntDef(); }
		if (Lower == TEXT("bool"))                             { return &FNiagaraTypeDefinition::GetBoolDef(); }
		if (Lower == TEXT("color") || Lower == TEXT("linearcolor")) { return &FNiagaraTypeDefinition::GetColorDef(); }
		if (Lower == TEXT("vector3") || Lower == TEXT("vec3") || Lower == TEXT("vector"))
		{
			return &FNiagaraTypeDefinition::GetVec3Def();
		}
		return nullptr;
	}

	// Phase A path whitelists (see memory scope locks / README).
	const TCHAR* TemplateRoot = TEXT("/Game/VFX/Templates/");
	const TCHAR* OutputRoot   = TEXT("/Game/VFX/");

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
		{ TEXT("template"), TEXT("outputPath"), TEXT("addMissingParams"), TEXT("params") };
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Root->Values)
	{
		if (!KnownTopLevelFields.Contains(Pair.Key))
		{
			Warn(TEXT("W001"), FString::Printf(
				TEXT("Unknown top-level field \"%s\" — ignored"), *Pair.Key));
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
					TEXT("%s: unsupported type \"%s\" (allowed: float, int, bool, color, vector3)"),
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
			else
			{
				ValueText = TEXT("(type not dumped by spike)");
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
			TEXT("W106: Param \"%s\": unsupported type \"%s\" (allowed: float, int, bool, color, vector3) — skipped"),
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
