#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "UMGJsonImporter.h"

DEFINE_LOG_CATEGORY_STATIC(LogUMGJsonGenerator, Log, All);

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
