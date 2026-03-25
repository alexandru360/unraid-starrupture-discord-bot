# StarRuptureDiscordBot — Installation Guide

## About

Discord bot written in C# (.NET 8) that monitors the JSON file exported by the
StarRuptureDiscordPlugin and publishes rupture cycle updates to a Discord channel.

## Requirements

- **.NET 8 SDK** — [Download](https://dotnet.microsoft.com/download/dotnet/8.0)
- **Discord bot** created on [Discord Developer Portal](https://discord.com/developers/applications)
- The `StarRuptureDiscordPlugin` installed on the StarRupture dedicated server

## Creating the Discord Bot

1. Go to [Discord Developer Portal](https://discord.com/developers/applications).
2. Click **New Application** → give it a name (e.g. `StarRupture Monitor`).
3. In the **Bot** tab:
   - Click **Reset Token** → **copy the token** (you will put it in `appsettings.json`).
   - Under **Privileged Gateway Intents**: enable **Message Content Intent** (optional but recommended).
4. In the **OAuth2 → URL Generator** tab:
   - Scopes: `bot`
   - Bot Permissions: `Send Messages`, `Embed Links`
   - Copy the generated URL and open it in a browser to invite the bot to your Discord server.
5. Note the **Channel ID** of the channel where you want the bot to post:
   - In Discord, enable Developer Mode (User Settings → Advanced → Developer Mode).
   - Right-click the channel → **Copy Channel ID**.

## Configuration

Edit `appsettings.json`:

```json
{
  "Discord": {
    "Token": "YOUR_BOT_TOKEN_HERE",
    "ChannelId": 1234567890123456789
  },
  "RuptureState": {
    "FilePath": "C:\\path\\to\\server\\Binaries\\Win64\\Plugins\\data\\rupture_state.json",
    "PollIntervalMs": 2000
  }
}
```

| Field | Description |
|-------|-------------|
| `Discord:Token` | Discord bot token |
| `Discord:ChannelId` | Discord channel ID (number) |
| `RuptureState:FilePath` | Absolute path to the JSON file exported by the plugin |
| `RuptureState:PollIntervalMs` | Polling interval in milliseconds (min 500, max 60000) |

You can also pass these as environment variables (e.g. `Discord__Token`, `Discord__ChannelId`).

## Build and Run

```powershell
cd StarRuptureDiscordBot

# Restore NuGet packages
dotnet restore

# Build
dotnet build -c Release

# Run
dotnet run -c Release
```

### Or publish as a standalone executable:

```powershell
dotnet publish -c Release -r win-x64 --self-contained true -o ./publish
```

Then run `./publish/StarRuptureDiscordBot.exe`.

### Or use Docker:

```bash
docker pull alex360/srupturabot
docker run -d \
  --name srupturabot \
  -e Discord__Token="YOUR_BOT_TOKEN" \
  -e Discord__ChannelId="YOUR_CHANNEL_ID" \
  -v /path/to/data:/data:ro \
  alex360/srupturabot:latest
```

## Running as a Windows Service (optional)

To run the bot permanently on your server, you can use **NSSM** (Non-Sucking Service Manager):

```powershell
# Download NSSM from https://nssm.cc/
nssm install StarRuptureDiscordBot "C:\path\to\publish\StarRuptureDiscordBot.exe"
nssm start StarRuptureDiscordBot
```

## What You Will See in Discord

The bot posts a rich **embed** every time the rupture cycle state changes:

```
🔥 Rupture Cycle Update
━━━━━━━━━━━━━━━━━━━━━━
Wave:     Heat
Stage:    Growback
Step:     MoonPhase
Elapsed:  179.8s
Sequence: #42
━━━━━━━━━━━━━━━━━━━━━━
StarRupture Rupture Cycle Monitor
```

- **Heat** → red embed with fire emoji 🔥
- **Cold** → blue embed with snowflake emoji ❄️
- **None** → grey embed with earth emoji 🌍

## Troubleshooting

- **"Discord bot token is not configured"**: Set the token in `appsettings.json`.
- **"Discord ChannelId is not configured"**: Set `ChannelId` with the numeric channel ID.
- **"Could not find Discord channel"**: Verify the bot is invited to the server and has permissions on the channel.
- **Bot does not post anything**: Verify the JSON file exists at the configured path and the server-side plugin is running.
- **IO errors**: The JSON file may be written at the same time — the bot retries automatically on the next poll.
