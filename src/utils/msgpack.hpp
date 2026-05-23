#pragma once
#include <cstdint>
#include <string>
#include <variant>
#include <vector>
#include <stdexcept>
#include <span>

namespace utils {

// Simplified MessagePack value types used in Growtopia
struct MsgPackValue;
using MsgPackArray = std::vector<MsgPackValue>;

struct MsgPackValue {
    using Value = std::variant<
        std::nullptr_t,
        bool,
        int64_t,
        uint64_t,
        float,
        double,
        std::string,
        MsgPackArray
    >;

    Value data;

    MsgPackValue() : data{ nullptr } {}
    explicit MsgPackValue(Value v) : data{ std::move(v) } {}

    [[nodiscard]] bool is_null() const { return std::holds_alternative<std::nullptr_t>(data); }
    [[nodiscard]] bool is_bool() const { return std::holds_alternative<bool>(data); }
    [[nodiscard]] bool is_int() const { return std::holds_alternative<int64_t>(data); }
    [[nodiscard]] bool is_uint() const { return std::holds_alternative<uint64_t>(data); }
    [[nodiscard]] bool is_float() const { return std::holds_alternative<float>(data); }
    [[nodiscard]] bool is_double() const { return std::holds_alternative<double>(data); }
    [[nodiscard]] bool is_string() const { return std::holds_alternative<std::string>(data); }
    [[nodiscard]] bool is_array() const { return std::holds_alternative<MsgPackArray>(data); }

    [[nodiscard]] bool as_bool() const { return std::get<bool>(data); }
    [[nodiscard]] int64_t as_int() const { return std::get<int64_t>(data); }
    [[nodiscard]] uint64_t as_uint() const { return std::get<uint64_t>(data); }
    [[nodiscard]] float as_float() const { return std::get<float>(data); }
    [[nodiscard]] double as_double() const { return std::get<double>(data); }
    [[nodiscard]] const std::string& as_string() const { return std::get<std::string>(data); }
    [[nodiscard]] const MsgPackArray& as_array() const { return std::get<MsgPackArray>(data); }

    [[nodiscard]] int64_t as_integer() const {
        if (is_int()) return as_int();
        if (is_uint()) return static_cast<int64_t>(as_uint());
        return 0;
    }
};

class MsgPackDecoder {
public:
    explicit MsgPackDecoder(std::span<const uint8_t> data)
        : data_{ data }
        , offset_{ 0 }
    { }

    explicit MsgPackDecoder(const std::string& str)
        : data_{ reinterpret_cast<const uint8_t*>(str.data()), str.size() }
        , offset_{ 0 }
    { }

    [[nodiscard]] MsgPackValue decode()
    {
        if (offset_ >= data_.size()) {
            return MsgPackValue{};
        }

        const uint8_t byte{ data_[offset_++] };

        // Positive fixint (0x00 - 0x7f)
        if (byte <= 0x7f) {
            return MsgPackValue{ static_cast<uint64_t>(byte) };
        }

        // Negative fixint (0xe0 - 0xff)
        if (byte >= 0xe0) {
            return MsgPackValue{ static_cast<int64_t>(static_cast<int8_t>(byte)) };
        }

        // Fixmap (0x80 - 0x8f) — skip as map, read as array of pairs
        if ((byte & 0xf0) == 0x80) {
            const uint32_t size{ static_cast<uint32_t>(byte & 0x0f) };
            MsgPackArray arr{};
            arr.reserve(size * 2);
            for (uint32_t i{ 0 }; i < size; ++i) {
                arr.push_back(decode()); // key
                arr.push_back(decode()); // value
            }
            return MsgPackValue{ std::move(arr) };
        }

        // Fixarray (0x90 - 0x9f)
        if ((byte & 0xf0) == 0x90) {
            const uint32_t size{ static_cast<uint32_t>(byte & 0x0f) };
            return decode_array(size);
        }

        // Fixstr (0xa0 - 0xbf)
        if ((byte & 0xe0) == 0xa0) {
            const uint32_t len{ static_cast<uint32_t>(byte & 0x1f) };
            return decode_string(len);
        }

        switch (byte) {
        case 0xc0: return MsgPackValue{ nullptr };
        case 0xc2: return MsgPackValue{ false };
        case 0xc3: return MsgPackValue{ true };

        // bin8
        case 0xc4: {
            const uint32_t len{ read_uint8() };
            return decode_string(len);
        }
        // bin16
        case 0xc5: {
            const uint32_t len{ read_uint16() };
            return decode_string(len);
        }
        // bin32
        case 0xc6: {
            const uint32_t len{ read_uint32() };
            return decode_string(len);
        }

        // float32
        case 0xca: {
            if (offset_ + 4 > data_.size()) return MsgPackValue{};
            uint32_t raw{ 0 };
            raw = (static_cast<uint32_t>(data_[offset_]) << 24)
                | (static_cast<uint32_t>(data_[offset_ + 1]) << 16)
                | (static_cast<uint32_t>(data_[offset_ + 2]) << 8)
                | static_cast<uint32_t>(data_[offset_ + 3]);
            offset_ += 4;
            float val{};
            std::memcpy(&val, &raw, sizeof(float));
            return MsgPackValue{ val };
        }
        // float64
        case 0xcb: {
            if (offset_ + 8 > data_.size()) return MsgPackValue{};
            uint64_t raw{ 0 };
            for (int i = 0; i < 8; ++i) {
                raw = (raw << 8) | data_[offset_ + i];
            }
            offset_ += 8;
            double val{};
            std::memcpy(&val, &raw, sizeof(double));
            return MsgPackValue{ val };
        }

        // uint8
        case 0xcc: return MsgPackValue{ static_cast<uint64_t>(read_uint8()) };
        // uint16
        case 0xcd: return MsgPackValue{ static_cast<uint64_t>(read_uint16()) };
        // uint32
        case 0xce: return MsgPackValue{ static_cast<uint64_t>(read_uint32()) };
        // uint64
        case 0xcf: {
            if (offset_ + 8 > data_.size()) return MsgPackValue{};
            uint64_t val{ 0 };
            for (int i = 0; i < 8; ++i) {
                val = (val << 8) | data_[offset_ + i];
            }
            offset_ += 8;
            return MsgPackValue{ val };
        }

        // int8
        case 0xd0: return MsgPackValue{ static_cast<int64_t>(static_cast<int8_t>(read_uint8())) };
        // int16
        case 0xd1: return MsgPackValue{ static_cast<int64_t>(static_cast<int16_t>(read_uint16())) };
        // int32
        case 0xd2: return MsgPackValue{ static_cast<int64_t>(static_cast<int32_t>(read_uint32())) };
        // int64
        case 0xd3: {
            if (offset_ + 8 > data_.size()) return MsgPackValue{};
            uint64_t raw{ 0 };
            for (int i = 0; i < 8; ++i) {
                raw = (raw << 8) | data_[offset_ + i];
            }
            offset_ += 8;
            return MsgPackValue{ static_cast<int64_t>(raw) };
        }

        // str8
        case 0xd9: {
            const uint32_t len{ read_uint8() };
            return decode_string(len);
        }
        // str16
        case 0xda: {
            const uint32_t len{ read_uint16() };
            return decode_string(len);
        }
        // str32
        case 0xdb: {
            const uint32_t len{ read_uint32() };
            return decode_string(len);
        }

        // array16
        case 0xdc: {
            const uint32_t size{ read_uint16() };
            return decode_array(size);
        }
        // array32
        case 0xdd: {
            const uint32_t size{ read_uint32() };
            return decode_array(size);
        }

        // map16
        case 0xde: {
            const uint32_t size{ read_uint16() };
            MsgPackArray arr{};
            arr.reserve(size * 2);
            for (uint32_t i{ 0 }; i < size; ++i) {
                arr.push_back(decode());
                arr.push_back(decode());
            }
            return MsgPackValue{ std::move(arr) };
        }
        // map32
        case 0xdf: {
            const uint32_t size{ read_uint32() };
            MsgPackArray arr{};
            arr.reserve(size * 2);
            for (uint32_t i{ 0 }; i < size; ++i) {
                arr.push_back(decode());
                arr.push_back(decode());
            }
            return MsgPackValue{ std::move(arr) };
        }

        default:
            return MsgPackValue{};
        }
    }

    [[nodiscard]] bool has_remaining() const { return offset_ < data_.size(); }
    [[nodiscard]] size_t offset() const { return offset_; }

private:
    [[nodiscard]] uint8_t read_uint8()
    {
        if (offset_ >= data_.size()) return 0;
        return data_[offset_++];
    }

    [[nodiscard]] uint16_t read_uint16()
    {
        if (offset_ + 2 > data_.size()) return 0;
        uint16_t val{ static_cast<uint16_t>(
            (static_cast<uint16_t>(data_[offset_]) << 8) | data_[offset_ + 1]
        ) };
        offset_ += 2;
        return val;
    }

    [[nodiscard]] uint32_t read_uint32()
    {
        if (offset_ + 4 > data_.size()) return 0;
        uint32_t val{
            (static_cast<uint32_t>(data_[offset_]) << 24)
            | (static_cast<uint32_t>(data_[offset_ + 1]) << 16)
            | (static_cast<uint32_t>(data_[offset_ + 2]) << 8)
            | static_cast<uint32_t>(data_[offset_ + 3])
        };
        offset_ += 4;
        return val;
    }

    MsgPackValue decode_string(uint32_t len)
    {
        if (offset_ + len > data_.size()) {
            len = static_cast<uint32_t>(data_.size() - offset_);
        }
        std::string str{ reinterpret_cast<const char*>(data_.data() + offset_), len };
        offset_ += len;
        return MsgPackValue{ std::move(str) };
    }

    MsgPackValue decode_array(uint32_t size)
    {
        MsgPackArray arr{};
        arr.reserve(size);
        for (uint32_t i{ 0 }; i < size; ++i) {
            arr.push_back(decode());
        }
        return MsgPackValue{ std::move(arr) };
    }

    std::span<const uint8_t> data_;
    size_t offset_;
};

} // namespace utils
