using System.Text.Json;
using Discord;
using Discord.WebSocket;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using StarRuptureDiscordBot.Models;

namespace StarRuptureDiscordBot.Services;

public sealed class RuptureStateWatcher : BackgroundService
{
    private readonly ILogger<RuptureStateWatcher> _logger;
    private readonly DiscordSocketClient _discord;
    private readonly string _filePath;
    private readonly int _pollIntervalMs;
    private readonly ulong _channelId;
    private ulong _lastSequence = ulong.MaxValue;
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNameCaseInsensitive = true
    };

    public RuptureStateWatcher(
        ILogger<RuptureStateWatcher> logger,
        DiscordSocketClient discord,
        IConfiguration configuration)
    {
        _logger = logger;
        _discord = discord;
        _filePath = configuration["RuptureState:FilePath"]
            ?? "Plugins/data/rupture_state.json";
        _pollIntervalMs = int.TryParse(configuration["RuptureState:PollIntervalMs"], out var interval)
            ? Math.Clamp(interval, 500, 60000)
            : 2000;
        _channelId = ulong.TryParse(configuration["Discord:ChannelId"], out var id) ? id : 0;
    }

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        _logger.LogInformation("RuptureStateWatcher started. Monitoring: {FilePath} every {Interval}ms",
            _filePath, _pollIntervalMs);

        if (_channelId == 0)
        {
            _logger.LogError("Discord ChannelId is not configured. Set 'Discord:ChannelId' in appsettings.json.");
            return;
        }

        // Wait for Discord to be ready
        while (_discord.ConnectionState != ConnectionState.Connected && !stoppingToken.IsCancellationRequested)
        {
            await Task.Delay(1000, stoppingToken);
        }

        while (!stoppingToken.IsCancellationRequested)
        {
            try
            {
                await PollAndPublishAsync(stoppingToken);
            }
            catch (OperationCanceledException) when (stoppingToken.IsCancellationRequested)
            {
                break;
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Error polling rupture state");
            }

            await Task.Delay(_pollIntervalMs, stoppingToken);
        }
    }

    private async Task PollAndPublishAsync(CancellationToken ct)
    {
        if (!File.Exists(_filePath))
            return;

        string json;
        try
        {
            // Use FileShare.ReadWrite so we don't block the plugin from writing
            using var stream = new FileStream(_filePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
            using var reader = new StreamReader(stream);
            json = await reader.ReadToEndAsync(ct);
        }
        catch (IOException)
        {
            // File might be mid-write; retry next poll
            return;
        }

        if (string.IsNullOrWhiteSpace(json))
            return;

        var state = JsonSerializer.Deserialize<RuptureState>(json, JsonOptions);
        if (state is null)
            return;

        // Only publish if sequence changed
        if (state.Sequence == _lastSequence)
            return;

        _lastSequence = state.Sequence;

        var channel = _discord.GetChannel(_channelId) as IMessageChannel;
        if (channel is null)
        {
            _logger.LogWarning("Could not find Discord channel {ChannelId}", _channelId);
            return;
        }

        var embed = BuildEmbed(state);
        await channel.SendMessageAsync(embed: embed);

        _logger.LogInformation("Published rupture state seq={Sequence} wave={Wave} stage={Stage}",
            state.Sequence, state.Wave, state.Stage);
    }

    private static Embed BuildEmbed(RuptureState state)
    {
        var color = state.Wave switch
        {
            "Heat" => Color.Red,
            "Cold" => Color.Blue,
            _ => Color.LightGrey
        };

        var waveEmoji = state.Wave switch
        {
            "Heat" => "\U0001f525",  // fire emoji
            "Cold" => "\u2744\ufe0f", // snowflake emoji
            _ => "\U0001f30d"         // earth emoji
        };

        var builder = new EmbedBuilder()
            .WithTitle($"{waveEmoji} Rupture Cycle Update")
            .WithColor(color)
            .AddField("Wave", state.Wave, inline: true)
            .AddField("Stage", state.Stage, inline: true)
            .AddField("Step", state.Step, inline: true)
            .AddField("Elapsed", $"{state.Elapsed:F1}s", inline: true)
            .AddField("Sequence", $"#{state.Sequence}", inline: true)
            .WithFooter("StarRupture Rupture Cycle Monitor")
            .WithTimestamp(DateTimeOffset.UtcNow);

        if (!string.IsNullOrEmpty(state.Timestamp))
        {
            if (DateTimeOffset.TryParse(state.Timestamp, out var ts))
            {
                builder.WithTimestamp(ts);
            }
        }

        return builder.Build();
    }
}
