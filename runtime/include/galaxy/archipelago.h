// SPDX-License-Identifier: GPL-3.0-only
// The Archipelago multiworld client (https://archipelago.gg): the host's side of the game's connection to an
// Archipelago server. The game reports the locations its save has reached (every star), and the goal; the host sends
// them to the server, keeps the items the server sends, and tells the game what it has (how many Power Stars, Grand
// Stars, Green Stars, the next 1-Up or Star Bits to hand over, the slot's options). It also turns what the server
// says into short notices the game shows on screen, and writes everything to archipelago.log.
//
// The connection is set up by archipelago.ini next to the settings:
//     server=archipelago.gg:38281
//     slot=Mario
//     password=
// Without that file the client stays off and the game plays as usual.
//
// The numbers shared with the Super Mario Galaxy PC Port APWorld (apworld/super_mario_galaxy_pc_port/ in this
// repository) are below: a location is kLocationBase + its number, an item kItemBase + its number.
#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "galaxy/json_lite.h"

namespace galaxy::archipelago {

inline constexpr std::uint32_t kLocationBase = 0x534D4700u;  // "SMG" and 0
inline constexpr std::uint32_t kItemBase = 0x534D4800u;
inline constexpr const char* kGameName = "Super Mario Galaxy PC Port";

// Item numbers (kItemBase + number); the game applies them (NebulaArchipelago).
inline constexpr std::uint32_t kItemOneUp = 1;
inline constexpr std::uint32_t kItemStarBits50 = 2;
inline constexpr std::uint32_t kItemPowerStar = 3;  // the three below are counted, not handed over one by one
inline constexpr std::uint32_t kItemGreenStar = 4;
inline constexpr std::uint32_t kItemGrandStar = 5;
// The traps: the game applies each one when it can (a hit while Mario is hurt, in a demo or invincible waits).
inline constexpr std::uint32_t kItemFireTrap = 6;      // Mario catches fire (and loses a health point)
inline constexpr std::uint32_t kItemShockTrap = 7;     // an electric shock: Mario is paralysed (and loses a health point)
inline constexpr std::uint32_t kItemFreezeTrap = 8;    // Mario is frozen
inline constexpr std::uint32_t kItemSpikeTrap = 9;     // a hit that hurts (a health point)
inline constexpr std::uint32_t kItemStarBitThief = 10;  // up to 50 Star Bits are taken
inline constexpr std::uint32_t kItemQuakeTrap = 11;     // the camera shakes for a while
inline constexpr std::uint32_t kItemConfusionTrap = 12; // the control stick is inverted for a while
inline constexpr std::uint32_t kItemRainbowStar = 13;  // Mario turns rainbow (invincible) for a while
// The transformations, when the slot makes them items (Option::TransformationItems): counted, not handed over. A power-up
// taken without its item does nothing.
inline constexpr std::uint32_t kItemBeeMushroom = 14;
inline constexpr std::uint32_t kItemBooMushroom = 15;
inline constexpr std::uint32_t kItemSpringMushroom = 16;
inline constexpr std::uint32_t kItemFireFlower = 17;
inline constexpr std::uint32_t kItemIceFlower = 18;
inline constexpr std::uint32_t kItemFlyingStar = 19;  // the Red Star (Flying Mario)
// More fillers, handed over one by one like the 1-Up Mushroom.
inline constexpr std::uint32_t kItemStarBits200 = 20;  // 200 Star Bits
inline constexpr std::uint32_t kItemOneUpBurst = 21;   // three 1-Up Mushrooms
inline constexpr std::uint32_t kItemFullHeal = 22;     // Mario's health filled up (waits while it is full)
// Mario's moves, when the slot makes them items (Option::MovementItems): counted, not handed over. A move without its item
// does nothing special (a jump stays a plain jump).
inline constexpr std::uint32_t kItemSpin = 23;
inline constexpr std::uint32_t kItemWallJump = 24;
inline constexpr std::uint32_t kItemLongJump = 25;
inline constexpr std::uint32_t kItemBackflip = 26;
inline constexpr std::uint32_t kItemSideFlip = 27;
inline constexpr std::uint32_t kItemTripleJump = 28;
inline constexpr std::uint32_t kItemUnknown = 0xFFFFu;  // an id this version does not know

// Location numbers: the star at that position (+1) of the APWorld's StarData.STARS, 1 to kLocationLast.
inline constexpr std::uint32_t kStarLocationLast = 121;
// Luigi's own save has its own stars: with the character option Luigi or Both, the star at number n is the location
// kLuigiOffset + n for him (n for Mario).
inline constexpr std::uint32_t kLuigiOffset = 300;
// The Observatory's Star Bit shop: its items are locations too, kShopFirst + n (n from 0), at most kShopMax of them. The
// price of each (in Star Bits) is slot_data's "shop_prices".
inline constexpr std::uint32_t kShopFirst = 201;
inline constexpr std::uint32_t kShopMax = 40;
inline constexpr std::uint32_t kShopLast = kShopFirst + kShopMax - 1u;
inline constexpr std::uint32_t kLocationLast = kLuigiOffset + kStarLocationLast;

// The goal the slot asks for (slot_data "options" "goal").
inline constexpr std::uint32_t kGoalEnding = 0;      // the ending seen (Bowser's Galaxy Reactor)
inline constexpr std::uint32_t kGoalPowerStars = 1;  // the number of Power Stars the slot asks for, whatever the ending
inline constexpr std::uint32_t kGoalEndingAndStars = 2;  // the ending seen, and every star that exists before it held
inline constexpr std::uint32_t kGoalAllStars = 3;        // the ending seen, and every star there is held (the Purple Coin stars too)

// Who plays (slot_data "options" "character").
inline constexpr std::uint32_t kCharacterMario = 0;
inline constexpr std::uint32_t kCharacterLuigi = 1;
inline constexpr std::uint32_t kCharacterBoth = 2;

// The slot's options, as slot_data's "options" object names them (the game asks by number).
enum class Option : std::uint32_t {
    Goal = 1,
    StarItems = 2,         // 1: the stars, Grand Stars and Green Stars the game counts are the items received
    StartObservatory = 3,  // 1: a new save starts in the Observatory
    Character = 4,         // 0 Mario, 1 Luigi: who plays a new save
    UnlockLuigi = 5,       // 1: Luigi can be chosen on any save file
    DomeShuffle = 6,       // 1: the domes' galaxies are in another order (slot_data "dome_order")
    StarsToFinish = 7,     // Power Stars for the goal NEBULA_AP_GOAL_POWER_STARS
    ExactRequirements = 8, // 1: each galaxy asks exactly the stars of slot_data "galaxy_caps" (more or less than the game's own)
    LumaCounts = 9,        // 1: the Hungry Lumas ask the Star Bits of slot_data "luma_counts"
    MusicShuffle = 12,     // 1: the stages' music is shuffled (by the multiworld's seed)
    EnemyShuffle = 14,     // 1: the stage's plain enemies are swapped for others (by the multiworld's seed)
    TransformationItems = 13,  // 1: each transformation (Bee, Boo, Spring, Fire, Ice, Flying) must be received to be used
    DeathLink = 11,        // 1: DeathLink: a life lost is sent to the others, and theirs are lives lost here
    StarColors = 10,       // 1: a star's colour tells the class of the item it hides (the client scouts the locations)
    ColorFirst = 16,       // 16 to 39: the colours of Mario's and Luigi's parts (see the APWorld's Options.py)
    ColorLast = 39,
    MovementItems = 40,    // 1: each move (Spin, Wall Jump, Long Jump, Backflip, Side Flip, Triple Jump) must be received to be used
    IncludeComets = 41,    // 0 (given): the comet stars are not locations: the star select hides them
    IncludePurple = 42,    // 0 (given): the Purple Coin stars are not locations: the star select hides them
    Last = 42,
};
inline constexpr std::uint32_t kOptionCount = static_cast<std::uint32_t>(Option::Last) + 1u;
// The name of an option in slot_data's "options" object; null for a number that is not one.
[[nodiscard]] const char* option_name(std::uint32_t option) noexcept;

struct Config {
    bool enabled = false;
    std::string server;  // "host:port" or "ws://host:port"
    std::string slot;
    std::string password;
    std::string path;  // the archipelago.ini it was read from ("" when unknown): read again while the game runs, for the overrides
};

// archipelago.ini can also change a few cosmetic options of the slot without a new multiworld, with lines
//     option.<name>=<value>        (a number, or on/off/true/false/yes/no)
// for star_colors, death_link, music_shuffle, enemy_shuffle, skybox_shuffle and color_0 to color_23 (the options the
// multiworld's logic does not depend on). They win over the slot's own values and are read again every few seconds while
// the game runs; the game applies them at the next stage it loads. Other names are ignored.
[[nodiscard]] std::map<std::uint32_t, std::int64_t> parse_option_overrides(std::string_view text);

// archipelago.ini: `key=value` lines, `#` and `;` start comments. `enabled=0` switches the client off while keeping the
// file. A file without server or slot leaves it off. A byte-order mark (Notepad adds one) is ignored, and a file saved
// as UTF-16 is read as text.
[[nodiscard]] Config parse_config(std::string_view text);
// Why parse_config left the client off ("" when it is on): for the log.
[[nodiscard]] std::string config_problem(std::string_view text);

enum class State : std::uint32_t {
    Off = 0,
    Connecting = 1,
    Connected = 2,
    Failed = 3,  // the server refused the slot (wrong name or password): shown in the log and on screen
};

using LogFn = void (*)(const char* text);

// The protocol: what the server says and what the client answers, apart from the socket (the tests drive it with
// messages; Client connects it to a real one). Thread-safe: the game's thread calls the report/item functions while
// the connection's thread calls the others.
class Engine {
public:
    Engine(Config config, std::filesystem::path state_folder, LogFn log);

    // One text message from the server (a JSON array of commands); the messages to send back are appended to `out`.
    void on_message(std::string_view text, std::vector<std::string>& out);
    // Called regularly: the messages for what the game reported since (new locations, the goal) once connected.
    void poll_outgoing(std::vector<std::string>& out);
    // The socket closed or could not open.
    void on_disconnected();
    // The socket could not open: `reason` is what the system said.
    void on_connect_failed(const std::string& reason);

    // The game's side.
    void report_location(std::uint32_t index);
    // The goal of one character (0 Mario, 1 Luigi) is reached in a save file. The slot is done when the goal of every
    // character it plays is (remembered across runs, in the slot's state file).
    void report_goal(std::uint32_t character = 0);
    // The next item the server sent that this file has not applied and the game hands over one by one (a 1-Up, Star
    // Bits): its number, 0 when none (kItemUnknown for an id this version does not know: apply nothing, but
    // acknowledge). Items the game only counts (stars) are skipped.
    [[nodiscard]] std::uint32_t next_item(std::uint32_t file);
    // This file applied that item.
    void item_applied(std::uint32_t file);
    // How many of that item (kItem*) the server has sent in all: the game's counts of stars.
    [[nodiscard]] std::uint32_t item_count(std::uint32_t item) const;
    // The slot's option (whether the server said it), a number of one of the slot's lists (kNoCap when absent), and a
    // number that tells one multiworld (and slot) from another.
    [[nodiscard]] bool option_set(Option option) const;
    // Replaces the overrides of archipelago.ini (parse_option_overrides); returns whether they changed.
    bool set_option_overrides(std::map<std::uint32_t, std::int64_t> overrides);
    [[nodiscard]] std::uint32_t option_value(Option option) const;
    [[nodiscard]] std::uint32_t list_value(std::uint32_t list, std::uint32_t index) const;
    [[nodiscard]] std::uint32_t seed_id() const;
    // The oldest notice for the player (a short line); false when none waits.
    // `icon`: the number (kItem*) of the item the notice is about when one of ours was received, else 0 (the logo).
    [[nodiscard]] bool pop_notice(std::string& text, std::uint32_t* icon = nullptr);
    // A notice made by the host itself (the game, the diagnostics).
    void add_notice(const std::string& text);

    [[nodiscard]] State state() const;
    [[nodiscard]] std::uint32_t goal_option() const;
    [[nodiscard]] std::size_t items_received() const;
    [[nodiscard]] std::size_t items_applied(std::uint32_t file) const;
    [[nodiscard]] bool location_valid(std::uint32_t index) const;
    // The class of the item a location holds, from the server's LocationInfo: kClassUnknown until it answered (or when
    // the option star_colors is off), else kClassFiller, kClassProgression, kClassUseful or kClassTrap.
    static constexpr std::uint32_t kClassUnknown = 0xFFFFFFFFu;
    static constexpr std::uint32_t kClassFiller = 0;
    static constexpr std::uint32_t kClassProgression = 1;
    static constexpr std::uint32_t kClassUseful = 2;
    static constexpr std::uint32_t kClassTrap = 3;
    [[nodiscard]] std::uint32_t star_class(std::uint32_t index) const;
    // DeathLink: the game lost a life (sent when the option is on and the death was not itself a DeathLink's), and
    // take_death: another player died, once (true clears it).
    void report_death();
    [[nodiscard]] bool take_death();
    // The shop: whether the shop item (0-based) was bought (reported by this game, or checked on the server), and what it
    // holds as a line for the shelf ("Power Star for Mario", "an item" while the server has not answered). Empty when
    // the server did not tell yet.
    [[nodiscard]] bool shop_bought(std::uint32_t item) const;
    [[nodiscard]] std::string shop_label(std::uint32_t item) const;
    // The item number (kItem* - kItemBase) of this game's own item on the shelf, or 0: another player's item (or one with no picture).
    [[nodiscard]] std::uint32_t shop_icon(std::uint32_t item) const;
    // The shop is open: its unbought shelves become hints for the players concerned (sent with the next outgoing packets).
    void request_shop_hints();

    static constexpr std::uint32_t kNoCap = 0xFFFFFFFFu;
    // The lists slot_data carries: "galaxy_caps" (by StarData.GALAXIES' index: the most Power Stars the galaxy asks)
    // and "dome_order" (by dome number - 1: the place, minus 1, at which the dome opens).
    static constexpr std::uint32_t kListGalaxyCaps = 1;
    static constexpr std::uint32_t kListDomeOrder = 2;
    // "luma_counts": the Star Bits each Hungry Luma asks, in the order of the domes' Lumas (Sweet Sweet, Sling Pod, Drip
    // Drop, Bigmouth, Sand Spiral, Snow Cap) and last Boo's Boneyard's (the Gateway Galaxy's).
    static constexpr std::uint32_t kListLumaCounts = 3;
    // "galaxy_shuffle": by galaxy slot (StarData.GALAXIES' index), the index of the galaxy that stands in the slot.
    static constexpr std::uint32_t kListGalaxyShuffle = 4;
    // "shop_prices": the Star Bits each shop item (kShopFirst + index) costs; its length is the number of items.
    static constexpr std::uint32_t kListShopPrices = 5;

private:
    void log(const std::string& text) const;
    void handle_command(const json::Value& command, std::vector<std::string>& out);
    void handle_connected(const json::Value& command, std::vector<std::string>& out);
    void handle_print(const json::Value& command);
    void notice_locked(const std::string& text, std::uint32_t icon = 0u);
    [[nodiscard]] std::string player_name_locked(std::int64_t slot) const;
    [[nodiscard]] std::string item_name_locked(std::int64_t item, std::int64_t receiving_slot) const;
    // The name of a location of `slot`'s game (empty when the data package did not name it).
    [[nodiscard]] std::string location_name_locked(std::int64_t location, std::int64_t slot) const;
    void load_state_locked();
    void save_state_locked();
    void load_cache();
    void save_cache_locked() const;
    void forget_slot_locked();
    [[nodiscard]] std::filesystem::path cache_path() const;
    [[nodiscard]] std::filesystem::path state_path_locked() const;

    const Config config_;
    const std::filesystem::path state_folder_;
    const LogFn log_;
    const std::string uuid_;

    mutable std::mutex mutex_;
    State state_ = State::Connecting;
    std::string seed_name_;
    std::uint32_t goal_ = kGoalEnding;
    std::int64_t my_slot_ = -1;
    bool connected_ = false;
    bool announced_failure_ = false;
    std::vector<std::uint32_t> received_;           // item ids, in the server's order
    std::map<std::uint32_t, std::size_t> applied_;  // per save file: how many of `received_` it applied
    std::vector<std::uint32_t> found_;              // locations the game reported, in order
    std::vector<bool> found_flags_;                 // by number
    std::size_t sent_count_ = 0;                    // how many of `found_` this connection sent
    std::uint32_t goals_done_ = 0;  // bit 0: Mario's goal reached, bit 1: Luigi's
    bool goal_reported_ = false;
    bool goal_sent_ = false;
    bool death_to_send_ = false;
    bool shop_hint_pending_ = false;
    bool death_received_ = false;
    double last_death_time_ = 0.0;  // the time of the last DeathLink sent or received (a bounce comes back to its sender)
    bool have_valid_ = false;
    std::set<std::uint32_t> valid_;                  // location numbers of this slot (missing and checked)
    std::map<std::uint32_t, std::int64_t> options_;  // by Option
    // The value of an option as the game sees it (an override of archipelago.ini first, else the slot's, else 0). Hold mutex_.
    [[nodiscard]] std::int64_t effective_option(Option option) const;
    std::map<std::uint32_t, std::int64_t> overrides_;  // by Option: archipelago.ini's option.<name> lines, they win
    std::map<std::uint32_t, std::uint32_t> star_classes_;  // location number -> kClass*, from LocationInfo
    std::vector<std::uint32_t> galaxy_caps_;
    std::vector<std::uint32_t> dome_order_;
    std::vector<std::uint32_t> luma_counts_;
    std::vector<std::uint32_t> galaxy_shuffle_;
    std::vector<std::uint32_t> shop_prices_;
    std::set<std::uint32_t> checked_shop_;  // shop locations known checked (the server's, and ours)
    struct Scouted {
        std::int64_t item = 0;
        std::int64_t player = 0;
    };
    std::map<std::uint32_t, Scouted> scouted_;  // location number -> the item it holds, from LocationInfo (shop only)
    std::uint32_t seed_id_ = 0;
    std::vector<std::string> room_games_;
    std::map<std::int64_t, std::string> player_names_;  // slot -> alias
    std::map<std::int64_t, std::string> slot_games_;    // slot -> game
    std::map<std::string, std::map<std::int64_t, std::string>> item_names_;  // game -> item id -> name
    std::map<std::string, std::map<std::int64_t, std::string>> location_names_;  // game -> location id -> name
    std::deque<std::string> notices_;
    std::deque<std::uint32_t> notice_icons_;
    bool data_package_requested_ = false;
};

// Engine on a real connection, in its own thread: connects, reconnects when the connection drops.
class Client {
public:
    Client(Config config, std::filesystem::path state_folder, LogFn log);
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    ~Client();

    void start();
    void stop();
    [[nodiscard]] Engine& engine() { return engine_; }

private:
    void run();
    [[nodiscard]] bool sleep_unless_stopped(int ms) const;
    void read_overrides(bool announce);

    const Config config_;
    const LogFn log_;
    Engine engine_;
    std::atomic<bool> stop_{false};
    std::thread thread_;
};

// ---- The host's one client ----

// Starts writing archipelago.log in `settings_folder` (it says what was looked for and found), reads archipelago.ini
// (or archipelago.ini.txt, which Windows makes of a file saved as .txt) there, and starts the client when it names a
// server and a slot. `log` receives one line at a time as well. Calling it again does nothing.
void start(const std::filesystem::path& settings_folder, LogFn log);
[[nodiscard]] bool active() noexcept;  // the client was started
[[nodiscard]] State state() noexcept;
[[nodiscard]] std::uint32_t goal_option() noexcept;
void report_location(std::uint32_t index);
void report_goal(std::uint32_t character = 0);
[[nodiscard]] std::uint32_t next_item(std::uint32_t file);
void item_applied(std::uint32_t file);
[[nodiscard]] std::uint32_t item_count(std::uint32_t item) noexcept;
[[nodiscard]] bool option_set(std::uint32_t option) noexcept;
[[nodiscard]] std::uint32_t option_value(std::uint32_t option) noexcept;
[[nodiscard]] std::uint32_t list_value(std::uint32_t list, std::uint32_t index) noexcept;
[[nodiscard]] std::uint32_t seed_id() noexcept;
[[nodiscard]] std::uint32_t star_class(std::uint32_t index) noexcept;
void report_death();
[[nodiscard]] bool take_death();
[[nodiscard]] bool shop_bought(std::uint32_t item);
[[nodiscard]] std::string shop_label(std::uint32_t item);
[[nodiscard]] std::uint32_t shop_icon(std::uint32_t item);
void request_shop_hints();
// A line the game writes to archipelago.log (and may show).
void game_log(const char* text);
// The oldest notice; the length copied into `buffer` (always terminated), 0 when none waits.
[[nodiscard]] std::size_t pop_notice(char* buffer, std::size_t capacity);
void stop();

// The green spheres in the domes (a galaxy without a miniature of its own is shown as the Observatory's green sphere) crashed
// the game once. They are on unless archipelago.ini says `green_spheres=0` or the last run stopped while they were on screen:
// the game calls sphere_guard_begin() when a dome holds one and sphere_guard_end() when it leaves the dome; a file in the
// settings folder (sphere-guard.txt) stays between the two (and is removed when the process exits normally), so a crash
// there turns the spheres off from the next start (green-spheres-off.txt; delete it to try again).
[[nodiscard]] bool spheres_allowed() noexcept;
void sphere_guard_begin();
void sphere_guard_end();

}  // namespace galaxy::archipelago
