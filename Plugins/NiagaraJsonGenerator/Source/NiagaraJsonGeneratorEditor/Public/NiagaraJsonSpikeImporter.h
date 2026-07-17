#pragma once

#include "CoreMinimal.h"

class UNiagaraSystem;
class FJsonObject;

// Production template-driven importer. The legacy class/file name is retained
// so existing automation and integrations remain source-compatible.
// Duplicates an existing Niagara System template asset and sets exposed User
// Parameter defaults from a JSON spec, then saves the asset.
// Intentional scope limits: no emitter/graph creation, no module
// stack or scratch module access — exposed-parameter store only.
class NIAGARAJSONGENERATOREDITOR_API FNiagaraJsonSpikeImporter
{
public:
	static UNiagaraSystem* ImportFromFile(const FString& FilePath);
	static bool ValidateFile(const FString& FilePath);
	static bool ImportBatchFromFile(const FString& FilePath);

	// Editor-preset workflow: generates a strict JSON spec internally from the
	// common artistic controls, then imports it. Artists never edit JSON for
	// Color/Energy/Speed/SparkCount iterations.
	static UNiagaraSystem* GeneratePreset(const FString& TemplatePath,
	                                     const FString& OutputPath,
	                                     const FLinearColor& Color,
	                                     float Energy, float Speed, int32 SparkCount);

	// Applies trail/impact settings to WTBR's Sniper Trigger Data Assets. This
	// is intentionally separate from effect generation: validate/generate first,
	// then bind the resulting Niagara assets to gameplay in one explicit step.
	static bool AutoBindSniperFromFile(const FString& FilePath);
	// Applies the same data-driven impact/trail configuration directly to any
	// Blueprint derived from AWTBRProjectileBase. This covers projectile
	// families that do not own a dedicated Trigger Data Asset VFX slot yet.
	static bool AutoBindProjectileBlueprintsFromFile(const FString& FilePath);
	// Logs a static Niagara complexity report (emitters, renderers, simulation
	// targets and allocation caps) without modifying the asset.
	static bool AuditSystemPerformance(const FString& AssetPath);

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

	// Phase A pre-flight: validates the parsed JSON spec (required fields,
	// template/output path whitelists, param entry shape, allowed types)
	// WITHOUT touching any asset. Returns false if any E-code error was found —
	// the caller must then skip DuplicateAsset/Modify/SaveLoadedAsset entirely.
	static bool ValidateSpec(const TSharedPtr<FJsonObject>& Root, FImportStats& Stats);
	static bool ValidateTemplateContract(const TSharedPtr<FJsonObject>& Root,
	                                     UNiagaraSystem* TemplateSystem,
	                                     bool bLogAsErrors, FImportStats& Stats);
	static bool ValidateOutputTarget(const FString& OutputPath);

	// Existence-detection + safe load/duplicate. Treats FPackageName::DoesPackageExist
	// (disk) as the source of truth — NOT UEditorAssetLibrary::DoesAssetExist (Asset
	// Registry cache) alone, which can be stale for a package a *different* process
	// just wrote (see README: cross-process caveat). If the package exists on disk,
	// this ALWAYS returns the loaded existing asset for update-in-place (or nullptr
	// + an E-code abort) — it never silently falls back to DuplicateAsset.
	static UNiagaraSystem* ResolveOutputAsset(const FString& TemplatePath,
	                                          const FString& OutputPath, bool& bOutIsNewAsset);

	static bool ApplyParam(UNiagaraSystem* System, const FString& ParamName,
	                       const TSharedPtr<FJsonObject>& ParamObj, bool bAddMissing,
	                       FImportStats& Stats);

	static void LogError(const FString& Msg);
	static void LogWarning(const FString& Msg, FImportStats* Stats = nullptr);
	static void Notify(const FString& Msg, bool bSuccess);
};
