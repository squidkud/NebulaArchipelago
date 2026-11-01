// The Archipelago client's protocol (galaxy/archipelago.h), driven with the server's messages.
#include "galaxy/archipelago.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {
int s_failures = 0;
void expect(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++s_failures;
    }
}

namespace ap = galaxy::archipelago;
namespace json = galaxy::json;

std::vector<std::string> s_log;
void log_line(const char* text) {
    s_log.emplace_back(text);
}

// The first command of a message the client sent.
json::Value first_command(const std::string& text) {
    const auto parsed = json::parse(text);
    if (!parsed || !parsed->is_array() || parsed->items().empty()) return json::Value();
    return parsed->items()[0];
}

const char* kRoomInfo = R"([{"cmd":"RoomInfo","version":{"major":0,"minor":5,"build":1,"class":"Version"},"seed_name":"Seed 42!","games":["Super Mario Galaxy PC Port"]}])";
const char* kConnected = R"([{"cmd":"Connected","team":0,"slot":1,"players":[{"team":0,"slot":1,"alias":"Mario","name":"Mario"},{"team":0,"slot":2,"alias":"Bob","name":"Bob"}],"slot_info":{"1":{"name":"Mario","game":"Super Mario Galaxy PC Port","type":1,"group_members":[]},"2":{"name":"Bob","game":"Other Game","type":1,"group_members":[]}},"missing_locations":[@L1,@L2,@L5,@L100,2000000000],"checked_locations":[],"slot_data":{"goal":1,"options":{"goal":1,"star_items":1,"character":1,"stars_to_finish":30,"star_colors":1,"death_link":1,"music_shuffle":1,"color_3":7},"galaxy_caps":[0,1,3],"dome_order":[2,1],"galaxy_shuffle":[2,0,1]}}])";

// kConnected with the location ids filled in (they are kLocationBase + number).
std::string connected_message() {
    std::string text = kConnected;
    for (const unsigned number : {1u, 2u, 5u, 100u}) {
        const std::string key = "@L" + std::to_string(number);
        const std::size_t at = text.find(key);
        if (at != std::string::npos) text.replace(at, key.size(), std::to_string(galaxy::archipelago::kLocationBase + number));
    }
    return text;
}

std::string received(int index, std::initializer_list<std::uint32_t> items) {
    std::string text = R"([{"cmd":"ReceivedItems","index":)" + std::to_string(index) + R"(,"items":[)";
    bool first = true;
    for (const std::uint32_t id : items) {
        if (!first) text += ',';
        first = false;
        text += R"({"item":)" + std::to_string(id) + R"(,"location":1,"player":1,"flags":0})";
    }
    return text + "]}]";
}
}  // namespace

int main() {
    const std::filesystem::path folder = std::filesystem::temp_directory_path() / "nebula_archipelago_test";
    std::filesystem::remove_all(folder);

    // A slot that plays both characters is done when both have reached the goal (also across runs).
    {
        std::string both = connected_message();
        const std::size_t at = both.find("\"character\":1");
        both.replace(at, 13, "\"character\":2");
        ap::Config config;
        config.enabled = true;
        config.server = "localhost:38281";
        config.slot = "Mario";
        const std::filesystem::path both_folder = std::filesystem::temp_directory_path() / "nebula_archipelago_test_both";
        std::filesystem::remove_all(both_folder);
        std::vector<std::string> out;
        {
            ap::Engine engine(config, both_folder, log_line);
            engine.on_message(kRoomInfo, out);
            engine.on_message(both, out);
            out.clear();
            engine.report_goal(0);
            engine.poll_outgoing(out);
            expect(out.empty(), "Mario alone does not finish a slot that plays both");
        }
        {
            ap::Engine engine(config, both_folder, log_line);
            engine.on_message(kRoomInfo, out);
            engine.on_message(both, out);
            out.clear();
            engine.report_goal(1);
            engine.poll_outgoing(out);
            expect(out.size() == 1u && first_command(out.at(0)).find("status")->as_int() == 30,
                   "Luigi's goal, with Mario's from an earlier run, finishes it");
        }
        std::filesystem::remove_all(both_folder);
    }

    // Option overrides in archipelago.ini.
    {
        const auto o = ap::parse_option_overrides(
            "server=a:1\n# comment\noption.star_colors = on\nOPTION.color_3=255\noption.death_link=0\n"
            "option.goal=2\noption.movement_items=1\noption.music_shuffle=maybe\noption.enemy_shuffle=0x1\n");
        expect(o.at(static_cast<std::uint32_t>(ap::Option::StarColors)) == 1, "an override can be on/off");
        expect(o.at(19u) == 255, "a colour override is a number");
        expect(o.at(static_cast<std::uint32_t>(ap::Option::DeathLink)) == 0, "off is 0");
        expect(o.count(static_cast<std::uint32_t>(ap::Option::Goal)) == 0, "the goal cannot be overridden");
        expect(o.count(static_cast<std::uint32_t>(ap::Option::MovementItems)) == 0, "logic options cannot be overridden");
        expect(o.count(static_cast<std::uint32_t>(ap::Option::MusicShuffle)) == 0, "a value that is not a number is ignored");
        expect(o.at(static_cast<std::uint32_t>(ap::Option::EnemyShuffle)) == 1, "hexadecimal is read");
        expect(ap::parse_option_overrides("").empty(), "no overrides in an empty file");
    }

    // The configuration file.
    {
        const ap::Config on = ap::parse_config("# comment\nserver = archipelago.gg:38281\nSlot=Mario\npassword=secret word\n");
        expect(on.enabled && on.server == "archipelago.gg:38281" && on.slot == "Mario" && on.password == "secret word",
               "a complete file");
        expect(!ap::parse_config("server=a:1\nslot=b\nenabled=0\n").enabled, "enabled=0 switches it off");
        expect(!ap::parse_config("server=a:1\n").enabled, "no slot leaves it off");
        expect(!ap::parse_config("").enabled, "an empty file leaves it off");
        expect(ap::parse_config("server=a:1\nslot=b\nenabled=yes\n").enabled, "enabled=yes");
    }

    ap::Config config;
    config.enabled = true;
    config.server = "localhost:38281";
    config.slot = "Mario";
    config.password = "pw";

    {
        ap::Engine engine(config, folder, log_line);
        std::vector<std::string> out;

        // Before the server speaks, nothing goes out and the game has nothing.
        engine.report_location(5);
        engine.poll_outgoing(out);
        expect(out.empty(), "nothing is sent before the connection is made");
        expect(engine.next_item(0) == 0, "no item yet");
        expect(engine.state() == ap::State::Connecting, "connecting");

        // RoomInfo is answered with Connect.
        engine.on_message(kRoomInfo, out);
        expect(out.size() == 1u, "one answer to RoomInfo");
        const json::Value connect = first_command(out.at(0));
        expect(connect.find("cmd") && connect.find("cmd")->as_string() == "Connect", "it is a Connect");
        expect(connect.find("game")->as_string() == "Super Mario Galaxy PC Port", "the game's name");
        expect(connect.find("name")->as_string() == "Mario" && connect.find("password")->as_string() == "pw", "slot and password");
        expect(connect.find("items_handling")->as_int() == 7, "all items are sent, own ones included");
        expect(connect.find("slot_data")->as_bool(), "slot data is asked for");
        expect(connect.find("version")->find("class")->as_string() == "Version", "a version object");
        expect(!connect.find("uuid")->as_string().empty(), "an uuid");
        out.clear();

        // Connected: the goal and options come from the slot data, the data package is asked for, and what was found
        // before goes out at once, apart from the stars this multiworld does not use.
        engine.on_message(connected_message(), out);
        expect(engine.state() == ap::State::Connected, "connected");
        expect(out.size() == 3u && first_command(out.at(2)).find("cmd")->as_string() == "GetDataPackage", "the data package is asked for");
        expect(first_command(out.at(2)).find("games")->items().size() == 2u, "for both games of the room");
        expect(first_command(out.at(1)).find("cmd")->as_string() == "ConnectUpdate" &&
                   first_command(out.at(1)).find("tags")->items().at(0).as_string() == "DeathLink",
               "the DeathLink tag is added");
        // star_colors: the slot's locations are scouted, and the classes of the items come from LocationInfo's flags.
        const json::Value scouts = first_command(out.at(0));
        expect(scouts.find("cmd")->as_string() == "LocationScouts" && scouts.find("locations")->items().size() == 4u, "the four locations are scouted");
        expect(scouts.find("create_as_hint")->as_int() == 0, "without hints");
        out.clear();
        expect(engine.star_class(1) == ap::Engine::kClassUnknown, "no class before the answer");
        engine.on_message(R"([{"cmd":"LocationInfo","locations":[{"item":1,"location":)" + std::to_string(ap::kLocationBase + 1) +
                              R"(,"player":2,"flags":1},{"item":1,"location":)" + std::to_string(ap::kLocationBase + 2) +
                              R"(,"player":1,"flags":2},{"item":1,"location":)" + std::to_string(ap::kLocationBase + 5) +
                              R"(,"player":1,"flags":4},{"item":1,"location":)" + std::to_string(ap::kLocationBase + 100) +
                              R"(,"player":1,"flags":0},{"item":1,"location":7,"player":1,"flags":1}]}])",
                          out);
        expect(engine.star_class(1) == ap::Engine::kClassProgression && engine.star_class(2) == ap::Engine::kClassUseful, "progression and useful");
        expect(engine.star_class(5) == ap::Engine::kClassTrap && engine.star_class(100) == ap::Engine::kClassFiller, "trap and filler");
        expect(engine.star_class(7) == ap::Engine::kClassUnknown, "a location that is not a star stays unknown");
        out.clear();
        expect(engine.goal_option() == ap::kGoalPowerStars, "the goal from the slot data");
        expect(engine.option_set(ap::Option::StarItems) && engine.option_value(ap::Option::StarItems) == 1u, "an option from the slot data");
        expect(engine.option_value(ap::Option::Character) == 1u && engine.option_value(ap::Option::StarsToFinish) == 30u, "more options");
        expect(!engine.option_set(ap::Option::DomeShuffle), "an option the server did not give");
        expect(engine.option_value(ap::Option::MusicShuffle) == 1u && engine.option_value(ap::Option::DeathLink) == 1u, "the music and DeathLink options");
        expect(engine.option_value(static_cast<ap::Option>(19)) == 7u, "a colour option");
        expect(engine.list_value(ap::Engine::kListGalaxyCaps, 2) == 3u && engine.list_value(ap::Engine::kListGalaxyCaps, 3) == ap::Engine::kNoCap,
               "the galaxy caps");
        expect(engine.list_value(ap::Engine::kListDomeOrder, 0) == 2u, "the dome order");
        expect(engine.list_value(ap::Engine::kListGalaxyShuffle, 0) == 2u && engine.list_value(ap::Engine::kListGalaxyShuffle, 2) == 1u, "the galaxy shuffle");
        expect(engine.list_value(ap::Engine::kListGalaxyShuffle, 3) == ap::Engine::kNoCap, "no shuffle past the list");
        expect(engine.seed_id() != 0u, "a seed number");
        expect(engine.location_valid(100) && !engine.location_valid(7), "the slot's locations");
        std::string notice;
        expect(engine.pop_notice(notice) && notice == "Connected to Archipelago as Mario", "a notice of the connection");
        expect(!engine.pop_notice(notice), "only one");
        engine.report_location(1);
        engine.report_location(1);  // a repeat
        engine.report_location(100);
        engine.report_location(7);    // not in this multiworld
        engine.report_location(0);    // not a location
        engine.report_location(121);  // beyond the last
        engine.poll_outgoing(out);
        expect(out.size() == 1u, "one LocationChecks message");
        const json::Value checks = first_command(out.at(0));
        expect(checks.find("cmd")->as_string() == "LocationChecks", "it is LocationChecks");
        expect(checks.find("locations")->items().size() == 3u, "the earlier report and the new ones once each, not the unused star");
        expect(checks.find("locations")->items()[0].as_int() == ap::kLocationBase + 5, "location 5 as reported before the connection");
        expect(checks.find("locations")->items()[1].as_int() == ap::kLocationBase + 1, "location 1");
        expect(checks.find("locations")->items()[2].as_int() == ap::kLocationBase + 100, "location 100");
        out.clear();
        engine.poll_outgoing(out);
        expect(out.empty(), "nothing is sent twice");

        // DeathLink: a life lost goes out as a Bounce; ours coming back does not count, another player's does, once.
        engine.report_death();
        engine.poll_outgoing(out);
        expect(out.size() == 1u && first_command(out.at(0)).find("cmd")->as_string() == "Bounce", "a life lost is a Bounce");
        const json::Value bounce = first_command(out.at(0));
        expect(bounce.find("tags")->items().at(0).as_string() == "DeathLink" && bounce.find("data")->find("source")->as_string() == "Mario",
               "tagged DeathLink, from this slot");
        const double sent_time = bounce.find("data")->find("time")->as_double();
        out.clear();
        engine.on_message(R"([{"cmd":"Bounce","tags":["DeathLink"],"data":{"time":)" + std::to_string(sent_time) + R"(,"source":"Mario","cause":"x"}}])", out);
        expect(!engine.take_death(), "our own death coming back is ignored");
        engine.on_message(R"([{"cmd":"Bounce","tags":["Other"],"data":{"time":5.5,"source":"Bob"}}])", out);
        expect(!engine.take_death(), "a bounce without the tag is ignored");
        engine.on_message(R"([{"cmd":"Bounce","tags":["DeathLink"],"data":{"time":5.5,"source":"Bob","cause":"Bob ate a mushroom"}}])", out);
        expect(engine.take_death() && !engine.take_death(), "another player's death arrives once");

        // The goal (this slot plays Luigi: character 1).
        engine.report_goal(0);
        engine.poll_outgoing(out);
        expect(out.empty(), "Mario's goal does not finish a Luigi slot");
        engine.report_goal(1);
        engine.poll_outgoing(out);
        expect(out.size() == 1u && first_command(out.at(0)).find("status")->as_int() == 30, "StatusUpdate with CLIENT_GOAL");
        out.clear();
        engine.report_goal(1);
        engine.poll_outgoing(out);
        expect(out.empty(), "the goal is sent once");

        // Items.
        (void)engine.pop_notice(notice);
        engine.on_message(received(0, {ap::kItemBase + 1, ap::kItemBase + 2, 12345}), out);
        expect(out.empty() && engine.items_received() == 3u, "three items kept");
        expect(engine.next_item(0) == ap::kItemOneUp, "the first is a 1-Up");
        expect(engine.next_item(0) == ap::kItemOneUp, "it stays until applied");
        engine.item_applied(0);
        expect(engine.next_item(0) == ap::kItemStarBits50, "then the Star Bits");
        expect(engine.next_item(1) == ap::kItemOneUp, "another file has its own place");
        engine.item_applied(0);
        expect(engine.next_item(0) == ap::kItemUnknown, "an id this version does not know");
        engine.item_applied(0);
        expect(engine.next_item(0) == 0, "all applied");
        engine.item_applied(0);
        expect(engine.items_applied(0) == 3u, "applying with nothing waiting changes nothing");

        // More items arrive after the first three; a gap asks for a resend.
        engine.on_message(received(3, {ap::kItemBase + 1}), out);
        expect(engine.next_item(0) == ap::kItemOneUp, "the new item");
        out.clear();
        engine.on_message(received(9, {ap::kItemBase + 1}), out);
        expect(out.size() == 1u && first_command(out.at(0)).find("cmd")->as_string() == "Sync", "a gap asks for a Sync");
        out.clear();
        // The full list again (a reconnection): the applied counts stay.
        engine.on_message(received(0, {ap::kItemBase + 1, ap::kItemBase + 2, 12345, ap::kItemBase + 1}), out);
        expect(engine.items_received() == 4u && engine.items_applied(0) == 3u, "a full list replaces the old one");
        expect(engine.next_item(0) == ap::kItemOneUp, "only the new item is left");
    }

    // What was applied survives a restart (same seed and slot), and is separate for another seed.
    {
        ap::Engine engine(config, folder, log_line);
        std::vector<std::string> out;
        // Before the server has said anything, the last connection's answers stand (the game starts first).
        expect(engine.option_value(ap::Option::StarItems) == 1u && engine.list_value(ap::Engine::kListGalaxyCaps, 1) == 1u &&
                   engine.item_count(ap::kItemOneUp) == 2u && engine.seed_id() != 0u && engine.location_valid(100) && !engine.location_valid(7),
               "the last answers are kept for the next start");
        expect(engine.items_applied(0) == 3u && engine.next_item(0) == ap::kItemOneUp, "and so is what a file applied");
        engine.on_message(kRoomInfo, out);
        engine.on_message(connected_message(), out);
        engine.on_message(received(0, {ap::kItemBase + 1, ap::kItemBase + 2, 12345, ap::kItemBase + 1}), out);
        expect(engine.items_applied(0) == 3u, "the count is read back after a restart");
        expect(engine.next_item(0) == ap::kItemOneUp, "and the fourth item waits");

        ap::Engine other(config, folder, log_line);
        std::vector<std::string> out2;
        other.on_message(R"([{"cmd":"RoomInfo","seed_name":"Another"}])", out2);
        other.on_message(connected_message(), out2);
        other.on_message(received(0, {ap::kItemBase + 1}), out2);
        expect(other.items_applied(0) == 0u && other.next_item(0) == ap::kItemOneUp, "another seed starts at zero");
    }

    // A refusal, and a dropped connection.
    {
        s_log.clear();
        ap::Engine engine(config, folder, log_line);
        std::vector<std::string> out;
        engine.on_message(kRoomInfo, out);
        engine.on_message(R"([{"cmd":"ConnectionRefused","errors":["InvalidSlot","InvalidPassword"]}])", out);
        expect(engine.state() == ap::State::Failed, "a refusal is a failure");
        expect(!s_log.empty() && s_log.back().find("InvalidSlot, InvalidPassword") != std::string::npos, "the reasons are logged");
        engine.on_disconnected();
        expect(engine.state() == ap::State::Failed, "a refusal stays until the next try");
        engine.on_message(kRoomInfo, out);
        engine.on_message(connected_message(), out);
        expect(engine.state() == ap::State::Connected, "the next try can succeed");
        engine.on_disconnected();
        expect(engine.state() == ap::State::Connecting, "a dropped connection is connecting again");
        out.clear();
        engine.report_location(5);
        engine.poll_outgoing(out);
        expect(out.empty(), "nothing is sent while disconnected");
        // A new connection sends everything found, including what an earlier connection sent.
        engine.on_message(kRoomInfo, out);
        engine.on_message(connected_message(), out);
        out.clear();
        engine.poll_outgoing(out);
        expect(out.size() == 1u && first_command(out.at(0)).find("locations")->items().size() == 1u,
               "locations found are sent again on a new connection");
    }

    // The stars are counted, not handed over; what the server said about items becomes notices.
    {
        ap::Engine engine(config, folder, log_line);
        std::vector<std::string> out;
        engine.on_message(kRoomInfo, out);
        engine.on_message(connected_message(), out);
        std::string notice;
        while (engine.pop_notice(notice)) {
        }
        engine.on_message(received(0, {ap::kItemBase + 3, ap::kItemBase + 5, ap::kItemBase + 3, ap::kItemBase + 1, ap::kItemBase + 4}), out);
        expect(engine.item_count(ap::kItemPowerStar) == 2u && engine.item_count(ap::kItemGrandStar) == 1u &&
                   engine.item_count(ap::kItemGreenStar) == 1u && engine.item_count(ap::kItemOneUp) == 1u,
               "the items are counted");
        expect(engine.next_item(0) == ap::kItemOneUp, "the counted items are passed, the 1-Up is handed over");
        engine.item_applied(0);
        expect(engine.next_item(0) == 0u && engine.items_applied(0) == 5u, "the green star after it is passed as well");

        // The transformations are counted too (never handed over).
        engine.on_message(received(5, {ap::kItemBase + ap::kItemFireFlower, ap::kItemBase + ap::kItemFlyingStar, ap::kItemBase + 1}), out);
        expect(engine.item_count(ap::kItemFireFlower) == 1u && engine.item_count(ap::kItemFlyingStar) == 1u &&
                   engine.item_count(ap::kItemBeeMushroom) == 0u,
               "the transformations are counted");
        expect(engine.next_item(0) == ap::kItemOneUp, "a transformation is passed, the 1-Up after it is handed over");
        engine.item_applied(0);

        // Items found: ours, from Bob, and for Bob.  (The notices of items received carry the item's number for the icon.)
        const auto send = [&](int item, int location, int sender, int receiving) {
            return std::string(R"([{"cmd":"PrintJSON","type":"ItemSend","data":[],"receiving":)") + std::to_string(receiving) +
                   R"(,"item":{"item":)" + std::to_string(item) + R"(,"location":)" + std::to_string(location) + R"(,"player":)" +
                   std::to_string(sender) + R"(,"flags":1}}])";
        };
        engine.on_message(send(ap::kItemBase + 3, 1, 1, 1), out);
        engine.on_message(send(ap::kItemBase + 5, 1, 2, 1), out);
        engine.on_message(send(77, 1, 1, 2), out);
        engine.on_message(send(78, 1, 2, 2), out);  // between others: not shown
        std::uint32_t icon = 99u;
        expect(engine.pop_notice(notice, &icon) && notice == "Found Power Star" && icon == ap::kItemPowerStar, "own item found, with its icon");
        expect(engine.pop_notice(notice, &icon) && notice == "Grand Star from Bob" && icon == ap::kItemGrandStar,
               "an item from another player, with its icon");
        expect(engine.pop_notice(notice, &icon) && notice == "Sent an item to Bob" && icon == 0u, "an item for another player keeps the logo");
        expect(!engine.pop_notice(notice), "nothing between others");
        // The console's /send arrives as ReceivedItems with the location -1 (the server prints no ItemSend for it).
        const auto console_item = [&](std::size_t index, int item) {
            return std::string(R"([{"cmd":"ReceivedItems","index":)") + std::to_string(index) + R"(,"items":[{"item":)" +
                   std::to_string(item) + R"(,"location":-1,"player":0,"flags":0}]}])";
        };
        const std::size_t known_items = engine.items_received();
        engine.on_message(console_item(known_items, ap::kItemBase + 2), out);
        std::uint32_t cheat_icon = 99u;
        expect(engine.pop_notice(notice, &cheat_icon) && notice == "Cheat console: 50 Star Bits" && cheat_icon == ap::kItemStarBits50,
               "the cheat console's item says so, with its icon");
        engine.on_message(console_item(0, ap::kItemBase + 2), out);  // a full resend: the old ones are not shown again
        expect(!engine.pop_notice(notice), "a resent console item is not shown again");

        // The data package names the other game's items.
        engine.on_message(R"([{"cmd":"DataPackage","data":{"games":{"Other Game":{"item_name_to_id":{"Ocarina":77},"location_name_to_id":{}}}}}])", out);
        engine.on_message(send(77, 1, 1, 2), out);
        expect(engine.pop_notice(notice) && notice == "Sent Ocarina to Bob", "a name from the data package");

        // An item from Bob names the place he found it when the data package knows it and the line stays short.
        engine.on_message(R"([{"cmd":"DataPackage","data":{"games":{"Other Game":{"item_name_to_id":{},"location_name_to_id":{"Chest":1,"A very long location name that cannot fit next to the rest of the line":2}}}}}])", out);
        engine.on_message(send(ap::kItemBase + 3, 1, 2, 1), out);
        engine.on_message(send(ap::kItemBase + 3, 2, 2, 1), out);
        expect(engine.pop_notice(notice) && notice == "Power Star from Bob (Chest)", "the place an item was found");
        expect(engine.pop_notice(notice) && notice == "Power Star from Bob", "no place when the line would be too long");

        // Notices are plain ASCII and bounded.
        engine.add_notice(std::string("caf\xC3\xA9 ") + std::string(200, 'x'));
        expect(engine.pop_notice(notice) && notice.size() <= 70u && notice.rfind("caf? ", 0) == 0, "a notice is ASCII and short");

        // A refusal and an unreachable server show on screen once.
        ap::Engine refused(config, folder, log_line);
        std::vector<std::string> out2;
        refused.on_message(kRoomInfo, out2);
        refused.on_message(R"([{"cmd":"ConnectionRefused","errors":["InvalidSlot"]}])", out2);
        expect(refused.pop_notice(notice) && notice.find("no slot called \"Mario\"") != std::string::npos, "the refusal on screen");
        ap::Engine away(config, folder, log_line);
        away.on_connect_failed("timeout");
        away.on_connect_failed("timeout");
        expect(away.pop_notice(notice) && !away.pop_notice(notice), "an unreachable server is shown once");
    }

    // Files saved by Notepad.
    {
        const std::string plain = "server=a:1\nslot=b\n";
        expect(ap::parse_config("\xEF\xBB\xBF" + plain).enabled, "a UTF-8 byte-order mark is ignored");
        std::string wide = "\xFF\xFE";
        for (const char c : plain) {
            wide += c;
            wide += '\0';
        }
        expect(ap::parse_config(wide).enabled && ap::parse_config(wide).slot == "b", "UTF-16 is read");
        expect(ap::config_problem("").find("server=") != std::string::npos, "an empty file is explained");
        expect(ap::config_problem("server=a:1").find("slot=") != std::string::npos, "a missing slot is explained");
        expect(ap::config_problem(plain).empty(), "a good file has no problem");
        expect(ap::option_name(static_cast<std::uint32_t>(ap::Option::StarItems)) != nullptr, "an option has a name");
        expect(ap::option_name(43) == nullptr, "a number that is no option has none");
    }

    // The shop: prices from the slot data, what each shelf holds from LocationInfo, purchases reported as locations.
    {
        const std::string base = std::to_string(ap::kLocationBase);
        const auto id = [&](unsigned number) { return std::to_string(ap::kLocationBase + number); };
        ap::Engine engine(config, folder, log_line);
        std::vector<std::string> out;
        engine.on_message(kRoomInfo, out);
        out.clear();
        engine.on_message(R"([{"cmd":"Connected","team":0,"slot":1,"players":[{"team":0,"slot":1,"alias":"Mario","name":"Mario"},{"team":0,"slot":2,"alias":"Bob","name":"Bob"}],"slot_info":{"1":{"name":"Mario","game":"Super Mario Galaxy PC Port","type":1,"group_members":[]},"2":{"name":"Bob","game":"Other Game","type":1,"group_members":[]}},"missing_locations":[)" +
                              id(1) + "," + id(201) + "," + id(202) + R"(],"checked_locations":[)" + id(203) +
                              R"(],"slot_data":{"options":{"goal":0},"shop_prices":[30,60,90]}}])",
                          out);
        expect(engine.list_value(ap::Engine::kListShopPrices, 1) == 60u, "the price of the second shelf");
        expect(engine.list_value(ap::Engine::kListShopPrices, 3) == ap::Engine::kNoCap, "no fourth shelf");
        expect(!engine.shop_bought(0) && !engine.shop_bought(1) && engine.shop_bought(2), "the shelf the server checked is bought");
        bool scouted = false;
        for (const std::string& message : out) {
            const json::Value command = first_command(message);
            if (command.find("cmd") && command.find("cmd")->as_string() == "LocationScouts") scouted = command.find("locations")->items().size() == 4u;
        }
        expect(scouted, "the shop's locations are scouted even without star colours");
        expect(engine.shop_label(0).empty(), "nothing known before LocationInfo");
        engine.on_message(R"([{"cmd":"LocationInfo","locations":[{"item":)" + std::to_string(ap::kItemBase + 3) + R"(,"location":)" + id(201) +
                              R"(,"player":1,"flags":1},{"item":77,"location":)" + id(202) + R"(,"player":2,"flags":0}]}])",
                          out);
        expect(engine.shop_label(0) == std::string("Power Star for Mario"), "an own item has its name and its owner");
        expect(engine.shop_label(1) == "an item for Bob", "another player's item is named with its owner");
        expect(engine.shop_icon(0) == ap::kItemPowerStar, "an own item on a shelf has its picture");
        expect(engine.shop_icon(1) == 0u && engine.shop_icon(5) == 0u, "another player's item (or none) shows the logo");
        out.clear();
        engine.poll_outgoing(out);
        out.clear();
        engine.request_shop_hints();
        engine.poll_outgoing(out);
        {
            bool hinted = false;
            for (const std::string& packet : out) {
                const auto parsed = first_command(packet);
                const auto* cmd = parsed.find("cmd");
                const auto* hint = parsed.find("create_as_hint");
                if (cmd != nullptr && cmd->as_string() == "LocationScouts" && hint != nullptr && hint->as_int() == 2) hinted = true;
            }
            expect(hinted, "opening the shop scouts its shelves as hints");
        }
        out.clear();
        engine.poll_outgoing(out);
        expect(out.empty(), "the hints are sent once");
        out.clear();
        engine.report_location(ap::kShopFirst + 0u);
        expect(engine.shop_bought(0), "a purchase is remembered");
        engine.poll_outgoing(out);
        expect(out.size() == 1u && first_command(out.at(0)).find("locations")->items().at(0).as_int() == ap::kLocationBase + ap::kShopFirst,
               "the purchase is sent as a location check");
        (void)base;
    }

    // Messages that are not understood do no harm.
    {
        ap::Engine engine(config, folder, log_line);
        std::vector<std::string> out;
        engine.on_message("not json", out);
        engine.on_message("{}", out);
        engine.on_message(R"([{"no_cmd":1},{"cmd":"Unknown"},3,"x"])", out);
        expect(out.empty() && engine.state() == ap::State::Connecting, "garbage is ignored");
    }

    // The green spheres' guard: a guard file left by a run that stopped turns them off, for good, and the guard can be set and cleared.
    {
        const std::filesystem::path settings = folder / "settings";
        std::filesystem::create_directories(settings);
        { std::ofstream(settings / "sphere-guard.txt") << "left behind\n"; }
        ap::start(settings, log_line);
        expect(!ap::spheres_allowed(), "a guard file left behind turns the spheres off");
        expect(!std::filesystem::exists(settings / "sphere-guard.txt"), "the stale guard is removed");
        expect(std::filesystem::exists(settings / "green-spheres-off.txt"), "and the off marker is written");
        ap::sphere_guard_begin();
        expect(std::filesystem::exists(settings / "sphere-guard.txt"), "the guard is set");
        ap::sphere_guard_end();
        expect(!std::filesystem::exists(settings / "sphere-guard.txt"), "and cleared");
    }

    std::filesystem::remove_all(folder);
    if (s_failures == 0) std::printf("archipelago: all tests passed\n");
    return s_failures == 0 ? 0 : 1;
}
