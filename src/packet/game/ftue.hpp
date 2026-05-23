#pragma once
#include <string>
#include <vector>
#include <spdlog/spdlog.h>
#include <fmt/format.h>

#include "../packet_helper.hpp"
#include "../../utils/msgpack.hpp"

namespace packet::game {

struct FtueTutorialEntry {
    uint64_t id{ 0 };
    std::string name;
};

struct FtueActiveTutorial {
    uint64_t id{ 0 };
    uint64_t status{ 0 };
    uint64_t progress{ 0 };
    uint64_t total_steps{ 0 };
    uint64_t target_count{ 0 };
    std::string name;
    std::string description;
    uint64_t priority{ 0 };
    std::string steps_json;
};

struct OnFtueButtonDataSet : VariantPacket<PacketId::OnFtueButtonDataSet> {
    GameUpdatePacket game_packet;
    PacketVariant original_variant;

    bool tutorials_enabled{ false };
    bool has_active{ false };
    bool all_completed{ false };
    int64_t current_selection{ -1 };

    std::vector<FtueActiveTutorial> active_tutorials;
    std::vector<FtueTutorialEntry> available_tutorials;
    std::string prompt_text;

    bool read(const Payload& payload) override
    {
        const auto* var{ get_payload_if<VariantPayload>(payload) };
        if (!var) {
            return false;
        }

        const auto& variant{ var->variant };
        if (variant.size() < 2) {
            return false;
        }

        game_packet = var->game_packet;
        original_variant = variant;

        const std::string raw_data{ variant.get<std::string>(1) };
        if (raw_data.empty()) {
            return false;
        }

        utils::MsgPackDecoder decoder{ raw_data };

        // Root is a fixarray
        const auto root{ decoder.decode() };
        if (!root.is_array()) {
            spdlog::warn("OnFtueButtonDataSet: root is not an array");
            return false;
        }

        const auto& root_arr{ root.as_array() };
        if (root_arr.size() < 4) {
            spdlog::warn("OnFtueButtonDataSet: root array too small ({})", root_arr.size());
            return false;
        }

        // Parse header flags
        size_t idx{ 0 };
        if (root_arr[idx].is_bool()) tutorials_enabled = root_arr[idx].as_bool();
        idx++;
        if (root_arr[idx].is_bool()) has_active = root_arr[idx].as_bool();
        idx++;
        if (root_arr[idx].is_bool()) all_completed = root_arr[idx].as_bool();
        idx++;
        current_selection = root_arr[idx].as_integer();
        idx++;

        // Parse active tutorials array
        if (idx < root_arr.size() && root_arr[idx].is_array()) {
            const auto& active_arr{ root_arr[idx].as_array() };
            for (const auto& entry : active_arr) {
                if (!entry.is_array()) continue;
                const auto& fields{ entry.as_array() };

                FtueActiveTutorial tutorial{};
                size_t fi{ 0 };
                if (fi < fields.size()) tutorial.id = static_cast<uint64_t>(fields[fi].as_integer()); fi++;
                if (fi < fields.size()) tutorial.status = static_cast<uint64_t>(fields[fi].as_integer()); fi++;
                if (fi < fields.size()) tutorial.progress = static_cast<uint64_t>(fields[fi].as_integer()); fi++;
                if (fi < fields.size()) tutorial.total_steps = static_cast<uint64_t>(fields[fi].as_integer()); fi++;
                if (fi < fields.size()) tutorial.target_count = static_cast<uint64_t>(fields[fi].as_integer()); fi++;
                if (fi < fields.size() && fields[fi].is_string()) tutorial.name = fields[fi].as_string(); fi++;
                if (fi < fields.size() && fields[fi].is_string()) tutorial.description = fields[fi].as_string(); fi++;
                if (fi < fields.size()) tutorial.priority = static_cast<uint64_t>(fields[fi].as_integer()); fi++;
                if (fi < fields.size() && fields[fi].is_string()) tutorial.steps_json = fields[fi].as_string();

                active_tutorials.push_back(std::move(tutorial));
            }
        }
        idx++;

        // Parse available tutorials array
        if (idx < root_arr.size() && root_arr[idx].is_array()) {
            const auto& avail_arr{ root_arr[idx].as_array() };
            for (const auto& entry : avail_arr) {
                if (!entry.is_array()) continue;
                const auto& fields{ entry.as_array() };

                FtueTutorialEntry tutorial{};
                if (fields.size() >= 2) {
                    tutorial.id = static_cast<uint64_t>(fields[0].as_integer());
                    if (fields[1].is_string()) tutorial.name = fields[1].as_string();
                }

                available_tutorials.push_back(std::move(tutorial));
            }
        }
        idx++;

        // Parse prompt text
        if (idx < root_arr.size() && root_arr[idx].is_string()) {
            prompt_text = root_arr[idx].as_string();
        }

        // Log decoded data
        auto pkt_log{ spdlog::get("packet") };
        pkt_log->info("OnFtueButtonDataSet decoded:");
        pkt_log->info("  Enabled: {}, HasActive: {}, AllCompleted: {}, Selection: {}",
            tutorials_enabled, has_active, all_completed, current_selection);

        for (const auto& t : active_tutorials) {
            pkt_log->info("  Active: [{}] \"{}\" - {} (progress: {}/{}, target: {})",
                t.id, t.name, t.description, t.progress, t.total_steps, t.target_count);
        }

        for (const auto& t : available_tutorials) {
            pkt_log->info("  Available: [{}] \"{}\"", t.id, t.name);
        }

        if (!prompt_text.empty()) {
            pkt_log->info("  Prompt: \"{}\"", prompt_text);
        }

        return true;
    }

    Payload write() override
    {
        return VariantPayload{ game_packet, original_variant };
    }
};

} // namespace packet::game
