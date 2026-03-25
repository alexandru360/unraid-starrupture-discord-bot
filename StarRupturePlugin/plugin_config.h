#pragma once

#include "plugin_interface.h"

namespace StarRuptureDiscordConfig
{
	static constexpr const char* kPluginName = "StarRuptureDiscordPlugin";

	static const ConfigEntry CONFIG_ENTRIES[] =
	{
		{
			"General",
			"Enabled",
			ConfigValueType::Boolean,
			"0",
			"Enable or disable the rupture cycle JSON exporter."
		},
		{
			"Export",
			"OutputPath",
			ConfigValueType::String,
			"Plugins/data/rupture_state.json",
			"Path to the JSON file where rupture cycle state is written."
		},
		{
			"Export",
			"PollIntervalMs",
			ConfigValueType::Integer,
			"1000",
			"Polling interval in milliseconds while the ChimeraMain world is running."
		},
		{
			"Diagnostics",
			"VerboseLogs",
			ConfigValueType::Boolean,
			"0",
			"Enable verbose lifecycle and export logs."
		}
	};

	static const ConfigSchema SCHEMA =
	{
		CONFIG_ENTRIES,
		static_cast<int>(sizeof(CONFIG_ENTRIES) / sizeof(ConfigEntry))
	};

	class Config
	{
	public:
		static void Initialize(IPluginConfig* config)
		{
			s_config = config;
			if (s_config)
			{
				s_config->InitializeFromSchema(kPluginName, &SCHEMA);
			}
		}

		static bool IsEnabled()
		{
			return s_config ? s_config->ReadBool(kPluginName, "General", "Enabled", false) : false;
		}

		static const char* OutputPath()
		{
			static char buffer[512];
			if (s_config && s_config->ReadString(kPluginName, "Export", "OutputPath", buffer, sizeof(buffer), "Plugins/data/rupture_state.json"))
			{
				return buffer;
			}
			return "Plugins/data/rupture_state.json";
		}

		static int PollIntervalMs()
		{
			int interval = s_config ? s_config->ReadInt(kPluginName, "Export", "PollIntervalMs", 1000) : 1000;
			if (interval < 100) interval = 100;
			if (interval > 60000) interval = 60000;
			return interval;
		}

		static bool VerboseLogs()
		{
			return s_config ? s_config->ReadBool(kPluginName, "Diagnostics", "VerboseLogs", false) : false;
		}

	private:
		static IPluginConfig* s_config;
	};
}
