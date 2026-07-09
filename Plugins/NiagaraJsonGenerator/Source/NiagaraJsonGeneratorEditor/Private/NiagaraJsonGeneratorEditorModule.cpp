#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "NiagaraJsonSpikeImporter.h"

DEFINE_LOG_CATEGORY_STATIC(LogNiagaraJsonGenerator, Log, All);

// Console-command entry point so the import can also run without the file
// dialog (and later from automation): WTBR.Niagara.ImportJson <path-to-json>
static FAutoConsoleCommand GImportNiagaraJsonCommand(
	TEXT("WTBR.Niagara.ImportJson"),
	TEXT("Import a Niagara JSON spec (spike). Usage: WTBR.Niagara.ImportJson <AbsolutePathToJson>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogNiagaraJsonGenerator, Error,
				TEXT("Usage: WTBR.Niagara.ImportJson <AbsolutePathToJson>"));
			return;
		}
		// Unquoted paths with spaces arrive as multiple args — rejoin them.
		FString FilePath = FString::Join(Args, TEXT(" "));
		FilePath = FilePath.TrimQuotes();
		FNiagaraJsonSpikeImporter::ImportFromFile(FilePath);
	}));

// Read-back validation: WTBR.Niagara.DumpUserParams /Game/Path/To/NiagaraSystem
static FAutoConsoleCommand GDumpNiagaraUserParamsCommand(
	TEXT("WTBR.Niagara.DumpUserParams"),
	TEXT("Log all exposed User Parameters (name, type, value) of a NiagaraSystem asset. Usage: WTBR.Niagara.DumpUserParams <ContentPath>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogNiagaraJsonGenerator, Error,
				TEXT("Usage: WTBR.Niagara.DumpUserParams <ContentPath>"));
			return;
		}
		FNiagaraJsonSpikeImporter::DumpUserParams(Args[0].TrimQuotes());
	}));

class FNiagaraJsonGeneratorEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(
				this, &FNiagaraJsonGeneratorEditorModule::RegisterMenus));
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

		UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(
			TEXT("LevelEditor.MainMenu.Tools"));

		FToolMenuSection& Section = ToolsMenu->FindOrAddSection(
			TEXT("NiagaraJsonGenerator"));

		Section.AddMenuEntry(
			TEXT("ImportNiagaraJson"),
			FText::FromString(TEXT("Import Niagara JSON (Spike)...")),
			FText::FromString(TEXT("Duplicate a Niagara System template and set User Parameter defaults from a JSON spec.")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(
				this, &FNiagaraJsonGeneratorEditorModule::OnImportClicked)));
	}

	void OnImportClicked()
	{
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		if (!DesktopPlatform)
		{
			UE_LOG(LogNiagaraJsonGenerator, Error, TEXT("DesktopPlatform module not available."));
			return;
		}

		TArray<FString> OutFiles;
		const void* ParentWindowHandle =
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

		const bool bOpened = DesktopPlatform->OpenFileDialog(
			ParentWindowHandle,
			TEXT("Select Niagara JSON Spec File"),
			FPaths::ProjectDir(),
			TEXT(""),
			TEXT("JSON Files (*.json)|*.json"),
			EFileDialogFlags::None,
			OutFiles);

		if (bOpened && OutFiles.Num() > 0)
		{
			FNiagaraJsonSpikeImporter::ImportFromFile(OutFiles[0]);
		}
	}
};

IMPLEMENT_MODULE(FNiagaraJsonGeneratorEditorModule, NiagaraJsonGeneratorEditor)
