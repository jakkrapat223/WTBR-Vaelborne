#include "NiagaraJsonSpikeImporter.h"

#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/FileHelper.h"
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
		if (Lower == TEXT("vec3") || Lower == TEXT("vector"))  { return &FNiagaraTypeDefinition::GetVec3Def(); }
		return nullptr;
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

UNiagaraSystem* FNiagaraJsonSpikeImporter::ImportFromFile(const FString& FilePath)
{
	// ── 1. Read + parse JSON ──────────────────────────────────────────────────
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *FilePath))
	{
		LogError(FString::Printf(TEXT("Cannot read file: %s"), *FilePath));
		return nullptr;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		LogError(FString::Printf(TEXT("Invalid JSON in file: %s"), *FilePath));
		return nullptr;
	}

	FString TemplatePath;
	if (!Root->TryGetStringField(TEXT("template"), TemplatePath) || TemplatePath.IsEmpty())
	{
		LogError(TEXT("Missing required field: \"template\""));
		return nullptr;
	}

	FString OutputPath;
	if (!Root->TryGetStringField(TEXT("outputPath"), OutputPath) || OutputPath.IsEmpty())
	{
		LogError(TEXT("Missing required field: \"outputPath\""));
		return nullptr;
	}
	if (!OutputPath.StartsWith(TEXT("/Game/")))
	{
		LogError(FString::Printf(TEXT("\"outputPath\" must start with /Game/ — got: %s"), *OutputPath));
		return nullptr;
	}

	bool bAddMissing = false;
	Root->TryGetBoolField(TEXT("addMissingParams"), bAddMissing);

	// ── 2. Load template ──────────────────────────────────────────────────────
	if (!UEditorAssetLibrary::DoesAssetExist(TemplatePath))
	{
		LogError(FString::Printf(TEXT("Template asset not found: %s"), *TemplatePath));
		return nullptr;
	}

	UNiagaraSystem* TemplateSystem = Cast<UNiagaraSystem>(UEditorAssetLibrary::LoadAsset(TemplatePath));
	if (!TemplateSystem)
	{
		LogError(FString::Printf(TEXT("Template asset is not a NiagaraSystem: %s"), *TemplatePath));
		return nullptr;
	}

	// ── 3. Duplicate template, or update the output asset in place ───────────
	UNiagaraSystem* System = nullptr;
	bool bIsNewAsset = false;

	if (UEditorAssetLibrary::DoesAssetExist(OutputPath))
	{
		System = Cast<UNiagaraSystem>(UEditorAssetLibrary::LoadAsset(OutputPath));
		if (!System)
		{
			LogError(FString::Printf(
				TEXT("Output path exists but is not a NiagaraSystem — refusing to overwrite: %s"),
				*OutputPath));
			return nullptr;
		}
		UE_LOG(LogNiagaraJsonSpike, Log,
			TEXT("Output asset exists — updating User Parameters in place: %s"), *OutputPath);
	}
	else
	{
		System = Cast<UNiagaraSystem>(UEditorAssetLibrary::DuplicateAsset(TemplatePath, OutputPath));
		if (!System)
		{
			LogError(FString::Printf(TEXT("DuplicateAsset failed: %s -> %s"),
				*TemplatePath, *OutputPath));
			return nullptr;
		}
		bIsNewAsset = true;
		UE_LOG(LogNiagaraJsonSpike, Log, TEXT("Duplicated template %s -> %s"),
			*TemplatePath, *OutputPath);
	}

	// ── 4. Apply User Parameter values ────────────────────────────────────────
	FImportStats Stats;
	System->Modify();

	const TSharedPtr<FJsonObject>* ParamsObj = nullptr;
	if (Root->TryGetObjectField(TEXT("params"), ParamsObj) && ParamsObj->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ParamsObj)->Values)
		{
			const TSharedPtr<FJsonObject>* Entry = nullptr;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(Entry) || !Entry->IsValid())
			{
				LogWarning(FString::Printf(
					TEXT("Param \"%s\": entry must be an object { \"type\": ..., \"value\": ... } — skipped"),
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
	else
	{
		LogWarning(TEXT("No \"params\" object found — asset duplicated/loaded with no parameter changes."), &Stats);
	}

	// ── 5. Save ───────────────────────────────────────────────────────────────
	System->MarkPackageDirty();
	const bool bSaved = UEditorAssetLibrary::SaveLoadedAsset(System, /*bOnlyIfIsDirty*/ false);
	if (!bSaved)
	{
		LogError(FString::Printf(TEXT("SaveLoadedAsset failed for: %s"), *OutputPath));
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
		LogWarning(FString::Printf(TEXT("Param \"%s\": missing \"type\" field — skipped"),
			*ParamName), &Stats);
		return false;
	}

	const FNiagaraTypeDefinition* TypeDef = ResolveTypeDef(TypeName);
	if (!TypeDef)
	{
		LogWarning(FString::Printf(
			TEXT("Param \"%s\": unsupported type \"%s\" (supported: float, int, bool, color, vec3) — skipped"),
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
			TEXT("Param \"%s\": type mismatch — template exposes \"%s\", JSON says \"%s\" — skipped"),
			*ParamName, *Existing->GetType().GetName(), *TypeName), &Stats);
		return false;
	}

	if (!Existing && !bAddMissing)
	{
		LogWarning(FString::Printf(
			TEXT("Param \"%s\": not exposed on template (set \"addMissingParams\": true to add) — skipped"),
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
			LogWarning(FString::Printf(TEXT("Param \"%s\": \"value\" must be a number — skipped"), *ParamName), &Stats);
			return false;
		}
		bSetOk = Store.SetParameterValue(static_cast<float>(Num), Var, bAdd);
	}
	else if (*TypeDef == FNiagaraTypeDefinition::GetIntDef())
	{
		double Num = 0.0;
		if (!ParamObj->TryGetNumberField(TEXT("value"), Num))
		{
			LogWarning(FString::Printf(TEXT("Param \"%s\": \"value\" must be a number — skipped"), *ParamName), &Stats);
			return false;
		}
		bSetOk = Store.SetParameterValue(static_cast<int32>(Num), Var, bAdd);
	}
	else if (*TypeDef == FNiagaraTypeDefinition::GetBoolDef())
	{
		bool bValue = false;
		if (!ParamObj->TryGetBoolField(TEXT("value"), bValue))
		{
			LogWarning(FString::Printf(TEXT("Param \"%s\": \"value\" must be true/false — skipped"), *ParamName), &Stats);
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
				TEXT("Param \"%s\": \"value\" must be an array [r,g,b] or [r,g,b,a] (0-1 floats) — skipped"),
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
				TEXT("Param \"%s\": \"value\" must be an array [x,y,z] — skipped"), *ParamName), &Stats);
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
		LogWarning(FString::Printf(TEXT("Param \"%s\": SetParameterValue failed — skipped"), *ParamName), &Stats);
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
