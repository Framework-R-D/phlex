#include "phlex/resource.hpp"

#include <spdlog/logger.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include <memory>

PHLEX_REGISTER_RESOURCES(r [[maybe_unused]])
{
  auto sink = std::make_shared<spdlog::sinks::null_sink_mt>();
  spdlog::set_default_logger(std::make_shared<spdlog::logger>("sinkless", sink));
}
