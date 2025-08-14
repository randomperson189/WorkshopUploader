// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WorkshopUploader : ModuleRules
{
	public WorkshopUploader(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(new string[] {
				// ... add public include paths required here ...
			});


        PrivateIncludePaths.AddRange(new string[] {
				// ... add other private include paths required here ...
			});


        PublicDependencyModuleNames.AddRange(new string[] { "Core", "OnlineSubsystem", "OnlineSubsystemUtils", "Networking", "Sockets", "SlateCore", "InputCore"
				// ... add other public dependencies that you statically link with here ...
				,"Steamworks"
			});


        PrivateDependencyModuleNames.AddRange(new string[] { "Projects", "InputCore", "UnrealEd", "LevelEditor", "CoreUObject", "Engine", "Slate", "SlateCore", "InputCore", "OnlineSubsystem", "Sockets", "Networking", "OnlineSubsystemUtils"
            ,"DesktopPlatform"
				// ... add private dependencies that you statically link with here ...	
			});


        DynamicallyLoadedModuleNames.AddRange(new string[]{// ... add any modules that your module loads dynamically here ...
            });
    }
}
