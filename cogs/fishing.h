#pragma once
#include "fluxerpp/bot_module.h"
#include "../database.h"
#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include <chrono>
#include <optional>
#include <sstream>

namespace bronx::cogs {

class FishingModule : public fluxerpp::bot_module {
private:
    struct FishEntry {
        std::string name;
        std::string emoji;
        std::string rarity;
        double weight_min;
        double weight_max;
        int64_t value_min;
        int64_t value_max;
        int weight_prob; // relative probability weight
    };

    static const std::vector<FishEntry>& get_fish_table() {
        static std::vector<FishEntry> table = {
            // name,           emoji,  rarity,      wmin,  wmax,   vmin,  vmax, prob
            {"Bass",           "🐟",   "normal",     0.5,   4.0,    50,    200,  35},
            {"Catfish",        "🐠",   "normal",     1.0,   6.0,    80,    300,  30},
            {"Trout",          "🎣",   "normal",     0.5,   3.0,    60,    250,  25},
            {"Carp",           "🐡",   "normal",     1.5,   8.0,    40,    180,  20},
            {"Snapper",        "🐟",   "rare",       0.8,   5.0,   300,    800,  10},
            {"Tuna",           "🐋",   "rare",       5.0,  20.0,   500,   1500,   8},
            {"Swordfish",      "⚔️",   "rare",      10.0,  40.0,   800,   2000,   5},
            {"Mahi-Mahi",      "🌊",   "epic",       3.0,  15.0,  2000,   5000,   3},
            {"Marlin",         "🏆",   "epic",      20.0,  80.0,  3000,   8000,   2},
            {"Shark",          "🦈",   "epic",      50.0, 200.0,  5000,  12000,   1},
            {"Oarfish",        "✨",   "legendary", 100.0, 500.0, 15000,  50000,   0},  // 0.3% via reroll
            {"Golden Koi",     "🌟",   "legendary",  1.0,   3.0,  20000,  80000,   0},  // 0.2% via reroll
            {"Void Anglerfish","🌑",   "mutated",    5.0,  25.0,  30000, 100000,   0},  // 0.1% via reroll
        };
        return table;
    }

    struct CatchResult {
        FishEntry fish;
        double weight;
        int64_t value;
    };

    CatchResult catch_fish(double prestige_mult, bool has_good_rod) {
        std::random_device rd;
        std::mt19937 gen(rd());

        // Roll for legendary/mutated (separate from main table)
        std::uniform_int_distribution<int> special_roll(1, 1000);
        int roll = special_roll(gen);

        const auto& table = get_fish_table();

        // Legendary/mutated special rolls
        if (roll <= 1) {  // 0.1% mutated
            const auto& f = table[12]; // Void Anglerfish
            std::uniform_real_distribution<double> wd(f.weight_min, f.weight_max);
            std::uniform_int_distribution<int64_t> vd(f.value_min, f.value_max);
            double w = wd(gen);
            int64_t v = static_cast<int64_t>(vd(gen) * prestige_mult);
            return {f, w, v};
        } else if (roll <= 3) {  // 0.2% golden koi
            const auto& f = table[11];
            std::uniform_real_distribution<double> wd(f.weight_min, f.weight_max);
            std::uniform_int_distribution<int64_t> vd(f.value_min, f.value_max);
            double w = wd(gen);
            int64_t v = static_cast<int64_t>(vd(gen) * prestige_mult);
            return {f, w, v};
        } else if (roll <= 6) {  // 0.3% oarfish
            const auto& f = table[10];
            std::uniform_real_distribution<double> wd(f.weight_min, f.weight_max);
            std::uniform_int_distribution<int64_t> vd(f.value_min, f.value_max);
            double w = wd(gen);
            int64_t v = static_cast<int64_t>(vd(gen) * prestige_mult);
            return {f, w, v};
        }

        // Normal weighted random from main pool
        std::vector<int> weights;
        std::vector<size_t> valid_idx;
        for (size_t i = 0; i < 10; ++i) {
            int w = table[i].weight_prob;
            if (has_good_rod && table[i].rarity != "normal") w = static_cast<int>(w * 1.5);
            weights.push_back(w);
            valid_idx.push_back(i);
        }
        std::discrete_distribution<int> dist(weights.begin(), weights.end());
        size_t idx = valid_idx[dist(gen)];
        const auto& f = table[idx];

        std::uniform_real_distribution<double> wd(f.weight_min, f.weight_max);
        std::uniform_int_distribution<int64_t> vd(f.value_min, f.value_max);
        double w = wd(gen);
        int64_t v = static_cast<int64_t>(vd(gen) * prestige_mult);
        return {f, w, v};
    }

    std::string rarity_emoji(const std::string& rarity) {
        if (rarity == "legendary") return "🌟";
        if (rarity == "mutated")   return "☣️";
        if (rarity == "epic")      return "💜";
        if (rarity == "rare")      return "💙";
        return "⬜";
    }

public:
    void register_handlers(fluxerpp::cluster& bot) override {

        // fish / cast — catch a fish
        bot.register_command({
            "fish",
            {"cast", "fsh"},
            "casts your line into the river to catch a fish.",
            "fish",
            {},
            [this](fluxerpp::cluster& bot, const fluxerpp::events::Message& msg, const std::vector<std::string>& args) {
                if (!g_db) return;

                auto eco = g_db->get_economy_user(msg.author.id);
                double prestige_mult = 1.0 + ((eco ? eco->prestige : 0) * 0.10);

                // Check active fishing gear for rod bonus
                bool has_good_rod = false;
                // (future: query user_fishing_gear table)

                auto result = catch_fish(prestige_mult, has_good_rod);

                // Save catch to database
                {
                    std::string sql = "INSERT INTO user_fish_catches "
                        "(user_id, rarity, fish_name, weight, value) VALUES ('"
                        + g_db->escape(msg.author.id) + "','"
                        + g_db->escape(result.fish.rarity) + "','"
                        + g_db->escape(result.fish.name) + "',"
                        + std::to_string(result.weight) + ","
                        + std::to_string(result.value) + ")";
                    g_db->exec(sql);
                }

                // Update fish_caught stat
                g_db->exec("UPDATE users SET fish_caught = fish_caught + 1 WHERE user_id = '"
                    + g_db->escape(msg.author.id) + "'");

                fluxerpp::models::MessagePayload payload;
                payload.content = " ";
                fluxerpp::models::Embed embed;
                embed.set_colour(
                    result.fish.rarity == "legendary" || result.fish.rarity == "mutated" ? 0xF1C40F :
                    result.fish.rarity == "epic"      ? 0x9B59B6 :
                    result.fish.rarity == "rare"      ? 0x3498DB : 0x2ECC71
                );
                embed.set_title("bronx fishing — catch!");

                char weight_buf[32];
                snprintf(weight_buf, sizeof(weight_buf), "%.2f", result.weight);

                std::string desc =
                    rarity_emoji(result.fish.rarity) + " **" + result.fish.name + "** " + result.fish.emoji + "\n"
                    "rarity: **" + result.fish.rarity + "**\n"
                    "weight: **" + weight_buf + " kg**\n"
                    "value: **" + std::to_string(result.value) + "** coins\n\n"
                    "_use `fishbag` to view your catch inventory, `sellfish` to sell_";
                embed.set_description(desc);

                payload.add_embed(embed);
                bot.send_message(msg.channel_id, payload);
            },
            "Fishing",
            0,
            30  // 30s cooldown
        });

        // fishbag — view unsold fish inventory
        bot.register_command({
            "fishbag",
            {"finv", "fishes"},
            "shows your unsold fish inventory.",
            "fishbag [page]",
            {"[page]"},
            [this](fluxerpp::cluster& bot, const fluxerpp::events::Message& msg, const std::vector<std::string>& args) {
                if (!g_db) return;

                int page = 1;
                if (!args.empty()) {
                    try { page = std::stoi(args[0]); } catch (...) {}
                }
                if (page < 1) page = 1;
                const int per_page = 10;

                std::string sql = "SELECT id, rarity, fish_name, weight, value FROM user_fish_catches "
                    "WHERE user_id = '" + g_db->escape(msg.author.id) + "' AND sold = 0 "
                    "ORDER BY caught_at DESC LIMIT " + std::to_string(per_page) +
                    " OFFSET " + std::to_string((page - 1) * per_page);

                auto res = g_db->query(sql);

                fluxerpp::models::MessagePayload payload;
                payload.content = " ";
                fluxerpp::models::Embed embed;
                embed.set_colour(0x3498DB);
                embed.set_title("bronx fishing — fish bag (page " + std::to_string(page) + ")");

                if (res.empty()) {
                    embed.set_description("your fish bag is empty on this page. go catch some fish with `fish`!");
                } else {
                    std::string desc;
                    for (auto& row : res) {
                        std::string id   = row.count("id")        ? row.at("id")        : "?";
                        std::string rar  = row.count("rarity")    ? row.at("rarity")    : "normal";
                        std::string name = row.count("fish_name") ? row.at("fish_name") : "Unknown";
                        std::string wt   = row.count("weight")    ? row.at("weight")    : "0";
                        std::string val  = row.count("value")     ? row.at("value")     : "0";
                        desc += rarity_emoji(rar) + " **" + name + "** — " + wt + " kg — **" + val + "** coins\n";
                    }
                    desc += "\n_use `sellfish all` or `sellfish <id>` to sell fish_";
                    embed.set_description(desc);
                }
                payload.add_embed(embed);
                bot.send_message(msg.channel_id, payload);
            },
            "Fishing"
        });

        // sellfish — sell fish from the inventory
        bot.register_command({
            "sellfish",
            {"fsell", "sellf"},
            "sells fish from your inventory.",
            "sellfish <all|id>",
            {"<all|id>"},
            [this](fluxerpp::cluster& bot, const fluxerpp::events::Message& msg, const std::vector<std::string>& args) {
                if (!g_db) return;

                std::string target = args.empty() ? "all" : args[0];
                int64_t total = 0;
                int count = 0;

                if (target == "all") {
                    // Fetch all unsold
                    auto rows = g_db->query(
                        "SELECT id, value FROM user_fish_catches WHERE user_id = '"
                        + g_db->escape(msg.author.id) + "' AND sold = 0");
                    for (auto& row : rows) {
                        try { total += std::stoll(row.at("value")); } catch (...) {}
                        count++;
                    }
                    if (count == 0) {
                        bot.send_message(msg.channel_id, "you have no fish to sell.");
                        return;
                    }
                    g_db->exec("UPDATE user_fish_catches SET sold = 1, sold_at = NOW() WHERE user_id = '"
                        + g_db->escape(msg.author.id) + "' AND sold = 0");
                } else {
                    // Sell by catch id
                    int catch_id = 0;
                    try { catch_id = std::stoi(target); } catch (...) {}
                    if (catch_id <= 0) {
                        bot.send_message(msg.channel_id, "invalid fish id.");
                        return;
                    }
                    auto rows = g_db->query(
                        "SELECT value FROM user_fish_catches WHERE id = " + std::to_string(catch_id)
                        + " AND user_id = '" + g_db->escape(msg.author.id) + "' AND sold = 0");
                    if (rows.empty()) {
                        bot.send_message(msg.channel_id, "fish not found or already sold.");
                        return;
                    }
                    try { total = std::stoll(rows[0].at("value")); } catch (...) {}
                    count = 1;
                    g_db->exec("UPDATE user_fish_catches SET sold = 1, sold_at = NOW() WHERE id = "
                        + std::to_string(catch_id));
                }

                // Credit wallet and update fish_sold stat
                g_db->exec("UPDATE users SET wallet = wallet + " + std::to_string(total)
                    + ", fish_sold = fish_sold + " + std::to_string(count)
                    + " WHERE user_id = '" + g_db->escape(msg.author.id) + "'");

                fluxerpp::models::MessagePayload payload;
                payload.content = " ";
                fluxerpp::models::Embed embed;
                embed.set_colour(0x2ECC71);
                embed.set_title("bronx fishing — sold!");
                embed.set_description(
                    "you sold **" + std::to_string(count) + "** fish for a total of **"
                    + std::to_string(total) + "** coins!");
                payload.add_embed(embed);
                bot.send_message(msg.channel_id, payload);
            },
            "Fishing"
        });

        // fishtop — fishing leaderboard
        bot.register_command({
            "fishtop",
            {"ftop"},
            "shows the top fishers by total fish caught.",
            "fishtop",
            {},
            [this](fluxerpp::cluster& bot, const fluxerpp::events::Message& msg, const std::vector<std::string>& args) {
                if (!g_db) return;

                auto rows = g_db->query(
                    "SELECT user_id, fish_caught FROM users ORDER BY fish_caught DESC LIMIT 10");

                fluxerpp::models::MessagePayload payload;
                payload.content = " ";
                fluxerpp::models::Embed embed;
                embed.set_colour(0x3498DB);
                embed.set_title("bronx fishing — top fishers");

                if (rows.empty()) {
                    embed.set_description("no fishing records yet!");
                } else {
                    std::string desc;
                    int rank = 1;
                    for (auto& r : rows) {
                        std::string uid = r.count("user_id") ? r.at("user_id") : "?";
                        std::string cnt = r.count("fish_caught") ? r.at("fish_caught") : "0";
                        desc += std::to_string(rank) + ". <@" + uid + "> — **" + cnt + "** fish caught\n";
                        rank++;
                    }
                    embed.set_description(desc);
                }
                payload.add_embed(embed);
                bot.send_message(msg.channel_id, payload);
            },
            "Fishing"
        });
    }
};

} // namespace bronx::cogs
