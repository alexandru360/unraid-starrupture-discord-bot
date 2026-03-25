# Stage 1: Build
FROM mcr.microsoft.com/dotnet/sdk:8.0 AS build
WORKDIR /src

COPY StarRuptureDiscordBot.csproj ./
RUN dotnet restore

COPY . .
RUN dotnet publish -c Release -o /app --no-restore

# Stage 2: Runtime
FROM mcr.microsoft.com/dotnet/aspnet:8.0 AS runtime
WORKDIR /app

RUN groupadd -r botuser && useradd -r -g botuser botuser

COPY --from=build /app .

# appsettings.json is baked in as defaults;
# override at runtime via environment variables or volume mount
ENV Discord__Token=""
ENV Discord__ChannelId=""
ENV RuptureState__FilePath="/data/rupture_state.json"
ENV RuptureState__PollIntervalMs="2000"

VOLUME ["/data"]

USER botuser

ENTRYPOINT ["dotnet", "StarRuptureDiscordBot.dll"]
