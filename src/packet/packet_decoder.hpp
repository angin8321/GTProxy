#pragma once
#include <optional>
#include <span>

#include <magic_enum/magic_enum.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>

#include "packet_registry.hpp"
#include "packet_types.hpp"
#include "payload.hpp"
#include "../utils/byte_stream.hpp"
#include "../utils/formatter/packet_variant_formatter.hpp"
#include "../utils/formatter/text_parse_formatter.hpp"

namespace packet {
class PacketDecoder {
public:
    std::optional<std::shared_ptr<IPacket>> decode(std::span<const std::byte> data) const
    {
        auto pkt_log{ spdlog::get("packet") };

        ByteStream stream{ data };

        NetMessageType msg_type{};
        if (!stream.read(msg_type)) {
            return std::nullopt;
        }

        if (msg_type != NET_MESSAGE_GAME_PACKET) {
            pkt_log->debug(
                "Decoding packet data ({} bytes):{}",
                data.size(),
                spdlog::to_hex(data.begin(), data.end())
            );
        }

        switch (msg_type) {
        case NET_MESSAGE_SERVER_HELLO: {
            TextPayload text_payload{ NET_MESSAGE_SERVER_HELLO };
            Payload payload = text_payload;

            auto packet = PacketRegistry::instance().create(payload);
            return packet;
        }
        case NET_MESSAGE_GENERIC_TEXT:
        case NET_MESSAGE_GAME_MESSAGE: {
            const std::size_t remaining{ stream.get_size() - sizeof(NetMessageType) };
            const std::size_t message_length{ remaining > 0 ? remaining - 1 : 0 };

            std::string message{};
            if (message_length > 0) {
                message.resize(message_length);
                if (!stream.read_data(message.data(), message_length)) {
                    pkt_log->warn("Failed to read {} bytes of text message", message_length);
                    return std::nullopt;
                }
            }

            TextParse parser{ message };
            pkt_log->info("Packet decoded to message:\n{}", fmt::format("{}", parser));

            TextPayload text_payload{ msg_type, std::move(parser) };
            Payload payload = text_payload;
            
            auto packet = PacketRegistry::instance().create(payload);
            if (!packet) {
                pkt_log->debug("No packet structure registered for this text message");
                return std::nullopt;
            }
            
            return packet;
        }
        case NET_MESSAGE_DUNGEON_REQUEST:
        case NET_MESSAGE_DUNGEON_RESPONSE: {
            const auto logger_name{
                msg_type == NET_MESSAGE_DUNGEON_REQUEST
                    ? "dungeon_request"
                    : "dungeon_response"
            };
            auto dungeon_log{ spdlog::get(logger_name) };

            const auto body{ data.subspan(sizeof(NetMessageType)) };
            if (dungeon_log) {
                dungeon_log->info(
                    "Dungeon packet ({}, {} bytes):{}",
                    magic_enum::enum_name(msg_type),
                    body.size(),
                    spdlog::to_hex(body.begin(), body.end())
                );
            }

            pkt_log->debug(
                "Dungeon packet logged to {} ({} bytes)",
                logger_name,
                body.size()
            );
            return std::nullopt;
        }
        case NET_MESSAGE_GAME_PACKET: {
            GameUpdatePacket game_pkt{};
            stream.read(game_pkt);

            auto game_pkt_log{ pkt_log };
            if (game_pkt.type == PACKET_SEND_MAP_DATA) {
                if (auto map_data_log{ spdlog::get("map_data") }) {
                    game_pkt_log = map_data_log;
                }
            }

            if (game_pkt.type != PACKET_SEND_MAP_DATA) {
                game_pkt_log->debug(
                    "Decoding packet data ({} bytes):{}",
                    data.size(),
                    spdlog::to_hex(data.begin(), data.end())
                );
            }

            const std::size_t extra_size{
                game_pkt.data_size > 0
                    ? static_cast<std::size_t>(game_pkt.data_size)
                    : stream.get_size() - stream.get_read_offset()
            };

            std::vector<std::byte> extra{};
            if (extra_size > 0) {
                if (stream.get_size() - stream.get_read_offset() < extra_size) {
                    game_pkt_log->warn(
                        "Game packet extra size {} exceeds remaining stream {}",
                        extra_size,
                        stream.get_size() - stream.get_read_offset()
                    );
                    return std::nullopt;
                }

                extra.resize(extra_size);
                if (!stream.read_data(extra.data(), extra_size)) {
                    game_pkt_log->warn("Failed to read {} bytes of game packet extra data", extra_size);
                    return std::nullopt;
                }
            }

            if (game_pkt.type == PACKET_SEND_MAP_DATA) {
                game_pkt_log->info(
                    "Map data packet ({} bytes, extra {} bytes):{}",
                    data.size(),
                    extra.size(),
                    spdlog::to_hex(data.begin(), data.end())
                );
                pkt_log->debug("PACKET_SEND_MAP_DATA logged to map_data ({} bytes)", data.size());
            }

            if (game_pkt.type == PACKET_CALL_FUNCTION) {
                PacketVariant variant{};
                if (!variant.deserialize(extra)) {
                    game_pkt_log->warn("Failed to deserialize variant data");
                    return std::nullopt;
                }

                game_pkt_log->info("Packet decoded to variant:\n{}", fmt::format("{}", variant));

                VariantPayload var_payload{ game_pkt, std::move(variant) };
                Payload payload = var_payload;
                
                auto packet = PacketRegistry::instance().create(payload);
                if (!packet) {
                    game_pkt_log->debug("No packet structure registered for variant: {}", var_payload.function_name());
                    return std::nullopt;
                }
                
                return packet;
            }

            GamePayload game_payload{ game_pkt, std::move(extra) };
            Payload payload = game_payload;
            
            auto packet = PacketRegistry::instance().create(payload);
            if (!packet) {
                game_pkt_log->debug("No packet structure registered for game packet type: {} ({})",
                    magic_enum::enum_name(game_pkt.type),
                    static_cast<uint16_t>(game_pkt.type));
                return std::nullopt;
            }

            return packet;
        }
        default:
            pkt_log->warn("Unknown message type: {}", static_cast<uint32_t>(msg_type));
            return std::nullopt;
        }
    }
};
}
