#pragma once
#include <utility>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace core {
class Logger {
public:
    Logger() = default;
    ~Logger() { logger_->flush(); }

    [[nodiscard]] static std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> create_console_sink()
    {
        auto console_sink{ std::make_shared<spdlog::sinks::stdout_color_sink_mt>() };
#ifdef GTPROXY_DEBUG
        console_sink->set_level(spdlog::level::trace);
#else
        console_sink->set_level(spdlog::level::info);
#endif
        return console_sink;
    }

    [[nodiscard]] static std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> create_file_sink()
    {
        return std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "proxy.log",
            1024 * 1024 * 2,
            4
        );
    }

    [[nodiscard]] static std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> create_packet_file_sink()
    {
        return std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "packets.log",
            1024 * 1024 * 5,
            3
        );
    }

    [[nodiscard]] static std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> create_dungeon_request_file_sink()
    {
        return std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "dungeon_request.log",
            1024 * 1024 * 5,
            3
        );
    }

    [[nodiscard]] static std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> create_dungeon_response_file_sink()
    {
        return std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "dungeon_response.log",
            1024 * 1024 * 5,
            3
        );
    }

    [[nodiscard]] static std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> create_map_data_file_sink()
    {
        return std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "map_data.log",
            1024 * 1024 * 5,
            3
        );
    }

    [[nodiscard]] static std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> create_tile_packets_file_sink()
    {
        return std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "tile_packets.log",
            1024 * 1024 * 5,
            3
        );
    }

    static void setup_packet_logger()
    {
        auto packet_sink{ create_packet_file_sink() };
        packet_sink->set_level(spdlog::level::trace);

        const auto packet_logger{ std::make_shared<spdlog::logger>("packet", packet_sink) };
        packet_logger->set_level(spdlog::level::trace);
        packet_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%^%l%$] %v");

        spdlog::register_logger(packet_logger);
    }

    static void setup_dungeon_loggers()
    {
        auto request_sink{ create_dungeon_request_file_sink() };
        request_sink->set_level(spdlog::level::trace);

        const auto request_logger{ std::make_shared<spdlog::logger>("dungeon_request", request_sink) };
        request_logger->set_level(spdlog::level::trace);
        request_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%^%l%$] %v");

        spdlog::register_logger(request_logger);

        auto response_sink{ create_dungeon_response_file_sink() };
        response_sink->set_level(spdlog::level::trace);

        const auto response_logger{ std::make_shared<spdlog::logger>("dungeon_response", response_sink) };
        response_logger->set_level(spdlog::level::trace);
        response_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%^%l%$] %v");

        spdlog::register_logger(response_logger);
    }

    static void setup_map_data_logger()
    {
        auto map_data_sink{ create_map_data_file_sink() };
        map_data_sink->set_level(spdlog::level::trace);

        const auto map_data_logger{ std::make_shared<spdlog::logger>("map_data", map_data_sink) };
        map_data_logger->set_level(spdlog::level::trace);
        map_data_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%^%l%$] %v");

        spdlog::register_logger(map_data_logger);
    }

    static void setup_tile_packets_logger()
    {
        auto tile_packets_sink{ create_tile_packets_file_sink() };
        tile_packets_sink->set_level(spdlog::level::trace);

        const auto tile_packets_logger{ std::make_shared<spdlog::logger>("tile_packets", tile_packets_sink) };
        tile_packets_logger->set_level(spdlog::level::trace);
        tile_packets_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%^%l%$] %v");

        spdlog::register_logger(tile_packets_logger);
    }

    [[nodiscard]] std::shared_ptr<spdlog::logger> get_logger() const { return logger_; }
    void set_logger(std::shared_ptr<spdlog::logger> logger) { logger_ = std::move(logger); }

private:
    std::shared_ptr<spdlog::logger> logger_;
};
}
