#ifndef TEST_OSTREAM_LOGGER_HPP
#define TEST_OSTREAM_LOGGER_HPP

#include "spdlog/sinks/ostream_sink.h"
#include "spdlog/spdlog.h"

#include <memory>
#include <ostream>
#include <utility>

namespace phlex::test {
  class ostream_logger {
  public:
    explicit ostream_logger(std::ostream& stream) : previous_logger_{spdlog::default_logger()}
    {
      auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
      spdlog::set_default_logger(std::make_shared<spdlog::logger>("test_logger", sink));
    }

    ostream_logger(ostream_logger const&) = delete;
    ostream_logger& operator=(ostream_logger const&) = delete;
    ostream_logger(ostream_logger&&) = delete;
    ostream_logger& operator=(ostream_logger&&) = delete;

    ~ostream_logger() { spdlog::set_default_logger(std::move(previous_logger_)); }

  private:
    std::shared_ptr<spdlog::logger> previous_logger_;
  };

  inline ostream_logger use_ostream_logger(std::ostream& stream) { return ostream_logger{stream}; }
}

#endif // TEST_OSTREAM_LOGGER_HPP
