// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "WorkshopUploaderCommands.h"

#define LOCTEXT_NAMESPACE "FWorkshopUploaderModule"

void FWorkshopUploaderCommands::RegisterCommands()
{
#if ENGINE_MAJOR_VERSION >= 5
	UI_COMMAND(OpenWorkshopUploaderWindow, "Workshop Uploader", "Bring up Workshop Uploader window", EUserInterfaceActionType::Button, FInputChord());
#else
	UI_COMMAND(OpenWorkshopUploaderWindow, "Workshop Uploader", "Bring up Workshop Uploader window", EUserInterfaceActionType::Button, FInputGesture());
#endif
}

#undef LOCTEXT_NAMESPACE
