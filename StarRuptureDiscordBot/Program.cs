using Discord;
using Discord.WebSocket;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using StarRuptureDiscordBot.Services;

var host = Host.CreateDefaultBuilder(args)
    .ConfigureAppConfiguration((context, config) =>
    {
        config.SetBasePath(AppContext.BaseDirectory);
        config.AddJsonFile("appsettings.json", optional: false, reloadOnChange: true);
        config.AddEnvironmentVariables();
    })
    .ConfigureLogging(logging =>
    {
        logging.ClearProviders();
        logging.AddConsole();
        logging.SetMinimumLevel(LogLevel.Information);
    })
    .ConfigureServices((context, services) =>
    {
        var discordConfig = new DiscordSocketConfig
        {
            GatewayIntents = GatewayIntents.Guilds | GatewayIntents.GuildMessages,
            LogLevel = LogSeverity.Info
        };

        var client = new DiscordSocketClient(discordConfig);
        services.AddSingleton(client);
        services.AddHostedService<DiscordBotService>();
        services.AddHostedService<RuptureStateWatcher>();
    })
    .Build();

await host.RunAsync();

/// <summary>
/// Manages the Discord bot connection lifecycle.
/// </summary>
internal sealed class DiscordBotService : IHostedService
{
    private readonly DiscordSocketClient _client;
    private readonly IConfiguration _configuration;
    private readonly ILogger<DiscordBotService> _logger;

    public DiscordBotService(
        DiscordSocketClient client,
        IConfiguration configuration,
        ILogger<DiscordBotService> logger)
    {
        _client = client;
        _configuration = configuration;
        _logger = logger;
    }

    public async Task StartAsync(CancellationToken cancellationToken)
    {
        _client.Log += msg =>
        {
            _logger.LogInformation("[Discord] {Message}", msg.ToString());
            return Task.CompletedTask;
        };

        _client.Ready += () =>
        {
            _logger.LogInformation("Discord bot connected as {User}", _client.CurrentUser?.Username);
            return Task.CompletedTask;
        };

        var token = _configuration["Discord:Token"];
        if (string.IsNullOrWhiteSpace(token) || token == "YOUR_BOT_TOKEN_HERE")
        {
            _logger.LogError("Discord bot token is not configured! Set 'Discord:Token' in appsettings.json.");
            return;
        }

        await _client.LoginAsync(TokenType.Bot, token);
        await _client.StartAsync();
    }

    public async Task StopAsync(CancellationToken cancellationToken)
    {
        await _client.StopAsync();
        await _client.DisposeAsync();
        _logger.LogInformation("Discord bot disconnected.");
    }
}
