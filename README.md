# StarRupture Discord Integration

[![Build & Push](https://github.com/alexandru360/unraid-starrupture-discord-bot/actions/workflows/docker-publish.yml/badge.svg)](https://github.com/alexandru360/unraid-starrupture-discord-bot/actions/workflows/docker-publish.yml)
[![Docker Image](https://img.shields.io/badge/Docker-alex360%2Fsrupturabot-blue?logo=docker)](https://hub.docker.com/r/alex360/srupturabot)
[![Plugin DLL](https://img.shields.io/badge/Download-Plugin%20DLL-green?logo=github)](https://github.com/alexandru360/unraid-starrupture-discord-bot/actions/workflows/docker-publish.yml)

Complete integration to publish StarRupture rupture cycle state to Discord.

## Downloads

| Component | Link |
|-----------|------|
| **Plugin DLL** | [Download from latest CI build](https://github.com/alexandru360/unraid-starrupture-discord-bot/actions/workflows/docker-publish.yml) — click the latest run → Artifacts → `StarRuptureDiscordPlugin-DLL` |
| **Docker Image** | [`docker pull alex360/srupturabot`](https://hub.docker.com/r/alex360/srupturabot) |

## Architecture

```
┌──────────────────────────┐     JSON file      ┌──────────────────────────┐
│  StarRupture Server      │  ──────────────►   │  StarRuptureDiscordBot   │
│  + StarRupturePlugin.dll │  rupture_state.json │  (C# .NET 8)            │
│  (C++ native)            │                     │                          │
└──────────────────────────┘                     └───────────┬──────────────┘
                                                             │
                                                             │ Discord API
                                                             ▼
                                                    ┌────────────────┐
                                                    │  Discord       │
                                                    │  #rupture-chan │
                                                    └────────────────┘
```

## Components

### 1. `StarRupturePlugin/` — C++ plugin for the dedicated server

Native plugin for [StarRupture-ModLoader](https://github.com/AlienXAXS/StarRupture-ModLoader).
Reads the rupture cycle state from `UCrEnviroWaveSubsystem` and exports it to a JSON file.

**Inspired by**: [RuptureCycleToChat_Plugin](https://github.com/Mralexandresys/RuptureCycleToChat_Plugin)

> **Why C++ and not C#?** The StarRupture ModLoader uses a native C++ API with
> vtable-based interfaces (`IPluginLogger*`, `IPluginHooks*`, etc.). Plugins export C
> functions (`GetPluginInfo`, `PluginInit`, `PluginShutdown`). A C# DLL cannot directly
> implement these interfaces without a native layer, so the plugin must be C++.

[Plugin installation instructions](StarRupturePlugin/INSTALL.md)

### 2. `StarRuptureDiscordBot/` — Discord bot in C#

Discord bot (.NET 8) that monitors the JSON file and publishes updates with rich embeds
to a Discord channel.

[Bot installation instructions](StarRuptureDiscordBot/INSTALL.md)

## Quick Start

### Step 1: Server-side plugin

1. Build `StarRupturePlugin` with Visual Studio 2022 (see [INSTALL.md](StarRupturePlugin/INSTALL.md)), or download the DLL from the [latest CI build](https://github.com/alexandru360/unraid-starrupture-discord-bot/actions/workflows/docker-publish.yml).
2. Copy the DLL to `<Server>/Binaries/Win64/Plugins/`.
3. Set `Enabled=1` in `Plugins/config/StarRuptureDiscordPlugin.ini`.

### Step 2: Discord bot

1. Create a bot application on [Discord Developer Portal](https://discord.com/developers/applications).
2. Configure `appsettings.json` with your token and channel ID.
3. Run the bot:

```powershell
cd StarRuptureDiscordBot
dotnet run -c Release
```

Or use Docker:

```bash
docker run -d \
  --name srupturabot \
  -e Discord__Token="YOUR_BOT_TOKEN" \
  -e Discord__ChannelId="YOUR_CHANNEL_ID" \
  -v /path/to/data:/data:ro \
  alex360/srupturabot:latest
```

## JSON Format

The plugin writes to `Plugins/data/rupture_state.json`:

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

MIT

## Credits

- [StarRupture-ModLoader](https://github.com/AlienXAXS/StarRupture-ModLoader) by AlienXAXS
- [RuptureCycleToChat_Plugin](https://github.com/Mralexandresys/RuptureCycleToChat_Plugin) by Mralexandresys — original inspiration
- [Discord.Net](https://github.com/discord-net/Discord.Net) — Discord library for C#
