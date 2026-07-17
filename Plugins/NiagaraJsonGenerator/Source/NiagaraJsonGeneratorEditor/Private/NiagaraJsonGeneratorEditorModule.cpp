#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "NiagaraJsonSpikeImporter.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SWindow.h"

DEFINE_LOG_CATEGORY_STATIC(LogNiagaraJsonGenerator, Log, All);

// Console-command entry point so the import can also run without the file
// dialog (and later from automation): WTBR.Niagara.ImportJson <path-to-json>
static FAutoConsoleCommand GImportNiagaraJsonCommand(
	TEXT("WTBR.Niagara.ImportJson"),
	TEXT("Import a Niagara JSON spec. Usage: WTBR.Niagara.ImportJson <AbsolutePathToJson>"),
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

static FAutoConsoleCommand GValidateNiagaraJsonCommand(
	TEXT("WTBR.Niagara.ValidateJson"),
	TEXT("Validate a Niagara JSON spec without modifying assets. Usage: WTBR.Niagara.ValidateJson <AbsolutePathToJson>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogNiagaraJsonGenerator, Error,
				TEXT("Usage: WTBR.Niagara.ValidateJson <AbsolutePathToJson>"));
			return;
		}
		FString FilePath = FString::Join(Args, TEXT(" "));
		FNiagaraJsonSpikeImporter::ValidateFile(FilePath.TrimQuotes());
	}));

static FAutoConsoleCommand GImportNiagaraJsonBatchCommand(
	TEXT("WTBR.Niagara.ImportBatch"),
	TEXT("Import or validate a Niagara JSON batch manifest. Usage: WTBR.Niagara.ImportBatch <AbsolutePathToJson>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogNiagaraJsonGenerator, Error,
				TEXT("Usage: WTBR.Niagara.ImportBatch <AbsolutePathToJson>"));
			return;
		}
		FString FilePath = FString::Join(Args, TEXT(" "));
		FNiagaraJsonSpikeImporter::ImportBatchFromFile(FilePath.TrimQuotes());
	}));

static FAutoConsoleCommand GAutoBindSniperVFXCommand(
	TEXT("WTBR.Niagara.AutoBindSniper"),
	TEXT("Bind generated VFX to WTBR Sniper Trigger Data Assets. Usage: WTBR.Niagara.AutoBindSniper <AbsolutePathToJson>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogNiagaraJsonGenerator, Error,
				TEXT("Usage: WTBR.Niagara.AutoBindSniper <AbsolutePathToJson>"));
			return;
		}
		FString FilePath = FString::Join(Args, TEXT(" "));
		FNiagaraJsonSpikeImporter::AutoBindSniperFromFile(FilePath.TrimQuotes());
	}));

static FAutoConsoleCommand GAuditNiagaraSystemCommand(
	TEXT("WTBR.Niagara.AuditSystem"),
	TEXT("Report static Niagara complexity without modifying the asset. Usage: WTBR.Niagara.AuditSystem <ContentPath>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogNiagaraJsonGenerator, Error,
				TEXT("Usage: WTBR.Niagara.AuditSystem <ContentPath>"));
			return;
		}
		FNiagaraJsonSpikeImporter::AuditSystemPerformance(Args[0].TrimQuotes());
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
			FText::FromString(TEXT("Import Niagara JSON...")),
			FText::FromString(TEXT("Duplicate a Niagara System template and set User Parameter defaults from a JSON spec.")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(
				this, &FNiagaraJsonGeneratorEditorModule::OnImportClicked)));

		Section.AddMenuEntry(
			TEXT("ValidateNiagaraJson"),
			FText::FromString(TEXT("Validate Niagara JSON...")),
			FText::FromString(TEXT("Check a Niagara JSON spec and template contract without modifying assets.")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(
				this, &FNiagaraJsonGeneratorEditorModule::OnValidateClicked)));

		Section.AddMenuEntry(
			TEXT("ImportNiagaraJsonBatch"),
			FText::FromString(TEXT("Import Niagara JSON Batch...")),
			FText::FromString(TEXT("Import or validate multiple Niagara JSON specs from one manifest.")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(
				this, &FNiagaraJsonGeneratorEditorModule::OnImportBatchClicked)));

		Section.AddMenuEntry(
			TEXT("AutoBindSniperVFX"),
			FText::FromString(TEXT("Auto-Bind Sniper VFX...")),
			FText::FromString(TEXT("Bind generated trail and impact Niagara assets to WTBR Sniper Trigger Data Assets.")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(
				this, &FNiagaraJsonGeneratorEditorModule::OnAutoBindSniperClicked)));

		Section.AddMenuEntry(
			TEXT("GenerateNiagaraPreset"),
			FText::FromString(TEXT("Generate VFX Preset...")),
			FText::FromString(TEXT("Create a strict Niagara variant from Color, Energy, Speed, and Spark Count without editing JSON.")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(
				this, &FNiagaraJsonGeneratorEditorModule::OnGeneratePresetClicked)));
	}

	bool OpenJsonDialog(const FString& Title, FString& OutFilePath)
	{
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		if (!DesktopPlatform)
		{
			UE_LOG(LogNiagaraJsonGenerator, Error, TEXT("DesktopPlatform module not available."));
			return false;
		}

		TArray<FString> OutFiles;
		const void* ParentWindowHandle =
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

		const bool bOpened = DesktopPlatform->OpenFileDialog(
			ParentWindowHandle,
			Title,
			FPaths::ProjectDir(),
			TEXT(""),
			TEXT("JSON Files (*.json)|*.json"),
			EFileDialogFlags::None,
			OutFiles);

		if (bOpened && OutFiles.Num() > 0)
		{
			OutFilePath = OutFiles[0];
			return true;
		}
		return false;
	}

	void OnImportClicked()
	{
		FString FilePath;
		if (OpenJsonDialog(TEXT("Select Niagara JSON Spec File"), FilePath))
		{
			FNiagaraJsonSpikeImporter::ImportFromFile(FilePath);
		}
	}

	void OnValidateClicked()
	{
		FString FilePath;
		if (OpenJsonDialog(TEXT("Select Niagara JSON Spec to Validate"), FilePath))
		{
			FNiagaraJsonSpikeImporter::ValidateFile(FilePath);
		}
	}

	void OnImportBatchClicked()
	{
		FString FilePath;
		if (OpenJsonDialog(TEXT("Select Niagara JSON Batch Manifest"), FilePath))
		{
			FNiagaraJsonSpikeImporter::ImportBatchFromFile(FilePath);
		}
	}

	void OnAutoBindSniperClicked()
	{
		FString FilePath;
		if (OpenJsonDialog(TEXT("Select WTBR Sniper VFX Binding Manifest"), FilePath))
		{
			FNiagaraJsonSpikeImporter::AutoBindSniperFromFile(FilePath);
		}
	}

	void OnGeneratePresetClicked()
	{
		TSharedRef<SEditableTextBox> TemplateInput = SNew(SEditableTextBox)
			.Text(FText::FromString(TEXT("/Game/VFX/Templates/NS_Template_Burst")));
		TSharedRef<SEditableTextBox> OutputInput = SNew(SEditableTextBox)
			.Text(FText::FromString(TEXT("/Game/VFX/Generated/NS_NewPreset")));
		TSharedRef<SEditableTextBox> EnergyInput = SNew(SEditableTextBox)
			.Text(FText::FromString(TEXT("1.0")));
		TSharedRef<SEditableTextBox> SpeedInput = SNew(SEditableTextBox)
			.Text(FText::FromString(TEXT("100.0")));
		TSharedRef<SEditableTextBox> SparkInput = SNew(SEditableTextBox)
			.Text(FText::FromString(TEXT("32")));
		TSharedRef<FLinearColor> PresetColor = MakeShared<FLinearColor>(
			FLinearColor(0.13f, 0.85f, 1.0f, 1.0f));

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(FText::FromString(TEXT("WTBR VFX Preset Editor")))
			.ClientSize(FVector2D(560.0f, 340.0f))
			.SupportsMinimize(false)
			.SupportsMaximize(false);

		Window->SetContent(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(12.0f)
			[
				SNew(STextBlock).Text(FText::FromString(
					TEXT("The template must expose User.Color (color), User.Energy (float), User.Speed (float), and User.SparkCount (int).")))
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 4.0f)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("Template path")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 0.0f, 12.0f, 6.0f)
			[
				TemplateInput
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 4.0f)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("Output path")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 0.0f, 12.0f, 6.0f)
			[
				OutputInput
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("Energy")))]
					+ SVerticalBox::Slot().AutoHeight()[EnergyInput]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(6.0f, 0.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("Speed")))]
					+ SVerticalBox::Slot().AutoHeight()[SpeedInput]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("Spark count")))]
					+ SVerticalBox::Slot().AutoHeight()[SparkInput]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(12.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("Color")))]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox).WidthOverride(52.0f).HeightOverride(22.0f)
						[
							SNew(SColorBlock)
							.Color_Lambda([PresetColor]() { return *PresetColor; })
							.OnMouseButtonDown_Lambda([PresetColor](const FGeometry&, const FPointerEvent&)
							{
								FColorPickerArgs Args;
								Args.InitialColorOverride = *PresetColor;
								Args.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda(
									[PresetColor](FLinearColor NewColor) { *PresetColor = NewColor; });
								OpenColorPicker(Args);
								return FReply::Handled();
							})
						]
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 14.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Generate and Open Niagara Asset")))
				.OnClicked_Lambda([TemplateInput, OutputInput, EnergyInput, SpeedInput, SparkInput, PresetColor, Window]()
				{
					FNiagaraJsonSpikeImporter::GeneratePreset(
						TemplateInput->GetText().ToString().TrimQuotes(),
						OutputInput->GetText().ToString().TrimQuotes(),
						*PresetColor,
						FCString::Atof(*EnergyInput->GetText().ToString()),
						FCString::Atof(*SpeedInput->GetText().ToString()),
						FCString::Atoi(*SparkInput->GetText().ToString()));
					Window->RequestDestroyWindow();
					return FReply::Handled();
				})
			]);

		FSlateApplication::Get().AddWindow(Window);
	}
};

IMPLEMENT_MODULE(FNiagaraJsonGeneratorEditorModule, NiagaraJsonGeneratorEditor)
