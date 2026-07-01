#include <gtest/gtest.h>
#include "utils/byte_stream.hpp"

TEST(ByteStreamTest, WriteAndReadBasicTypes)
{
    ByteStream bs{};
    bs.write<int>(42);
    bs.write<float>(3.14f);

    int i{};
    float f{};
    
    bs.read(i);
    bs.read(f);

    EXPECT_EQ(i, 42);
    EXPECT_EQ(f, 3.14f);
}

TEST(ByteStreamTest, WriteAndReadString)
{
    ByteStream bs{};
    const std::string original{ "hello world" };
    bs.write(original);

    std::string result;
    bs.read(result);

    EXPECT_EQ(result, original);
}

TEST(ByteStreamTest, WriteAndReadVector)
{
    ByteStream bs{};
    const std::vector data{ std::byte{ 0x01 }, std::byte{ 0x02 }, std::byte{ 0x03 } };
    bs.write_vector(data);

    std::vector<std::byte> result;
    bs.read_vector(result);

    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], std::byte{ 0x01 });
    EXPECT_EQ(result[2], std::byte{ 0x03 });
}

TEST(ByteStreamTest, ReadVectorWithExplicitLength)
{
    ByteStream bs{};
    const std::vector data{ std::byte{ 0xA }, std::byte{ 0xB } };
    // Write without length prefix
    bs.write_data(data.data(), data.size());

    std::vector<std::byte> result;
    // Read with explicit length
    const bool success{ bs.read_vector(result, 2) };

    EXPECT_TRUE(success);
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], std::byte{ 0xA });
}

TEST(ByteStreamTest, StreamOperators)
{
    ByteStream bs{};
    bs << 100 << std::string("test");

    int val{};
    std::string str{};

    bs >> val >> str;

    EXPECT_EQ(val, 100);
    EXPECT_EQ(str, "test");
}

TEST(ByteStreamTest, SpanConstructor)
{
    std::vector raw{
        std::byte{ 0x05 },
        std::byte{ 0x00 },
        std::byte{ 0x01 },
        std::byte{ 0x02 },
        std::byte{ 0x03 },
        std::byte{ 0x04 },
        std::byte{ 0x05 }
    };
    // 0x05, 0x00 is length (5) in little endian uint16_t, followed by 5 bytes

    ByteStream bs{ raw };

    std::vector<std::byte> result{};
    // Default ReadVector reads length first (which is 5 at the beginning) and then payload
    bs.read_vector(result);

    EXPECT_EQ(result.size(), 5);
}

#include "packet/packet_types.hpp"

TEST(ByteStreamTest, GameUpdatePacketLayout)
{
    EXPECT_EQ(sizeof(packet::GameUpdatePacket), 56);

    packet::GameUpdatePacket packet{};
    packet.type = packet::PACKET_STATE;
    packet.pad1 = 1;
    packet.pad2 = 2;
    packet.pad3 = 3;
    packet.net_id = 42;
    packet.secondary_id = 100;
    packet.flags.value = static_cast<packet::PacketFlag>(packet::PACKET_FLAG_ON_SOLID | packet::PACKET_FLAG_EXTENDED);
    packet.float1 = 1.5f;
    packet.int_data = 500;
    packet.pos_x = 10.0f;
    packet.pos_y = 20.0f;
    packet.speed_x = 0.5f;
    packet.speed_y = -0.5f;
    packet.float2 = 2.5f;
    packet.tile_x = 5;
    packet.tile_y = 6;
    packet.data_size = 128;

    ByteStream bs{};
    bs.write(packet);

    EXPECT_EQ(bs.get_size(), 56);

    packet::GameUpdatePacket decoded{};
    bs.read(decoded);

    EXPECT_EQ(decoded.type, packet::PACKET_STATE);
    EXPECT_EQ(decoded.pad1, 1);
    EXPECT_EQ(decoded.pad2, 2);
    EXPECT_EQ(decoded.pad3, 3);
    EXPECT_EQ(decoded.net_id, 42);
    EXPECT_EQ(decoded.secondary_id, 100);
    EXPECT_TRUE(decoded.flags.on_solid);
    EXPECT_TRUE(decoded.flags.extended);
    EXPECT_EQ(decoded.float1, 1.5f);
    EXPECT_EQ(decoded.int_data, 500);
    EXPECT_EQ(decoded.pos_x, 10.0f);
    EXPECT_EQ(decoded.pos_y, 20.0f);
    EXPECT_EQ(decoded.speed_x, 0.5f);
    EXPECT_EQ(decoded.speed_y, -0.5f);
    EXPECT_EQ(decoded.float2, 2.5f);
    EXPECT_EQ(decoded.tile_x, 5);
    EXPECT_EQ(decoded.tile_y, 6);
    EXPECT_EQ(decoded.data_size, 128);
}
