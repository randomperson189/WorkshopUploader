# Workshop Uploader Quick Start
## Prerequisites
To follow along with this guide, you’ll need the following: 
- This **WorkshopUploader** Plugin
- My custom fork of **SimpleUGC** plugin ([UE4 version](https://github.com/randomperson189/UGCExample/tree/release-ue4-custom) and [UE5 version](https://github.com/randomperson189/UGCExample/tree/release-ue5-custom))
- An **Unreal Engine 4** or **5** project with C++ code
- **Visual Studio**

### Adding the plugins to your project

1. Download **WorkshopUploader** as zip from the github page
2. Open the zip file and extract the **WorkshopUploader-main** folder into your unreal project's **Plugins** folder (if a **Plugins** folder doesn't exist then create one)
3. In the **Plugins** folder, rename **WorkshopUploader-main** to **WorkshopUploader**
4. Download my custom fork of **SimpleUGC** as zip from the github page shown in prerequisites section
5. Open the zip file and navigate to **UGCExample-main\Plugins** and extract **SimpleUGC** into your unreal project's plugins folder

### Duplicating and modifying OnlineSubsystemSteam plugin to make it work in editor
By default, Unreal's built-in **OnlineSubsystemSteam** plugin does not initialise if you're in editor, but we need to have it enabled in editor so that the **WorkshopUploader** plugin can use the **Steam API** for uploaing. Fortunately there's a simple and easy solution for that

1. Navigate to your base Unreal Engine directory (for example: **C:\Program Files\Epic Games\UE_4.27** depending on what engine version your project is using)
2. Navigate to **Engine\Plugins\Online**
3. Copy the folder called **OnlineSubsystemSteam** and paste it into your unreal project's plugins folder
4. Navigate to your project directory and right click your .uproject file and click **Generate project files**
5. Open your project's .sln file in Visual Studio
6. In the solution explorer within your game's project, navigate to **Plugins\OnlineSubsystemSteam\Source\Private** and open **OnlineSubsystemSteam.cpp**
7. Scroll down to where it shows **#if UE_EDITOR** (around line 578) and comment this part out so that it now looks like this
```
/*#if UE_EDITOR
		if (bEnableSteam)
		{
			bEnableSteam = IsRunningDedicatedServer() || IsRunningGame();
		}
#endif*/
```
9. Set your solution configuration to **Development Editor** and **Win64** and build your project

### Setting up OnlineSubsystemSteam in config files (skip if already done)
TODO: Add this
