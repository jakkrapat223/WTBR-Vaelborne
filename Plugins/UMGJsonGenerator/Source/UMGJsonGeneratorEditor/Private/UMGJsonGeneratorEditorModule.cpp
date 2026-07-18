#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "UMGJsonImporter.h"

DEFINE_LOG_CATEGORY_STATIC(LogUMGJsonGenerator, Log, All);

// Sandboxed structural re-import test. The first argument is a content path;
// all remaining arguments are rejoined so unquoted JSON paths with spaces work.
static FAutoConsoleCommand GReimportUMGProbeCommand(
	TEXT("WTBR.UMG.ReimportProbe"),
	TEXT("Duplicate a WBP, run the real JSON in-place rebuild on the duplicate, report the diff, and delete it. Usage: WTBR.UMG.ReimportProbe <WBPContentPath> <AbsolutePathToJson>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogUMGJsonGenerator, Error,
				TEXT("Usage: WTBR.UMG.ReimportProbe <WBPContentPath> <AbsolutePathToJson>"));
			return;
		}

		const FString SourceAssetPath = Args[0].TrimQuotes();
		TArray<FString> JsonPathParts;
		for (int32 Index = 1; Index < Args.Num(); ++Index)
		{
			JsonPathParts.Add(Args[Index]);
		}
		const FString JsonFilePath = FString::Join(JsonPathParts, TEXT(" ")).TrimQuotes();
		FUMGJsonImporter::RunReimportProbe(SourceAssetPath, JsonFilePath);
	}));

// Non-interactive production import.  The importer performs its normal
// convenience backup before rebuilding the existing asset in place.
static FAutoConsoleCommand GImportUMGJsonCommand(
	TEXT("WTBR.UMG.ImportJson"),
	TEXT("Rebuild an existing UMG JSON asset in place with a convenience backup. Usage: WTBR.UMG.ImportJson <PathToJson>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogUMGJsonGenerator, Error,
				TEXT("Usage: WTBR.UMG.ImportJson <PathToJson>"));
			return;
		}

		FString JsonFilePath = FString::Join(Args, TEXT(" ")).TrimQuotes();
		if (FPaths::IsRelative(JsonFilePath))
		{
			JsonFilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), JsonFilePath);
		}
		FPaths::NormalizeFilename(JsonFilePath);
		if (!FUMGJsonImporter::ImportUpdateInPlaceFromFile(JsonFilePath))
		{
			UE_LOG(LogUMGJsonGenerator, Error,
				TEXT("UMG JSON import failed: %s"), *JsonFilePath);
		}
	}));

// Read-only post-import verification for commandlet workflows.
static FAutoConsoleCommand GReportUMGJsonImportCommand(
	TEXT("WTBR.UMG.ReportImport"),
	TEXT("Report Blueprint compile state and widget counts. Usage: WTBR.UMG.ReportImport <WBPContentPath>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() != 1)
		{
			UE_LOG(LogUMGJsonGenerator, Error,
				TEXT("Usage: WTBR.UMG.ReportImport <WBPContentPath>"));
			return;
		}

		FString PackageName = Args[0].TrimQuotes();
		if (PackageName.Contains(TEXT(".")))
		{
			PackageName = FPackageName::ObjectPathToPackageName(PackageName);
		}
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(
			nullptr, *(PackageName + TEXT(".") + AssetName));
		if (!WBP || !WBP->WidgetTree)
		{
			UE_LOG(LogUMGJsonGenerator, Error,
				TEXT("UMG JSON import report failed to load: %s"), *PackageName);
			return;
		}

		TArray<UWidget*> Widgets;
		WBP->WidgetTree->GetAllWidgets(Widgets);
		int32 VariableWidgetCount = 0;
		for (const UWidget* Widget : Widgets)
		{
			VariableWidgetCount += Widget && Widget->bIsVariable ? 1 : 0;
		}

		const TCHAR* CompileStatus = WBP->Status == BS_Error ? TEXT("Error")
			: (WBP->Status == BS_UpToDateWithWarnings ? TEXT("Warning")
				: (WBP->Status == BS_UpToDate ? TEXT("Success") : TEXT("Dirty")));
		UE_LOG(LogUMGJsonGenerator, Log,
			TEXT("UMG JSON Import Verification: Asset=%s CompileStatus=%s BlueprintStatus=%d TotalWidgets=%d VariableWidgets=%d"),
			*WBP->GetPathName(), CompileStatus, static_cast<int32>(WBP->Status),
			Widgets.Num(), VariableWidgetCount);
	}));

class FUMGJsonGeneratorEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(
				this, &FUMGJsonGeneratorEditorModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	}

private:
	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);

		// Add entry under Tools menu
		UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(
			TEXT("LevelEditor.MainMenu.Tools"));

		FToolMenuSection& Section = ToolsMenu->FindOrAddSection(
			TEXT("UMGJsonGenerator"));

		Section.AddMenuEntry(
			TEXT("ImportUMGJson"),
			FText::FromString(TEXT("Import UMG JSON...")),
			FText::FromString(TEXT("Import a JSON layout file and generate a Widget Blueprint.")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(
				this, &FUMGJsonGeneratorEditorModule::OnImportClicked)));

		// Also add a toolbar button
		UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu(
			TEXT("LevelEditor.LevelEditorToolBar.PlayToolBar"));

		FToolMenuSection& ToolbarSection = Toolbar->FindOrAddSection(
			TEXT("UMGJsonGeneratorToolbar"));

		ToolbarSection.AddEntry(
			FToolMenuEntry::InitToolBarButton(
				TEXT("ImportUMGJsonButton"),
				FUIAction(FExecuteAction::CreateRaw(
					this, &FUMGJsonGeneratorEditorModule::OnImportClicked)),
				FText::FromString(TEXT("Import UMG JSON")),
				FText::FromString(TEXT("Import a JSON layout file and generate a Widget Blueprint.")),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Blueprint"))));
	}

	void OnImportClicked()
	{
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		if (!DesktopPlatform)
		{
			UE_LOG(LogUMGJsonGenerator, Error, TEXT("DesktopPlatform module not available."));
			return;
		}

		TArray<FString> OutFiles;
		const void* ParentWindowHandle =
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

		const bool bOpened = DesktopPlatform->OpenFileDialog(
			ParentWindowHandle,
			TEXT("Select UMG JSON Layout File"),
			FPaths::ProjectDir(),
			TEXT(""),
			TEXT("JSON Files (*.json)|*.json"),
			EFileDialogFlags::None,
			OutFiles);

		if (bOpened && OutFiles.Num() > 0)
		{
			FUMGJsonImporter::ImportFromFile(OutFiles[0]);
		}
	}
};

IMPLEMENT_MODULE(FUMGJsonGeneratorEditorModule, UMGJsonGeneratorEditor)
