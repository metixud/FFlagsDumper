# FFlagsDumper

Dumps FFlags directly from the memory of a running Roblox process.

## Requirements

- Visual Studio or any MSVC-compatible compiler

## Build

Open the project in Visual Studio, make sure you're targeting **Release x64**, then build and run.

## Usage

1. Launch Roblox and join a game (not just the main menu)
2. Run the compiled binary
3. Once done, navigate to the `version-*********` folder, all dumped FFlags will be there

## Notes
- This tool performs a memory dump. For more complete FFlags, a static dump from a dumped `RobloxPlayerBeta.exe` is recommended
- The dumped FFlags depend on the game you are connected to. Each game can have different flags enabled/disabled.
- Flags break on Roblox updates; you'll need to redump after each update
## i'm not posting fflags anymore but the dumper works!
# DISCONTINUED
