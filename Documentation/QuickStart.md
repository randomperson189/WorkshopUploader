# Workshop Uploader Quick Start
## Prerequisites
To follow along with this guide, you’ll need the following: 
- At least some surface level knowledge of how **Unreal Plugins** work and **C++**
- An **Unreal Engine 4** or **5** Steam game with workshop enabled and configured via **Steamworks**
- An **Unreal Engine 4** or **5** game project with **C++ code**
- This **WorkshopUploader** plugin
- My custom fork of **SimpleUGC** plugin ([UE4 version](https://github.com/randomperson189/UGCExample/tree/release-ue4-custom) and [UE5 version](https://github.com/randomperson189/UGCExample/tree/release-ue5-custom)), requires linking **Epic Games** account to **GitHub** for access
- **Visual Studio** (whichever version your **Unreal** installation supports)<br/><br/>

### Adding the plugins to your game project

1. Download **WorkshopUploader** as zip from the github page
2. Open the zip file and extract the **WorkshopUploader-main** folder into your game project's **Plugins** folder (if a **Plugins** folder doesn't exist then create one)
3. In the **Plugins** folder, rename **WorkshopUploader-main** to **WorkshopUploader**
4. Download the custom fork of **SimpleUGC** as zip from the github page shown in prerequisites section
5. Open the zip file and navigate to **UGCExample\Plugins** directory and extract the **SimpleUGC** folder into your game project's **Plugins** folder<br/><br/>

### Duplicating and modifying OnlineSubsystemSteam plugin to work in editor
By default, Unreal's built-in **OnlineSubsystemSteam** plugin does not initialise if you're in editor, but we need to have it enabled in editor so that the **WorkshopUploader** plugin can use the **Steam API** for uploading. Fortunately there's a solution for that.

1. Navigate to your base **Unreal Engine** directory (for example: **C:\Program Files\Epic Games\UE_4.27** depending on what engine version your game project is using)
2. Within the base **Unreal Engine** directory, navigate to **Engine\Plugins\Online**
3. Copy the folder called **OnlineSubsystemSteam** and paste it into your game project's **Plugins** folder
4. Navigate to your game project directory and right click your **.uproject** file and click **Generate Visual Studio project files**
5. Open your game project's **.sln** file in **Visual Studio**
6. In the **Visual Studio** solution explorer within your game's code project, navigate to **Plugins\OnlineSubsystemSteam\Source\Private** and open **OnlineSubsystemSteam.cpp**
7. Scroll down to where it shows **#if UE_EDITOR** (around line 578) and comment this part out so that it now looks like this
```
/*#if UE_EDITOR
		if (bEnableSteam)
		{
			bEnableSteam = IsRunningDedicatedServer() || IsRunningGame();
		}
#endif*/
```
9. Set your solution configuration to **Development Editor** and **Win64** then build your game project<br/><br/>

### Setting up Workshop item tags in plugin

1. Open your game project's .sln file in **Visual Studio**
2. In the **Visual Studio** solution explorer within your game's code project, navigate to **Plugins\WorkshopUploader\Source\Public** and open **WorkshopUploader.h**
3. Go to line 113 and set these tags to what your game's steam workshop has (add more if necessary)
```
TArray<FString> DefaultTags = {
	TEXT("Map"),
	TEXT("Mod"),
};
```
4. Build your game project in **Development Editor** and **Win64** configuration <br/><br/>

### Setting up OnlineSubsystemSteam in config files (skip if already done)

1. Navigate to your game project's **Config** folder and open **DefaultEngine.ini**
2. Paste this at the top of the file and change **SteamDevAppId** from 480 (SpaceWar) to your game's appid
```
[OnlineSubsystem]
DefaultPlatformService=Steam

[OnlineSubsystemSteam]
bEnabled=true
SteamDevAppId=480
bInitServerOnClient=true
GameServerQueryPort=27015
bRelaunchInSteam=false
GameVersion=1.0.0.0
bVACEnabled=false
bAllowP2PPacketRelay=true
P2PConnectionTimeout=90

[/Script/OnlineSubsystemSteam.SteamNetDriver]
NetConnectionClassName="OnlineSubsystemSteam.SteamNetConnection"

[/Script/Engine.GameEngine]
NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="OnlineSubsystemSteam.SteamNetDriver",DriverClassNameFallback="OnlineSubsystemUtils.IpNetDriver")
+NetDriverDefinitions=(DefName="DemoNetDriver",DriverClassName="/Script/Engine.DemoNetDriver",DriverClassNameFallback="/Script/Engine.DemoNetDriver")

```

### Distributing your ModKit

There are 2 methods of distributing your **ModKit** so that people can use it.

#### Method 1: Epic Games Store

This is the official way of shipping your **ModKit** and is how games like **Robo Recall** and **Hello Neighbor** ship their **ModKits**.

It allows you to include:
- Your game's project files.
- A copy of the **Unreal Engine** version your game uses (including a custom engine build if used).

One thing to keep in mind is that **ModKits** published on the Epic Games Store must be **free**, meaning anyone can download them and gain access to the project files you include. If you want access to the **ModKit** to be restricted to players who own your game, consider the **Steam DLC** approach below instead.

#### Method 2: Steam DLC

For this method, you distribute your game's project files as **free Steam DLC** and then provide instructions for downloading the correct version of **Unreal Engine** the game uses from the **Epic Games Launcher** (needed to open the project), or if you have a custom engine build, then they will have to link their **Epic Games Account** to **GitHub** and download your precompiled fork.

This is generally the best option if you want your **ModKit** to be available only to people who own your game on Steam, since **Steam DLC** is tied to game ownership.


