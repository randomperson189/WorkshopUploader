// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "WorkshopUploader.h"
#include "WorkshopUploaderStyle.h"
#include "WorkshopUploaderCommands.h"
#include "LevelEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Misc/MessageDialog.h"
#include "Misc/CommandLine.h"
#include "Async/Async.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include <string>

#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SExpandableArea.h"

static const FName WorkshopUploaderTabName("Workshop Uploader");

#define LOCTEXT_NAMESPACE "FWorkshopUploaderModule"

/* Default plugin stuff */

void FWorkshopUploaderModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FWorkshopUploaderStyle::Initialize();
	FWorkshopUploaderStyle::ReloadTextures();

	FWorkshopUploaderCommands::Register();
	
	// Define text styles
	UploadProgressStyle = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
	UploadProgressStyle.SetColorAndOpacity(FSlateColor(FLinearColor::Yellow));

	UploadSuccessStyle = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
	UploadSuccessStyle.SetColorAndOpacity(FSlateColor(FLinearColor::Green));

	UploadFailureStyle = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
	UploadFailureStyle.SetColorAndOpacity(FSlateColor(FLinearColor::Red));

	// Rest of plugin code
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FWorkshopUploaderCommands::Get().OpenWorkshopUploaderWindow,
		FExecuteAction::CreateRaw(this, &FWorkshopUploaderModule::PluginButtonClicked),
		FCanExecuteAction());
		
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

#if ENGINE_MAJOR_VERSION >= 5
	FName MenuSection = "FileProject";
	FName ToolbarSection = "Content";
#else
	FName MenuSection = "FileProject";
	FName ToolbarSection = "Misc";
#endif

	{
		TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
		MenuExtender->AddMenuExtension(MenuSection, EExtensionHook::After, PluginCommands, FMenuExtensionDelegate::CreateRaw(this, &FWorkshopUploaderModule::AddMenuExtension));

		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
	}
	
	{
		TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
		ToolbarExtender->AddToolBarExtension(ToolbarSection, EExtensionHook::After, PluginCommands, FToolBarExtensionDelegate::CreateRaw(this, &FWorkshopUploaderModule::AddToolbarExtension));
		
		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
	}
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(WorkshopUploaderTabName, FOnSpawnTab::CreateRaw(this, &FWorkshopUploaderModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FWorkshopUploaderTabTitle", "Workshop Uploader"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	// Set a default size for this tab
	FVector2D DefaultSize(430.0f, 670.0f);
	FTabManager::RegisterDefaultTabWindowSize(WorkshopUploaderTabName, DefaultSize);

	TickDelegate = FTickerDelegate::CreateRaw(this, &FWorkshopUploaderModule::Tick);

#if ENGINE_MAJOR_VERSION >= 5
	TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(TickDelegate);
#else
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(TickDelegate);
#endif
}

bool FWorkshopUploaderModule::Tick(float DeltaTime)
{
	SteamAPI_RunCallbacks();
	
	return true;
}

void FWorkshopUploaderModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FWorkshopUploaderStyle::Shutdown();

	FWorkshopUploaderCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(WorkshopUploaderTabName);

#if ENGINE_MAJOR_VERSION >= 5
	FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
#else
	FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
#endif
}

TSharedRef<SDockTab> FWorkshopUploaderModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	// The workshop uploader can't function without SteamUGC which requires steam to be running
	if (SteamUGC() == nullptr)
	{
		FText WidgetText = LOCTEXT("SteamNotRunning", "Steam needs to be running in order for the workshop uploader to function, please make sure Steam is running and then restart the editor.");

		return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(WidgetText)
				.AutoWrapText(true)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
			]
		];
	}

	// Reset values to default
	NewModTitle = "My Mod Title";
	NewModDescription = "My Mod Description";
	NewModTags.Empty();
	NewModThumbnail = "";
	NewModPackage = "";

	UpdateModWorkshopId = 0;
	UpdateModTitle = "";
	UpdateModDescription = "";
	UpdateModTags.Empty();
	UpdateModThumbnail = "";
	UpdateModPackage = "";
	UpdateModChangeNote = "Updated.";

	SelectedNewModOption = nullptr;
	SelectedUpdateModOption = nullptr;

	FindPackagedPlugins();

	auto CreateTagCheckboxes = [this](bool IsUpdateMod = false)
	{
		TArray<FString>& TagsArray = IsUpdateMod ? UpdateModTags : NewModTags;
		TSharedRef<SVerticalBox> TagsVerticalBox = SNew(SVerticalBox);

		for (const FString& Tag : DefaultTags)
		{
			TagsVerticalBox->AddSlot()
			.AutoHeight()
			.Padding(0, 2)
			[
				SNew(SCheckBox)
				.OnCheckStateChanged_Lambda([this, IsUpdateMod, &TagsArray, Tag](ECheckBoxState NewState)
				{
					if (NewState == ECheckBoxState::Checked)
					{
						if (!TagsArray.Contains(Tag))
							TagsArray.Add(Tag);
					}
					else
					{
						TagsArray.Remove(Tag);
					}

					UE_LOG(LogTemp, Warning, TEXT("%s changed: "), IsUpdateMod ? TEXT("UpdateModTags") : TEXT("NewModTags"));

					for (const FString& TagInArray : TagsArray)
						UE_LOG(LogTemp, Warning, TEXT("%s"), *TagInArray);
				})
				.Content()
				[
					SNew(STextBlock)
					.Text(FText::FromString(Tag))
				]
			];
		}
		return TagsVerticalBox;
	};

	return SNew(SDockTab)
	.TabRole(ETabRole::NomadTab)
	[
		// Put your tab content here!

		SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSpacer)
				.Size(FVector2D(0.0f, 20.0f))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("UploadNewItem", "Upload New Workshop Item"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSpacer)
				.Size(FVector2D(0.0f, 20.0f))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NewModTitle", "Title"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SEditableTextBox)
				.Text(FText::FromString(NewModTitle))
				.OnTextChanged_Lambda([this](const FText& Value) {OnTitleTextChanged(Value, false); })
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSpacer)
				.Size(FVector2D(0.0f, 10.0f))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NewModDescription", "Description"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SEditableTextBox)
				.Text(FText::FromString(NewModDescription))
				.OnTextChanged_Lambda([this](const FText& Value) {OnDescriptionTextChanged(Value, false); })
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSpacer)
				.Size(FVector2D(0.0f, 10.0f))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SExpandableArea)
				.AreaTitle(LOCTEXT("NewModTags", "Tags"))
				.InitiallyCollapsed(true)
				.Padding(8.0f)
				.BodyContent()
				[
					CreateTagCheckboxes()
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSpacer)
				.Size(FVector2D(0.0f, 10.0f))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Thumbnail", "Thumbnail (must be under 1MB size)"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SAssignNew(NewModThumbnailTextBox, SEditableTextBox)
					.Text(FText::FromString(NewModThumbnail))
					.OnTextChanged_Lambda([this](const FText& Value) {OnThumbnailTextChanged(Value, false); })
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("Browse", "Browse..."))
					.OnClicked_Lambda([this]() { return OnBrowseClicked(NewModThumbnailTextBox); })
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSpacer)
				.Size(FVector2D(0.0f, 10.0f))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PackagedMod", "Packaged Mod"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(SelectedNewModComboBox, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&SelectedModOptions)
				.OnGenerateWidget(SComboBox<TSharedPtr<FString>>::FOnGenerateWidget::CreateLambda(
					[](TSharedPtr<FString> Item)
					{
						return SNew(STextBlock).Text(FText::FromString(*Item));
					}))
				.OnSelectionChanged(SComboBox<TSharedPtr<FString>>::FOnSelectionChanged::CreateLambda(
					[this](TSharedPtr<FString> NewSelection, ESelectInfo::Type)
					{
						if (NewSelection.IsValid())
						{
							SelectedNewModOption = NewSelection;

							NewModPackage = *NewSelection;

							UE_LOG(LogTemp, Warning, TEXT("NewModPackage changed: %s"), *NewModPackage);
						}
					}))
				.InitiallySelectedItem(SelectedNewModOption)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						return SelectedNewModOption.IsValid() ? FText::FromString(*SelectedNewModOption) : FText::FromString(TEXT("Select..."));
					})
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSpacer)
				.Size(FVector2D(0.0f, 10.0f))
			]
			/*+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString("Publicly Visible"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SCheckBox)
				.OnCheckStateChanged_Raw(this, &FWorkshopUploaderModule::OnVisibilityChanged)
				.ToolTipText(FText::FromString("Whether or not to publish the workshop item as visible"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSpacer)
				.Size(FVector2D(0.0f, 10.0f))
			]*/
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(NewModPublishButton, SButton)
				.OnClicked_Raw(this, &FWorkshopUploaderModule::OnPublishNewModClicked)
				.Text(LOCTEXT("PublishToWorkshop", "Publish to Steam Workshop"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(NewModUploadStatusText, SMultiLineEditableText)
				.Text(FText::FromString(""))
				.IsReadOnly(true)
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSpacer)
				.Size(FVector2D(0.0f, 20.0f))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSpacer)
				.Size(FVector2D(0.0f, 20.0f))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("UpdateExistingItem", "Update Existing Workshop Item"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSpacer)
				.Size(FVector2D(0.0f, 20.0f))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ItemIDToUpdate", "Workshop item ID to update"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SEditableTextBox)
				//.Text(FText::FromString("123456789"))
				.OnTextChanged_Raw(this, &FWorkshopUploaderModule::OnModIdTextChanged)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSpacer)
				.Size(FVector2D(0.0f, 10.0f))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SExpandableArea)
				.AreaTitle(LOCTEXT("OptionalUpdateFields", "Optional Update Fields"))
				.InitiallyCollapsed(true)
				.Padding(8.0f)
				.BodyContent()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("UpdateModTitle", "Title (Leave blank to keep the current one)"))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SEditableTextBox)
						.Text(FText::FromString(UpdateModTitle))
						.OnTextChanged_Lambda([this](const FText& Value) {OnTitleTextChanged(Value, true); })
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SSpacer)
						.Size(FVector2D(0.0f, 10.0f))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("UpdateModDescription", "Description (Leave blank to keep the current one)"))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SEditableTextBox)
						.Text(FText::FromString(UpdateModDescription))
						.OnTextChanged_Lambda([this](const FText& Value) {OnDescriptionTextChanged(Value, true); })
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SSpacer)
						.Size(FVector2D(0.0f, 10.0f))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("UpdateModThumbnail", "Thumbnail (Leave blank to keep the current one)"))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						[
							SAssignNew(UpdateModThumbnailTextBox, SEditableTextBox)
							.Text(FText::FromString(NewModThumbnail))
							.OnTextChanged_Lambda([this](const FText& Value) {OnThumbnailTextChanged(Value, true); })
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
							.Text(LOCTEXT("Browse", "Browse..."))
							.OnClicked_Lambda([this]() { return OnBrowseClicked(UpdateModThumbnailTextBox); })
						]
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSpacer)
				.Size(FVector2D(0.0f, 10.0f))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SExpandableArea)
				.AreaTitle(LOCTEXT("UpdateModTags", "Tags (leave blank to keep the current ones)"))
				.InitiallyCollapsed(true)
				.Padding(8.0f)
				.BodyContent()
				[
					CreateTagCheckboxes(true)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSpacer)
				.Size(FVector2D(0.0f, 10.0f))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PackagedMod", "Packaged Mod"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(SelectedUpdateModComboBox, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&SelectedModOptions)
				.OnGenerateWidget(SComboBox<TSharedPtr<FString>>::FOnGenerateWidget::CreateLambda(
					[](TSharedPtr<FString> Item)
					{
						return SNew(STextBlock).Text(FText::FromString(*Item));
					}))
				.OnSelectionChanged(SComboBox<TSharedPtr<FString>>::FOnSelectionChanged::CreateLambda(
					[this](TSharedPtr<FString> NewSelection, ESelectInfo::Type)
					{
						if (NewSelection.IsValid())
						{
							SelectedUpdateModOption = NewSelection;

							UpdateModPackage = *NewSelection;

							UE_LOG(LogTemp, Warning, TEXT("UpdateModPackage changed: %s"), *UpdateModPackage);
						}
					}))
				.InitiallySelectedItem(SelectedUpdateModOption)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						return SelectedUpdateModOption.IsValid() ? FText::FromString(*SelectedUpdateModOption) : FText::FromString(TEXT("Select..."));
					})
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSpacer)
				.Size(FVector2D(0.0f, 10.0f))
			]
						+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("UpdateModChangeNote", "Change Note"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SEditableTextBox)
				.Text(FText::FromString(UpdateModChangeNote))
				.OnTextChanged_Raw(this, &FWorkshopUploaderModule::OnChangeNoteTextChanged)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSpacer)
				.Size(FVector2D(0.0f, 10.0f))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(UpdateModPublishButton, SButton)
				.OnClicked_Raw(this, &FWorkshopUploaderModule::OnPublishUpdateModClicked)
				.Text(LOCTEXT("PublishToWorkshop", "Publish to Steam Workshop"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(UpdateModUploadStatusText, SMultiLineEditableText)
				.Text(FText::FromString(""))
				.IsReadOnly(true)
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSpacer)
				.Size(FVector2D(0.0f, 20.0f))
			]
		]
	];
}

void FWorkshopUploaderModule::PluginButtonClicked()
{
	// Check if SteamUGC is null and if it is then steam api was most likely destroyed so attempt to reinitialise it
	if (SteamUGC() == nullptr)
	{
		SteamAPI_Init();

		//UE_LOG(LogTemp, Warning, TEXT("Reinitialised SteamAPI"));
	}

#if ENGINE_MAJOR_VERSION >= 5
	FGlobalTabmanager::Get()->TryInvokeTab(WorkshopUploaderTabName);
#else
	FGlobalTabmanager::Get()->InvokeTab(WorkshopUploaderTabName);
#endif
}

void FWorkshopUploaderModule::AddMenuExtension(FMenuBuilder& Builder)
{
	Builder.AddMenuEntry(FWorkshopUploaderCommands::Get().OpenWorkshopUploaderWindow);
}

void FWorkshopUploaderModule::AddToolbarExtension(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(FWorkshopUploaderCommands::Get().OpenWorkshopUploaderWindow);
}

/* Other functions */

struct FPlatformFolderVisitor : public IPlatformFile::FDirectoryVisitor
{
	bool bFoundValidFolder = false;

	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
	{
		if (bIsDirectory)
		{
			FString FolderName = FPaths::GetCleanFilename(FilenameOrDirectory);

			if (FolderName.StartsWith(TEXT("Windows")) ||
				FolderName.StartsWith(TEXT("Mac")) ||
				FolderName.StartsWith(TEXT("Linux")))
			{
				bFoundValidFolder = true;
				return false; // stop iterating
			}
		}
		return true; // continue iterating
	}
};

void FWorkshopUploaderModule::FindPackagedPlugins()
{
	SelectedModOptions.Empty();

	TArray<TSharedRef<IPlugin>> DiscoveredPlugins = IPluginManager::Get().GetDiscoveredPlugins();

	for (TSharedRef<IPlugin> Plugin : DiscoveredPlugins)
	{
		if (Plugin->GetLoadedFrom() == EPluginLoadedFrom::Project && Plugin->GetType() == EPluginType::Mod)
		{
			FString PluginDir = Plugin->GetBaseDir();

			FString StagedBuildsPath = FPaths::Combine(PluginDir, TEXT("Saved"), TEXT("StagedBuilds"));

			if (IFileManager::Get().DirectoryExists(*StagedBuildsPath))
			{
				FPlatformFolderVisitor Visitor;
				IFileManager::Get().IterateDirectory(*StagedBuildsPath, Visitor);

				if (Visitor.bFoundValidFolder)
				{
					SelectedModOptions.Add(MakeShared<FString>(Plugin->GetName()));
				}
			}
		}
	}
}

/* FReply events */

FReply FWorkshopUploaderModule::OnPublishNewModClicked()
{
	bool bMissingFields = false;
	FString MissingFields;

	if (NewModTitle.IsEmpty())
	{
		bMissingFields = true;
		MissingFields += TEXT("Title, ");
	}
	if (NewModDescription.IsEmpty())
	{
		bMissingFields = true;
		MissingFields += TEXT("Description, ");
	}
	if (NewModTags.Num() == 0)
	{
		bMissingFields = true;
		MissingFields += TEXT("Tags, ");
	}
	if (NewModThumbnail.IsEmpty())
	{
		bMissingFields = true;
		MissingFields += TEXT("Thumbnail, ");
	}
	if (NewModPackage.IsEmpty())
	{
		bMissingFields = true;
		MissingFields += TEXT("Packaged Mod, ");
	}

	if (bMissingFields)
	{
		// Don't leave an extra comma/space at the end
		MissingFields.TrimEndInline();
		MissingFields.RemoveFromEnd(TEXT(","), ESearchCase::IgnoreCase);

		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(TEXT("These fields must be filled in to publish: %s"), *MissingFields)));

		return FReply::Handled();
	}

	NewModPublishButton->SetEnabled(false);
	UpdateModPublishButton->SetEnabled(false);

	if (NewModUploadStatusText.IsValid())
	{
		NewModUploadStatusText->SetText(FText::FromString("Publishing to Steam Workshop, please wait..."));
		
		FTextBlockStyle NewTextStyle;
		NewTextStyle = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
		NewTextStyle.SetColorAndOpacity(FSlateColor(FLinearColor::Yellow));

		NewModUploadStatusText->SetTextStyle(&NewTextStyle);
	}

	SteamAPICall_t hSteamAPICall = SteamUGC()->CreateItem(SteamUtils()->GetAppID(), k_EWorkshopFileTypeCommunity);
	m_CreateItemResult.Set(hSteamAPICall, this, &FWorkshopUploaderModule::onItemCreated);

	return FReply::Handled();
}

FReply FWorkshopUploaderModule::OnPublishUpdateModClicked()
{
	bool bMissingFields = false;
	FString MissingFields;

	if (UpdateModWorkshopId == 0)
	{
		bMissingFields = true;
		MissingFields += TEXT("Mod Id, ");
	}
	if (UpdateModPackage.IsEmpty())
	{
		bMissingFields = true;
		MissingFields += TEXT("Packaged Mod, ");
	}
	if (UpdateModChangeNote.IsEmpty())
	{
		bMissingFields = true;
		MissingFields += TEXT("Change Note, ");
	}

	if (bMissingFields)
	{
		// Don't leave an extra comma/space at the end
		MissingFields.TrimEndInline();
		MissingFields.RemoveFromEnd(TEXT(","), ESearchCase::IgnoreCase);

		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(TEXT("These fields must be filled in to publish: %s"), *MissingFields)));

		return FReply::Handled();
	}

	NewModPublishButton->SetEnabled(false);
	UpdateModPublishButton->SetEnabled(false);

	if (UpdateModUploadStatusText.IsValid())
	{
		UpdateModUploadStatusText->SetText(FText::FromString("Publishing to Steam Workshop, please wait..."));
		
		FTextBlockStyle NewTextStyle;
		NewTextStyle = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
		NewTextStyle.SetColorAndOpacity(FSlateColor(FLinearColor::Yellow));

		UpdateModUploadStatusText->SetTextStyle(&NewTextStyle);
	}

	UpdateWorkshopItem(SteamUtils()->GetAppID(), static_cast<PublishedFileId_t>(UpdateModWorkshopId), true);

	return FReply::Handled();
}


FReply FWorkshopUploaderModule::OnBrowseClicked(TSharedPtr<SEditableTextBox> TargetTextBox)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	if (DesktopPlatform)
	{
		void* ParentWindowHandle = nullptr;
		const TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindBestParentWindowForDialogs(nullptr);

		if (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
			ParentWindowHandle = ParentWindow->GetNativeWindow()->GetOSWindowHandle();

		TArray<FString> OutFiles;
		const FString FileTypes = TEXT("Image Files (*.png;*.jpg;*.bmp)|*.png;*.jpg;*.bmp|All Files (*.*)|*.*");
		bool bOpened = DesktopPlatform->OpenFileDialog(ParentWindowHandle, TEXT("Select an Image"), FPaths::ProjectContentDir(), TEXT(""), FileTypes, EFileDialogFlags::None, OutFiles);

		if (bOpened && OutFiles.Num() > 0)
			TargetTextBox->SetText(FText::FromString(FPaths::ConvertRelativePathToFull(*OutFiles[0])));
	}

	return FReply::Handled();
}

/* OnChanged events */

void FWorkshopUploaderModule::OnTitleTextChanged(const FText& Value, bool IsUpdateMod)
{
	(IsUpdateMod ? UpdateModTitle : NewModTitle) = Value.ToString();

	UE_LOG(LogTemp, Warning, TEXT("%s changed: %s"), IsUpdateMod ? TEXT("UpdateModTitle") : TEXT("NewModTitle"), *Value.ToString());
}
void FWorkshopUploaderModule::OnDescriptionTextChanged(const FText& Value, bool IsUpdateMod)
{
	(IsUpdateMod ? UpdateModTitle : NewModTitle) = Value.ToString();

	UE_LOG(LogTemp, Warning, TEXT("%s changed: %s"), IsUpdateMod ? TEXT("UpdateModDescription") : TEXT("NewModDescription"), *Value.ToString());
}
void FWorkshopUploaderModule::OnThumbnailTextChanged(const FText& Value, bool IsUpdateMod)
{
	(IsUpdateMod ? UpdateModThumbnail : NewModThumbnail) = Value.ToString();

	UE_LOG(LogTemp, Warning, TEXT("%s changed: %s"), IsUpdateMod ? TEXT("UpdateModThumbnail") : TEXT("NewModThumbnail"), *Value.ToString());
}
void FWorkshopUploaderModule::OnModIdTextChanged(const FText& Value)
{
	UpdateModWorkshopId = FCString::Strtoui64(*Value.ToString(), nullptr, 10);

	UE_LOG(LogTemp, Warning, TEXT("UpdateModWorkshopId changed: %llu"), UpdateModWorkshopId);
}

void FWorkshopUploaderModule::OnChangeNoteTextChanged(const FText& Value)
{
	UpdateModChangeNote = Value.ToString();

	UE_LOG(LogTemp, Warning, TEXT("UpdateModChangeNote changed: %s"), *UpdateModChangeNote);
}

void FWorkshopUploaderModule::OnVisibilityChanged(ECheckBoxState NewState)
{
	bIsVisible = (NewState == ECheckBoxState::Checked);

	UE_LOG(LogTemp, Warning, TEXT("bIsVisible changed: %s"), bIsVisible ? TEXT("True") : TEXT("False"));
}

/* Workshop functions */

void FWorkshopUploaderModule::UpdateWorkshopItem(AppId_t nConsumerAppId, PublishedFileId_t nPublishedFileID, bool IsUpdateMod)
{
	// Setup local values depending on mode
	FString MyTitle = IsUpdateMod ? UpdateModTitle : NewModTitle;
	FString MyDescription = IsUpdateMod ? UpdateModDescription : NewModDescription;
	TArray<FString> MyTags = IsUpdateMod ? UpdateModTags : NewModTags;
	FString MyThumbnail = IsUpdateMod ? UpdateModThumbnail : NewModThumbnail;
	FString MyPackage = IsUpdateMod ? UpdateModPackage : NewModPackage;
	uint64 MyWorkshopId = UpdateModWorkshopId;
	FString MyChangeNote = IsUpdateMod ? UpdateModChangeNote : FString("Initial creation.");

	UGCUpdateHandle_t handle = SteamUGC()->StartItemUpdate(nConsumerAppId, nPublishedFileID);

	if (!UpdateModTitle.IsEmpty() || !IsUpdateMod) { SteamUGC()->SetItemTitle(handle, TCHAR_TO_UTF8(*MyTitle)); }
	if (!UpdateModDescription.IsEmpty() || !IsUpdateMod) { SteamUGC()->SetItemDescription(handle, TCHAR_TO_UTF8(*MyDescription)); }
	SteamUGC()->SetItemUpdateLanguage(handle, "English");
	SteamUGC()->SetItemMetadata(handle, "Test Metadata");
	//SteamUGC()->SetItemVisibility(handle, k_ERemoteStoragePublishedFileVisibilityPublic);

	if (MyTags.Num() > 0 || !IsUpdateMod)
	{
		TArray<ANSICHAR*> ConvertedTags;
		ConvertedTags.SetNum(MyTags.Num());

		for (int32 i = 0; i < MyTags.Num(); ++i)
		{
			FTCHARToUTF8 Converter(*MyTags[i]);
			int32 Len = Converter.Length();

			ANSICHAR* Buffer = new ANSICHAR[Len + 1]; // +1 for null terminator
			FMemory::Memcpy(Buffer, Converter.Get(), Len);
			Buffer[Len] = '\0';

			ConvertedTags[i] = Buffer;
		}

		SteamParamStringArray_t* pTags = new SteamParamStringArray_t();
		pTags->m_ppStrings = new const char*[ConvertedTags.Num()];
		for (int32 i = 0; i < ConvertedTags.Num(); ++i)
			pTags->m_ppStrings[i] = ConvertedTags[i];
		pTags->m_nNumStrings = ConvertedTags.Num();

		SteamUGC()->SetItemTags(handle, pTags);
	}

	SteamUGC()->AddItemKeyValueTag(handle, "test_key", "test_value");

	FString FullModDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Mods/") / MyPackage / TEXT("Saved/StagedBuilds"));
	std::string mod_directory = TCHAR_TO_UTF8(*FullModDirectory);
	SteamUGC()->SetItemContent(handle, mod_directory.c_str());

	if (!MyThumbnail.IsEmpty() || !IsUpdateMod)
	{
		std::string preview_image = TCHAR_TO_UTF8(*MyThumbnail);
		SteamUGC()->SetItemPreview(handle, preview_image.c_str());
	}

	std::string pchChangeNote = TCHAR_TO_UTF8(*MyChangeNote);

	SteamAPICall_t submit_item_call = SteamUGC()->SubmitItemUpdate(handle, pchChangeNote.c_str());
	m_SubmitItemUpdateResult.Set(submit_item_call, this, IsUpdateMod ? &FWorkshopUploaderModule::onItemSubmitted2 : &FWorkshopUploaderModule::onItemSubmitted);
}

void FWorkshopUploaderModule::onItemCreated(CreateItemResult_t* pCallback, bool bIOFailure)
{
	if (pCallback->m_eResult == k_EResultOK && !bIOFailure)
	{
		if (pCallback->m_bUserNeedsToAcceptWorkshopLegalAgreement)
		{
			char* fileurl;
			fileurl = (TCHAR_TO_UTF8(*CommunityFileUrl));
			SteamFriends()->ActivateGameOverlayToWebPage(fileurl);
		}

		//UE_LOG(LogTemp, Warning, TEXT("Item created!"));

		UpdateWorkshopItem(SteamUtils()->GetAppID(), pCallback->m_nPublishedFileId);
	}
	else
	{
		// Make sure to do this on Game Thread in order to prevent crashes
		AsyncTask(ENamedThreads::GameThread, [this, pCallback, bIOFailure]()
		{
			NewModPublishButton->SetEnabled(true);
			UpdateModPublishButton->SetEnabled(true);

			if (NewModUploadStatusText.IsValid())
			{
				NewModUploadStatusText->SetText(FText::FromString(FString::Printf(TEXT("Workshop creation failed! %s"), *GetSubmitItemUpdateResultString(pCallback->m_eResult))));
				NewModUploadStatusText->SetTextStyle(&UploadFailureStyle);
			}
		});

		//UE_LOG(LogTemp, Warning, TEXT("Failed to create item! %s"), *GetCreateItemResultString(pCallback->m_eResult));
	}
}

void FWorkshopUploaderModule::onItemSubmitted(SubmitItemUpdateResult_t* pCallback, bool bIOFailure)
{
	if (pCallback->m_eResult == k_EResultOK && !bIOFailure)
	{
		// Make sure to do this on Game Thread in order to prevent crashes
		AsyncTask(ENamedThreads::GameThread, [this, pCallback, bIOFailure]()
		{
			NewModPublishButton->SetEnabled(true);
			UpdateModPublishButton->SetEnabled(true);

			if (NewModUploadStatusText.IsValid())
			{
				NewModUploadStatusText->SetText(FText::FromString("Workshop submission successful!"));
				NewModUploadStatusText->SetTextStyle(&UploadSuccessStyle);
			}
		});

		//UE_LOG(LogTemp, Warning, TEXT("Updated item submitted!"));
	}
	else
	{
		// Make sure to do this on Game Thread in order to prevent crashes
		AsyncTask(ENamedThreads::GameThread, [this, pCallback, bIOFailure]()
		{
			NewModPublishButton->SetEnabled(true);
			UpdateModPublishButton->SetEnabled(true);

			if (NewModUploadStatusText.IsValid())
			{
				NewModUploadStatusText->SetText(FText::FromString(FString::Printf(TEXT("Workshop submission failed! %s"), *GetSubmitItemUpdateResultString(pCallback->m_eResult))));
				NewModUploadStatusText->SetTextStyle(&UploadFailureStyle);
			}
		});

		//UE_LOG(LogTemp, Warning, TEXT("Failed to submit item! %s"), *GetSubmitItemUpdateResultString(pCallback->m_eResult));
	}
}

void FWorkshopUploaderModule::onItemSubmitted2(SubmitItemUpdateResult_t* pCallback, bool bIOFailure)
{
	if (pCallback->m_eResult == k_EResultOK && !bIOFailure)
	{
		// Make sure to do this on Game Thread in order to prevent crashes
		AsyncTask(ENamedThreads::GameThread, [this, pCallback, bIOFailure]()
		{
			NewModPublishButton->SetEnabled(true);
			UpdateModPublishButton->SetEnabled(true);

			if (UpdateModUploadStatusText.IsValid())
			{
				UpdateModUploadStatusText->SetText(FText::FromString("Workshop submission successful!"));
				UpdateModUploadStatusText->SetTextStyle(&UploadSuccessStyle);
			}
		});

		//UE_LOG(LogTemp, Warning, TEXT("Updated item submitted!"));
	}
	else
	{
		// Make sure to do this on Game Thread in order to prevent crashes
		AsyncTask(ENamedThreads::GameThread, [this, pCallback, bIOFailure]()
		{
			NewModPublishButton->SetEnabled(true);
			UpdateModPublishButton->SetEnabled(true);

			if (UpdateModUploadStatusText.IsValid())
			{
				UpdateModUploadStatusText->SetText(FText::FromString(FString::Printf(TEXT("Workshop submission failed! %s"), *GetSubmitItemUpdateResultString(pCallback->m_eResult))));
				UpdateModUploadStatusText->SetTextStyle(&UploadFailureStyle);
			}
		});

		//UE_LOG(LogTemp, Warning, TEXT("Failed to submit item! %s"), *GetSubmitItemUpdateResultString(pCallback->m_eResult));
	}
}

/* GetResultString functions */

FString FWorkshopUploaderModule::GetCreateItemResultString(EResult result)
{
	switch (result)
	{
	case k_EResultOK:
		return TEXT("k_EResultOK - The operation completed successfully.");
	case k_EResultInsufficientPrivilege:
		return TEXT("k_EResultInsufficientPrivilege - You are restricted from uploading due to a hub ban, account lock, or community ban. Contact Steam Support.");
	case k_EResultBanned:
		return TEXT("k_EResultBanned - You cannot upload to this hub due to an active VAC or Game ban.");
	case k_EResultTimeout:
		return TEXT("k_EResultTimeout - The operation timed out. Please retry the upload.");
	case k_EResultNotLoggedOn:
		return TEXT("k_EResultNotLoggedOn - You are not logged into Steam.");
	case k_EResultServiceUnavailable:
		return TEXT("k_EResultServiceUnavailable - The Steam Workshop server is currently unavailable. Try again later.");
	case k_EResultInvalidParam:
		return TEXT("k_EResultInvalidParam - One or more submission fields are invalid.");
	case k_EResultAccessDenied:
		return TEXT("k_EResultAccessDenied - Access denied when saving title and description.");
	case k_EResultLimitExceeded:
		return TEXT("k_EResultLimitExceeded - Steam Cloud quota exceeded. Remove items and try again.");
	case k_EResultFileNotFound:
		return TEXT("k_EResultFileNotFound - Uploaded file not found.");
	case k_EResultDuplicateRequest:
		return TEXT("k_EResultDuplicateRequest - The file was already uploaded successfully. Refresh to see the item.");
	case k_EResultDuplicateName:
		return TEXT("k_EResultDuplicateName - You already have a Workshop item with this name.");
	case k_EResultServiceReadOnly:
		return TEXT("k_EResultServiceReadOnly - You cannot upload due to a recent password/email change. This restriction usually expires in 5 days.");

	default:
		return TEXT("An unhandled error occurred.");
	}
}

FString FWorkshopUploaderModule::GetSubmitItemUpdateResultString(EResult result)
{
	switch (result)
	{
	case k_EResultOK:
		return TEXT("k_EResultOK - The operation completed successfully.");
	case k_EResultFail:
		return TEXT("k_EResultFail - Generic failure.");
	case k_EResultInvalidParam:
		return TEXT("k_EResultInvalidParam - Either the app ID is invalid or doesn't match the item's consumer app ID, or ISteamUGC is not enabled for the app ID on the Steam Workshop Configuration page. The preview file may also be smaller than 16 bytes.");
	case k_EResultAccessDenied:
		return TEXT("k_EResultAccessDenied - The user doesn't own a license for the provided app ID.");
	case k_EResultFileNotFound:
		return TEXT("k_EResultFileNotFound - Failed to get the workshop info or read the preview file. The provided content folder may also be invalid.");
	case k_EResultLockingFailed:
		return TEXT("k_EResultLockingFailed - Failed to acquire UGC lock.");
	case k_EResultLimitExceeded:
		return TEXT("k_EResultLimitExceeded - The preview image is too large (must be under 1 MB) or the user has exceeded their Steam Cloud quota.");

	default:
		return TEXT("An unhandled error occurred.");
	}
}

FString FWorkshopUploaderModule::GetSteamResultString(EResult result)
{
	switch (result)
	{
	case k_EResultOK: return "k_EResultOK";
	case k_EResultFail: return "k_EResultFail";
	case k_EResultNoConnection: return "k_EResultNoConnection";
	case k_EResultInvalidPassword: return "k_EResultInvalidPassword";
	case k_EResultLoggedInElsewhere: return "k_EResultLoggedInElsewhere";
	case k_EResultInvalidProtocolVer: return "k_EResultInvalidProtocolVer";
	case k_EResultInvalidParam: return "k_EResultInvalidParam";
	case k_EResultFileNotFound: return "k_EResultFileNotFound";
	case k_EResultBusy: return "k_EResultBusy";
	case k_EResultInvalidState: return "k_EResultInvalidState";
	case k_EResultInvalidName: return "k_EResultInvalidName";
	case k_EResultInvalidEmail: return "k_EResultInvalidEmail";
	case k_EResultDuplicateName: return "k_EResultDuplicateName";
	case k_EResultAccessDenied: return "k_EResultAccessDenied";
	case k_EResultTimeout: return "k_EResultTimeout";
	case k_EResultBanned: return "k_EResultBanned";
	case k_EResultAccountNotFound: return "k_EResultAccountNotFound";
	case k_EResultInvalidSteamID: return "k_EResultInvalidSteamID";
	case k_EResultServiceUnavailable: return "k_EResultServiceUnavailable";
	case k_EResultNotLoggedOn: return "k_EResultNotLoggedOn";
	case k_EResultPending: return "k_EResultPending";
	case k_EResultEncryptionFailure: return "k_EResultEncryptionFailure";
	case k_EResultInsufficientPrivilege: return "k_EResultInsufficientPrivilege";
	case k_EResultLimitExceeded: return "k_EResultLimitExceeded";
	case k_EResultRevoked: return "k_EResultRevoked";
	case k_EResultExpired: return "k_EResultExpired";
	case k_EResultAlreadyRedeemed: return "k_EResultAlreadyRedeemed";
	case k_EResultDuplicateRequest: return "k_EResultDuplicateRequest";
	case k_EResultAlreadyOwned: return "k_EResultAlreadyOwned";
	case k_EResultIPNotFound: return "k_EResultIPNotFound";
	case k_EResultPersistFailed: return "k_EResultPersistFailed";
	case k_EResultLockingFailed: return "k_EResultLockingFailed";
	case k_EResultLogonSessionReplaced: return "k_EResultLogonSessionReplaced";
	case k_EResultConnectFailed: return "k_EResultConnectFailed";
	case k_EResultHandshakeFailed: return "k_EResultHandshakeFailed";
	case k_EResultIOFailure: return "k_EResultIOFailure";
	case k_EResultRemoteDisconnect: return "k_EResultRemoteDisconnect";
	case k_EResultShoppingCartNotFound: return "k_EResultShoppingCartNotFound";
	case k_EResultBlocked: return "k_EResultBlocked";
	case k_EResultIgnored: return "k_EResultIgnored";
	case k_EResultNoMatch: return "k_EResultNoMatch";
	case k_EResultAccountDisabled: return "k_EResultAccountDisabled";
	case k_EResultServiceReadOnly: return "k_EResultServiceReadOnly";
	case k_EResultAccountNotFeatured: return "k_EResultAccountNotFeatured";
	case k_EResultAdministratorOK: return "k_EResultAdministratorOK";
	case k_EResultContentVersion: return "k_EResultContentVersion";
	case k_EResultTryAnotherCM: return "k_EResultTryAnotherCM";
	case k_EResultPasswordRequiredToKickSession: return "k_EResultPasswordRequiredToKickSession";
	case k_EResultAlreadyLoggedInElsewhere: return "k_EResultAlreadyLoggedInElsewhere";
	case k_EResultSuspended: return "k_EResultSuspended";
	case k_EResultCancelled: return "k_EResultCancelled";
	case k_EResultDataCorruption: return "k_EResultDataCorruption";
	case k_EResultDiskFull: return "k_EResultDiskFull";
	case k_EResultRemoteCallFailed: return "k_EResultRemoteCallFailed";
	case k_EResultPasswordUnset: return "k_EResultPasswordUnset";
	case k_EResultExternalAccountUnlinked: return "k_EResultExternalAccountUnlinked";
	case k_EResultPSNTicketInvalid: return "k_EResultPSNTicketInvalid";
	case k_EResultExternalAccountAlreadyLinked: return "k_EResultExternalAccountAlreadyLinked";
	case k_EResultRemoteFileConflict: return "k_EResultRemoteFileConflict";
	case k_EResultIllegalPassword: return "k_EResultIllegalPassword";
	case k_EResultSameAsPreviousValue: return "k_EResultSameAsPreviousValue";
	case k_EResultAccountLogonDenied: return "k_EResultAccountLogonDenied";
	case k_EResultCannotUseOldPassword: return "k_EResultCannotUseOldPassword";
	case k_EResultInvalidLoginAuthCode: return "k_EResultInvalidLoginAuthCode";
	case k_EResultAccountLogonDeniedNoMail: return "k_EResultAccountLogonDeniedNoMail";
	case k_EResultHardwareNotCapableOfIPT: return "k_EResultHardwareNotCapableOfIPT";
	case k_EResultIPTInitError: return "k_EResultIPTInitError";
	case k_EResultParentalControlRestricted: return "k_EResultParentalControlRestricted";
	case k_EResultFacebookQueryError: return "k_EResultFacebookQueryError";
	case k_EResultExpiredLoginAuthCode: return "k_EResultExpiredLoginAuthCode";
	case k_EResultIPLoginRestrictionFailed: return "k_EResultIPLoginRestrictionFailed";
	case k_EResultAccountLockedDown: return "k_EResultAccountLockedDown";
	case k_EResultAccountLogonDeniedVerifiedEmailRequired: return "k_EResultAccountLogonDeniedVerifiedEmailRequired";
	case k_EResultNoMatchingURL: return "k_EResultNoMatchingURL";
	case k_EResultBadResponse: return "k_EResultBadResponse";
	case k_EResultRequirePasswordReEntry: return "k_EResultRequirePasswordReEntry";
	case k_EResultValueOutOfRange: return "k_EResultValueOutOfRange";
	case k_EResultUnexpectedError: return "k_EResultUnexpectedError";
	case k_EResultDisabled: return "k_EResultDisabled";
	case k_EResultInvalidCEGSubmission: return "k_EResultInvalidCEGSubmission";
	case k_EResultRestrictedDevice: return "k_EResultRestrictedDevice";
	case k_EResultRegionLocked: return "k_EResultRegionLocked";
	case k_EResultRateLimitExceeded: return "k_EResultRateLimitExceeded";
	case k_EResultAccountLoginDeniedNeedTwoFactor: return "k_EResultAccountLoginDeniedNeedTwoFactor";
	case k_EResultItemDeleted: return "k_EResultItemDeleted";
	case k_EResultAccountLoginDeniedThrottle: return "k_EResultAccountLoginDeniedThrottle";
	case k_EResultTwoFactorCodeMismatch: return "k_EResultTwoFactorCodeMismatch";
	case k_EResultTwoFactorActivationCodeMismatch: return "k_EResultTwoFactorActivationCodeMismatch";
	case k_EResultAccountAssociatedToMultiplePartners: return "k_EResultAccountAssociatedToMultiplePartners";
	case k_EResultNotModified: return "k_EResultNotModified";
	case k_EResultNoMobileDevice: return "k_EResultNoMobileDevice";
	case k_EResultTimeNotSynced: return "k_EResultTimeNotSynced";
	case k_EResultSmsCodeFailed: return "k_EResultSmsCodeFailed";
	case k_EResultAccountLimitExceeded: return "k_EResultAccountLimitExceeded";
	case k_EResultAccountActivityLimitExceeded: return "k_EResultAccountActivityLimitExceeded";
	case k_EResultPhoneActivityLimitExceeded: return "k_EResultPhoneActivityLimitExceeded";
	case k_EResultRefundToWallet: return "k_EResultRefundToWallet";
	case k_EResultEmailSendFailure: return "k_EResultEmailSendFailure";
	case k_EResultNotSettled: return "k_EResultNotSettled";
	case k_EResultNeedCaptcha: return "k_EResultNeedCaptcha";
	case k_EResultGSLTDenied: return "k_EResultGSLTDenied";
	case k_EResultGSOwnerDenied: return "k_EResultGSOwnerDenied";
	case k_EResultInvalidItemType: return "k_EResultInvalidItemType";
	case k_EResultIPBanned: return "k_EResultIPBanned";
	case k_EResultGSLTExpired: return "k_EResultGSLTExpired";
	default: return "Unknown Result";
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FWorkshopUploaderModule, WorkshopUploader)