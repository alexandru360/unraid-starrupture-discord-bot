namespace StarRuptureDiscordBot.Models;

public sealed class RuptureState
{
    public ulong Sequence { get; set; }
    public string Wave { get; set; } = "None";
    public string Stage { get; set; } = "None";
    public string Step { get; set; } = "None";
    public double Elapsed { get; set; }
    public string Timestamp { get; set; } = "";
}
