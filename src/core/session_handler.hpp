#pragma once
#include <cstdint>
#include <string>

#include "config.hpp"
#include "../event/event.hpp"
#include "../network/client.hpp"
#include "../network/server.hpp"
#include "../world/world.hpp"

namespace core {
class SessionHandler {
public:
    explicit SessionHandler(
        Config& config,
        event::Dispatcher& dispatcher,
        network::Client& client,
        network::Server& server
    );
    ~SessionHandler();

private:
    void setup_raw_packet_handlers() const;
    void setup_unknown_packet_forwarders() const;
    void setup_on_send_to_server_handler();
    void setup_quit_handler() const;
    void setup_join_request_handler() const;
    void setup_disconnect_handler() const;
    void setup_connection_handlers();
    void setup_on_spawn_handler() const;
    void setup_on_remove_handler() const;

    bool try_connect_to_server();

private:
    Config& config_;
    event::Dispatcher& dispatcher_;
    network::Client& client_;
    network::Server& server_;

    std::string pending_address_;
    uint16_t pending_port_;

    // Retry state
    std::string last_server_address_;
    uint16_t last_server_port_{ 0 };
    uint8_t retry_count_{ 0 };
    static constexpr uint8_t MAX_RETRIES{ 3 };
};
}
