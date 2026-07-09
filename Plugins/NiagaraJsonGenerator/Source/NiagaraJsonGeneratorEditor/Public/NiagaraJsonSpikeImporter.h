#pragma once

#include "CoreMinimal.h"

class UNiagaraSystem;
class FJsonObject;

// ── Spike-scoped importer ─────────────────────────────────────────────────────
// Duplicates an existing Niagara System template asset and sets exposed User
// Parameter defaults from a JSON spec, then saves the asset.
// Hard scope limits (per spike contract): no emitter/graph creation, no module
// stack or scratch module access — exposed-parameter store only.
class NIAGARAJSONGENERATOREDITOR_API FNiagaraJsonSpikeImporter
{
public:
	static UNiagaraSystem* ImportFromFile(const FString& FilePath);

	// Read-back for validation: logs every exposed User Parameter (name, type,
	// value) of a saved NiagaraSystem asset. Lets headless runs assert that
	// imported values actually landed, without opening the editor.
	static bool DumpUserParams(const FString& AssetPath);

private:
	struct FImportStats
	{
		int32 ParamsSet = 0;
		int32 ParamsAdded = 0;
		int32 Skipped = 0;
		int32 Warnings = 0;
	};

	static bool ApplyParam(UNiagaraSystem* System, const FString& ParamName,
	                       const TSharedPtr<FJsonObject>& ParamObj, bool bAddMissing,
	                       FImportStats& Stats);

	static void LogError(const FString& Msg);
	static void LogWarning(const FString& Msg, FImportStats* Stats = nullptr);
	static void Notify(const FString& Msg, bool bSuccess);
};
