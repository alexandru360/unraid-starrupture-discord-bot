# StarRuptureDiscordPlugin — Installation Guide

## About

Native C++ plugin for the StarRupture dedicated server (using [StarRupture-ModLoader](https://github.com/AlienXAXS/StarRupture-ModLoader)).

Reads the rupture cycle state (`UCrEnviroWaveSubsystem`) and writes it to a JSON file
that the Discord bot monitors.

## Requirements

- **StarRupture Dedicated Server** (Windows)
- **StarRupture-ModLoader** installed on the server ([instructions](https://github.com/AlienXAXS/StarRupture-ModLoader/blob/main/How%20To%20Use.md))
- **Visual Studio 2022** with the "Desktop development with C++" workload (to compile the DLL)

## Building

### Required directory structure

```
StarRupture-ModLoader/
├── Shared.props
├── Version_Mod_Loader/
├── StarRupture SDK/
└── StarRuptureDiscordPlugin/       ← copy this directory here
    ├── plugin.cpp
    ├── plugin.h
    ├── plugin_config.cpp
    ├── plugin_config.h
    ├── plugin_helpers.h
    └── dllmain.cpp
```

### Steps

1. **Clone** [StarRupture-ModLoader](https://github.com/AlienXAXS/StarRupture-ModLoader).
2. **Copy** the `StarRupturePlugin/` directory into `StarRupture-ModLoader/` and rename it to `StarRuptureDiscordPlugin/`.
3. **Create a `.vcxproj`** from the `ExamplePlugin` template:
   - Copy `ExamplePlugin/ExamplePlugin.vcxproj` → `StarRuptureDiscordPlugin/StarRuptureDiscordPlugin.vcxproj`
   - Edit the `.vcxproj`: change `<ProjectGuid>` to a new GUID, `<RootNamespace>` to `StarRuptureDiscordPlugin`, and list the plugin source files.
   - Add the project to the ModLoader solution (right-click Solution → Add → Existing Project).
4. **Select** the `Server Release|x64` configuration.
5. **Build** → The DLL will be generated at `build/Server Release/Plugins/StarRuptureDiscordPlugin.dll`.

Alternatively, download the pre-built DLL from the [latest CI build](https://github.com/alexandru360/unraid-starrupture-discord-bot/actions/workflows/docker-publish.yml) (Artifacts → `StarRuptureDiscordPlugin-DLL`).

## Server Installation

1. Copy `StarRuptureDiscordPlugin.dll` to the server directory:
   ```
   <Server>/Binaries/Win64/Plugins/StarRuptureDiscordPlugin.dll
   ```
2. Start the server once — the plugin will automatically create the config file:
   ```
   Plugins/config/StarRuptureDiscordPlugin.ini
   ```
3. Edit the configuration:
   ```ini
   [General]
   Enabled=1

   [Export]
   OutputPath=Plugins/data/rupture_state.json
   PollIntervalMs=1000

   [Diagnostics]
   VerboseLogs=0
   ```
4. Restart the server.

## Exported JSON File

The plugin writes the current state on every change to `Plugins/data/rupture_state.json`:

```json
{
  "sequence": 42,
  "wave": "Heat",
  "stage": "Growback",
  "step": "MoonPhase",
  "elapsed": 179.774,
  "timestamp": "2026-03-25T12:00:00Z"
}
```

Fields:
| Field | Description |
|-------|-------------|
| `sequence` | Sequential update number |
| `wave` | Wave type: `None`, `Heat`, `Cold` |
| `stage` | Stage: `None`, `PreWave`, `Moving`, `Fadeout`, `Growback` |
| `step` | Active sub-stage |
| `elapsed` | Seconds since the current wave started |
| `timestamp` | ISO 8601 timestamp (UTC) |

## Troubleshooting

- **Plugin does not load**: Verify the DLL is in `Plugins/` and `Enabled=1` is set in the `.ini` file.
- **JSON file is not created**: Check the logs in `Plugins/logs/modloader.log`. Set `VerboseLogs=1`.
- **Plugin says "intended for dedicated server"**: It only runs on the dedicated server, not on the client.

## Note

The plugin **must** be compiled in C++ because StarRupture-ModLoader uses a native C++ API
with vtable-based interfaces (`IPluginLogger*`, `IPluginHooks*`, etc.). A pure C# DLL cannot
implement these interfaces without a native layer.
