#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace world {

enum class TileExtraType : uint8_t {
    None = 0,
    Door = 1,
    Sign = 2,
    Lock = 3,
    Seed = 4,
    Mailbox = 5,
    Bulletin = 6,
    Dice = 7,
    ChemicalSource = 8,
    AchievementBlock = 9,
    HeartMonitor = 10,
    DonationBox = 11,
    Mannequin = 14,
    BunnyEgg = 15,
    GamePack = 16,
    GameGenerator = 17,
    Xenonite = 18,
    PhoneBooth = 19,
    Crystal = 20,
    CrimeInProgress = 21,
    DisplayBlock = 23,
    VendingMachine = 24,
    FishTankPort = 25,
    SolarCollector = 26,
    Forge = 27,
    GivingTree = 28,
    GivingTreeStump = 29,
    SteamOrgan = 30,
    SilkWorm = 31,
    SewingMachine = 32,
    CountryFlag = 33,
    LobsterTrap = 34,
    PaintingEasel = 35,
    PetBattleCage = 36,
    PetTrainer = 37,
    SteamEngine = 38,
    LockBot = 39,
    WeatherMachine = 40,
    SpiritStorage = 41,
    DataBedrock = 42,
    Shelf = 43,
    VipEntrance = 44,
    ChallengeTimer = 45,
    FishWallMount = 46,
    Portrait = 47,
    WeatherSpecial = 48,
    FossilPrep = 49,
    DnaMachine = 50,
    Trickster = 51,
    Chemtank = 52,
    StorageBlock = 53,
    CookingOven = 54,
    AudioRack = 55,
    GeigerCharger = 56,
    AdventureBegins = 57,
    TombRobber = 58,
    BalloonOMatic = 59,
    TrainingPort = 60,
    ItemSucker = 61,
    CyBot = 62,
    GuildItem = 63,
    Growscan = 64,
    ContainmentFieldPowerNode = 65,
    SpiritBoard = 66,
    StormyCloud = 67,
    TemporaryPlatform = 68,
    SafeVault = 69,
    AngelicCountingCloud = 70,
    InfinityWeatherMachine = 71,
    PressurePlate = 72,
    PeacefulPeacock = 73,
    GhostJar = 74,
    AutoBreak = 75,
    AutoHarvest = 76,
    AutoPlant = 77,
    Sucker2 = 78,
    Magplant = 79,
    CrystalBlock = 80,
    Automation = 81,
};

struct TileExtraDoor {
    std::string label;
    uint8_t unknown{ 0 };
};

struct TileExtraSign {
    std::string text;
    int32_t unknown{ 0 };
};

struct TileExtraLock {
    uint8_t settings{ 0 };
    uint32_t owner_uid{ 0 };
    std::vector<uint32_t> admin_uids;
    int32_t tempo_type{ 0 }; // for music blocks
};

struct TileExtraSeed {
    uint32_t time_passed{ 0 };
    uint8_t fruit_count{ 0 };
};

struct TileExtraDice {
    uint8_t symbol{ 0 };
};

struct TileExtraProvider {
    int32_t time_passed{ 0 };
};

struct TileExtraHeartMonitor {
    std::string player_name;
    uint32_t player_uid{ 0 };
};

struct TileExtraMannequin {
    std::string label;
    uint8_t unknown_1{ 0 };
    uint8_t unknown_2{ 0 };
    uint8_t unknown_3{ 0 };
    uint8_t unknown_4{ 0 };
    uint8_t unknown_5{ 0 };
    uint16_t hair{ 0 };
    uint16_t shirt{ 0 };
    uint16_t pants{ 0 };
    uint16_t feet{ 0 };
    uint16_t hat{ 0 };
    uint16_t hand{ 0 };
    uint16_t back{ 0 };
    uint16_t face{ 0 };
    uint16_t neck{ 0 };
};

struct TileExtraVending {
    uint16_t item_id{ 0 };
    int32_t price{ 0 };
};

struct TileExtraDisplayBlock {
    uint32_t item_id{ 0 };
};

struct TileExtraGameGenerator {
    // Simplified - just store raw bytes for now
    std::vector<std::byte> raw_data;
};

struct TileExtraWeatherMachine {
    uint32_t weather_id{ 0 };
};

struct TileExtraShelf {
    uint32_t top_left{ 0 };
    uint32_t top_right{ 0 };
    uint32_t bottom_left{ 0 };
    uint32_t bottom_right{ 0 };
};

struct TileExtraPortrait {
    std::string label;
    uint32_t unknown_1{ 0 };
    uint32_t unknown_2{ 0 };
    uint32_t unknown_3{ 0 };
    uint32_t unknown_4{ 0 };
    uint32_t face{ 0 };
    uint32_t hat{ 0 };
    uint32_t hair{ 0 };
};

struct TileExtraCountryFlag {
    std::string country;
};

struct Tile {
    uint16_t foreground{ 0 };
    uint16_t background{ 0 };
    uint16_t parent_block_index{ 0 }; // lock reference
    uint16_t flags{ 0 };
    uint32_t flags_full{ 0 };

    TileExtraType extra_type{ TileExtraType::None };

    // Tile extra data (only one is active based on extra_type)
    std::optional<TileExtraDoor> door;
    std::optional<TileExtraSign> sign;
    std::optional<TileExtraLock> lock;
    std::optional<TileExtraSeed> seed;
    std::optional<TileExtraDice> dice;
    std::optional<TileExtraProvider> provider;
    std::optional<TileExtraHeartMonitor> heart_monitor;
    std::optional<TileExtraMannequin> mannequin;
    std::optional<TileExtraVending> vending;
    std::optional<TileExtraDisplayBlock> display_block;
    std::optional<TileExtraWeatherMachine> weather_machine;
    std::optional<TileExtraShelf> shelf;
    std::optional<TileExtraPortrait> portrait;
    std::optional<TileExtraCountryFlag> country_flag;
};

struct DroppedItem {
    uint16_t item_id{ 0 };
    float x{ 0.0f };
    float y{ 0.0f };
    uint8_t count{ 0 };
    uint8_t flags{ 0 };
    uint32_t uid{ 0 };
};

struct WorldData {
    uint16_t version{ 0 };
    std::string name;
    uint32_t width{ 0 };
    uint32_t height{ 0 };
    uint32_t tile_count{ 0 };
    std::vector<Tile> tiles;

    // Dropped items
    uint32_t dropped_item_count{ 0 };
    uint32_t last_dropped_item_uid{ 0 };
    std::vector<DroppedItem> dropped_items;

    // Weather
    uint16_t weather_id{ 0 };

    [[nodiscard]] bool is_valid() const
    {
        return !name.empty() && width > 0 && height > 0 && tile_count > 0;
    }
};

} // namespace world
