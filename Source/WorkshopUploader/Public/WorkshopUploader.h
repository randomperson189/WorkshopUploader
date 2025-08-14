// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "Widgets/Docking/SDockTab.h"
#include "Containers/Ticker.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SComboBox.h"

#pragma region SteamInclude
// @todo Steam: Steam headers trigger secure-C-runtime warnings in Visual C++. Rather than mess with _CRT_SECURE_NO_WARNINGS, we'll just
//	disable the warnings locally. Remove when this is fixed in the SDK
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4996)
// #TODO check back on this at some point
#pragma warning(disable:4265) // SteamAPI CCallback< specifically, this warning is off by default but 4.17 turned it on....
#endif

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#pragma push_macro("ARRAY_COUNT")
#undef ARRAY_COUNT

#if USING_CODE_ANALYSIS
MSVC_PRAGMA(warning(push))
MSVC_PRAGMA(warning(disable : ALL_CODE_ANALYSIS_WARNINGS))
#endif	// USING_CODE_ANALYSIS

#include <steam/steam_api.h>

#if USING_CODE_ANALYSIS
MSVC_PRAGMA(warning(pop))
#endif	// USING_CODE_ANALYSIS


#pragma pop_macro("ARRAY_COUNT")

#endif

// @todo Steam: See above
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#pragma endregion SteamInclude

class FToolBarBuilder;
class FMenuBuilder;

class FWorkshopUploaderModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** This function will be bound to Command (by default it will bring up the plugin window) */
	void PluginButtonClicked();

	/** Steam Workshop URL format for community files */
	static constexpr const char* CommunityFileUrl = "steam://url/CommunityFilePage/";

private:

	/** Tick function for periodic updates */
	bool Tick(float DeltaTime);

	/** UI related functions */
	void AddToolbarExtension(FToolBarBuilder& Builder);
	void AddMenuExtension(FMenuBuilder& Builder);
	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

	/** UI command list */
	TSharedPtr<class FUICommandList> PluginCommands;

	/** Delegates for ticking */
	FTickerDelegate TickDelegate;

#if ENGINE_MAJOR_VERSION >= 5
	FTSTicker::FDelegateHandle TickDelegateHandle;
#else
	FDelegateHandle TickDelegateHandle;
#endif

	/** Steam Workshop Callbacks */
	CCallResult<FWorkshopUploaderModule, CreateItemResult_t> m_CreateItemResult;
	CCallResult<FWorkshopUploaderModule, SubmitItemUpdateResult_t> m_SubmitItemUpdateResult;

	void onItemCreated(CreateItemResult_t* pCallback, bool bIOFailure);
	void onItemSubmitted(SubmitItemUpdateResult_t* pCallback, bool bIOFailure);
	void onItemSubmitted2(SubmitItemUpdateResult_t* pCallback, bool bIOFailure);

	/* Upload Status */
	TSharedPtr<SMultiLineEditableText> NewModUploadStatusText;
	TSharedPtr<SMultiLineEditableText> UpdateModUploadStatusText;

	FTextBlockStyle UploadProgressStyle;
	FTextBlockStyle UploadSuccessStyle;
	FTextBlockStyle UploadFailureStyle;

	/* Thumbnail Path */
	TSharedPtr<SEditableTextBox> NewModThumbnailTextBox;
	TSharedPtr<SEditableTextBox> UpdateModThumbnailTextBox;

	/* Default Tags (set these to what your game's workshop tags are set to in steamworks) */
	TArray<FString> DefaultTags = {
		TEXT("Map"),
		TEXT("Mod"),
	};

	/* Create New Mod */
	FString NewModTitle;
	FString NewModDescription;
	TArray<FString> NewModTags;
	FString NewModThumbnail;
	FString NewModPackage;

	bool bIsVisible;

	/* Update Existing Mod */
	uint64 UpdateModWorkshopId;

	FString UpdateModTitle;
	FString UpdateModDescription;
	TArray<FString> UpdateModTags;
	FString UpdateModThumbnail;
	FString UpdateModPackage;

	FString UpdateModChangeNote;

	void UpdateWorkshopItem(AppId_t nConsumerAppId, PublishedFileId_t nPublishedFileID, bool IsUpdateMod = false);

	/* SteamAPI result strings */
	FString GetSteamResultString(EResult result);
	FString GetCreateItemResultString(EResult result);
	FString GetSubmitItemUpdateResultString(EResult result);

	/* Buttons */
	TSharedPtr<SButton> NewModPublishButton;
	TSharedPtr<SButton> UpdateModPublishButton;

	/* Button click events */
	FReply OnPublishNewModClicked();
	FReply OnPublishUpdateModClicked();

	FReply OnBrowseClicked(TSharedPtr<SEditableTextBox> TargetTextBox);

	/* Selected Mod ComboBox */
	TSharedPtr<SComboBox<TSharedPtr<FString>>> SelectedNewModComboBox;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> SelectedUpdateModComboBox;

	TArray<TSharedPtr<FString>> SelectedModOptions;

	TSharedPtr<FString> SelectedNewModOption;
	TSharedPtr<FString> SelectedUpdateModOption;

	void FindPackagedPlugins();

	/* Text field update events */
	void OnTitleTextChanged(const FText& Value, bool IsUpdateMod = false);
	void OnDescriptionTextChanged(const FText& Value, bool IsUpdateMod = false);
	void OnThumbnailTextChanged(const FText& Value, bool IsUpdateMod = false);
	void OnVisibilityChanged(ECheckBoxState NewState);
	void OnModIdTextChanged(const FText& Value);
	void OnChangeNoteTextChanged(const FText& Value);
};