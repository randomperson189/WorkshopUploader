// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "WorkshopUploaderStyle.h"

class FWorkshopUploaderCommands : public TCommands<FWorkshopUploaderCommands>
{
public:

	FWorkshopUploaderCommands()
		: TCommands<FWorkshopUploaderCommands>(TEXT("WorkshopUploader"), NSLOCTEXT("Contexts", "WorkshopUploader", "WorkshopUploader Plugin"), NAME_None, FWorkshopUploaderStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenWorkshopUploaderWindow;
};