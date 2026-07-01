#pragma once
#include <spdlog/spdlog.h>

#include "../packet_helper.hpp"
#include "../../world/world_data.hpp"
#include "../../utils/byte_stream.hpp"

namespace packet::game {

struct SendMapData : GamePacket<PacketId::SendMapData, PACKET_SEND_MAP_DATA> {
    world::WorldData world_data;
    GameUpdatePacket original_packet{};
    std::vector<std::byte> original_extra;

    bool read(const Payload& payload) override
    {
        const auto* game = get_payload_if<GamePayload>(payload);
        if (!game) return false;

        // Store original data for forwarding
        original_packet = game->packet;
        original_extra = game->extra;

        ByteStream<uint16_t> stream{ game->extra };
        if (game->extra.empty()) {
            spdlog::warn("SendMapData: empty extra data");
            return false;
        }

        spdlog::debug("SendMapData: extra data size = {} bytes", game->extra.size());

        // The extra data format (world map data after GameUpdatePacket):
        // Based on Growtopia protocol analysis:
        // - uint8: world_version (or uint16 in newer)
        // - ... variable header depending on version

        // Let's try to detect the format by reading first few bytes
        // Version field
        if (!stream.read(world_data.version)) return false;
        spdlog::debug("SendMapData: version = {}", world_data.version);

        uint32_t unknown_flags{ 0 };
        if (!stream.read(unknown_flags)) return false;
        spdlog::debug("SendMapData: unknown_flags = {:#010x}", unknown_flags);

        // World name - uint16 length-prefixed
        std::string name{};
        if (!stream.read(name)) return false;
        world_data.name = name;
        spdlog::debug("SendMapData: name = '{}' (len={})", world_data.name, world_data.name.size());

        // World dimensions
        if (!stream.read(world_data.width)) return false;
        if (!stream.read(world_data.height)) return false;
        spdlog::debug("SendMapData: dimensions = {}x{}", world_data.width, world_data.height);

        // Tile count
        uint16_t tile_count_raw{ 0 };
        if (!stream.read(tile_count_raw)) return false;
        world_data.tile_count = static_cast<uint32_t>(tile_count_raw);
        spdlog::debug("SendMapData: tile_count_raw = {}", tile_count_raw);

        // Validate dimensions
        if (world_data.width == 0 || world_data.width > 255 ||
            world_data.height == 0 || world_data.height > 255) {
            spdlog::warn("SendMapData: invalid dimensions {}x{}, trying alternate header format",
                world_data.width, world_data.height);
            return false;
        }

        if (world_data.tile_count != world_data.width * world_data.height) {
            spdlog::warn("SendMapData: tile_count {} != width*height {}*{}={}",
                world_data.tile_count, world_data.width, world_data.height,
                world_data.width * world_data.height);
            world_data.tile_count = world_data.width * world_data.height;
        }

        spdlog::info("Parsing world '{}' ({}x{}, version {}, tiles {}, offset={})",
            world_data.name, world_data.width, world_data.height,
            world_data.version, world_data.tile_count, stream.get_read_offset());

        // Parse tiles
        world_data.tiles.resize(world_data.tile_count);
        bool tiles_truncated{ false };
        for (uint32_t i = 0; i < world_data.tile_count; ++i) {
            if (!parse_tile(stream, world_data.tiles[i])) {
                spdlog::warn("SendMapData: failed to parse tile at index {} (offset {}), keeping {} tiles",
                    i, stream.get_read_offset(), i);
                world_data.tile_count = i;
                world_data.tiles.resize(i);
                tiles_truncated = true;
                break;
            }
        }

        if (!tiles_truncated) {
            // After tiles: 12 bytes padding/reserved
            stream.skip(12);

            // Parse dropped items
            if (!parse_dropped_items(stream)) {
                spdlog::warn("SendMapData: failed to parse dropped items");
            }

            // Weather footer
            uint16_t weather_pad1{ 0 };
            stream.read(weather_pad1);
            uint16_t weather_pad2{ 0 };
            stream.read(weather_pad2);
            stream.read(world_data.weather_id);
        }

        spdlog::info("World '{}' parsed successfully: {} tiles, {} drops",
            world_data.name, world_data.tile_count, world_data.dropped_items.size());

        return true;
    }

    Payload write() override
    {
        // Return original data for forwarding to client
        GamePayload game_payload{ original_packet, original_extra };
        return game_payload;
    }

private:
    bool parse_tile(ByteStream<uint16_t>& stream, world::Tile& tile)
    {
        // Base tile: fg(uint16) + bg(uint16) + flags(uint32) = 8 bytes
        if (!stream.read(tile.foreground)) return false;
        if (!stream.read(tile.background)) return false;

        uint32_t flags32{ 0 };
        if (!stream.read(flags32)) return false;
        tile.flags = static_cast<uint16_t>(flags32 & 0xFFFF);
        tile.flags_full = flags32;

        // If foreground is non-zero and has extra data (type byte follows)
        // The extra type byte is present when the tile has extra data.
        // We detect this by checking if bit 0x0001 of flags is set (has_extra_data flag)
        // OR by checking specific known patterns.
        // Actually from Firefly source: extra data is written based on blockType from item DB.
        // Since we don't have item DB, we check the flag bit.
        if (flags32 & 0x0001) {
            uint8_t extra_type_raw{ 0 };
            if (!stream.read(extra_type_raw)) return false;
            tile.extra_type = static_cast<world::TileExtraType>(extra_type_raw);

            if (!parse_tile_extra(stream, tile)) {
                return false;
            }
        }

        return true;
    }

    bool parse_tile_extra(ByteStream<uint16_t>& stream, world::Tile& tile)
    {
        switch (tile.extra_type) {
        case world::TileExtraType::None:
            break;

        case world::TileExtraType::Door: {
            // type=1: text(uint16+string) + open_flag(uint8)
            world::TileExtraDoor door{};
            if (!stream.read(door.label)) return false;
            if (!stream.read(door.unknown)) return false;
            tile.door = std::move(door);
            break;
        }
        case world::TileExtraType::Sign: {
            // type=2: text_len(uint16) + text + padding(int32)
            world::TileExtraSign sign{};
            if (!stream.read(sign.text)) return false;
            if (!stream.read(sign.unknown)) return false;
            tile.sign = std::move(sign);
            break;
        }
        case world::TileExtraType::Lock: {
            // type=3: settings(uint8) + owner_id(uint32) + admin_count(uint32) + bpm(int32) + admin_ids(uint32 each)
            world::TileExtraLock lock{};
            if (!stream.read(lock.settings)) return false;
            if (!stream.read(lock.owner_uid)) return false;

            uint32_t admin_count{ 0 };
            if (!stream.read(admin_count)) return false;

            if (admin_count > 512) {
                spdlog::warn("Lock admin count too large: {}", admin_count);
                return false;
            }

            // BPM (tempo) - always present in newer versions
            if (!stream.read(lock.tempo_type)) return false;

            lock.admin_uids.resize(admin_count);
            for (uint32_t i = 0; i < admin_count; ++i) {
                if (!stream.read(lock.admin_uids[i])) return false;
            }

            tile.lock = std::move(lock);
            break;
        }
        case world::TileExtraType::Seed: {
            // type=4: elapsed_time(uint32) + fruit_count(uint8)
            world::TileExtraSeed seed{};
            if (!stream.read(seed.time_passed)) return false;
            if (!stream.read(seed.fruit_count)) return false;
            tile.seed = std::move(seed);
            break;
        }
        case world::TileExtraType::Mailbox:
        case world::TileExtraType::Bulletin: {
            // type=5/6: text(uint16+string) + unknown(int32) -- or just 1 byte for weather type 5
            // Actually type 5 = Mailbox in our enum, type 6 = Bulletin
            // From Firefly: type 5 (weather default) = 0 extra bytes after type byte
            // But our enum maps differently. Let's handle based on actual Firefly type IDs.
            // Firefly type 5 = just the type byte, no extra data
            // Firefly type 6 = simple_load, 0 extra bytes
            // We need to be careful here - let me just skip 0 bytes for these
            break;
        }
        case world::TileExtraType::Dice: {
            // type=7: not in Firefly list, skip
            // Actually type 8 in Firefly = RANDOM_BLOCK with roll_value(int32)
            world::TileExtraDice dice{};
            if (!stream.read(dice.symbol)) return false;
            tile.dice = std::move(dice);
            break;
        }
        case world::TileExtraType::ChemicalSource: {
            // type=8: roll_value(int32) in Firefly
            uint32_t roll_value{ 0 };
            if (!stream.read(roll_value)) return false;
            break;
        }
        case world::TileExtraType::AchievementBlock: {
            // type=9: provider - elapsed_time(int32)
            world::TileExtraProvider provider{};
            if (!stream.read(provider.time_passed)) return false;
            tile.provider = std::move(provider);
            break;
        }
        case world::TileExtraType::HeartMonitor: {
            // type=10: not directly mapped. Skip.
            // Actually type 11 in Firefly = Heart Monitor
            // type 10 = unknown, skip 0
            break;
        }
        case world::TileExtraType::DonationBox: {
            // type=11: Heart Monitor in Firefly: online_flag(uint32) + name(uint16+string)
            world::TileExtraHeartMonitor hm{};
            if (!stream.read(hm.player_uid)) return false; // online_flag
            if (!stream.read(hm.player_name)) return false;
            tile.heart_monitor = std::move(hm);
            break;
        }
        case world::TileExtraType::Mannequin: {
            // type=14: text(uint16+string) + unknown(5 bytes) + items(9 x uint16)
            world::TileExtraMannequin mannequin{};
            if (!stream.read(mannequin.label)) return false;
            if (!stream.read(mannequin.unknown_1)) return false;
            if (!stream.read(mannequin.unknown_2)) return false;
            if (!stream.read(mannequin.unknown_3)) return false;
            if (!stream.read(mannequin.unknown_4)) return false;
            if (!stream.read(mannequin.unknown_5)) return false;
            if (!stream.read(mannequin.hair)) return false;
            if (!stream.read(mannequin.shirt)) return false;
            if (!stream.read(mannequin.pants)) return false;
            if (!stream.read(mannequin.feet)) return false;
            if (!stream.read(mannequin.hat)) return false;
            if (!stream.read(mannequin.hand)) return false;
            if (!stream.read(mannequin.back)) return false;
            if (!stream.read(mannequin.face)) return false;
            if (!stream.read(mannequin.neck)) return false;
            tile.mannequin = std::move(mannequin);
            break;
        }
        case world::TileExtraType::BunnyEgg: {
            // type=15: egg_count(uint16)
            uint16_t egg_count{ 0 };
            if (!stream.read(egg_count)) return false;
            break;
        }
        case world::TileExtraType::GamePack: {
            // type=16 (0x10): block_id(uint8)
            uint8_t block_id{ 0 };
            if (!stream.read(block_id)) return false;
            break;
        }
        case world::TileExtraType::GameGenerator: {
            // type=17 (0x11): no extra data
            break;
        }
        case world::TileExtraType::Xenonite: {
            // type=18: unknown
            break;
        }
        case world::TileExtraType::PhoneBooth: {
            // type=19: unknown
            break;
        }
        case world::TileExtraType::Crystal: {
            // type=20: 2 bytes (uint8 + uint8)
            stream.skip(2);
            break;
        }
        case world::TileExtraType::CrimeInProgress: {
            // type=21: unknown
            break;
        }
        case world::TileExtraType::DisplayBlock: {
            // type=23: item_id(uint32)
            world::TileExtraDisplayBlock display{};
            if (!stream.read(display.item_id)) return false;
            tile.display_block = std::move(display);
            break;
        }
        case world::TileExtraType::VendingMachine: {
            // type=24: item_id(int32) + price(int32)
            world::TileExtraVending vending{};
            uint32_t item_id_32{ 0 };
            if (!stream.read(item_id_32)) return false;
            vending.item_id = static_cast<uint16_t>(item_id_32);
            if (!stream.read(vending.price)) return false;
            tile.vending = std::move(vending);
            break;
        }
        case world::TileExtraType::FishTankPort: {
            // type=25: unknown, skip
            break;
        }
        case world::TileExtraType::SolarCollector: {
            // type=26: unknown, skip
            break;
        }
        case world::TileExtraType::Forge: {
            // type=27: unknown, skip
            break;
        }
        case world::TileExtraType::GivingTree: {
            // type=28 (0x1C): unknown(uint8) + timer(uint16) + count(uint8) + pad(uint16)
            stream.skip(6);
            break;
        }
        case world::TileExtraType::GivingTreeStump: {
            // type=29: similar to giving tree
            stream.skip(6);
            break;
        }
        case world::TileExtraType::SteamOrgan: {
            // type=30: unknown
            break;
        }
        case world::TileExtraType::SilkWorm: {
            // type=31: unknown
            break;
        }
        case world::TileExtraType::SewingMachine: {
            // type=32: unknown
            break;
        }
        case world::TileExtraType::CountryFlag: {
            // type=33 (0x21): text(uint16+string)
            world::TileExtraCountryFlag flag{};
            if (!stream.read(flag.country)) return false;
            tile.country_flag = std::move(flag);
            break;
        }
        case world::TileExtraType::LobsterTrap: {
            // type=34: unknown
            break;
        }
        case world::TileExtraType::PaintingEasel: {
            // type=35: item_id(uint16) + pad(uint16) + text(uint16+string)
            uint16_t item_id{ 0 };
            if (!stream.read(item_id)) return false;
            uint16_t pad{ 0 };
            if (!stream.read(pad)) return false;
            std::string text{};
            if (!stream.read(text)) return false;
            break;
        }
        case world::TileExtraType::PetBattleCage: {
            // type=36 (0x24): text(uint16+string) + cage_item(int32)
            std::string text{};
            if (!stream.read(text)) return false;
            uint32_t cage_item{ 0 };
            if (!stream.read(cage_item)) return false;
            break;
        }
        case world::TileExtraType::PetTrainer: {
            // type=37: unknown
            break;
        }
        case world::TileExtraType::SteamEngine: {
            // type=38: unknown
            break;
        }
        case world::TileExtraType::LockBot: {
            // type=39: unknown
            break;
        }
        case world::TileExtraType::WeatherMachine: {
            // type=40: weather_id(int32)
            world::TileExtraWeatherMachine weather{};
            if (!stream.read(weather.weather_id)) return false;
            tile.weather_machine = std::move(weather);
            break;
        }
        case world::TileExtraType::SpiritStorage: {
            // type=41: count(uint16)
            uint16_t count{ 0 };
            if (!stream.read(count)) return false;
            break;
        }
        case world::TileExtraType::DataBedrock: {
            // type=42: unknown
            break;
        }
        case world::TileExtraType::Shelf: {
            // type=43: 4 items, each uint32 (16 bytes)
            world::TileExtraShelf shelf{};
            if (!stream.read(shelf.top_left)) return false;
            if (!stream.read(shelf.top_right)) return false;
            if (!stream.read(shelf.bottom_left)) return false;
            if (!stream.read(shelf.bottom_right)) return false;
            tile.shelf = std::move(shelf);
            break;
        }
        case world::TileExtraType::VipEntrance: {
            // type=44: unknown(uint32) + member_count(uint16) + pad(uint16) + member_uids(uint32 each)
            uint32_t unknown{ 0 };
            if (!stream.read(unknown)) return false;
            uint16_t member_count{ 0 };
            if (!stream.read(member_count)) return false;
            uint16_t pad{ 0 };
            if (!stream.read(pad)) return false;
            if (member_count > 512) return false;
            for (uint16_t i = 0; i < member_count; ++i) {
                uint32_t uid{ 0 };
                if (!stream.read(uid)) return false;
            }
            break;
        }
        case world::TileExtraType::ChallengeTimer: {
            // type=45: no extra data
            break;
        }
        case world::TileExtraType::FishWallMount: {
            // type=46: unknown
            break;
        }
        case world::TileExtraType::Portrait: {
            // type=47: fish mount - pad(uint8)
            // Actually 47 in Firefly = Fish_Mount with 1 byte pad
            uint8_t pad{ 0 };
            if (!stream.read(pad)) return false;
            break;
        }
        case world::TileExtraType::WeatherSpecial: {
            // type=48: Portrait in Firefly: text(uint16+string) + expression + hair_colour + skin + face + head + hair (each uint32)
            world::TileExtraPortrait portrait{};
            if (!stream.read(portrait.label)) return false;
            if (!stream.read(portrait.unknown_1)) return false; // expression
            if (!stream.read(portrait.unknown_2)) return false; // hair_colour
            if (!stream.read(portrait.unknown_3)) return false; // skin
            if (!stream.read(portrait.unknown_4)) return false; // face
            if (!stream.read(portrait.face)) return false;      // head
            if (!stream.read(portrait.hat)) return false;       // hat/head
            if (!stream.read(portrait.hair)) return false;      // hair
            tile.portrait = std::move(portrait);
            break;
        }
        case world::TileExtraType::FossilPrep: {
            // type=49: Weather Special in Firefly: id(uint16) + pad(uint16) + gravity(uint16) + pad(uint16) + spin_invert(uint16)
            stream.skip(10);
            break;
        }
        case world::TileExtraType::DnaMachine: {
            // type=50: unknown
            break;
        }
        case world::TileExtraType::Trickster: {
            // type=52: no extra data
            break;
        }
        case world::TileExtraType::Chemtank: {
            // type=52 already handled above
            break;
        }
        case world::TileExtraType::StorageBlock: {
            // type=53: unknown
            break;
        }
        case world::TileExtraType::CookingOven: {
            // type=54: unknown
            break;
        }
        case world::TileExtraType::AudioRack: {
            // type=56: text(uint16+string) + price(uint16)
            std::string text{};
            if (!stream.read(text)) return false;
            uint32_t note{ 0 };
            if (!stream.read(note)) return false;
            break;
        }
        case world::TileExtraType::GeigerCharger: {
            // type=57: elapsed(int32)
            uint32_t elapsed{ 0 };
            if (!stream.read(elapsed)) return false;
            break;
        }
        case world::TileExtraType::AdventureBegins: {
            // type=58: unknown
            break;
        }
        case world::TileExtraType::TombRobber: {
            // type=59: unknown
            break;
        }
        case world::TileExtraType::BalloonOMatic: {
            // type=60: unknown
            break;
        }
        case world::TileExtraType::TrainingPort: {
            // type=61: unknown
            break;
        }
        case world::TileExtraType::CyBot: {
            // type=62: Sucker/Magplant: item_id(uint16) + pad(uint16) + count(uint16) + pad(uint16) + enabled(uint8) + spin(uint8) + max_cap(uint16)
            stream.skip(12);
            break;
        }
        case world::TileExtraType::GuildItem: {
            // type=63: Fish: 4 bytes
            stream.skip(4);
            break;
        }
        case world::TileExtraType::Growscan: {
            // type=64: unknown
            break;
        }
        case world::TileExtraType::ContainmentFieldPowerNode: {
            // type=65: unknown
            break;
        }
        case world::TileExtraType::SpiritBoard: {
            // type=66: unknown
            break;
        }
        case world::TileExtraType::StormyCloud: {
            // type=67: unknown
            break;
        }
        case world::TileExtraType::TemporaryPlatform: {
            // type=68: unknown
            break;
        }
        case world::TileExtraType::SafeVault: {
            // type=69: Auto block: has_item(uint8) + pad(3) + item_count(uint16) + pad(uint16) + item_id(uint16) + pad(uint16) + enabled(uint8)
            stream.skip(12);
            break;
        }
        case world::TileExtraType::AngelicCountingCloud: {
            // type=70: Auto block same as 69
            stream.skip(12);
            break;
        }
        case world::TileExtraType::InfinityWeatherMachine: {
            // type=71: unknown
            break;
        }
        case world::TileExtraType::PressurePlate: {
            // type=72: unknown
            break;
        }
        case world::TileExtraType::PeacefulPeacock: {
            // type=73: unknown
            break;
        }
        case world::TileExtraType::GhostJar: {
            // type=74: unknown
            break;
        }
        case world::TileExtraType::AutoBreak:
        case world::TileExtraType::AutoHarvest:
        case world::TileExtraType::AutoPlant: {
            // type=75/76/77: unknown
            break;
        }
        case world::TileExtraType::Sucker2: {
            // type=78: unknown
            break;
        }
        case world::TileExtraType::Magplant: {
            // type=79: unknown
            break;
        }
        case world::TileExtraType::CrystalBlock: {
            // type=80 (0x50): Kranken: pattern(int32) + rgb_color(int32)
            stream.skip(8);
            break;
        }
        case world::TileExtraType::Automation: {
            // type=81: unknown
            break;
        }
        default: {
            spdlog::debug("Unhandled tile extra type: {} at offset {}",
                static_cast<uint8_t>(tile.extra_type), stream.get_read_offset());
            break;
        }
        }

        return true;
    }

    bool parse_dropped_items(ByteStream<uint16_t>& stream)
    {
        if (!stream.read(world_data.dropped_item_count)) return false;
        if (!stream.read(world_data.last_dropped_item_uid)) return false;

        if (world_data.dropped_item_count > 10000) {
            spdlog::warn("Dropped item count too large: {}", world_data.dropped_item_count);
            return false;
        }

        world_data.dropped_items.resize(world_data.dropped_item_count);
        for (uint32_t i = 0; i < world_data.dropped_item_count; ++i) {
            auto& item = world_data.dropped_items[i];
            if (!stream.read(item.item_id)) return false;
            if (!stream.read(item.x)) return false;
            if (!stream.read(item.y)) return false;
            if (!stream.read(item.count)) return false;
            // 1 byte padding between count and uid
            uint8_t pad{ 0 };
            if (!stream.read(pad)) return false;
            if (!stream.read(item.uid)) return false;
        }

        return true;
    }
};

} // namespace packet::game
