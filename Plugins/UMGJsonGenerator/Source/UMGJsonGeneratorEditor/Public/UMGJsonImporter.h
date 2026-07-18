#pragma once

#include "CoreMinimal.h"
#include "UMGJsonTypes.h"

class UWidgetBlueprint;

// ── Parses a .json file and drives WBP asset creation ────────────────────────
class UMGJSONGENERATOREDITOR_API FUMGJsonImporter
{
public:
	static UWidgetBlueprint* ImportFromFile(const FString& FilePath);

	// Runs the real in-place rebuild against a temporary duplicate, reports the
	// structural before/after diff, then deletes the duplicate. The source asset
	// and source JSON are never modified.
	static bool RunReimportProbe(const FString& SourceAssetPath, const FString& JsonFilePath);

private:
	enum class EOverwriteAction : uint8
	{
		UpdateInPlace,
		Duplicate,
		Cancel
	};

	static bool ParseJson(const FString& FilePath, FUMGJsonLayoutData& OutLayout,
	                       FUMGJsonImportStats& Stats);
	static bool ParseWidgetEntry(const TSharedPtr<FJsonObject>& Obj, FUMGJsonWidgetData& OutData,
	                              int32 Index, FUMGJsonImportStats& Stats);
	static bool IsValidHexColor(const FString& Value);
	static FLinearColor ParseHexColor(const FString& Hex);

	static EOverwriteAction HandleExistingAsset(const FString& AssetName,
	                                             const FString& FullPackageName);
	static bool DeleteExistingAsset(const FString& FullPackageName, const FString& AssetName);
	static FString FindUniqueName(const FString& BaseName, const FString& PackagePath);

	// Resolves "parentClass" to a UUserWidget subclass; nullptr + warning on failure.
	static UClass* ResolveParentClass(const FString& ParentClassPath, FUMGJsonImportStats& Stats);

	// Strips the old widget tree out of an existing WBP so it can be rebuilt in-place.
	static void ClearWidgetTree(UWidgetBlueprint* WBP);

	static UWidgetBlueprint* CreateWidgetBlueprintAsset(FUMGJsonLayoutData& Layout,
	                                                     FUMGJsonImportStats& Stats,
	                                                     bool& bOutIsNewAsset,
	                                                     bool bForceUpdateInPlace = false,
	                                                     bool bCreateBackup = true);
	static void BuildWidgetTree(UWidgetBlueprint* WBP, const FUMGJsonLayoutData& Layout,
	                             FUMGJsonImportStats& Stats);
	static bool SaveWidgetBlueprint(UWidgetBlueprint* WBP, bool bIsNewAsset);
	static void SaveAndOpen(UWidgetBlueprint* WBP, bool bIsNewAsset, bool bOpenEditor = true);
	static bool CreateConvenienceBackup(UWidgetBlueprint* WBP, FString& OutBackupObjectPath);
	static UWidgetBlueprint* ImportFromFileInternal(const FString& FilePath,
	                                                bool bForceUpdateInPlace,
	                                                bool bCreateBackup,
	                                                bool bOpenEditor,
	                                                bool bNotifyResult);

	static void LogError(const FString& Msg);
	static void LogWarning(const FString& Msg, FUMGJsonImportStats* Stats = nullptr);
	static void Notify(const FString& Msg, bool bSuccess);
	static void NotifySummary(const FString& AssetPath, const FUMGJsonImportStats& Stats);
};
