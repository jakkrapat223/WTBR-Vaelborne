#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
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
