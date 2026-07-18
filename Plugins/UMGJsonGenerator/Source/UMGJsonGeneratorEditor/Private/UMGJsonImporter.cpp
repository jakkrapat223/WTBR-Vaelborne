#include "UMGJsonImporter.h"

#include "UMGWidgetFactory.h"

// JSON
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// Widget Blueprint creation
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Widget.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"

// Asset tools
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/SavePackage.h"
#include "ObjectTools.h"

// File I/O
#include "Misc/FileHelper.h"

// Editor open
#include "Subsystems/AssetEditorSubsystem.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "Editor.h"

// Notifications
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

// Dialog
#include "Misc/MessageDialog.h"

// Kismet compile
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"

// Compiler report formatting
#include "Logging/TokenizedMessage.h"

// UMG widgets
#include "Components/CanvasPanel.h"
#include "Components/PanelWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogUMGJsonImporter, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Supported widget types list (for validation messages)
// ─────────────────────────────────────────────────────────────────────────────
static const TArray<FString> GSupportedTypes = {
	TEXT("CanvasPanel"), TEXT("TextBlock"), TEXT("Button"), TEXT("Image"),
	TEXT("Border"), TEXT("VerticalBox"), TEXT("HorizontalBox"),
	TEXT("ProgressBar"), TEXT("ScrollBox"), TEXT("Overlay"),
	TEXT("SizeBox"), TEXT("Spacer")
};

namespace
{
	struct FUMGReimportProbeSnapshot
	{
		FString ParentClass;
		TArray<FString> EventGraphs;
		TArray<FString> FunctionGraphs;
		int32 BindingCount = 0;
		int32 AnimationCount = 0;
		TMap<FString, FString> VariableWidgets;
	};

	static void SortStrings(TArray<FString>& Values)
	{
		Values.Sort([](const FString& A, const FString& B)
		{
			return A.Compare(B, ESearchCase::IgnoreCase) < 0;
		});
	}

	static FUMGReimportProbeSnapshot CaptureProbeSnapshot(const UWidgetBlueprint* WBP)
	{
		FUMGReimportProbeSnapshot Snapshot;
		if (!WBP)
		{
			return Snapshot;
		}

		Snapshot.ParentClass = WBP->ParentClass
			? WBP->ParentClass->GetPathName()
			: TEXT("<null>");
		Snapshot.BindingCount = WBP->Bindings.Num();
		Snapshot.AnimationCount = WBP->Animations.Num();

		for (const UEdGraph* Graph : WBP->UbergraphPages)
		{
			if (Graph)
			{
				Snapshot.EventGraphs.Add(Graph->GetName());
			}
		}
		for (const UEdGraph* Graph : WBP->FunctionGraphs)
		{
			if (Graph)
			{
				Snapshot.FunctionGraphs.Add(Graph->GetName());
			}
		}
		SortStrings(Snapshot.EventGraphs);
		SortStrings(Snapshot.FunctionGraphs);

		if (WBP->WidgetTree)
		{
			WBP->WidgetTree->ForEachWidget([&Snapshot](UWidget* Widget)
			{
				if (Widget && Widget->bIsVariable)
				{
					Snapshot.VariableWidgets.Add(
						Widget->GetName(), Widget->GetClass()->GetPathName());
				}
			});
		}

		return Snapshot;
	}

	static FString BlueprintStatusToString(EBlueprintStatus Status)
	{
		switch (Status)
		{
		case BS_Unknown:              return TEXT("Unknown");
		case BS_Dirty:                return TEXT("Dirty");
		case BS_Error:                return TEXT("Error");
		case BS_UpToDate:             return TEXT("UpToDate");
		case BS_BeingCreated:         return TEXT("BeingCreated");
		case BS_UpToDateWithWarnings: return TEXT("UpToDateWithWarnings");
		default:                      return TEXT("Invalid");
		}
	}

	static const TCHAR* MessageSeverityToString(EMessageSeverity::Type Severity)
	{
		switch (Severity)
		{
		case EMessageSeverity::Error:         return TEXT("Error");
		case EMessageSeverity::PerformanceWarning:
		case EMessageSeverity::Warning:       return TEXT("Warning");
		case EMessageSeverity::Info:          return TEXT("Info");
		default:                              return TEXT("Log");
		}
	}

	static void LogStringList(const TCHAR* Label, const TArray<FString>& Values)
	{
		UE_LOG(LogUMGJsonImporter, Log, TEXT("%s (%d):"), Label, Values.Num());
		if (Values.Num() == 0)
		{
			UE_LOG(LogUMGJsonImporter, Log, TEXT("  <none>"));
			return;
		}
		for (const FString& Value : Values)
		{
			UE_LOG(LogUMGJsonImporter, Log, TEXT("  %s"), *Value);
		}
	}

	static TArray<FString> WidgetMapToStrings(const TMap<FString, FString>& Widgets)
	{
		TArray<FString> Values;
		Values.Reserve(Widgets.Num());
		for (const TPair<FString, FString>& Pair : Widgets)
		{
			Values.Add(FString::Printf(TEXT("%s : %s"), *Pair.Key, *Pair.Value));
		}
		SortStrings(Values);
		return Values;
	}

	static void DiffStringArrays(const TArray<FString>& Before, const TArray<FString>& After,
		TArray<FString>& OutCreated, TArray<FString>& OutRemoved)
	{
		for (const FString& Value : After)
		{
			if (!Before.Contains(Value))
			{
				OutCreated.Add(Value);
			}
		}
		for (const FString& Value : Before)
		{
			if (!After.Contains(Value))
			{
				OutRemoved.Add(Value);
			}
		}
		SortStrings(OutCreated);
		SortStrings(OutRemoved);
	}

	static bool CreateProbeJsonClone(const FString& SourceJsonPath, const FString& ProbeWidgetName,
		FString& OutProbeJsonPath)
	{
		FString RawJson;
		if (!FFileHelper::LoadFileToString(RawJson, *SourceJsonPath))
		{
			UE_LOG(LogUMGJsonImporter, Error,
				TEXT("Reimport Probe: cannot read JSON source: %s"), *SourceJsonPath);
			return false;
		}

		TSharedPtr<FJsonObject> RootObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawJson);
		if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
		{
			UE_LOG(LogUMGJsonImporter, Error,
				TEXT("Reimport Probe: source JSON is invalid: %s"), *SourceJsonPath);
			return false;
		}

		RootObject->SetStringField(TEXT("widgetName"), ProbeWidgetName);
		FString ClonedJson;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ClonedJson);
		if (!FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
		{
			UE_LOG(LogUMGJsonImporter, Error, TEXT("Reimport Probe: failed to serialize cloned JSON."));
			return false;
		}

		const FString ProbeJsonDir = FPaths::ProjectIntermediateDir()
			/ TEXT("UMGJsonGenerator") / TEXT("ReimportProbes");
		IFileManager::Get().MakeDirectory(*ProbeJsonDir, true);
		OutProbeJsonPath = ProbeJsonDir / FString::Printf(TEXT("%s_%s.json"),
			*ProbeWidgetName, *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")));
		if (!FFileHelper::SaveStringToFile(ClonedJson, *OutProbeJsonPath))
		{
			UE_LOG(LogUMGJsonImporter, Error,
				TEXT("Reimport Probe: failed to write cloned JSON: %s"), *OutProbeJsonPath);
			OutProbeJsonPath.Reset();
			return false;
		}
		return true;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Public entry point
// ─────────────────────────────────────────────────────────────────────────────

UWidgetBlueprint* FUMGJsonImporter::ImportFromFile(const FString& FilePath)
{
	return ImportFromFileInternal(FilePath,
		/*bForceUpdateInPlace=*/false,
		/*bCreateBackup=*/true,
		/*bOpenEditor=*/true,
		/*bNotifyResult=*/true);
}

UWidgetBlueprint* FUMGJsonImporter::ImportUpdateInPlaceFromFile(const FString& FilePath)
{
	return ImportFromFileInternal(FilePath,
		/*bForceUpdateInPlace=*/true,
		/*bCreateBackup=*/true,
		/*bOpenEditor=*/false,
		/*bNotifyResult=*/true);
}

UWidgetBlueprint* FUMGJsonImporter::ImportFromFileInternal(const FString& FilePath,
	bool bForceUpdateInPlace, bool bCreateBackup, bool bOpenEditor, bool bNotifyResult)
{
	UE_LOG(LogUMGJsonImporter, Log, TEXT("=== UMG JSON Import START: %s ==="), *FilePath);

	FUMGJsonImportStats Stats;
	FUMGJsonLayoutData Layout;

	if (!ParseJson(FilePath, Layout, Stats))
	{
		return nullptr;
	}

	bool bIsNewAsset = true;
	UWidgetBlueprint* WBP = CreateWidgetBlueprintAsset(
		Layout, Stats, bIsNewAsset, bForceUpdateInPlace, bCreateBackup);
	if (!WBP)
	{
		return nullptr;
	}

	BuildWidgetTree(WBP, Layout, Stats);
	SaveAndOpen(WBP, bIsNewAsset, bOpenEditor);

	const FString AssetPath = FString::Printf(TEXT("/Game/UI/Generated/%s"), *Layout.WidgetName);
	UE_LOG(LogUMGJsonImporter, Log, TEXT("=== UMG JSON Import DONE: %s ==="), *AssetPath);
	if (bNotifyResult)
	{
		NotifySummary(AssetPath, Stats);
	}
	return WBP;
}

bool FUMGJsonImporter::RunReimportProbe(const FString& SourceAssetPath,
	const FString& JsonFilePath)
{
	FString SourcePackageName = SourceAssetPath.TrimStartAndEnd();
	SourcePackageName = SourcePackageName.TrimQuotes();
	if (SourcePackageName.Contains(TEXT(".")))
	{
		SourcePackageName = FPackageName::ObjectPathToPackageName(SourcePackageName);
	}
	if (!FPackageName::IsValidLongPackageName(SourcePackageName))
	{
		UE_LOG(LogUMGJsonImporter, Error,
			TEXT("Reimport Probe: invalid source WBP path: %s"), *SourceAssetPath);
		return false;
	}

	const FString SourceAssetName = FPackageName::GetLongPackageAssetName(SourcePackageName);
	const FString SourceObjectPath = SourcePackageName + TEXT(".") + SourceAssetName;
	UWidgetBlueprint* SourceWBP = LoadObject<UWidgetBlueprint>(nullptr, *SourceObjectPath);
	if (!SourceWBP)
	{
		UE_LOG(LogUMGJsonImporter, Error,
			TEXT("Reimport Probe: source is not a loadable Widget Blueprint: %s"), *SourceObjectPath);
		return false;
	}
	FString ResolvedJsonFilePath = JsonFilePath.TrimStartAndEnd();
	ResolvedJsonFilePath = ResolvedJsonFilePath.TrimQuotes();
	if (FPaths::IsRelative(ResolvedJsonFilePath))
	{
		ResolvedJsonFilePath = FPaths::ConvertRelativePathToFull(
			FPaths::ProjectDir(), ResolvedJsonFilePath);
	}
	FPaths::NormalizeFilename(ResolvedJsonFilePath);
	if (!IFileManager::Get().FileExists(*ResolvedJsonFilePath))
	{
		UE_LOG(LogUMGJsonImporter, Error,
			TEXT("Reimport Probe: JSON file does not exist: %s"), *ResolvedJsonFilePath);
		return false;
	}

	const FString ProbePackagePath = TEXT("/Game/UI/Generated");
	FString ProbeBaseName = SourceAssetName;
	if (ProbeBaseName.EndsWith(TEXT("_Generated")))
	{
		ProbeBaseName.LeftChopInline(10);
	}
	ProbeBaseName += TEXT("_ReimportProbe");
	const FString ProbeBasePackageName = ProbePackagePath / ProbeBaseName;
	const FString ProbeAssetName =
		(!FPackageName::DoesPackageExist(ProbeBasePackageName)
			&& !FindPackage(nullptr, *ProbeBasePackageName))
		? ProbeBaseName
		: FindUniqueName(ProbeBaseName, ProbePackagePath);
	const FString ProbePackageName = ProbePackagePath / ProbeAssetName;
	const FString ProbeObjectPath = ProbePackageName + TEXT(".") + ProbeAssetName;

	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	UWidgetBlueprint* ProbeWBP = Cast<UWidgetBlueprint>(
		AssetTools.DuplicateAsset(ProbeAssetName, ProbePackagePath, SourceWBP));
	if (!ProbeWBP)
	{
		UE_LOG(LogUMGJsonImporter, Error,
			TEXT("Reimport Probe: failed to duplicate source WBP."));
		return false;
	}

	FString ProbeJsonPath;
	bool bProbeAssetCreated = true;
	ON_SCOPE_EXIT
	{
		if (!ProbeJsonPath.IsEmpty())
		{
			IFileManager::Get().Delete(*ProbeJsonPath, false, true, true);
		}
		if (bProbeAssetCreated)
		{
			bool bDeleted = false;
			if (GEditor)
			{
				if (UAssetEditorSubsystem* AssetEditorSubsystem =
					GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
				{
					AssetEditorSubsystem->CloseAllEditorsForAsset(ProbeWBP);
				}
				if (UEditorAssetSubsystem* EditorAssetSubsystem =
					GEditor->GetEditorSubsystem<UEditorAssetSubsystem>())
				{
					bDeleted = EditorAssetSubsystem->DeleteLoadedAsset(ProbeWBP);
				}
			}
			if (bDeleted)
			{
				UE_LOG(LogUMGJsonImporter, Log,
					TEXT("Reimport Probe Cleanup: Deleted probe asset: %s"), *ProbeObjectPath);
			}
			else
			{
				UE_LOG(LogUMGJsonImporter, Error,
					TEXT("Reimport Probe Cleanup: FAILED to delete probe asset: %s"), *ProbeObjectPath);
			}
		}
	};

	UE_LOG(LogUMGJsonImporter, Log, TEXT("=== UMG REIMPORT PROBE START ==="));
	UE_LOG(LogUMGJsonImporter, Log, TEXT("Source WBP: %s"), *SourceObjectPath);
	UE_LOG(LogUMGJsonImporter, Log, TEXT("Source JSON: %s"), *ResolvedJsonFilePath);
	UE_LOG(LogUMGJsonImporter, Log, TEXT("Created Probe: %s"), *ProbeObjectPath);

	const FUMGReimportProbeSnapshot Before = CaptureProbeSnapshot(ProbeWBP);
	if (!CreateProbeJsonClone(ResolvedJsonFilePath, ProbeAssetName, ProbeJsonPath))
	{
		return false;
	}
	UE_LOG(LogUMGJsonImporter, Log, TEXT("Created JSON Clone: %s (widgetName=%s)"),
		*ProbeJsonPath, *ProbeAssetName);

	ProbeWBP = ImportFromFileInternal(ProbeJsonPath,
		/*bForceUpdateInPlace=*/true,
		/*bCreateBackup=*/false,
		/*bOpenEditor=*/false,
		/*bNotifyResult=*/false);
	if (!ProbeWBP)
	{
		UE_LOG(LogUMGJsonImporter, Error,
			TEXT("Reimport Probe: real import pipeline failed for probe asset."));
		return false;
	}

	FCompilerResultsLog CompileLog;
	CompileLog.bSilentMode = true;
	CompileLog.bLogInfoOnly = false;
	FKismetEditorUtilities::CompileBlueprint(
		ProbeWBP, EBlueprintCompileOptions::SkipSave, &CompileLog);
	const FUMGReimportProbeSnapshot After = CaptureProbeSnapshot(ProbeWBP);

	TArray<FString> CreatedEventGraphs;
	TArray<FString> RemovedEventGraphs;
	TArray<FString> CreatedFunctionGraphs;
	TArray<FString> RemovedFunctionGraphs;
	DiffStringArrays(Before.EventGraphs, After.EventGraphs,
		CreatedEventGraphs, RemovedEventGraphs);
	DiffStringArrays(Before.FunctionGraphs, After.FunctionGraphs,
		CreatedFunctionGraphs, RemovedFunctionGraphs);

	TArray<FString> CreatedWidgets;
	TArray<FString> RemovedWidgets;
	TArray<FString> TypeChangedWidgets;
	for (const TPair<FString, FString>& Pair : After.VariableWidgets)
	{
		const FString* BeforeType = Before.VariableWidgets.Find(Pair.Key);
		if (!BeforeType)
		{
			CreatedWidgets.Add(FString::Printf(TEXT("%s : %s"), *Pair.Key, *Pair.Value));
		}
		else if (*BeforeType != Pair.Value)
		{
			TypeChangedWidgets.Add(FString::Printf(TEXT("%s : %s -> %s"),
				*Pair.Key, **BeforeType, *Pair.Value));
		}
	}
	for (const TPair<FString, FString>& Pair : Before.VariableWidgets)
	{
		if (!After.VariableWidgets.Contains(Pair.Key))
		{
			RemovedWidgets.Add(FString::Printf(TEXT("%s : %s"), *Pair.Key, *Pair.Value));
		}
	}
	SortStrings(CreatedWidgets);
	SortStrings(RemovedWidgets);
	SortStrings(TypeChangedWidgets);

	const bool bParentPreserved = Before.ParentClass == After.ParentClass;
	const bool bGraphsPreserved = CreatedEventGraphs.Num() == 0 && RemovedEventGraphs.Num() == 0
		&& CreatedFunctionGraphs.Num() == 0 && RemovedFunctionGraphs.Num() == 0;
	const bool bExistingVariablesPreserved = RemovedWidgets.Num() == 0
		&& TypeChangedWidgets.Num() == 0;
	const bool bCompileSucceeded = CompileLog.NumErrors == 0
		&& (ProbeWBP->Status == BS_UpToDate || ProbeWBP->Status == BS_UpToDateWithWarnings);
	const bool bStructuralContractPassed = bParentPreserved && bGraphsPreserved
		&& bExistingVariablesPreserved && bCompileSucceeded;

	UE_LOG(LogUMGJsonImporter, Log, TEXT("=== UMG REIMPORT PROBE REPORT ==="));
	UE_LOG(LogUMGJsonImporter, Log, TEXT("Parent Class: %s"),
		bParentPreserved ? TEXT("UNCHANGED") : TEXT("CHANGED"));
	UE_LOG(LogUMGJsonImporter, Log, TEXT("  Before: %s"), *Before.ParentClass);
	UE_LOG(LogUMGJsonImporter, Log, TEXT("  After : %s"), *After.ParentClass);

	LogStringList(TEXT("Before Event Graphs"), Before.EventGraphs);
	LogStringList(TEXT("After Event Graphs"), After.EventGraphs);
	LogStringList(TEXT("Created Event Graphs"), CreatedEventGraphs);
	LogStringList(TEXT("Removed Event Graphs"), RemovedEventGraphs);
	LogStringList(TEXT("Before Function Graphs"), Before.FunctionGraphs);
	LogStringList(TEXT("After Function Graphs"), After.FunctionGraphs);
	LogStringList(TEXT("Created Function Graphs"), CreatedFunctionGraphs);
	LogStringList(TEXT("Removed Function Graphs"), RemovedFunctionGraphs);

	UE_LOG(LogUMGJsonImporter, Log,
		TEXT("UMG Property Bindings: Before=%d After=%d (clearing is expected)"),
		Before.BindingCount, After.BindingCount);
	UE_LOG(LogUMGJsonImporter, Log,
		TEXT("UMG Animations: Before=%d After=%d (clearing is expected)"),
		Before.AnimationCount, After.AnimationCount);

	LogStringList(TEXT("Before Variable Widgets"), WidgetMapToStrings(Before.VariableWidgets));
	LogStringList(TEXT("After Variable Widgets"), WidgetMapToStrings(After.VariableWidgets));
	UE_LOG(LogUMGJsonImporter, Log,
		TEXT("Variable Widgets: UPDATED (Before=%d After=%d Created=%d Removed=%d TypeChanged=%d)"),
		Before.VariableWidgets.Num(), After.VariableWidgets.Num(), CreatedWidgets.Num(),
		RemovedWidgets.Num(), TypeChangedWidgets.Num());
	LogStringList(TEXT("Created Variable Widgets"), CreatedWidgets);
	LogStringList(TEXT("Removed Variable Widgets"), RemovedWidgets);
	LogStringList(TEXT("Type-Changed Variable Widgets"), TypeChangedWidgets);

	const TCHAR* CompileOutcome = CompileLog.NumErrors > 0 || ProbeWBP->Status == BS_Error
		? TEXT("Error")
		: (CompileLog.NumWarnings > 0 || ProbeWBP->Status == BS_UpToDateWithWarnings
			? TEXT("Warning") : TEXT("Success"));
	UE_LOG(LogUMGJsonImporter, Log,
		TEXT("Compile Status: %s (BlueprintStatus=%s Errors=%d Warnings=%d Messages=%d)"),
		CompileOutcome, *BlueprintStatusToString(ProbeWBP->Status), CompileLog.NumErrors,
		CompileLog.NumWarnings, CompileLog.Messages.Num());
	if (CompileLog.Messages.Num() == 0)
	{
		UE_LOG(LogUMGJsonImporter, Log, TEXT("  <no compiler messages>"));
	}
	else
	{
		for (const TSharedRef<FTokenizedMessage>& Message : CompileLog.Messages)
		{
			UE_LOG(LogUMGJsonImporter, Log, TEXT("  [%s] %s"),
				MessageSeverityToString(Message->GetSeverity()), *Message->ToText().ToString());
		}
	}

	UE_LOG(LogUMGJsonImporter, Log, TEXT("Blueprint Graph Structure: %s"),
		bGraphsPreserved ? TEXT("PASS") : TEXT("FAIL"));
	UE_LOG(LogUMGJsonImporter, Log, TEXT("Native BindWidgetOptional Structural Contract: %s"),
		(bParentPreserved && bExistingVariablesPreserved && bCompileSucceeded)
			? TEXT("PASS") : TEXT("FAIL"));
	UE_LOG(LogUMGJsonImporter, Log, TEXT("Overall Structural Verdict: %s"),
		bStructuralContractPassed ? TEXT("PASS") : TEXT("FAIL"));
	UE_LOG(LogUMGJsonImporter, Log, TEXT("=== UMG REIMPORT PROBE END ==="));

	return bStructuralContractPassed;
}

// ─────────────────────────────────────────────────────────────────────────────
// JSON parsing
// ─────────────────────────────────────────────────────────────────────────────

bool FUMGJsonImporter::ParseJson(const FString& FilePath, FUMGJsonLayoutData& OutLayout,
                                  FUMGJsonImportStats& Stats)
{
	FString RawJson;
	if (!FFileHelper::LoadFileToString(RawJson, *FilePath))
	{
		LogError(FString::Printf(TEXT("Cannot read file: %s"), *FilePath));
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawJson);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		LogError(TEXT("Invalid JSON — failed to parse."));
		return false;
	}

	// widgetName (required)
	if (!Root->TryGetStringField(TEXT("widgetName"), OutLayout.WidgetName))
	{
		LogError(TEXT("Missing required field: 'widgetName'."));
		return false;
	}
	if (OutLayout.WidgetName.IsEmpty())
	{
		LogError(TEXT("Empty required field: 'widgetName'."));
		return false;
	}

	// parentClass (optional — native UUserWidget subclass for the generated WBP)
	Root->TryGetStringField(TEXT("parentClass"), OutLayout.ParentClassPath);

	// resolution (optional — defaults to 1920x1080)
	{
		const TSharedPtr<FJsonObject>* ResObj;
		if (Root->TryGetObjectField(TEXT("resolution"), ResObj))
		{
			double W = 1920.0, H = 1080.0;
			(*ResObj)->TryGetNumberField(TEXT("width"), W);
			(*ResObj)->TryGetNumberField(TEXT("height"), H);
			OutLayout.Resolution.Width = (float)W;
			OutLayout.Resolution.Height = (float)H;
		}
	}

	// root (required)
	{
		const TSharedPtr<FJsonObject>* RootWidgetObj;
		if (!Root->TryGetObjectField(TEXT("root"), RootWidgetObj))
		{
			LogError(TEXT("Missing required field: 'root'."));
			return false;
		}
		(*RootWidgetObj)->TryGetStringField(TEXT("type"), OutLayout.Root.Type);
		(*RootWidgetObj)->TryGetStringField(TEXT("name"), OutLayout.Root.Name);

		if (OutLayout.Root.Type.IsEmpty())
		{
			LogError(TEXT("Root widget 'type' is empty."));
			return false;
		}
		if (OutLayout.Root.Name.IsEmpty())
		{
			LogError(TEXT("Root widget 'name' is empty."));
			return false;
		}
		if (!GSupportedTypes.Contains(OutLayout.Root.Type))
		{
			LogError(FString::Printf(TEXT("Root widget type '%s' is not supported."),
				*OutLayout.Root.Type));
			return false;
		}
		if (OutLayout.Root.Type != TEXT("CanvasPanel"))
		{
			LogError(FString::Printf(
				TEXT("Invalid root type '%s'. Root must be 'CanvasPanel'."),
				*OutLayout.Root.Type));
			return false;
		}
	}

	// widgets array
	{
		const TArray<TSharedPtr<FJsonValue>>* WidgetsArray = nullptr;
		if (Root->TryGetArrayField(TEXT("widgets"), WidgetsArray))
		{
			TSet<FString> SeenNames;
			SeenNames.Add(OutLayout.Root.Name);

			for (int32 i = 0; i < WidgetsArray->Num(); ++i)
			{
				const TSharedPtr<FJsonValue>& Val = (*WidgetsArray)[i];
				if (!Val.IsValid() || Val->Type != EJson::Object) { continue; }

				FUMGJsonWidgetData Entry;
				if (ParseWidgetEntry(Val->AsObject(), Entry, i, Stats))
				{
					if (SeenNames.Contains(Entry.Name))
					{
						LogWarning(FString::Printf(
							TEXT("widgets[%d]: duplicate name '%s' — skipping."),
							i, *Entry.Name), &Stats);
						++Stats.SkippedCount;
						continue;
					}
					SeenNames.Add(Entry.Name);
					OutLayout.Widgets.Add(Entry);
				}
				else
				{
					++Stats.SkippedCount;
				}
			}
		}
	}

	Stats.TotalWidgets = OutLayout.Widgets.Num();

	UE_LOG(LogUMGJsonImporter, Log, TEXT("Parsed '%s': root=%s, %d widgets, %d warnings, %d skipped"),
		*OutLayout.WidgetName, *OutLayout.Root.Name, OutLayout.Widgets.Num(),
		Stats.WarningCount, Stats.SkippedCount);
	return true;
}

bool FUMGJsonImporter::ParseWidgetEntry(const TSharedPtr<FJsonObject>& Obj,
                                         FUMGJsonWidgetData& OutData, int32 Index,
                                         FUMGJsonImportStats& Stats)
{
	Obj->TryGetStringField(TEXT("type"), OutData.Type);
	Obj->TryGetStringField(TEXT("name"), OutData.Name);
	Obj->TryGetStringField(TEXT("parent"), OutData.Parent);
	Obj->TryGetStringField(TEXT("text"), OutData.Text);
	Obj->TryGetStringField(TEXT("color"), OutData.Color);

	// imagePath / texturePath (support both, texturePath takes priority)
	Obj->TryGetStringField(TEXT("imagePath"), OutData.ImagePath);
	{
		FString TexPath;
		if (Obj->TryGetStringField(TEXT("texturePath"), TexPath) && !TexPath.IsEmpty())
		{
			OutData.ImagePath = TexPath;
		}
	}

	// fontSize and zOrder
	{
		double FontSizeD = 12.0;
		if (Obj->TryGetNumberField(TEXT("fontSize"), FontSizeD))
		{
			OutData.FontSize = FMath::Max(1, (int32)FontSizeD);
		}
	}
	{
		double ZOrderD = 0.0;
		if (Obj->TryGetNumberField(TEXT("zOrder"), ZOrderD))
		{
			OutData.ZOrder = (int32)ZOrderD;
		}
	}

	// position
	{
		const TSharedPtr<FJsonObject>* PosObj;
		if (Obj->TryGetObjectField(TEXT("position"), PosObj))
		{
			OutData.bHasPosition = true;
			double X = 0.0, Y = 0.0;
			(*PosObj)->TryGetNumberField(TEXT("x"), X);
			(*PosObj)->TryGetNumberField(TEXT("y"), Y);
			OutData.Position.X = (float)X;
			OutData.Position.Y = (float)Y;
		}
	}

	// size
	{
		const TSharedPtr<FJsonObject>* SzObj;
		if (Obj->TryGetObjectField(TEXT("size"), SzObj))
		{
			OutData.bHasSize = true;
			double W = 100.0, H = 30.0;
			(*SzObj)->TryGetNumberField(TEXT("width"), W);
			(*SzObj)->TryGetNumberField(TEXT("height"), H);
			OutData.Size.Width = (float)W;
			OutData.Size.Height = (float)H;
		}
	}

	// anchor
	{
		const TSharedPtr<FJsonObject>* AnchorObj;
		if (Obj->TryGetObjectField(TEXT("anchor"), AnchorObj))
		{
			double MinX = 0.0, MinY = 0.0, MaxX = 0.0, MaxY = 0.0;
			(*AnchorObj)->TryGetNumberField(TEXT("minX"), MinX);
			(*AnchorObj)->TryGetNumberField(TEXT("minY"), MinY);
			(*AnchorObj)->TryGetNumberField(TEXT("maxX"), MaxX);
			(*AnchorObj)->TryGetNumberField(TEXT("maxY"), MaxY);
			OutData.Anchor.MinX = (float)MinX;
			OutData.Anchor.MinY = (float)MinY;
			OutData.Anchor.MaxX = (float)MaxX;
			OutData.Anchor.MaxY = (float)MaxY;
			OutData.bHasAnchor = true;
		}
	}

	// alignment
	{
		const TSharedPtr<FJsonObject>* AlignObj;
		if (Obj->TryGetObjectField(TEXT("alignment"), AlignObj))
		{
			double AX = 0.0, AY = 0.0;
			(*AlignObj)->TryGetNumberField(TEXT("x"), AX);
			(*AlignObj)->TryGetNumberField(TEXT("y"), AY);
			OutData.Alignment.X = (float)AX;
			OutData.Alignment.Y = (float)AY;
			OutData.bHasAlignment = true;
		}
	}

	// Text alignment
	Obj->TryGetStringField(TEXT("justify"), OutData.Justify);
	Obj->TryGetStringField(TEXT("verticalAlign"), OutData.VerticalAlign);

	// Button styles
	Obj->TryGetStringField(TEXT("normalColor"), OutData.NormalColor);
	Obj->TryGetStringField(TEXT("hoveredColor"), OutData.HoveredColor);
	Obj->TryGetStringField(TEXT("pressedColor"), OutData.PressedColor);
	Obj->TryGetStringField(TEXT("disabledColor"), OutData.DisabledColor);
	Obj->TryGetStringField(TEXT("textColor"), OutData.TextColor);

	// Image brush
	Obj->TryGetStringField(TEXT("tintColor"), OutData.TintColor);
	Obj->TryGetStringField(TEXT("drawAs"), OutData.DrawAs);

	// isVariable (explicit override; unset = auto by name prefix at build time)
	{
		bool bIsVar = false;
		if (Obj->TryGetBoolField(TEXT("isVariable"), bIsVar))
		{
			OutData.bIsVariable = bIsVar;
			OutData.bHasIsVariable = true;
		}
	}

	// ── Validation ────────────────────────────────────────────────────────────

	if (OutData.Type.IsEmpty())
	{
		LogWarning(FString::Printf(TEXT("widgets[%d]: missing 'type' — skipping."), Index), &Stats);
		return false;
	}
	if (OutData.Name.IsEmpty())
	{
		LogWarning(FString::Printf(TEXT("widgets[%d]: missing 'name' (type=%s) — skipping."),
			Index, *OutData.Type), &Stats);
		return false;
	}

	if (!GSupportedTypes.Contains(OutData.Type))
	{
		LogWarning(FString::Printf(TEXT("widgets[%d] '%s': unsupported type '%s' — skipping."),
			Index, *OutData.Name, *OutData.Type), &Stats);
		return false;
	}

	// Validate color hex format
	auto ValidateHex = [&](const FString& FieldName, FString& Value)
	{
		if (Value.IsEmpty()) { return; }
		if (!IsValidHexColor(Value))
		{
			LogWarning(FString::Printf(
				TEXT("widgets[%d] '%s': invalid %s '%s' (expected #RRGGBB or #RRGGBBAA) — using default."),
				Index, *OutData.Name, *FieldName, *Value), &Stats);
			Value = TEXT("");
		}
	};

	ValidateHex(TEXT("color"), OutData.Color);
	ValidateHex(TEXT("normalColor"), OutData.NormalColor);
	ValidateHex(TEXT("hoveredColor"), OutData.HoveredColor);
	ValidateHex(TEXT("pressedColor"), OutData.PressedColor);
	ValidateHex(TEXT("disabledColor"), OutData.DisabledColor);
	ValidateHex(TEXT("textColor"), OutData.TextColor);
	ValidateHex(TEXT("tintColor"), OutData.TintColor);

	// Validate justify
	if (!OutData.Justify.IsEmpty() &&
	    OutData.Justify != TEXT("left") && OutData.Justify != TEXT("center") &&
	    OutData.Justify != TEXT("right"))
	{
		LogWarning(FString::Printf(
			TEXT("widgets[%d] '%s': invalid justify '%s' (expected left/center/right) — using left."),
			Index, *OutData.Name, *OutData.Justify), &Stats);
		OutData.Justify = TEXT("left");
	}

	// Validate verticalAlign
	if (!OutData.VerticalAlign.IsEmpty() &&
	    OutData.VerticalAlign != TEXT("top") && OutData.VerticalAlign != TEXT("center") &&
	    OutData.VerticalAlign != TEXT("bottom"))
	{
		LogWarning(FString::Printf(
			TEXT("widgets[%d] '%s': invalid verticalAlign '%s' (expected top/center/bottom) — using top."),
			Index, *OutData.Name, *OutData.VerticalAlign), &Stats);
		OutData.VerticalAlign = TEXT("top");
	}

	// Validate drawAs
	if (!OutData.DrawAs.IsEmpty() &&
	    OutData.DrawAs != TEXT("image") && OutData.DrawAs != TEXT("box") &&
	    OutData.DrawAs != TEXT("border") && OutData.DrawAs != TEXT("roundedBox") &&
	    OutData.DrawAs != TEXT("none"))
	{
		LogWarning(FString::Printf(
			TEXT("widgets[%d] '%s': invalid drawAs '%s' — using 'image'."),
			Index, *OutData.Name, *OutData.DrawAs), &Stats);
		OutData.DrawAs = TEXT("image");
	}

	// Warn if CanvasPanel child has no position/size
	if (!OutData.Parent.IsEmpty())
	{
		const TSharedPtr<FJsonObject>* PosCheck;
		const TSharedPtr<FJsonObject>* SzCheck;
		bool bHasPos = Obj->TryGetObjectField(TEXT("position"), PosCheck);
		bool bHasSz  = Obj->TryGetObjectField(TEXT("size"), SzCheck);
		if (!bHasPos && !bHasSz)
		{
			// Not an error — widget may be parented to a box layout, not a CanvasPanel
		}
	}

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Overwrite handling
// ─────────────────────────────────────────────────────────────────────────────

FUMGJsonImporter::EOverwriteAction FUMGJsonImporter::HandleExistingAsset(
	const FString& AssetName, const FString& FullPackageName)
{
	const FText Title = FText::FromString(TEXT("UMG JSON Generator"));
	const FText Message = FText::FromString(FString::Printf(
		TEXT("Asset '%s' already exists at:\n%s\n\n"
		     "Yes = Update in place (rebuild layout, keep parent class + references)\n"
		     "No = Create copy with _N suffix\n"
		     "Cancel = Abort import"),
		*AssetName, *FullPackageName));

	EAppReturnType::Type Result = FMessageDialog::Open(
		EAppMsgType::YesNoCancel, Message, &Title);

	switch (Result)
	{
	case EAppReturnType::Yes:    return EOverwriteAction::UpdateInPlace;
	case EAppReturnType::No:     return EOverwriteAction::Duplicate;
	default:                     return EOverwriteAction::Cancel;
	}
}

bool FUMGJsonImporter::DeleteExistingAsset(const FString& FullPackageName,
                                            const FString& AssetName)
{
	UPackage* OldPackage = FindPackage(nullptr, *FullPackageName);
	if (OldPackage)
	{
		UObject* OldAsset = StaticFindObject(UObject::StaticClass(), OldPackage, *AssetName);
		if (OldAsset)
		{
			TArray<UObject*> ToDelete;
			ToDelete.Add(OldAsset);
			int32 Deleted = ObjectTools::ForceDeleteObjects(ToDelete, false);
			if (Deleted == 0)
			{
				LogError(TEXT("Failed to delete existing asset — it may be referenced."));
				return false;
			}
			UE_LOG(LogUMGJsonImporter, Log, TEXT("Deleted existing asset: %s"), *FullPackageName);
			return true;
		}
	}

	// File exists on disk but not loaded
	FString FilePath = FPackageName::LongPackageNameToFilename(
		FullPackageName, FPackageName::GetAssetPackageExtension());
	if (IFileManager::Get().FileExists(*FilePath))
	{
		IFileManager::Get().Delete(*FilePath, false, true, false);
		UE_LOG(LogUMGJsonImporter, Log, TEXT("Deleted asset file: %s"), *FilePath);
	}
	return true;
}

FString FUMGJsonImporter::FindUniqueName(const FString& BaseName, const FString& PackagePath)
{
	for (int32 i = 1; i < 100; ++i)
	{
		FString Candidate = FString::Printf(TEXT("%s_%d"), *BaseName, i);
		FString FullName = PackagePath / Candidate;
		if (!FPackageName::DoesPackageExist(FullName) && !FindPackage(nullptr, *FullName))
		{
			return Candidate;
		}
	}
	return BaseName + TEXT("_Copy");
}

// ─────────────────────────────────────────────────────────────────────────────
// Parent class resolution
// ─────────────────────────────────────────────────────────────────────────────

UClass* FUMGJsonImporter::ResolveParentClass(const FString& ParentClassPath,
                                              FUMGJsonImportStats& Stats)
{
	if (ParentClassPath.IsEmpty())
	{
		return nullptr;
	}

	UClass* Resolved = LoadClass<UUserWidget>(nullptr, *ParentClassPath);

	// Allow the short form "WTBR.WTBRGeneratedHUDWidget" (missing "/Script/")
	if (!Resolved && !ParentClassPath.StartsWith(TEXT("/")))
	{
		const FString ScriptPath = FString::Printf(TEXT("/Script/%s"), *ParentClassPath);
		Resolved = LoadClass<UUserWidget>(nullptr, *ScriptPath);
	}

	if (!Resolved)
	{
		LogWarning(FString::Printf(
			TEXT("parentClass '%s' not found or not a UUserWidget subclass — keeping default parent."),
			*ParentClassPath), &Stats);
	}
	return Resolved;
}

// ─────────────────────────────────────────────────────────────────────────────
// In-place widget tree reset
// ─────────────────────────────────────────────────────────────────────────────

void FUMGJsonImporter::ClearWidgetTree(UWidgetBlueprint* WBP)
{
	WBP->Modify();

	// Property bindings and animations reference widgets by name — after the tree
	// is rebuilt they would dangle (or silently rebind to a same-named widget), so
	// a regenerated WBP starts clean.
	if (WBP->Bindings.Num() > 0)
	{
		UE_LOG(LogUMGJsonImporter, Warning,
			TEXT("In-place update: discarding %d property binding(s) from the previous widget tree."),
			WBP->Bindings.Num());
		WBP->Bindings.Empty();
	}
	if (WBP->Animations.Num() > 0)
	{
		UE_LOG(LogUMGJsonImporter, Warning,
			TEXT("In-place update: discarding %d widget animation(s) from the previous widget tree."),
			WBP->Animations.Num());
		WBP->Animations.Empty();
	}

	UWidgetTree* WT = WBP->WidgetTree;
	if (!WT)
	{
		WBP->WidgetTree = NewObject<UWidgetTree>(WBP, TEXT("WidgetTree"));
		return;
	}

	// Move every old widget out of the tree so rebuilt widgets can reuse the
	// same names without collisions; GC collects the trashed ones later.
	TArray<UWidget*> OldWidgets;
	WT->GetAllWidgets(OldWidgets);
	WT->Modify();
	WT->RootWidget = nullptr;

	for (UWidget* Old : OldWidgets)
	{
		if (!Old) { continue; }
		Old->Rename(nullptr, GetTransientPackage(),
			REN_DontCreateRedirectors | REN_NonTransactional);
		Old->SetFlags(RF_Transient);
		Old->MarkAsGarbage();
	}

	UE_LOG(LogUMGJsonImporter, Log, TEXT("Cleared %d old widgets for in-place update."),
		OldWidgets.Num());
}

bool FUMGJsonImporter::CreateConvenienceBackup(UWidgetBlueprint* WBP,
	FString& OutBackupObjectPath)
{
	OutBackupObjectPath.Reset();
	if (!WBP)
	{
		return false;
	}

	const FString SourcePackageName = WBP->GetOutermost()->GetName();
	const FString SourceAssetName = WBP->GetName();
	const FString SourcePackagePath = FPackageName::GetLongPackagePath(SourcePackageName);
	const FString BackupPackagePath = SourcePackagePath / TEXT("Backups");
	const FString Timestamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S"));
	const FString BackupBaseName = FString::Printf(TEXT("%s_%s"),
		*SourceAssetName, *Timestamp);
	const FString BackupAssetName = FindUniqueName(BackupBaseName, BackupPackagePath);

	UWidgetBlueprint* BackupWBP = Cast<UWidgetBlueprint>(
		FAssetToolsModule::GetModule().Get().DuplicateAsset(
			BackupAssetName, BackupPackagePath, WBP));
	if (!BackupWBP)
	{
		UE_LOG(LogUMGJsonImporter, Error,
			TEXT("Backup FAILED: could not duplicate %s."), *WBP->GetPathName());
		return false;
	}

	if (!SaveWidgetBlueprint(BackupWBP, /*bIsNewAsset=*/false))
	{
		UE_LOG(LogUMGJsonImporter, Error,
			TEXT("Backup FAILED: duplicate could not be saved: %s."), *BackupWBP->GetPathName());
		return false;
	}

	OutBackupObjectPath = BackupWBP->GetPathName();
	UE_LOG(LogUMGJsonImporter, Log, TEXT("Backup Created: %s (convenience copy; not full rollback)"),
		*OutBackupObjectPath);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Asset creation
// ─────────────────────────────────────────────────────────────────────────────

UWidgetBlueprint* FUMGJsonImporter::CreateWidgetBlueprintAsset(FUMGJsonLayoutData& Layout,
                                                                FUMGJsonImportStats& Stats,
                                                                bool& bOutIsNewAsset,
	                                                            bool bForceUpdateInPlace,
	                                                            bool bCreateBackup)
{
	const FString PackagePath = TEXT("/Game/UI/Generated");
	FString AssetName = Layout.WidgetName;
	FString FullPackageName = PackagePath / AssetName;
	bOutIsNewAsset = true;

	UClass* RequestedParent = ResolveParentClass(Layout.ParentClassPath, Stats);

	// Check if asset already exists
	bool bExistsOnDisk = FPackageName::DoesPackageExist(FullPackageName);
	bool bExistsInMemory = (FindPackage(nullptr, *FullPackageName) != nullptr);

	if (bExistsOnDisk || bExistsInMemory)
	{
		EOverwriteAction Action = bForceUpdateInPlace
			? EOverwriteAction::UpdateInPlace
			: HandleExistingAsset(AssetName, FullPackageName);

		switch (Action)
		{
		case EOverwriteAction::Cancel:
			UE_LOG(LogUMGJsonImporter, Log, TEXT("Import cancelled by user."));
			Notify(TEXT("Import cancelled."), false);
			return nullptr;

		case EOverwriteAction::UpdateInPlace:
		{
			const FString ObjectPath = FullPackageName + TEXT(".") + AssetName;
			UWidgetBlueprint* ExistingWBP = LoadObject<UWidgetBlueprint>(nullptr, *ObjectPath);
			if (ExistingWBP)
			{
				if (bCreateBackup)
				{
					FString BackupObjectPath;
					if (!CreateConvenienceBackup(ExistingWBP, BackupObjectPath))
					{
						LogError(FString::Printf(
							TEXT("Update aborted: convenience backup failed for '%s'."),
							*ObjectPath));
						return nullptr;
					}
				}

				// Close any open editor so the designer doesn't hold stale tree pointers
				if (GEditor)
				{
					GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()
						->CloseAllEditorsForAsset(ExistingWBP);
				}

				ClearWidgetTree(ExistingWBP);

				// parentClass in JSON overrides; otherwise the existing parent is kept
				if (RequestedParent && ExistingWBP->ParentClass != RequestedParent)
				{
					ExistingWBP->ParentClass = RequestedParent;
					UE_LOG(LogUMGJsonImporter, Log, TEXT("Parent class set to '%s'."),
						*RequestedParent->GetPathName());
				}

				bOutIsNewAsset = false;
				UE_LOG(LogUMGJsonImporter, Log, TEXT("Updating asset in place: %s"),
					*FullPackageName);
				return ExistingWBP;
			}

			// Existing asset is not a loadable WidgetBlueprint — fall back to delete+recreate
			LogWarning(FString::Printf(
				TEXT("Existing asset '%s' is not a Widget Blueprint — deleting and recreating."),
				*FullPackageName), &Stats);
			if (!DeleteExistingAsset(FullPackageName, AssetName))
			{
				return nullptr;
			}
			break;
		}

		case EOverwriteAction::Duplicate:
			AssetName = FindUniqueName(AssetName, PackagePath);
			FullPackageName = PackagePath / AssetName;
			Layout.WidgetName = AssetName;
			UE_LOG(LogUMGJsonImporter, Log, TEXT("Creating duplicate: %s"), *FullPackageName);
			break;
		}
	}

	// Ensure output directory exists on disk
	const FString ContentDir = FPaths::ProjectContentDir() / TEXT("UI") / TEXT("Generated");
	if (!IFileManager::Get().DirectoryExists(*ContentDir))
	{
		IFileManager::Get().MakeDirectory(*ContentDir, true);
	}

	UPackage* Package = CreatePackage(*FullPackageName);
	if (!Package)
	{
		LogError(FString::Printf(TEXT("Failed to create package: %s"), *FullPackageName));
		return nullptr;
	}

	UBlueprint* NewBP = FKismetEditorUtilities::CreateBlueprint(
		RequestedParent ? RequestedParent : UUserWidget::StaticClass(),
		Package,
		FName(*AssetName),
		BPTYPE_Normal,
		UWidgetBlueprint::StaticClass(),
		UWidgetBlueprintGeneratedClass::StaticClass(),
		NAME_None);

	UWidgetBlueprint* WBP = Cast<UWidgetBlueprint>(NewBP);
	if (!WBP)
	{
		LogError(FString::Printf(TEXT("Failed to create Widget Blueprint: %s"), *FullPackageName));
		return nullptr;
	}

	if (!WBP->WidgetTree)
	{
		WBP->WidgetTree = NewObject<UWidgetTree>(WBP, TEXT("WidgetTree"));
	}

	WBP->SetFlags(RF_Public | RF_Standalone);

	UE_LOG(LogUMGJsonImporter, Log, TEXT("Created WBP asset: %s (parent: %s)"),
		*FullPackageName,
		RequestedParent ? *RequestedParent->GetName() : TEXT("UserWidget"));
	return WBP;
}

// ─────────────────────────────────────────────────────────────────────────────
// Widget tree construction
// ─────────────────────────────────────────────────────────────────────────────

// Binding-candidate prefixes get bIsVariable automatically (BindWidget/BP access)
static bool ShouldAutoExposeAsVariable(const FString& Name)
{
	return Name.StartsWith(TEXT("Txt_")) || Name.StartsWith(TEXT("PB_")) ||
	       Name.StartsWith(TEXT("Img_")) || Name.StartsWith(TEXT("Btn_"));
}

void FUMGJsonImporter::BuildWidgetTree(UWidgetBlueprint* WBP,
                                        const FUMGJsonLayoutData& Layout,
                                        FUMGJsonImportStats& Stats)
{
	UWidgetTree* WT = WBP->WidgetTree;
	check(WT);

	WBP->Modify();
	WT->Modify();

	TMap<FString, UWidget*> WidgetMap;

	// Build root widget
	FUMGJsonWidgetData RootData;
	RootData.Type = Layout.Root.Type;
	RootData.Name = Layout.Root.Name;
	RootData.Size = Layout.Resolution;

	UWidget* RootWidget = FUMGWidgetFactory::CreateWidget(WT, RootData);
	if (!RootWidget)
	{
		LogError(FString::Printf(TEXT("Failed to create root widget of type '%s'."),
			*RootData.Type));
		return;
	}

	WT->RootWidget = RootWidget;
	WidgetMap.Add(RootData.Name, RootWidget);
	UE_LOG(LogUMGJsonImporter, Log, TEXT("Root widget: '%s' (%s)"),
		*RootData.Name, *RootData.Type);

	// Build child widgets
	for (const FUMGJsonWidgetData& Data : Layout.Widgets)
	{
		UWidget* NewWidget = FUMGWidgetFactory::CreateWidget(WT, Data);
		if (!NewWidget)
		{
			LogWarning(FString::Printf(TEXT("Failed to create widget '%s' (type: '%s') — skipped."),
				*Data.Name, *Data.Type), &Stats);
			++Stats.SkippedCount;
			continue;
		}

		const FString ParentName = Data.Parent.IsEmpty() ? Layout.Root.Name : Data.Parent;
		UWidget** ParentPtr = WidgetMap.Find(ParentName);
		if (!ParentPtr || !(*ParentPtr))
		{
			LogWarning(FString::Printf(TEXT("Parent '%s' not found for widget '%s' — skipped."),
				*ParentName, *Data.Name), &Stats);
			++Stats.SkippedCount;
			continue;
		}

		UPanelWidget* ParentPanel = Cast<UPanelWidget>(*ParentPtr);
		if (!ParentPanel)
		{
			LogWarning(FString::Printf(
				TEXT("Parent '%s' is not a panel — cannot add child '%s'."),
				*ParentName, *Data.Name), &Stats);
			++Stats.SkippedCount;
			continue;
		}

		if (Cast<UCanvasPanel>(ParentPanel))
		{
			if (!Data.bHasPosition)
			{
				LogWarning(FString::Printf(
					TEXT("Widget '%s' has CanvasPanel parent '%s' but is missing 'position' - using default {x:0,y:0}."),
					*Data.Name, *ParentName), &Stats);
			}
			if (!Data.bHasSize)
			{
				LogWarning(FString::Printf(
					TEXT("Widget '%s' has CanvasPanel parent '%s' but is missing 'size' - using default {width:100,height:30}."),
					*Data.Name, *ParentName), &Stats);
			}
		}

		FUMGWidgetFactory::AttachToParent(ParentPanel, NewWidget, Data);
		NewWidget->bIsVariable = Data.bHasIsVariable
			? Data.bIsVariable
			: ShouldAutoExposeAsVariable(Data.Name);
		WidgetMap.Add(Data.Name, NewWidget);
		++Stats.SuccessCount;
	}

	UE_LOG(LogUMGJsonImporter, Log, TEXT("Widget tree built: %d success, %d skipped."),
		Stats.SuccessCount, Stats.SkippedCount);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);
	WBP->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WBP);
}

// ─────────────────────────────────────────────────────────────────────────────
// Save + open
// ─────────────────────────────────────────────────────────────────────────────

bool FUMGJsonImporter::SaveWidgetBlueprint(UWidgetBlueprint* WBP, bool bIsNewAsset)
{
	UPackage* Package = WBP->GetPackage();

	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		Package->GetName(),
		FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.bForceByteSwapping = false;
	SaveArgs.bWarnOfLongFilename = true;

	const FSavePackageResultStruct Result = UPackage::Save(
		Package, WBP, *PackageFilename, SaveArgs);

	if (Result.IsSuccessful())
	{
		if (bIsNewAsset)
		{
			FAssetRegistryModule::AssetCreated(WBP);
		}
		UE_LOG(LogUMGJsonImporter, Log, TEXT("Saved: %s"), *PackageFilename);
		return true;
	}

	UE_LOG(LogUMGJsonImporter, Warning,
		TEXT("SavePackage returned failure for %s — asset may still be usable in-editor."),
		*PackageFilename);
	return false;
}

void FUMGJsonImporter::SaveAndOpen(UWidgetBlueprint* WBP, bool bIsNewAsset,
	bool bOpenEditor)
{
	SaveWidgetBlueprint(WBP, bIsNewAsset);

	if (bOpenEditor && GEditor)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(WBP);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Log / notify helpers
// ─────────────────────────────────────────────────────────────────────────────

void FUMGJsonImporter::LogError(const FString& Msg)
{
	UE_LOG(LogUMGJsonImporter, Error, TEXT("[UMGJsonImporter] %s"), *Msg);
	Notify(Msg, false);
}

void FUMGJsonImporter::LogWarning(const FString& Msg, FUMGJsonImportStats* Stats)
{
	UE_LOG(LogUMGJsonImporter, Warning, TEXT("[UMGJsonImporter] %s"), *Msg);
	if (Stats)
	{
		Stats->AddWarning(Msg);
	}
}

void FUMGJsonImporter::Notify(const FString& Msg, bool bSuccess)
{
	FNotificationInfo Info(FText::FromString(Msg));
	Info.bFireAndForget = true;
	Info.ExpireDuration = 5.0f;

	TSharedPtr<SNotificationItem> Item =
		FSlateNotificationManager::Get().AddNotification(Info);
	if (Item.IsValid())
	{
		Item->SetCompletionState(bSuccess
			? SNotificationItem::CS_Success
			: SNotificationItem::CS_Fail);
	}
}

void FUMGJsonImporter::NotifySummary(const FString& AssetPath,
                                      const FUMGJsonImportStats& Stats)
{
	FString Summary = FString::Printf(
		TEXT("UMG JSON Import Complete\n"
		     "Asset: %s\n"
		     "Widgets created: %d\n"
		     "Warnings: %d\n"
		     "Skipped: %d"),
		*AssetPath,
		Stats.SuccessCount,
		Stats.WarningCount,
		Stats.SkippedCount);

	const bool bSuccess = (Stats.SkippedCount == 0);
	Notify(Summary, bSuccess);

	// Log all warnings to Output Log for reference
	if (Stats.Warnings.Num() > 0)
	{
		UE_LOG(LogUMGJsonImporter, Log, TEXT("=== Import Warnings ==="));
		for (const FString& W : Stats.Warnings)
		{
			UE_LOG(LogUMGJsonImporter, Warning, TEXT("  %s"), *W);
		}
	}
}

bool FUMGJsonImporter::IsValidHexColor(const FString& Value)
{
	if (Value.IsEmpty())
	{
		return false;
	}

	const FString Clean = Value.StartsWith(TEXT("#")) ? Value.Mid(1) : Value;
	if (Clean.Len() != 6 && Clean.Len() != 8)
	{
		return false;
	}

	for (int32 Index = 0; Index < Clean.Len(); ++Index)
	{
		const TCHAR Character = Clean[Index];
		const bool bIsHex =
			(Character >= TCHAR('0') && Character <= TCHAR('9')) ||
			(Character >= TCHAR('A') && Character <= TCHAR('F')) ||
			(Character >= TCHAR('a') && Character <= TCHAR('f'));
		if (!bIsHex)
		{
			return false;
		}
	}

	return true;
}

FLinearColor FUMGJsonImporter::ParseHexColor(const FString& Hex)
{
	if (Hex.IsEmpty()) { return FLinearColor::White; }
	FString Clean = Hex.StartsWith(TEXT("#")) ? Hex.Mid(1) : Hex;
	return FLinearColor(FColor::FromHex(Clean));
}
