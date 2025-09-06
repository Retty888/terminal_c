#include "config_schema.h"

namespace Config {

std::optional<ConfigData> ConfigSchema::parse(const nlohmann::json &j,
                                              std::string &error) {
  ConfigData cfg;
  if (j.contains("pairs")) {
    if (!j["pairs"].is_array()) {
      error = "'pairs' must be an array";
      return std::nullopt;
    }
    for (const auto &item : j["pairs"]) {
      if (item.is_string())
        cfg.pairs.push_back(item.get<std::string>());
      else {
        error = "'pairs' entries must be strings";
        return std::nullopt;
      }
    }
  }

  if (j.contains("log_level")) {
    if (!j["log_level"].is_string()) {
      error = "'log_level' must be a string";
      return std::nullopt;
    }
    std::string level = j["log_level"].get<std::string>();
    if (level == "INFO")
      cfg.log_level = Core::LogLevel::Info;
    else if (level == "WARN" || level == "WARNING")
      cfg.log_level = Core::LogLevel::Warning;
    else if (level == "ERROR")
      cfg.log_level = Core::LogLevel::Error;
    else {
      error = "Unknown log level '" + level + "'";
      return std::nullopt;
    }
  }

  if (j.contains("log_sinks")) {
    if (!j["log_sinks"].is_array()) {
      error = "'log_sinks' must be an array";
      return std::nullopt;
    }
    cfg.log_to_file = false;
    cfg.log_to_console = false;
    for (const auto &item : j["log_sinks"]) {
      if (!item.is_string()) {
        error = "'log_sinks' entries must be strings";
        return std::nullopt;
      }
      auto sink = item.get<std::string>();
      if (sink == "file")
        cfg.log_to_file = true;
      else if (sink == "console")
        cfg.log_to_console = true;
      else {
        error = "Unknown log sink '" + sink + "'";
        return std::nullopt;
      }
    }
    if (!cfg.log_to_file && !cfg.log_to_console) {
      error = "'log_sinks' must contain at least one valid sink";
      return std::nullopt;
    }
  }

  if (j.contains("log_file")) {
    if (!j["log_file"].is_string()) {
      error = "'log_file' must be a string";
      return std::nullopt;
    }
    cfg.log_file = j["log_file"].get<std::string>();
  }

  if (j.contains("candles_limit")) {
    if (!j["candles_limit"].is_number_unsigned()) {
      error = "'candles_limit' must be an unsigned number";
      return std::nullopt;
    }
    cfg.candles_limit = j["candles_limit"].get<std::size_t>();
  }
  if (j.contains("fetch_chunk_size")) {
    if (!j["fetch_chunk_size"].is_number_unsigned()) {
      error = "'fetch_chunk_size' must be an unsigned number";
      return std::nullopt;
    }
    // Will be applied into AppContext during load_config
  }

  if (j.contains("enable_streaming")) {
    if (!j["enable_streaming"].is_boolean()) {
      error = "'enable_streaming' must be a boolean";
      return std::nullopt;
    }
    cfg.enable_streaming = j["enable_streaming"].get<bool>();
  }

  if (j.contains("save_journal_csv")) {
    if (!j["save_journal_csv"].is_boolean()) {
      error = "'save_journal_csv' must be a boolean";
      return std::nullopt;
    }
    cfg.save_journal_csv = j["save_journal_csv"].get<bool>();
  }

  if (j.contains("enable_chart")) {
    if (!j["enable_chart"].is_boolean()) {
      error = "'enable_chart' must be a boolean";
      return std::nullopt;
    }
    cfg.enable_chart = j["enable_chart"].get<bool>();
  }

  if (j.contains("require_tv_chart")) {
    if (!j["require_tv_chart"].is_boolean()) {
      error = "'require_tv_chart' must be a boolean";
      return std::nullopt;
    }
    cfg.require_tv_chart = j["require_tv_chart"].get<bool>();
  }

  if (j.contains("http_timeout_ms")) {
    if (!j["http_timeout_ms"].is_number_unsigned()) {
      error = "'http_timeout_ms' must be an unsigned number";
      return std::nullopt;
    }
    cfg.http_timeout_ms = static_cast<int>(j["http_timeout_ms"].get<unsigned int>());
  }

  if (j.contains("webview_ready_timeout_ms")) {
    if (!j["webview_ready_timeout_ms"].is_number_unsigned()) {
      error = "'webview_ready_timeout_ms' must be an unsigned number";
      return std::nullopt;
    }
    cfg.webview_ready_timeout_ms = static_cast<int>(j["webview_ready_timeout_ms"].get<unsigned int>());
  }

  if (j.contains("webview_throttle_ms")) {
    if (!j["webview_throttle_ms"].is_number_unsigned()) {
      error = "'webview_throttle_ms' must be an unsigned number";
      return std::nullopt;
    }
    cfg.webview_throttle_ms = static_cast<int>(j["webview_throttle_ms"].get<unsigned int>());
  }

  if (j.contains("chart_html_path")) {
    if (!j["chart_html_path"].is_string()) {
      error = "'chart_html_path' must be a string";
      return std::nullopt;
    }
    cfg.chart_html_path = j["chart_html_path"].get<std::string>();
  }

  if (j.contains("primary_provider")) {
    if (!j["primary_provider"].is_string()) {
      error = "'primary_provider' must be a string";
      return std::nullopt;
    }
    cfg.primary_provider = j["primary_provider"].get<std::string>();
  }

  if (j.contains("fallback_provider")) {
    if (!j["fallback_provider"].is_string()) {
      error = "'fallback_provider' must be a string";
      return std::nullopt;
    }
    cfg.fallback_provider = j["fallback_provider"].get<std::string>();
  }

  if (j.contains("signal")) {
    if (!j["signal"].is_object()) {
      error = "'signal' must be an object";
      return std::nullopt;
    }
    const auto &s = j["signal"];
    if (s.contains("type")) {
      if (!s["type"].is_string()) {
        error = "'signal.type' must be a string";
        return std::nullopt;
      }
      cfg.signal.type = s["type"].get<std::string>();
    }
    if (s.contains("short_period")) {
      if (!s["short_period"].is_number_unsigned()) {
        error = "'signal.short_period' must be an unsigned number";
        return std::nullopt;
      }
      cfg.signal.short_period = s["short_period"].get<std::size_t>();
    }
    if (s.contains("long_period")) {
      if (!s["long_period"].is_number_unsigned()) {
        error = "'signal.long_period' must be an unsigned number";
        return std::nullopt;
      }
      cfg.signal.long_period = s["long_period"].get<std::size_t>();
    }
    if (s.contains("params")) {
      if (!s["params"].is_object()) {
        error = "'signal.params' must be an object";
        return std::nullopt;
      }
      for (auto it = s["params"].begin(); it != s["params"].end(); ++it) {
        if (!it.value().is_number()) {
          error = "'signal.params." + it.key() + "' must be a number";
          return std::nullopt;
        }
        cfg.signal.params[it.key()] = it.value().get<double>();
      }
    }
  }

  return cfg;
}

} // namespace Config
