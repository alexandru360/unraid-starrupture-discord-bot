#include "plugin.h"
#include "plugin_config.h"
#include "plugin_helpers.h"

#include "Chimera_classes.hpp"
#include "Chimera_structs.hpp"
#include "Engine_classes.hpp"

#include <cstdint>
#include <cstring>
#include <cwchar>
#include <sstream>
#include <string>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>

static IPluginLogger* g_logger = nullptr;
static IPluginConfig* g_config = nullptr;
static IPluginScanner* g_scanner = nullptr;
static IPluginHooks* g_hooks = nullptr;

IPluginLogger* GetLogger() { return g_logger; }
IPluginConfig* GetConfig() { return g_config; }
IPluginScanner* GetScanner() { return g_scanner; }
IPluginHooks* GetHooks() { return g_hooks; }

#ifndef MODLOADER_BUILD_TAG
#define MODLOADER_BUILD_TAG "dev"
#endif

static PluginInfo s_pluginInfo =
{
	"StarRuptureDiscordPlugin",
	MODLOADER_BUILD_TAG,
	"StarRuptureDiscordBot",
	"Dedicated-server plugin that exports rupture cycle state to JSON for Discord integration",
	PLUGIN_INTERFACE_VERSION
};

namespace
{
	struct RuptureCycleState final
	{
		SDK::EEnviroWave Type = SDK::EEnviroWave::None;
		SDK::EEnviroWaveStage Stage = SDK::EEnviroWaveStage::None;
		SDK::EEnviroWavePreWaveSubstage PreWaveSubstage = SDK::EEnviroWavePreWaveSubstage::None;
		SDK::EEnviroWaveFadeoutSubstage FadeoutSubstage = SDK::EEnviroWaveFadeoutSubstage::None;
		SDK::EEnviroWaveGrowbackSubstage GrowbackSubstage = SDK::EEnviroWaveGrowbackSubstage::None;
		double TimeSinceLastWaveStarted = 0.0;
	};

	bool CaptureRuptureCycleState(SDK::UWorld* world, RuptureCycleState& outState);

	SDK::UWorld* g_chimeraWorld = nullptr;
	bool g_worldReady = false;
	bool g_forceExport = false;
	float g_tickAccumulatorSeconds = 0.0f;
	uint64_t g_messageSequence = 0;
	std::string g_lastExportSignature;

	bool ShouldLogVerbose()
	{
		return StarRuptureDiscordConfig::Config::VerboseLogs();
	}

	bool IsServerBinary()
	{
		wchar_t path[MAX_PATH] = {};
		if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0)
		{
			return false;
		}

		wchar_t* filename = wcsrchr(path, L'\\');
		if (!filename)
		{
			filename = wcsrchr(path, L'/');
		}

		const wchar_t* exeName = filename ? (filename + 1) : path;
		return _wcsicmp(exeName, L"StarRuptureServerEOS-Win64-Shipping.exe") == 0;
	}

	float GetPollIntervalSeconds()
	{
		return static_cast<float>(StarRuptureDiscordConfig::Config::PollIntervalMs()) / 1000.0f;
	}

	bool IsChimeraWorldName(const char* worldName)
	{
		return worldName != nullptr && std::strstr(worldName, "ChimeraMain") != nullptr;
	}

	bool IsMenuWorldName(const char* worldName)
	{
		return worldName != nullptr && std::strstr(worldName, "MainMenu") != nullptr;
	}

	const char* EnviroWaveToString(SDK::EEnviroWave wave)
	{
		switch (wave)
		{
		case SDK::EEnviroWave::None:  return "None";
		case SDK::EEnviroWave::Heat:  return "Heat";
		case SDK::EEnviroWave::Cold:  return "Cold";
		default:                      return "Unknown";
		}
	}

	const char* EnviroWaveStageToString(SDK::EEnviroWaveStage stage)
	{
		switch (stage)
		{
		case SDK::EEnviroWaveStage::None:     return "None";
		case SDK::EEnviroWaveStage::PreWave:  return "PreWave";
		case SDK::EEnviroWaveStage::Moving:   return "Moving";
		case SDK::EEnviroWaveStage::Fadeout:  return "Fadeout";
		case SDK::EEnviroWaveStage::Growback: return "Growback";
		default:                              return "Unknown";
		}
	}

	const char* PreWaveSubstageToString(SDK::EEnviroWavePreWaveSubstage substage)
	{
		switch (substage)
		{
		case SDK::EEnviroWavePreWaveSubstage::None:            return "None";
		case SDK::EEnviroWavePreWaveSubstage::BeforeExplosion: return "BeforeExplosion";
		case SDK::EEnviroWavePreWaveSubstage::AfterExplosion:  return "AfterExplosion";
		default:                                               return "Unknown";
		}
	}

	const char* FadeoutSubstageToString(SDK::EEnviroWaveFadeoutSubstage substage)
	{
		switch (substage)
		{
		case SDK::EEnviroWaveFadeoutSubstage::None:     return "None";
		case SDK::EEnviroWaveFadeoutSubstage::FireWave: return "FireWave";
		case SDK::EEnviroWaveFadeoutSubstage::Burning:  return "Burning";
		case SDK::EEnviroWaveFadeoutSubstage::Fading:   return "Fading";
		default:                                        return "Unknown";
		}
	}

	const char* GrowbackSubstageToString(SDK::EEnviroWaveGrowbackSubstage substage)
	{
		switch (substage)
		{
		case SDK::EEnviroWaveGrowbackSubstage::None:          return "None";
		case SDK::EEnviroWaveGrowbackSubstage::MoonPhase:     return "MoonPhase";
		case SDK::EEnviroWaveGrowbackSubstage::RegrowthStart: return "RegrowthStart";
		case SDK::EEnviroWaveGrowbackSubstage::Regrowth:      return "Regrowth";
		default:                                              return "Unknown";
		}
	}

	const char* EnviroWaveStepToString(const RuptureCycleState& state)
	{
		if (state.PreWaveSubstage != SDK::EEnviroWavePreWaveSubstage::None)
			return PreWaveSubstageToString(state.PreWaveSubstage);
		if (state.FadeoutSubstage != SDK::EEnviroWaveFadeoutSubstage::None)
			return FadeoutSubstageToString(state.FadeoutSubstage);
		if (state.GrowbackSubstage != SDK::EEnviroWaveGrowbackSubstage::None)
			return GrowbackSubstageToString(state.GrowbackSubstage);
		return "None";
	}

	template <typename TSubsystem>
	TSubsystem* TryGetWorldSubsystem(SDK::UWorld* world, SDK::UClass* subsystemClass)
	{
		if (!world || !subsystemClass)
			return nullptr;

		return static_cast<TSubsystem*>(
			SDK::USubsystemBlueprintLibrary::GetWorldSubsystem(world, subsystemClass));
	}

	SDK::ACrGameStateBase* TryGetGameState(SDK::UWorld* world)
	{
		if (!world)
			return nullptr;

		SDK::AGameStateBase* gameStateBase = SDK::UGameplayStatics::GetGameState(world);
		return static_cast<SDK::ACrGameStateBase*>(gameStateBase);
	}

	bool CaptureRuptureCycleState(SDK::UWorld* world, RuptureCycleState& outState)
	{
		SDK::ACrGameStateBase* gameState = TryGetGameState(world);
		if (!gameState || !gameState->bIsDedicatedServer)
			return false;

		SDK::UCrEnviroWaveSubsystem* waveSubsystem =
			TryGetWorldSubsystem<SDK::UCrEnviroWaveSubsystem>(
				world,
				SDK::UCrEnviroWaveSubsystem::StaticClass());
		if (!waveSubsystem)
			return false;

		outState.Type = waveSubsystem->GetCurrentType();
		outState.Stage = waveSubsystem->GetCurrentStage();
		outState.PreWaveSubstage = waveSubsystem->CurrentPreWaveSubstage;
		outState.FadeoutSubstage = waveSubsystem->CurrentFadeoutSubstage;
		outState.GrowbackSubstage = waveSubsystem->CurrentGrowbackSubstage;
		outState.TimeSinceLastWaveStarted = waveSubsystem->GetTimeSinceLastWaveStarted();

		return true;
	}

	std::string BuildExportSignature(const RuptureCycleState& state)
	{
		std::ostringstream oss;
		oss
			<< static_cast<int>(state.Type) << '|'
			<< static_cast<int>(state.Stage) << '|'
			<< static_cast<int>(state.PreWaveSubstage) << '|'
			<< static_cast<int>(state.FadeoutSubstage) << '|'
			<< static_cast<int>(state.GrowbackSubstage);
		return oss.str();
	}

	// Escapes a string for safe JSON embedding
	std::string JsonEscape(const std::string& input)
	{
		std::string output;
		output.reserve(input.size());
		for (char c : input)
		{
			switch (c)
			{
			case '"':  output += "\\\""; break;
			case '\\': output += "\\\\"; break;
			case '\n': output += "\\n";  break;
			case '\r': output += "\\r";  break;
			case '\t': output += "\\t";  break;
			default:   output += c;      break;
			}
		}
		return output;
	}

	std::string GetISO8601Timestamp()
	{
		auto now = std::chrono::system_clock::now();
		auto time_t_now = std::chrono::system_clock::to_time_t(now);
		struct tm tm_buf;
		gmtime_s(&tm_buf, &time_t_now);
		std::ostringstream oss;
		oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
		return oss.str();
	}

	bool WriteRuptureStateToJson(const RuptureCycleState& state)
	{
		const char* outputPath = StarRuptureDiscordConfig::Config::OutputPath();

		// Ensure output directory exists
		std::filesystem::path filePath(outputPath);
		std::filesystem::path parentDir = filePath.parent_path();
		if (!parentDir.empty())
		{
			std::error_code ec;
			std::filesystem::create_directories(parentDir, ec);
			if (ec)
			{
				LOG_ERROR("Failed to create output directory '%s': %s", parentDir.string().c_str(), ec.message().c_str());
				return false;
			}
		}

		// Build JSON payload manually (no external JSON library dependency)
		std::ostringstream json;
		json.setf(std::ios::fixed);
		json.precision(3);
		json << "{\n";
		json << "  \"sequence\": " << g_messageSequence << ",\n";
		json << "  \"wave\": \"" << JsonEscape(EnviroWaveToString(state.Type)) << "\",\n";
		json << "  \"stage\": \"" << JsonEscape(EnviroWaveStageToString(state.Stage)) << "\",\n";
		json << "  \"step\": \"" << JsonEscape(EnviroWaveStepToString(state)) << "\",\n";
		json << "  \"elapsed\": " << state.TimeSinceLastWaveStarted << ",\n";
		json << "  \"timestamp\": \"" << GetISO8601Timestamp() << "\"\n";
		json << "}\n";

		// Write to a temp file first, then rename for atomicity
		std::string tempPath = std::string(outputPath) + ".tmp";
		{
			std::ofstream outFile(tempPath, std::ios::out | std::ios::trunc);
			if (!outFile.is_open())
			{
				LOG_ERROR("Failed to open temp file for writing: %s", tempPath.c_str());
				return false;
			}
			outFile << json.str();
			outFile.flush();
			if (outFile.fail())
			{
				LOG_ERROR("Failed to write rupture state to temp file: %s", tempPath.c_str());
				return false;
			}
		}

		// Atomic rename
		std::error_code ec;
		std::filesystem::rename(tempPath, outputPath, ec);
		if (ec)
		{
			LOG_ERROR("Failed to rename temp file to output: %s", ec.message().c_str());
			// Try direct delete + rename as fallback
			std::filesystem::remove(outputPath, ec);
			std::filesystem::rename(tempPath, outputPath, ec);
			if (ec)
			{
				LOG_ERROR("Fallback rename also failed: %s", ec.message().c_str());
				return false;
			}
		}

		++g_messageSequence;

		if (ShouldLogVerbose())
		{
			LOG_INFO("Exported rupture state to '%s': wave=%s stage=%s step=%s elapsed=%.3f",
				outputPath,
				EnviroWaveToString(state.Type),
				EnviroWaveStageToString(state.Stage),
				EnviroWaveStepToString(state),
				state.TimeSinceLastWaveStarted);
		}

		return true;
	}

	bool TryBootstrapCurrentWorld(const char* reason)
	{
		SDK::UWorld* world = SDK::UWorld::GetWorld();
		if (!world)
			return false;

		const std::string worldName = world->GetName();
		if (!IsChimeraWorldName(worldName.c_str()))
			return false;

		if (g_chimeraWorld != world)
		{
			g_chimeraWorld = world;
			g_forceExport = true;
			g_tickAccumulatorSeconds = 0.0f;
			g_lastExportSignature.clear();
			if (ShouldLogVerbose())
			{
				LOG_INFO("Bootstrapped ChimeraMain world=%p via %s",
					static_cast<void*>(world), reason ? reason : "unknown");
			}
		}

		RuptureCycleState probeState{};
		if (!g_worldReady && CaptureRuptureCycleState(world, probeState))
		{
			g_worldReady = true;
			g_forceExport = true;
			if (ShouldLogVerbose())
			{
				LOG_INFO("Bootstrapped ChimeraMain readiness via %s", reason ? reason : "unknown");
			}
		}

		return g_chimeraWorld != nullptr;
	}

	void ResetRuntimeState()
	{
		g_chimeraWorld = nullptr;
		g_worldReady = false;
		g_forceExport = false;
		g_tickAccumulatorSeconds = 0.0f;
		g_messageSequence = 0;
		g_lastExportSignature.clear();
	}

	void MarkWorldReady(const char* reason)
	{
		if (!g_chimeraWorld)
			return;

		g_worldReady = true;
		g_forceExport = true;
		if (ShouldLogVerbose())
		{
			LOG_INFO("ChimeraMain world marked ready by %s", reason ? reason : "unknown");
		}
	}

	void OnAnyWorldBeginPlay(SDK::UWorld* world, const char* worldName)
	{
		if (IsChimeraWorldName(worldName))
		{
			g_chimeraWorld = world;
			g_worldReady = false;
			g_forceExport = true;
			g_tickAccumulatorSeconds = 0.0f;
			g_lastExportSignature.clear();
			if (ShouldLogVerbose())
			{
				LOG_INFO("Tracking ChimeraMain world=%p", static_cast<void*>(world));
			}
			return;
		}

		if (IsMenuWorldName(worldName))
		{
			if (ShouldLogVerbose())
			{
				LOG_INFO("Clearing tracked world on menu transition: %s", worldName ? worldName : "(null)");
			}
			ResetRuntimeState();
		}
	}

	void OnSaveLoaded()
	{
		MarkWorldReady("SaveLoaded");
	}

	void OnExperienceLoadComplete()
	{
		MarkWorldReady("ExperienceLoadComplete");
	}

	void OnEngineTick(float deltaSeconds)
	{
		if (!g_chimeraWorld || !g_worldReady)
		{
			TryBootstrapCurrentWorld("EngineTick");
		}

		if (!g_chimeraWorld || !g_worldReady)
			return;

		const float interval = GetPollIntervalSeconds();
		if (deltaSeconds <= 0.0f || deltaSeconds > 5.0f)
		{
			g_tickAccumulatorSeconds = interval;
		}
		else
		{
			g_tickAccumulatorSeconds += deltaSeconds;
		}

		if (g_tickAccumulatorSeconds < interval)
			return;

		g_tickAccumulatorSeconds = 0.0f;

		RuptureCycleState state{};
		if (!CaptureRuptureCycleState(g_chimeraWorld, state))
			return;

		const std::string currentSignature = BuildExportSignature(state);
		if (!g_forceExport && currentSignature == g_lastExportSignature)
			return;

		if (WriteRuptureStateToJson(state))
		{
			g_lastExportSignature = currentSignature;
			g_forceExport = false;
		}
	}

	void OnEngineShutdown()
	{
		ResetRuntimeState();
	}
}

extern "C" {

__declspec(dllexport) PluginInfo* GetPluginInfo()
{
	return &s_pluginInfo;
}

__declspec(dllexport) bool PluginInit(IPluginLogger* logger, IPluginConfig* config, IPluginScanner* scanner, IPluginHooks* hooks)
{
	g_logger = logger;
	g_config = config;
	g_scanner = scanner;
	g_hooks = hooks;

	LOG_INFO("Plugin initializing...");

	StarRuptureDiscordConfig::Config::Initialize(config);
	if (!StarRuptureDiscordConfig::Config::IsEnabled())
	{
		LOG_INFO("Plugin is disabled in config");
		return true;
	}

	if (!IsServerBinary())
	{
		LOG_WARN("This plugin is intended for dedicated server use only - skipping initialization");
		return true;
	}

	if (!hooks || !hooks->Engine || !hooks->World)
	{
		LOG_ERROR("Required hook interfaces are unavailable");
		return false;
	}

	hooks->Engine->RegisterOnShutdown(OnEngineShutdown);
	hooks->Engine->RegisterOnTick(OnEngineTick);
	hooks->World->RegisterOnAnyWorldBeginPlay(OnAnyWorldBeginPlay);
	hooks->World->RegisterOnSaveLoaded(OnSaveLoaded);
	hooks->World->RegisterOnExperienceLoadComplete(OnExperienceLoadComplete);

	LOG_INFO("Rupture cycle JSON export is active (output: %s)", StarRuptureDiscordConfig::Config::OutputPath());
	return true;
}

__declspec(dllexport) void PluginShutdown()
{
	if (g_hooks)
	{
		if (g_hooks->World)
		{
			g_hooks->World->UnregisterOnAnyWorldBeginPlay(OnAnyWorldBeginPlay);
			g_hooks->World->UnregisterOnSaveLoaded(OnSaveLoaded);
			g_hooks->World->UnregisterOnExperienceLoadComplete(OnExperienceLoadComplete);
		}
		if (g_hooks->Engine)
		{
			g_hooks->Engine->UnregisterOnShutdown(OnEngineShutdown);
			g_hooks->Engine->UnregisterOnTick(OnEngineTick);
		}
	}

	ResetRuntimeState();
	LOG_INFO("Plugin shutting down");

	g_hooks = nullptr;
	g_scanner = nullptr;
	g_config = nullptr;
	g_logger = nullptr;
}

} // extern "C"
