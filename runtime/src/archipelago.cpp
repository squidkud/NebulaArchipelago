// SPDX-License-Identifier: GPL-3.0-only
#include "galaxy/archipelago.h"

#include "galaxy/atomic_file.h"
#include "galaxy/websocket_client.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <memory>
#include <random>
#include <sstream>
#include <system_error>

namespace galaxy::archipelago {

namespace {

constexpr std::uint32_t kStatusGoal = 30;  // Archipelago's ClientStatus.CLIENT_GOAL
constexpr std::size_t kMaxNotices = 32;

std::string trim(std::string_view text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && (text[begin] == ' ' || text[begin] == '\t' || text[begin] == '\r' || text[begin] == '\n')) ++begin;
    while (end > begin && (text[end - 1u] == ' ' || text[end - 1u] == '\t' || text[end - 1u] == '\r' || text[end - 1u] == '\n')) --end;
    return std::string(text.substr(begin, end - begin));
}

std::string lower(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return text;
}

// A name as part of a file name: letters, digits, - and _ only.
std::string file_safe(std::string_view text) {
    std::string out;
    for (const char c : text) {
        const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
        out += plain ? c : '_';
        if (out.size() >= 48u) break;
    }
    return out;
}

// The most characters of a notice the pop-up shows.
constexpr std::size_t kNoticeTextLimit = 70u;

// Text for the game's on-screen notices: printable ASCII only (the game's font has no more), cut to `limit`.
std::string ascii_line(std::string_view text, std::size_t limit) {
    std::string out;
    for (const char c : text) {
        const auto u = static_cast<unsigned char>(c);
        if (u >= 0x20u && u < 0x7Fu) {
            out += c;
        } else if (u == '\n' || u == '\t') {
            out += ' ';
        } else if (u < 0x80u) {
            continue;
        } else if ((u & 0xC0u) != 0x80u) {
            out += '?';  // the first byte of a longer character; the rest are skipped
        }
        if (out.size() >= limit) break;
    }
    return out;
}

std::string make_uuid() {
    std::random_device device;
    std::mt19937_64 engine{(static_cast<std::uint64_t>(device()) << 32) ^ device()};
    char buffer[40];
    std::snprintf(buffer, sizeof(buffer), "%016llx%016llx", static_cast<unsigned long long>(engine()),
                  static_cast<unsigned long long>(engine()));
    return buffer;
}

std::string command_message(const char* name) {
    return std::string("{\"cmd\":\"") + name + "\"}";
}

std::uint32_t hash_text(std::string_view text) {
    std::uint32_t hash = 2166136261u;
    for (const char c : text) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 16777619u;
    }
    return hash == 0u ? 1u : hash;
}

// The file's text as plain bytes: a UTF-8 byte-order mark is dropped, UTF-16 (Notepad's "Unicode") is narrowed.
std::string normalize_text(std::string_view text) {
    if (text.size() >= 2u && static_cast<unsigned char>(text[0]) == 0xFFu && static_cast<unsigned char>(text[1]) == 0xFEu) {
        std::string out;
        for (std::size_t i = 2u; i + 1u < text.size(); i += 2u) {
            out += text[i + 1u] == 0 ? text[i] : '?';
        }
        return out;
    }
    if (text.size() >= 2u && static_cast<unsigned char>(text[0]) == 0xFEu && static_cast<unsigned char>(text[1]) == 0xFFu) {
        std::string out;
        for (std::size_t i = 2u; i + 1u < text.size(); i += 2u) {
            out += text[i] == 0 ? text[i + 1u] : '?';
        }
        return out;
    }
    if (text.size() >= 3u && static_cast<unsigned char>(text[0]) == 0xEFu && static_cast<unsigned char>(text[1]) == 0xBBu &&
        static_cast<unsigned char>(text[2]) == 0xBFu) {
        text.remove_prefix(3u);
    }
    return std::string(text);
}

const char* const kOptionNames[kOptionCount] = {
    nullptr,
    "goal",
    "star_items",
    "start_at_observatory",
    "character",
    "unlock_luigi",
    "dome_shuffle",
    "stars_to_finish",
    "exact_requirements",
    "luma_counts_set",
    "star_colors",
    "death_link",
    "music_shuffle",
    "transformation_items",
    "enemy_shuffle",
    "skybox_shuffle",
    "color_0",  "color_1",  "color_2",  "color_3",  "color_4",  "color_5",  "color_6",  "color_7",
    "color_8",  "color_9",  "color_10", "color_11", "color_12", "color_13", "color_14", "color_15",
    "color_16", "color_17", "color_18", "color_19", "color_20", "color_21", "color_22", "color_23",
    "movement_items",
    "include_comet_stars",
    "include_purple_coin_stars",
};

// The options an override may change: the ones no logic depends on.
bool overridable(std::uint32_t option) {
    return option == static_cast<std::uint32_t>(Option::StarColors) || option == static_cast<std::uint32_t>(Option::DeathLink) ||
           option == static_cast<std::uint32_t>(Option::MusicShuffle) || option == static_cast<std::uint32_t>(Option::EnemyShuffle) ||
           option == 15u /* skybox_shuffle */ ||
           (option >= static_cast<std::uint32_t>(Option::ColorFirst) && option <= static_cast<std::uint32_t>(Option::ColorLast));
}

bool is_counted_item(std::uint32_t item) {
    return item == kItemPowerStar || item == kItemGreenStar || item == kItemGrandStar ||
           (item >= kItemBeeMushroom && item <= kItemFlyingStar) || (item >= kItemSpin && item <= kItemTripleJump);
}

}  // namespace

const char* option_name(std::uint32_t option) noexcept {
    return option < kOptionCount ? kOptionNames[option] : nullptr;
}

std::map<std::uint32_t, std::int64_t> parse_option_overrides(std::string_view raw) {
    std::map<std::uint32_t, std::int64_t> result;
    std::istringstream stream{normalize_text(raw)};
    std::string line;
    while (std::getline(stream, line)) {
        const std::string trimmed = trim(line);
        const std::size_t equals = trimmed.find('=');
        if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == ';' || equals == std::string::npos) continue;
        const std::string key = lower(trim(std::string_view(trimmed).substr(0, equals)));
        if (key.rfind("option.", 0) != 0) continue;
        const std::string name = key.substr(7);
        const std::string value = lower(trim(std::string_view(trimmed).substr(equals + 1u)));
        std::int64_t number = 0;
        if (value == "on" || value == "true" || value == "yes") number = 1;
        else if (value == "off" || value == "false" || value == "no") number = 0;
        else {
            try {
                std::size_t used = 0;
                number = std::stoll(value, &used, 0);
                if (used != value.size()) continue;
            } catch (...) {
                continue;
            }
        }
        for (std::uint32_t id = 1; id < kOptionCount; ++id) {
            if (kOptionNames[id] != nullptr && name == kOptionNames[id] && overridable(id)) result[id] = number;
        }
    }
    return result;
}

Config parse_config(std::string_view raw) {
    Config config;
    bool enabled = true;
    std::istringstream stream{normalize_text(raw)};
    std::string line;
    while (std::getline(stream, line)) {
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == ';') continue;
        const std::size_t equals = trimmed.find('=');
        if (equals == std::string::npos) continue;
        const std::string key = lower(trim(std::string_view(trimmed).substr(0, equals)));
        const std::string value = trim(std::string_view(trimmed).substr(equals + 1u));
        if (key == "server") {
            config.server = value;
        } else if (key == "slot" || key == "name") {
            config.slot = value;
        } else if (key == "password") {
            config.password = value;
        } else if (key == "enabled") {
            const std::string v = lower(value);
            enabled = !(v == "0" || v == "false" || v == "no" || v == "off");
        }
    }
    config.enabled = enabled && !config.server.empty() && !config.slot.empty();
    return config;
}

std::string config_problem(std::string_view raw) {
    const Config config = parse_config(raw);
    if (config.enabled) return std::string();
    if (config.server.empty() && config.slot.empty()) {
        return "the file has no \"server=...\" line and no \"slot=...\" line (is it empty, or are the lines written another way?)";
    }
    if (config.server.empty()) return "the file has no \"server=...\" line";
    if (config.slot.empty()) return "the file has no \"slot=...\" line";
    return "the file says enabled=0";
}

// ---- Engine ----

Engine::Engine(Config config, std::filesystem::path state_folder, LogFn logger)
    : config_(std::move(config)), state_folder_(std::move(state_folder)), log_(logger), uuid_(make_uuid()) {
    found_flags_.assign(kLocationLast + 1u, false);
    load_cache();
}

std::filesystem::path Engine::cache_path() const {
    return state_folder_ / ("archipelago_cache_" + file_safe(config_.slot) + ".json");
}

// What the last connection told (the slot's options, the items received): the game starts before the server answers,
// and must not take a slot's Grand Stars away for those seconds.
void Engine::load_cache() {
    if (state_folder_.empty()) return;
    std::ifstream in(cache_path(), std::ios::binary);
    if (!in) return;
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const std::optional<json::Value> parsed = json::parse(text);
    if (!parsed || !parsed->is_object()) return;
    const std::lock_guard<std::mutex> lock(mutex_);
    if (const json::Value* seed = parsed->find("seed")) seed_name_ = seed->as_string();
    if (seed_name_.empty()) return;
    if (const json::Value* options = parsed->find("options")) {
        for (const auto& [key, value] : options->members()) {
            try {
                options_[static_cast<std::uint32_t>(std::stoul(key))] = value.as_int(0);
            } catch (...) {
            }
        }
    }
    const auto read_list = [&](const char* key, std::vector<std::uint32_t>& into) {
        if (const json::Value* list = parsed->find(key)) {
            for (const json::Value& item : list->items()) into.push_back(static_cast<std::uint32_t>(item.as_int(0)));
        }
    };
    read_list("galaxy_caps", galaxy_caps_);
    read_list("dome_order", dome_order_);
    read_list("luma_counts", luma_counts_);
    read_list("galaxy_shuffle", galaxy_shuffle_);
    read_list("shop_prices", shop_prices_);
    {
        std::vector<std::uint32_t> bought;
        read_list("shop_checked", bought);
        checked_shop_.insert(bought.begin(), bought.end());
    }
    read_list("received", received_);
    std::vector<std::uint32_t> valid;
    read_list("valid", valid);
    if (!valid.empty()) {
        have_valid_ = true;
        valid_.insert(valid.begin(), valid.end());
    }
    if (const json::Value* id = parsed->find("seed_id")) seed_id_ = static_cast<std::uint32_t>(id->as_int(0));
    if (const json::Value* goal = parsed->find("goal")) goal_ = static_cast<std::uint32_t>(goal->as_int(0));
    load_state_locked();
}

void Engine::save_cache_locked() const {
    if (state_folder_.empty() || seed_name_.empty()) return;
    json::Value root = json::Value::object();
    root.set("seed", json::Value::string(seed_name_));
    root.set("seed_id", json::Value::integer(seed_id_));
    root.set("goal", json::Value::integer(goal_));
    json::Value options = json::Value::object();
    for (const auto& [id, value] : options_) options.set(std::to_string(id), json::Value::integer(value));
    root.set("options", std::move(options));
    const auto write_list = [&](const char* key, const std::vector<std::uint32_t>& from) {
        json::Value list = json::Value::array();
        for (const std::uint32_t item : from) list.push(json::Value::integer(item));
        root.set(key, std::move(list));
    };
    write_list("galaxy_caps", galaxy_caps_);
    write_list("dome_order", dome_order_);
    write_list("luma_counts", luma_counts_);
    write_list("galaxy_shuffle", galaxy_shuffle_);
    write_list("shop_prices", shop_prices_);
    write_list("shop_checked", std::vector<std::uint32_t>(checked_shop_.begin(), checked_shop_.end()));
    write_list("received", received_);
    write_list("valid", std::vector<std::uint32_t>(valid_.begin(), valid_.end()));
    std::error_code error;
    std::filesystem::create_directories(state_folder_, error);
    (void)io::write_text_file_atomically(cache_path(), root.dump());
}

// A different multiworld than the cache's: nothing of the old one applies.
void Engine::forget_slot_locked() {
    received_.clear();
    options_.clear();
    galaxy_caps_.clear();
    dome_order_.clear();
    luma_counts_.clear();
    galaxy_shuffle_.clear();
    shop_prices_.clear();
    checked_shop_.clear();
    valid_.clear();
    have_valid_ = false;
    seed_id_ = 0;
    applied_.clear();
}

void Engine::log(const std::string& text) const {
    if (log_ != nullptr) log_(text.c_str());
}

std::filesystem::path Engine::state_path_locked() const {
    return state_folder_ / ("archipelago_" + file_safe(seed_name_) + "_" + file_safe(config_.slot) + ".ini");
}

void Engine::load_state_locked() {
    applied_.clear();
    goals_done_ = 0;
    if (state_folder_.empty() || seed_name_.empty()) return;
    std::ifstream in(state_path_locked());
    std::string line;
    while (std::getline(in, line)) {
        const std::size_t equals = line.find('=');
        if (equals != std::string::npos && line.compare(0, equals, "goals") == 0) {
            try {
                goals_done_ = static_cast<std::uint32_t>(std::stoul(line.substr(equals + 1u))) & 3u;
            } catch (...) {
            }
            continue;
        }
        if (equals == std::string::npos || line.compare(0, 4, "file") != 0) continue;
        try {
            const auto file = static_cast<std::uint32_t>(std::stoul(line.substr(4, equals - 4u)));
            applied_[file] = static_cast<std::size_t>(std::stoull(line.substr(equals + 1u)));
        } catch (...) {
        }
    }
}

void Engine::save_state_locked() {
    if (state_folder_.empty() || seed_name_.empty()) return;
    std::error_code error;
    std::filesystem::create_directories(state_folder_, error);
    std::ostringstream out;
    out << "# Items this save file has received from the Archipelago server (do not edit)\n";
    for (const auto& [file, count] : applied_) out << "file" << file << '=' << count << '\n';
    if (goals_done_ != 0u) out << "goals=" << goals_done_ << '\n';
    (void)io::write_text_file_atomically(state_path_locked(), out.str());
}

void Engine::notice_locked(const std::string& text, std::uint32_t icon) {
    const std::string line = ascii_line(text, kNoticeTextLimit);
    if (line.empty()) return;
    if (notices_.size() >= kMaxNotices) {
        notices_.pop_front();
        notice_icons_.pop_front();
    }
    notices_.push_back(line);
    notice_icons_.push_back(icon);
}

void Engine::add_notice(const std::string& text) {
    const std::lock_guard<std::mutex> lock(mutex_);
    notice_locked(text);
}

bool Engine::pop_notice(std::string& text, std::uint32_t* icon) {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (notices_.empty()) return false;
    text = std::move(notices_.front());
    notices_.pop_front();
    if (icon != nullptr) *icon = notice_icons_.front();
    notice_icons_.pop_front();
    return true;
}

void Engine::on_message(std::string_view text, std::vector<std::string>& out) {
    const std::optional<json::Value> parsed = json::parse(text);
    if (!parsed || !parsed->is_array()) {
        log("a message from the server could not be read");
        return;
    }
    for (const json::Value& command : parsed->items()) handle_command(command, out);
}

std::string Engine::player_name_locked(std::int64_t slot) const {
    const auto it = player_names_.find(slot);
    return it != player_names_.end() ? it->second : "player " + std::to_string(slot);
}

std::string Engine::item_name_locked(std::int64_t item, std::int64_t receiving_slot) const {
    const auto game = slot_games_.find(receiving_slot);
    if (game != slot_games_.end()) {
        const auto names = item_names_.find(game->second);
        if (names != item_names_.end()) {
            const auto name = names->second.find(item);
            if (name != names->second.end()) return name->second;
        }
        if (game->second == kGameName) {
            // Our own items, even before the data package arrived.
            switch (static_cast<std::uint32_t>(item - static_cast<std::int64_t>(kItemBase))) {
            case kItemOneUp: return "1-Up Mushroom";
            case kItemStarBits50: return "50 Star Bits";
            case kItemPowerStar: return "Power Star";
            case kItemGreenStar: return "Green Star";
            case kItemGrandStar: return "Grand Star";
            case kItemBeeMushroom: return "Bee Mushroom";
            case kItemBooMushroom: return "Boo Mushroom";
            case kItemSpringMushroom: return "Spring Mushroom";
            case kItemFireFlower: return "Fire Flower";
            case kItemIceFlower: return "Ice Flower";
            case kItemFlyingStar: return "Flying (Red Star)";
            case kItemStarBits200: return "200 Star Bits";
            case kItemOneUpBurst: return "3 1-Up Mushrooms";
            case kItemFullHeal: return "Full Heal";
            case kItemSpin: return "Spin";
            case kItemWallJump: return "Wall Jump";
            case kItemLongJump: return "Long Jump";
            case kItemBackflip: return "Backflip";
            case kItemSideFlip: return "Side Flip";
            case kItemTripleJump: return "Triple Jump";
            default: break;
            }
        }
    }
    return "an item";
}

std::string Engine::location_name_locked(std::int64_t location, std::int64_t slot) const {
    const auto game = slot_games_.find(slot);
    if (game == slot_games_.end()) return {};
    const auto names = location_names_.find(game->second);
    if (names == location_names_.end()) return {};
    const auto name = names->second.find(location);
    return name != names->second.end() ? name->second : std::string();
}

void Engine::handle_print(const json::Value& command) {
    const json::Value* type = command.find("type");
    if (type == nullptr) return;
    // The items of the server's console (/send, !getitem) come as ReceivedItems with the location -1 (the console's pop-up
    // is made there): the server prints no ItemSend for them, and an ItemCheat line would show the same item twice.
    if (type->as_string() != "ItemSend") return;
    const json::Value* item = command.find("item");
    const json::Value* receiving = command.find("receiving");
    if (item == nullptr || receiving == nullptr) return;
    const json::Value* item_id = item->find("item");
    const json::Value* sender = item->find("player");
    const json::Value* location_id = item->find("location");
    if (item_id == nullptr || sender == nullptr) return;

    const std::lock_guard<std::mutex> lock(mutex_);
    const std::int64_t to = receiving->as_int(-1);
    const std::int64_t from = sender->as_int(-1);
    const std::string name = item_name_locked(item_id->as_int(0), to);
    // The place it was found, when the line stays short enough for the pop-up.
    const std::string place = location_id != nullptr ? location_name_locked(location_id->as_int(0), from) : std::string();
    const auto with_place = [&](const std::string& line) {
        return !place.empty() && line.size() + place.size() + 3u <= kNoticeTextLimit ? line + " (" + place + ")" : line;
    };
    // What we receive shows the item's own icon (the ones that have a picture; the others keep the logo).
    std::uint32_t icon = 0u;
    if (to == my_slot_) {
        const std::int64_t number = item_id->as_int(0) - static_cast<std::int64_t>(kItemBase);
        if (number > 0 && number <= 0xFF) icon = static_cast<std::uint32_t>(number);
    }
    if (to == my_slot_ && from == my_slot_) {
        notice_locked("Found " + name, icon);
    } else if (to == my_slot_) {
        notice_locked(with_place(name + " from " + player_name_locked(from)), icon);
    } else if (from == my_slot_) {
        notice_locked("Sent " + name + " to " + player_name_locked(to));
    }
}

void Engine::handle_connected(const json::Value& command, std::vector<std::string>& out) {
    std::string game_list;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        state_ = State::Connected;
        connected_ = true;
        announced_failure_ = false;
        sent_count_ = 0;  // everything found so far goes to this connection (the server ignores a repeat)
        goal_sent_ = false;
        death_to_send_ = false;
        shop_hint_pending_ = false;
        death_received_ = false;
        goal_ = kGoalEnding;
        options_.clear();
        galaxy_caps_.clear();
        dome_order_.clear();
        luma_counts_.clear();
        galaxy_shuffle_.clear();
        shop_prices_.clear();
        scouted_.clear();
        valid_.clear();
        star_classes_.clear();
        have_valid_ = false;
        player_names_.clear();
        slot_games_.clear();

        if (const json::Value* slot = command.find("slot")) my_slot_ = slot->as_int(-1);
        if (const json::Value* players = command.find("players")) {
            for (const json::Value& player : players->items()) {
                const json::Value* slot = player.find("slot");
                const json::Value* alias = player.find("alias");
                const json::Value* name = player.find("name");
                if (slot == nullptr) continue;
                const std::string shown = alias != nullptr && !alias->as_string().empty() ? alias->as_string()
                                                                                          : (name != nullptr ? name->as_string() : std::string());
                player_names_[slot->as_int()] = shown;
            }
        }
        std::set<std::string> games;
        if (const json::Value* info = command.find("slot_info")) {
            for (const auto& [slot, member] : info->members()) {
                const json::Value* game = member.find("game");
                if (game == nullptr) continue;
                try {
                    slot_games_[std::stoll(slot)] = game->as_string();
                } catch (...) {
                }
                games.insert(game->as_string());
            }
        }
        if (games.empty()) games.insert(kGameName);
        for (const std::string& g : games) game_list += (game_list.empty() ? "" : ", ") + g;
        slot_games_[my_slot_] = kGameName;

        for (const char* key : {"missing_locations", "checked_locations"}) {
            if (const json::Value* list = command.find(key)) {
                have_valid_ = true;
                for (const json::Value& id : list->items()) {
                    const std::int64_t number = id.as_int(0) - static_cast<std::int64_t>(kLocationBase);
                    if (number >= 1 && number <= static_cast<std::int64_t>(kLocationLast)) valid_.insert(static_cast<std::uint32_t>(number));
                    if (number >= static_cast<std::int64_t>(kShopFirst) && number <= static_cast<std::int64_t>(kShopLast)) {
                        if (std::string_view(key) == "checked_locations") checked_shop_.insert(static_cast<std::uint32_t>(number));
                        else checked_shop_.erase(static_cast<std::uint32_t>(number));
                    }
                }
            }
        }
        if (const json::Value* slot_data = command.find("slot_data")) {
            if (const json::Value* goal = slot_data->find("goal")) goal_ = static_cast<std::uint32_t>(goal->as_int(0));
            if (const json::Value* options = slot_data->find("options")) {
                for (std::uint32_t id = 1; id < kOptionCount; ++id) {
                    const char* name = kOptionNames[id];
                    if (name == nullptr) continue;
                    if (const json::Value* value = options->find(name)) options_[id] = value->as_int(0);
                }
                if (const auto it = options_.find(static_cast<std::uint32_t>(Option::Goal)); it != options_.end()) {
                    goal_ = static_cast<std::uint32_t>(it->second);
                }
            }
            if (const json::Value* caps = slot_data->find("galaxy_caps")) {
                for (const json::Value& cap : caps->items()) galaxy_caps_.push_back(static_cast<std::uint32_t>(cap.as_int(0)));
            }
            if (const json::Value* order = slot_data->find("dome_order")) {
                for (const json::Value& dome : order->items()) dome_order_.push_back(static_cast<std::uint32_t>(dome.as_int(0)));
            }
            if (const json::Value* shuffle = slot_data->find("galaxy_shuffle")) {
                for (const json::Value& galaxy : shuffle->items()) galaxy_shuffle_.push_back(static_cast<std::uint32_t>(galaxy.as_int(0)));
            }
            if (const json::Value* prices = slot_data->find("shop_prices")) {
                for (const json::Value& price : prices->items()) shop_prices_.push_back(static_cast<std::uint32_t>(price.as_int(0)));
            }
            if (const json::Value* lumas = slot_data->find("luma_counts")) {
                for (const json::Value& luma : lumas->items()) luma_counts_.push_back(static_cast<std::uint32_t>(luma.as_int(0)));
            }
        }
        seed_id_ = hash_text(seed_name_ + "|" + config_.slot);
        const bool star_colors = [&] {
            return effective_option(Option::StarColors) != 0;
        }();
        if (star_colors || !shop_prices_.empty()) {
            // The colour of a star tells the class of its item, and the shop shows what each shelf holds: ask the server
            // what every location of this slot holds.
            json::Value wanted = json::Value::array();
            for (const std::uint32_t number : valid_) wanted.push(json::Value::integer(static_cast<std::int64_t>(kLocationBase) + number));
            if (!wanted.items().empty()) {
                json::Value scout = json::Value::object();
                scout.set("cmd", json::Value::string("LocationScouts"));
                scout.set("locations", std::move(wanted));
                scout.set("create_as_hint", json::Value::integer(0));
                json::Value scout_packet = json::Value::array();
                scout_packet.push(std::move(scout));
                out.push_back(scout_packet.dump());
            }
        }
        if (effective_option(Option::DeathLink) != 0) {
            // DeathLink is a tag of the connection: tell the server this one takes part.
            json::Value tags = json::Value::array();
            tags.push(json::Value::string("DeathLink"));
            json::Value update = json::Value::object();
            update.set("cmd", json::Value::string("ConnectUpdate"));
            update.set("tags", std::move(tags));
            json::Value update_packet = json::Value::array();
            update_packet.push(std::move(update));
            out.push_back(update_packet.dump());
        }
        notice_locked("Connected to Archipelago as " + config_.slot);
        save_cache_locked();

        if (!data_package_requested_) {
            data_package_requested_ = true;
            json::Value games_array = json::Value::array();
            for (const std::string& g : games) games_array.push(json::Value::string(g));
            json::Value request = json::Value::object();
            request.set("cmd", json::Value::string("GetDataPackage"));
            request.set("games", std::move(games_array));
            json::Value packet = json::Value::array();
            packet.push(std::move(request));
            out.push_back(packet.dump());
        }
    }
    log("connected to the server as " + config_.slot + " (games in the room: " + game_list + ")");
}

void Engine::handle_command(const json::Value& command, std::vector<std::string>& out) {
    const json::Value* name = command.find("cmd");
    if (name == nullptr) return;
    const std::string& cmd = name->as_string();

    if (cmd == "RoomInfo") {
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            const json::Value* seed = command.find("seed_name");
            const std::string new_seed = seed != nullptr ? seed->as_string() : std::string();
            if (new_seed != seed_name_) {
                forget_slot_locked();
                seed_name_ = new_seed;
                load_state_locked();
            }
            state_ = State::Connecting;
            data_package_requested_ = false;
        }
        log("the server answered (room \"" + (command.find("seed_name") ? command.find("seed_name")->as_string() : std::string()) +
            "\"); logging in as \"" + config_.slot + "\" for the game \"" + kGameName + "\"");
        json::Value connect = json::Value::object();
        connect.set("cmd", json::Value::string("Connect"));
        connect.set("password", json::Value::string(config_.password));
        connect.set("game", json::Value::string(kGameName));
        connect.set("name", json::Value::string(config_.slot));
        connect.set("uuid", json::Value::string(uuid_));
        json::Value version = json::Value::object();
        version.set("major", json::Value::integer(0));
        version.set("minor", json::Value::integer(5));
        version.set("build", json::Value::integer(0));
        version.set("class", json::Value::string("Version"));
        connect.set("version", std::move(version));
        connect.set("items_handling", json::Value::integer(7));  // all items, including those from this world
        connect.set("tags", json::Value::array());
        connect.set("slot_data", json::Value::boolean(true));
        json::Value packet = json::Value::array();
        packet.push(std::move(connect));
        out.push_back(packet.dump());
        return;
    }

    if (cmd == "Connected") {
        handle_connected(command, out);
        return;
    }

    if (cmd == "ConnectionRefused") {
        std::string reasons;
        if (const json::Value* errors = command.find("errors")) {
            for (const json::Value& error : errors->items()) reasons += (reasons.empty() ? "" : ", ") + error.as_string();
        }
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            state_ = State::Failed;
            connected_ = false;
            std::string shown = reasons.empty() ? std::string("unknown reason") : reasons;
            if (reasons.find("InvalidSlot") != std::string::npos) shown = "no slot called \"" + config_.slot + "\" in this room";
            else if (reasons.find("InvalidGame") != std::string::npos) shown = "that slot is not a Super Mario Galaxy PC Port slot";
            else if (reasons.find("InvalidPassword") != std::string::npos) shown = "wrong password";
            notice_locked("Archipelago refused: " + shown);
        }
        log("the server refused the connection: " + (reasons.empty() ? std::string("unknown reason") : reasons) +
            " (check server, slot and password in archipelago.ini; InvalidSlot: the name must be the YAML's \"name:\"; "
            "InvalidGame: the slot is of another game, the APWorld is \"" + kGameName + "\")");
        return;
    }

    if (cmd == "ReceivedItems") {
        bool resync = false;
        std::size_t total = 0;
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            const json::Value* index_value = command.find("index");
            const std::size_t index = index_value != nullptr ? static_cast<std::size_t>(std::max<std::int64_t>(0, index_value->as_int(0))) : 0u;
            const std::size_t known = received_.size();
            if (index > received_.size()) {
                resync = true;  // items missed: ask for them all again
            } else {
                received_.resize(index);
                if (const json::Value* items = command.find("items")) {
                    for (const json::Value& item : items->items()) {
                        const json::Value* id = item.find("item");
                        const json::Value* location = item.find("location");
                        // An item the server's console gave (location -1) that we had not seen yet: a pop-up, with its icon.
                        // (Not at the first connection after the cache is lost: only items past the ones we knew.)
                        if (id != nullptr && location != nullptr && location->as_int(0) == -1 && received_.size() >= known && known > 0u) {
                            const std::int64_t number = id->as_int(0) - static_cast<std::int64_t>(kItemBase);
                            notice_locked("Cheat console: " + item_name_locked(id->as_int(0), my_slot_),
                                          number > 0 && number <= 0xFF ? static_cast<std::uint32_t>(number) : 0u);
                        }
                        received_.push_back(id != nullptr ? static_cast<std::uint32_t>(id->as_int(0)) : 0u);
                    }
                }
            }
            total = received_.size();
            save_cache_locked();
        }
        log("items received: " + std::to_string(total) + " in all");
        if (resync) out.push_back("[" + command_message("Sync") + "]");
        return;
    }

    if (cmd == "DataPackage") {
        const json::Value* data = command.find("data");
        const json::Value* games = data != nullptr ? data->find("games") : nullptr;
        if (games == nullptr) return;
        const std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [game, content] : games->members()) {
            const json::Value* items = content.find("item_name_to_id");
            if (items == nullptr) continue;
            std::map<std::int64_t, std::string>& names = item_names_[game];
            for (const auto& [item_name, id] : items->members()) names[id.as_int()] = item_name;
            if (const json::Value* locations = content.find("location_name_to_id")) {
                std::map<std::int64_t, std::string>& place_names = location_names_[game];
                for (const auto& [location_name, id] : locations->members()) place_names[id.as_int()] = location_name;
            }
        }
        return;
    }

    if (cmd == "LocationInfo") {
        const json::Value* locations = command.find("locations");
        if (locations == nullptr) return;
        std::size_t count = 0;
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            for (const json::Value& entry : locations->items()) {
                const json::Value* id = entry.find("location");
                const json::Value* flags = entry.find("flags");
                if (id == nullptr) continue;
                const std::int64_t number = id->as_int(0) - static_cast<std::int64_t>(kLocationBase);
                if (number < 1 || number > static_cast<std::int64_t>(kLocationLast)) continue;
                const std::int64_t bits = flags != nullptr ? flags->as_int(0) : 0;
                if (number >= static_cast<std::int64_t>(kShopFirst) && number <= static_cast<std::int64_t>(kShopLast)) {
                    const json::Value* item = entry.find("item");
                    const json::Value* player = entry.find("player");
                    scouted_[static_cast<std::uint32_t>(number)] = {item != nullptr ? item->as_int(0) : 0, player != nullptr ? player->as_int(0) : 0};
                }
                star_classes_[static_cast<std::uint32_t>(number)] = (bits & 1) != 0   ? Engine::kClassProgression
                                                                    : (bits & 4) != 0 ? Engine::kClassTrap
                                                                    : (bits & 2) != 0 ? Engine::kClassUseful
                                                                                      : Engine::kClassFiller;
                ++count;
            }
        }
        log("the classes of " + std::to_string(count) + " items of the stars are known");
        return;
    }

    if (cmd == "Bounce") {
        const json::Value* tags = command.find("tags");
        bool death_link = false;
        if (tags != nullptr) {
            for (const json::Value& tag : tags->items()) death_link = death_link || tag.as_string() == "DeathLink";
        }
        const json::Value* data = command.find("data");
        if (!death_link || data == nullptr) return;
        const json::Value* source = data->find("source");
        const json::Value* time = data->find("time");
        const json::Value* cause = data->find("cause");
        const std::string who = source != nullptr ? source->as_string() : std::string();
        const double when = time != nullptr ? time->as_double(0.0) : 0.0;
        const std::lock_guard<std::mutex> lock(mutex_);
        if (effective_option(Option::DeathLink) == 0) return;
        if (who == config_.slot || (when != 0.0 && when == last_death_time_)) return;  // our own, back from the server
        last_death_time_ = when;
        death_received_ = true;
        notice_locked("DeathLink: " + (cause != nullptr && !cause->as_string().empty() ? cause->as_string() : who + " died"));
        return;
    }

    if (cmd == "PrintJSON") {
        handle_print(command);
        return;
    }

    if (cmd == "InvalidPacket") {
        const json::Value* text = command.find("text");
        log("the server did not understand a message: " + (text != nullptr ? text->as_string() : std::string()));
    }
}

void Engine::poll_outgoing(std::vector<std::string>& out) {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_) return;
    if (sent_count_ < found_.size()) {
        json::Value locations = json::Value::array();
        for (std::size_t i = sent_count_; i < found_.size(); ++i) {
            if (have_valid_ && valid_.count(found_[i]) == 0u) continue;  // a star this multiworld does not use
            locations.push(json::Value::integer(static_cast<std::int64_t>(kLocationBase) + found_[i]));
        }
        sent_count_ = found_.size();
        if (!locations.items().empty()) {
            json::Value message = json::Value::object();
            message.set("cmd", json::Value::string("LocationChecks"));
            message.set("locations", std::move(locations));
            json::Value packet = json::Value::array();
            packet.push(std::move(message));
            out.push_back(packet.dump());
        }
    }
    if (shop_hint_pending_) {
        shop_hint_pending_ = false;
        // The shelves on display become hints for the players whose items stand on them (2: only the new ones).
        json::Value wanted = json::Value::array();
        for (std::uint32_t i = 0; i < shop_prices_.size(); ++i) {
            const std::uint32_t number = kShopFirst + i;
            if (number > kShopLast || found_flags_[number] || checked_shop_.count(number) != 0u) continue;
            wanted.push(json::Value::integer(static_cast<std::int64_t>(kLocationBase) + number));
        }
        if (!wanted.items().empty()) {
            json::Value scout = json::Value::object();
            scout.set("cmd", json::Value::string("LocationScouts"));
            scout.set("locations", std::move(wanted));
            scout.set("create_as_hint", json::Value::integer(2));
            json::Value scout_packet = json::Value::array();
            scout_packet.push(std::move(scout));
            out.push_back(scout_packet.dump());
        }
    }
    if (death_to_send_) {
        death_to_send_ = false;
        const double now = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
        last_death_time_ = now;
        json::Value data = json::Value::object();
        data.set("time", json::Value::number(now));
        data.set("source", json::Value::string(config_.slot));
        data.set("cause", json::Value::string(config_.slot + " lost a life in Super Mario Galaxy"));
        json::Value tags = json::Value::array();
        tags.push(json::Value::string("DeathLink"));
        json::Value message = json::Value::object();
        message.set("cmd", json::Value::string("Bounce"));
        message.set("tags", std::move(tags));
        message.set("data", std::move(data));
        json::Value packet = json::Value::array();
        packet.push(std::move(message));
        out.push_back(packet.dump());
    }
    if (goal_reported_ && !goal_sent_) {
        goal_sent_ = true;
        json::Value message = json::Value::object();
        message.set("cmd", json::Value::string("StatusUpdate"));
        message.set("status", json::Value::integer(kStatusGoal));
        json::Value packet = json::Value::array();
        packet.push(std::move(message));
        out.push_back(packet.dump());
    }
}

void Engine::on_disconnected() {
    bool was_connected = false;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        was_connected = connected_;
        connected_ = false;
        if (state_ != State::Failed) state_ = State::Connecting;
        if (was_connected) notice_locked("Archipelago: connection lost, trying again");
    }
}

void Engine::on_connect_failed(const std::string& reason) {
    const std::lock_guard<std::mutex> lock(mutex_);
    connected_ = false;
    if (state_ != State::Failed) state_ = State::Connecting;
    if (!announced_failure_) {
        announced_failure_ = true;
        notice_locked("Archipelago: cannot reach the server (see archipelago.log)");
    }
    (void)reason;
}

void Engine::report_location(std::uint32_t index) {
    if (index == 0u || index > kLocationLast) return;
    const std::lock_guard<std::mutex> lock(mutex_);
    if (found_flags_[index]) return;
    found_flags_[index] = true;
    found_.push_back(index);
    if (index >= kShopFirst && index <= kShopLast) {
        checked_shop_.insert(index);
        save_cache_locked();  // a shelf bought stays bought when the game restarts before the server answers
    }
}

void Engine::report_goal(std::uint32_t character) {
    const std::lock_guard<std::mutex> lock(mutex_);
    const std::uint32_t bit = character == 1u ? 2u : 1u;
    if ((goals_done_ & bit) == 0u) {
        goals_done_ |= bit;
        save_state_locked();  // the other character may finish in another run
    }
    const std::int64_t who = effective_option(Option::Character);
    const std::uint32_t needed = who == static_cast<std::int64_t>(kCharacterBoth) ? 3u : who == static_cast<std::int64_t>(kCharacterLuigi) ? 2u : 1u;
    if ((goals_done_ & needed) == needed) goal_reported_ = true;
}

std::uint32_t Engine::next_item(std::uint32_t file) {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (seed_name_.empty()) return 0u;  // which multiworld this is is not known yet: what a file applied is not either
    std::size_t done = 0u;
    if (const auto it = applied_.find(file); it != applied_.end()) done = it->second;
    bool skipped = false;
    // The stars are counted from everything received (item_count), not handed over: pass them.
    while (done < received_.size()) {
        const std::uint32_t id = received_[done];
        if (id > kItemBase && id <= kItemBase + 0xFFu && is_counted_item(id - kItemBase)) {
            ++done;
            skipped = true;
            continue;
        }
        break;
    }
    if (skipped) {
        applied_[file] = done;
        save_state_locked();
    }
    if (done >= received_.size()) return 0u;
    const std::uint32_t id = received_[done];
    if (id <= kItemBase || id > kItemBase + 0xFFu) return kItemUnknown;
    return id - kItemBase;
}

void Engine::item_applied(std::uint32_t file) {
    const std::lock_guard<std::mutex> lock(mutex_);
    std::size_t& done = applied_[file];
    if (done >= received_.size()) return;
    ++done;
    save_state_locked();
}

std::uint32_t Engine::item_count(std::uint32_t item) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    std::uint32_t count = 0;
    for (const std::uint32_t id : received_) {
        if (id == kItemBase + item) ++count;
    }
    return count;
}

bool Engine::option_set(Option option) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return options_.count(static_cast<std::uint32_t>(option)) != 0u || overrides_.count(static_cast<std::uint32_t>(option)) != 0u;
}

std::uint32_t Engine::option_value(Option option) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (const auto over = overrides_.find(static_cast<std::uint32_t>(option)); over != overrides_.end()) {
        return static_cast<std::uint32_t>(over->second);
    }
    const auto it = options_.find(static_cast<std::uint32_t>(option));
    return it != options_.end() ? static_cast<std::uint32_t>(it->second) : 0u;
}

std::int64_t Engine::effective_option(Option option) const {
    const std::uint32_t id = static_cast<std::uint32_t>(option);
    if (const auto over = overrides_.find(id); over != overrides_.end()) return over->second;
    const auto it = options_.find(id);
    return it != options_.end() ? it->second : 0;
}

bool Engine::set_option_overrides(std::map<std::uint32_t, std::int64_t> overrides) {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (overrides == overrides_) return false;
    overrides_ = std::move(overrides);
    return true;
}

std::uint32_t Engine::list_value(std::uint32_t list, std::uint32_t index) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (list != kListGalaxyCaps && list != kListDomeOrder && list != kListLumaCounts && list != kListGalaxyShuffle &&
        list != kListShopPrices) {
        return kNoCap;
    }
    const std::vector<std::uint32_t>& values = list == kListGalaxyCaps  ? galaxy_caps_
                                               : list == kListDomeOrder ? dome_order_
                                               : list == kListGalaxyShuffle ? galaxy_shuffle_
                                               : list == kListShopPrices ? shop_prices_
                                                                            : luma_counts_;
    return index < values.size() ? values[index] : kNoCap;
}

std::uint32_t Engine::seed_id() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return seed_id_;
}

State Engine::state() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

std::uint32_t Engine::goal_option() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return goal_;
}

std::size_t Engine::items_received() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return received_.size();
}

std::size_t Engine::items_applied(std::uint32_t file) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto it = applied_.find(file);
    return it == applied_.end() ? 0u : it->second;
}

bool Engine::location_valid(std::uint32_t index) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return !have_valid_ || valid_.count(index) != 0u;
}

void Engine::report_death() {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (connected_ && effective_option(Option::DeathLink) != 0) death_to_send_ = true;
}

bool Engine::take_death() {
    const std::lock_guard<std::mutex> lock(mutex_);
    const bool was = death_received_;
    death_received_ = false;
    return was;
}

bool Engine::shop_bought(std::uint32_t item) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    const std::uint32_t number = kShopFirst + item;
    if (number > kLocationLast) return false;
    return found_flags_[number] || checked_shop_.count(number) != 0u;
}

std::string Engine::shop_label(std::uint32_t item) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto it = scouted_.find(kShopFirst + item);
    if (it == scouted_.end()) return {};
    const std::string name = item_name_locked(it->second.item, it->second.player);
    // Whose item it is, always: "Power Star for Kudo" (the player's own name too), "Ocarina for Bob".
    return name + " for " + player_name_locked(it->second.player);
}

std::uint32_t Engine::shop_icon(std::uint32_t item) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto it = scouted_.find(kShopFirst + item);
    // Only this game's own items have a picture; the others of the multiworld show the Archipelago logo (0).
    if (it == scouted_.end() || it->second.player != my_slot_) return 0u;
    const std::int64_t number = it->second.item - static_cast<std::int64_t>(kItemBase);
    return number > 0 && number <= 0xFF ? static_cast<std::uint32_t>(number) : 0u;
}

void Engine::request_shop_hints() {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (connected_ && !shop_prices_.empty()) shop_hint_pending_ = true;
}

std::uint32_t Engine::star_class(std::uint32_t index) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto it = star_classes_.find(index);
    return it != star_classes_.end() ? it->second : kClassUnknown;
}

// ---- Client ----

Client::Client(Config config, std::filesystem::path state_folder, LogFn logger)
    : config_(config), log_(logger), engine_(std::move(config), std::move(state_folder), logger) {}

Client::~Client() {
    stop();
}

void Client::start() {
    if (thread_.joinable()) return;
    stop_ = false;
    thread_ = std::thread([this] { run(); });
}

void Client::stop() {
    stop_ = true;
    if (thread_.joinable()) thread_.join();
}

bool Client::sleep_unless_stopped(int ms) const {
    for (int waited = 0; waited < ms && !stop_; waited += 100) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return !stop_;
}

// Reads archipelago.ini again for its option.<name> lines (see parse_option_overrides).
void Client::read_overrides(bool announce) {
    if (config_.path.empty()) return;
    std::ifstream in(config_.path, std::ios::binary);
    if (!in) return;
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const auto overrides = parse_option_overrides(text);
    const std::size_t count = overrides.size();
    if (engine_.set_option_overrides(overrides) && (announce || count != 0u) && log_ != nullptr) {
        log_(("archipelago.ini: " + std::to_string(count) + " option override(s) in force; the game applies them at its next stage").c_str());
    }
}

void Client::run() {
    read_overrides(false);
    const auto say = [this](const std::string& text) {
        if (log_ != nullptr) log_(text.c_str());
    };
    const std::optional<net::WebSocketUrl> url = net::parse_websocket_url(config_.server);
    if (!url) {
        say("the server address \"" + config_.server + "\" is not valid (expected host:port)");
        engine_.add_notice("Archipelago: bad server address in archipelago.ini");
        return;
    }
    // An address without ws:// or wss:// is tried encrypted first (archipelago.gg only takes that), then plain (a
    // server hosted on this computer).
    std::vector<net::WebSocketUrl> candidates;
    if (url->explicit_scheme) {
        candidates.push_back(*url);
    } else {
        net::WebSocketUrl secure = *url;
        secure.secure = true;
        if (net::WebSocket::tls_available()) candidates.push_back(secure);
        candidates.push_back(*url);
    }
    const auto describe = [](const net::WebSocketUrl& u) {
        return std::string(u.secure ? "wss://" : "ws://") + u.host + ":" + u.port;
    };
    std::size_t preferred = 0;
    net::WebSocket socket;
    bool first = true;
    while (!stop_) {
        bool connected = false;
        std::string failures;
        for (std::size_t k = 0; k < candidates.size() && !connected && !stop_; ++k) {
            const std::size_t index = (preferred + k) % candidates.size();
            std::string error;
            if (first || k > 0) say("connecting to " + describe(candidates[index]));
            if (socket.connect(candidates[index], 8000, error)) {
                connected = true;
                preferred = index;
            } else {
                say("could not connect to " + describe(candidates[index]) + ": " + error);
                if (!failures.empty()) failures += "; ";
                failures += (candidates[index].secure ? "wss: " : "ws: ") + error;
            }
        }
        first = false;
        if (stop_) break;
        if (!connected) {
            engine_.on_connect_failed(failures);
            say("trying again in 10 seconds");
            if (!sleep_unless_stopped(10000)) break;
            continue;
        }
        say("the connection is open; waiting for the server's greeting");
        auto last_overrides = std::chrono::steady_clock::now();
        std::vector<std::string> out;
        bool refused = false;
        auto last_ping = std::chrono::steady_clock::now();
        while (!stop_ && socket.is_open()) {
            std::string message;
            const net::WebSocket::Event event = socket.receive(message, 100);
            if (event == net::WebSocket::Event::Closed) break;
            if (event == net::WebSocket::Event::Message) {
                engine_.on_message(message, out);
                if (engine_.state() == State::Failed) {
                    refused = true;
                    break;
                }
            }
            if (std::chrono::steady_clock::now() - last_overrides > std::chrono::seconds(5)) {
                last_overrides = std::chrono::steady_clock::now();
                read_overrides(true);
            }
            // A connection can die without any close (Wi-Fi dropped, computer asleep). The server pings about every
            // 20 s, so a quiet line is probed with our own ping, and given up after 60 s so the retry below starts.
            if (socket.idle_ms() > 20000 && std::chrono::steady_clock::now() - last_ping > std::chrono::seconds(10)) {
                last_ping = std::chrono::steady_clock::now();
                if (!socket.send_ping()) break;
            }
            if (socket.idle_ms() > 60000) {
                say("the server has been silent for 60 seconds; reconnecting");
                break;
            }
            engine_.poll_outgoing(out);
            bool sent = true;
            for (const std::string& packet : out) sent = sent && socket.send_text(packet);
            out.clear();
            if (!sent) break;
        }
        socket.close();
        engine_.on_disconnected();
        if (stop_) break;
        if (refused) {
            // A wrong slot or password stays wrong: try again rarely, in case the server's room was changed.
            if (!sleep_unless_stopped(30000)) break;
        } else {
            say("the connection to the server was lost; trying again in 5 seconds");
            if (!sleep_unless_stopped(5000)) break;
        }
        first = true;
    }
}

// ---- The host's one client ----

namespace {
std::mutex g_mutex;
std::filesystem::path g_settings_folder;  // set by start()
std::atomic<bool> g_spheres_allowed{true};
std::atomic<bool> g_guard_exit_registered{false};
Client* g_client = nullptr;  // never destroyed: its thread may still run when the process ends

std::mutex g_log_mutex;
std::ofstream g_log_file;
LogFn g_user_log = nullptr;

// One line to archipelago.log (with the time) and to the host's own log.
void log_sink(const char* text) {
    {
        const std::lock_guard<std::mutex> lock(g_log_mutex);
        if (g_log_file.is_open()) {
            const std::time_t now = std::time(nullptr);
            char stamp[16];
            std::tm local{};
#if defined(_WIN32)
            localtime_s(&local, &now);
#else
            localtime_r(&now, &local);
#endif
            std::strftime(stamp, sizeof(stamp), "%H:%M:%S", &local);
            g_log_file << stamp << "  " << text << '\n';
            g_log_file.flush();
        }
    }
    if (g_user_log != nullptr) g_user_log(text);
}

Client* client() {
    const std::lock_guard<std::mutex> lock(g_mutex);
    return g_client;
}

bool read_file(const std::filesystem::path& path, std::string& text) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    text.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return true;
}
}  // namespace

void start(const std::filesystem::path& settings_folder, LogFn log) {
    const std::lock_guard<std::mutex> lock(g_mutex);
    if (g_client != nullptr || settings_folder.empty()) return;
    g_user_log = log;
    g_settings_folder = settings_folder;
    {
        std::error_code error;
        std::filesystem::create_directories(settings_folder, error);
        const std::lock_guard<std::mutex> log_lock(g_log_mutex);
        g_log_file.open(settings_folder / "archipelago.log", std::ios::trunc);
    }
    log_sink(("Nebula Archipelago client for the game \"" + std::string(kGameName) + "\"; settings folder: " + settings_folder.string()).c_str());

    {
        // A guard file left by the last run: it stopped while the green spheres were on screen.
        std::error_code error;
        const std::filesystem::path guard = settings_folder / "sphere-guard.txt";
        const std::filesystem::path off = settings_folder / "green-spheres-off.txt";
        if (std::filesystem::exists(guard, error)) {
            std::filesystem::remove(guard, error);
            std::ofstream(off) << "The game stopped while the green spheres were shown in a dome, so they are off. Delete this file to try again.\n";
            log_sink("the last run stopped while the green spheres were on screen: they are off (delete green-spheres-off.txt to try again)");
        }
        if (std::filesystem::exists(off, error)) {
            g_spheres_allowed.store(false);
            log_sink(("green spheres in the domes: off, because " + off.string() + " exists (delete it to turn them back on)").c_str());
        }
    }

    std::vector<std::filesystem::path> candidates = {settings_folder / "archipelago.ini", settings_folder / "archipelago.ini.txt"};
    {
        std::error_code error;
        const std::filesystem::path here = std::filesystem::current_path(error);
        if (!error && here != settings_folder) {
            candidates.push_back(here / "archipelago.ini");
            candidates.push_back(here / "archipelago.ini.txt");
        }
    }
    std::string text;
    std::filesystem::path found;
    for (const std::filesystem::path& candidate : candidates) {
        if (read_file(candidate, text)) {
            found = candidate;
            log_sink(("read " + candidate.string()).c_str());
            break;
        }
        log_sink(("no file at " + candidate.string()).c_str());
    }
    if (found.empty()) {
        log_sink("no archipelago.ini found: the Archipelago client stays off (create the file in the settings folder above)");
        return;
    }
    {
        // `green_spheres=0` in archipelago.ini turns the spheres off.
        std::istringstream lines{normalize_text(text)};
        std::string line;
        while (std::getline(lines, line)) {
            const std::string trimmed = trim(line);
            const std::size_t equals = trimmed.find('=');
            if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == ';' || equals == std::string::npos) continue;
            if (lower(trim(std::string_view(trimmed).substr(0, equals))) == "green_spheres") {
                const std::string v = lower(trim(std::string_view(trimmed).substr(equals + 1u)));
                if (v == "0" || v == "false" || v == "no" || v == "off") {
                    g_spheres_allowed.store(false);
                    log_sink("green spheres in the domes: off, because archipelago.ini says green_spheres=0");
                }
            }
        }
    }
    Config config = parse_config(text);
    config.path = found.string();
    if (!config.enabled) {
        log_sink(("the Archipelago client stays off: " + config_problem(text)).c_str());
        return;
    }
    log_sink(("server " + config.server + ", slot \"" + config.slot + "\", password " + (config.password.empty() ? "none" : "set")).c_str());
    g_client = new Client(config, settings_folder / "archipelago", &log_sink);
    g_client->start();
}

bool spheres_allowed() noexcept {
    return g_spheres_allowed.load();
}

void sphere_guard_begin() {
    std::filesystem::path folder;
    {
        const std::lock_guard<std::mutex> lock(g_mutex);
        folder = g_settings_folder;
    }
    if (folder.empty()) return;
    std::error_code error;
    const std::filesystem::path guard = folder / "sphere-guard.txt";
    if (std::filesystem::exists(guard, error)) return;
    std::ofstream(guard) << "A dome with green spheres is on screen. If the game stops here, the spheres are turned off at the next start.\n";
    if (!g_guard_exit_registered.exchange(true)) {
        // A normal exit removes the file; a crash does not.
        static std::filesystem::path s_guard_path;
        s_guard_path = guard;
        std::atexit([] {
            std::error_code ignored;
            std::filesystem::remove(s_guard_path, ignored);
        });
    }
}

void sphere_guard_end() {
    std::filesystem::path folder;
    {
        const std::lock_guard<std::mutex> lock(g_mutex);
        folder = g_settings_folder;
    }
    if (folder.empty()) return;
    std::error_code error;
    std::filesystem::remove(folder / "sphere-guard.txt", error);
}

bool active() noexcept {
    return client() != nullptr;
}

State state() noexcept {
    Client* c = client();
    return c != nullptr ? c->engine().state() : State::Off;
}

std::uint32_t goal_option() noexcept {
    Client* c = client();
    return c != nullptr ? c->engine().goal_option() : kGoalEnding;
}

void report_location(std::uint32_t index) {
    if (Client* c = client()) c->engine().report_location(index);
}

void report_goal(std::uint32_t character) {
    if (Client* c = client()) c->engine().report_goal(character);
}

std::uint32_t next_item(std::uint32_t file) {
    Client* c = client();
    return c != nullptr ? c->engine().next_item(file) : 0u;
}

void item_applied(std::uint32_t file) {
    if (Client* c = client()) c->engine().item_applied(file);
}

std::uint32_t item_count(std::uint32_t item) noexcept {
    Client* c = client();
    return c != nullptr ? c->engine().item_count(item) : 0u;
}

bool option_set(std::uint32_t option) noexcept {
    Client* c = client();
    return c != nullptr && option < kOptionCount && c->engine().option_set(static_cast<Option>(option));
}

std::uint32_t option_value(std::uint32_t option) noexcept {
    Client* c = client();
    return c != nullptr && option < kOptionCount ? c->engine().option_value(static_cast<Option>(option)) : 0u;
}

std::uint32_t list_value(std::uint32_t list, std::uint32_t index) noexcept {
    Client* c = client();
    return c != nullptr ? c->engine().list_value(list, index) : Engine::kNoCap;
}

std::uint32_t seed_id() noexcept {
    Client* c = client();
    return c != nullptr ? c->engine().seed_id() : 0u;
}

void report_death() {
    if (Client* c = client()) c->engine().report_death();
}

bool take_death() {
    Client* c = client();
    return c != nullptr && c->engine().take_death();
}

std::uint32_t star_class(std::uint32_t index) noexcept {
    Client* c = client();
    return c != nullptr ? c->engine().star_class(index) : Engine::kClassUnknown;
}

bool shop_bought(std::uint32_t item) {
    Client* c = client();
    return c != nullptr && c->engine().shop_bought(item);
}

std::string shop_label(std::uint32_t item) {
    Client* c = client();
    return c != nullptr ? c->engine().shop_label(item) : std::string();
}

std::uint32_t shop_icon(std::uint32_t item) {
    Client* c = client();
    return c != nullptr ? c->engine().shop_icon(item) : 0u;
}

void request_shop_hints() {
    Client* c = client();
    if (c != nullptr) c->engine().request_shop_hints();
}

void game_log(const char* text) {
    if (client() == nullptr || text == nullptr) return;
    const std::string line = std::string("game: ") + text;
    log_sink(line.c_str());
}

std::size_t pop_notice(char* buffer, std::size_t capacity) {
    if (buffer == nullptr || capacity == 0u) return 0u;
    buffer[0] = '\0';
    Client* c = client();
    if (c == nullptr) return 0u;
    std::string text;
    std::uint32_t icon = 0u;
    if (!c->engine().pop_notice(text, &icon)) return 0u;
    // An item's notice starts with a byte 1 and the item number as the character '0' + number: the game takes it off.
    if (icon > 0u && icon < 0x40u) text = std::string{'\x01', static_cast<char>('0' + icon)} + text;
    const std::size_t length = std::min(text.size(), capacity - 1u);
    std::memcpy(buffer, text.data(), length);
    buffer[length] = '\0';
    return length;
}

void stop() {
    if (Client* c = client()) c->stop();
}

}  // namespace galaxy::archipelago
