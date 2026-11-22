#pragma once
#include "fluxerpp/bot_module.h"
#include "helpers.h"
#include "command_config.h"
#include <regex>
#include <thread>

namespace bronx::cogs {

class EmojiModule : public fluxerpp::bot_module {
public:
    void register_handlers(fluxerpp::cluster& bot) override {
        auto callback = [](fluxerpp::cluster& bot, const fluxerpp::events::Message& msg, const std::vector<std::string>& args) {
            if (msg.server_id.empty()) {
                bot.send_reply(msg, "this command can only be used within a server.", true);
                return;
            }

            if (args.empty()) {
                std::string pfx = msg.prefix.empty() ? bot.config().command_prefix : msg.prefix;
                bot.send_reply(msg, "usage: `" + pfx + "emoji <emoji1> [emoji2] ...`\n"
                                    "examples:\n"
                                    "• `" + pfx + "emoji <:smile:123456789012345678>`\n"
                                    "• `" + pfx + "steal <:smile:123456789012345678> custom_name`", true);
                return;
            }

            bot.fetch_member(msg.server_id, msg.author.id, [&bot, msg, args](fluxerpp::models::Member member, bool success) {
                if (!success) {
                    bot.send_message(msg.channel_id, bronx::emj_warning + " Failed to verify your permissions.");
                    return;
                }

                bool allowed = is_user_trusted_or_owner(bot, msg.server_id, msg.author.id, member);
                if (!allowed) {
                    bot.send_message(msg.channel_id, bronx::emj_warning + " Permission error: you need Manage Server permission to clone emojis.");
                    return;
                }

                struct TargetEmoji {
                    std::string name;
                    std::string custom_name;
                    std::string id;
                    bool animated = false;
                    std::string status = "pending...";
                };
                std::vector<TargetEmoji> targets;
                std::regex single_emoji_regex(R"(^(?:<a?:([a-zA-Z0-9_]{1,32}):([0-9]{15,22}|[0-9a-zA-Z]{26})>|:([0-9]{15,22}|[0-9a-zA-Z]{26}):)$)");
                std::smatch match;

                for (const auto& arg : args) {
                    if (std::regex_match(arg, match, single_emoji_regex)) {
                        TargetEmoji te;
                        if (!match[3].str().empty()) {
                            te.id = match[3].str();
                        } else {
                            te.name = match[1].str();
                            te.id = match[2].str();
                            te.animated = !match[0].str().empty() && match[0].str()[1] == 'a';
                        }
                        targets.push_back(te);
                    } else if (!targets.empty() && !arg.empty()) {
                        std::string clean_name;
                        for (char c : arg) {
                            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') clean_name += c;
                        }
                        if (!clean_name.empty()) {
                            if (clean_name.length() > 32) clean_name = clean_name.substr(0, 32);
                            targets.back().custom_name = clean_name;
                        }
                    }
                }

                if (targets.empty()) {
                    bot.send_reply(msg, "no valid custom emojis found to clone.", true);
                    return;
                }

                auto render_payload = [](const std::vector<TargetEmoji>& tgts, int current, int total) {
                    fluxerpp::models::MessagePayload payload;
                    nlohmann::json embed = nlohmann::json::object();
                    embed["title"] = "emoji cloner progress";

                    int failed_count = 0;
                    for (const auto& t : tgts) {
                        if (t.status != "success!" && t.status != "pending..." && t.status != "processing...") failed_count++;
                    }
                    embed["colour"] = (current == total) ? (failed_count > 0 ? "#E74C3C" : "#2ECC71") : "#F1C40F";

                    int percentage = (total == 0) ? 0 : (current * 100) / total;
                    int filled = percentage / 10;
                    std::string bar = "[";
                    for (int i = 0; i < 10; ++i) bar += (i < filled) ? "█" : "░";
                    bar += "] " + std::to_string(percentage) + "%";

                    std::string desc = "**progress**: " + bar + "\n\n";
                    for (const auto& t : tgts) {
                        std::string label = !t.custom_name.empty() ? t.custom_name : (!t.name.empty() ? t.name : t.id);
                        desc += "• `" + label + "` ➔ " + t.status + "\n";
                    }
                    embed["description"] = desc;
                    payload.embeds.push_back(embed);
                    payload.content = " ";
                    return payload;
                };

                bot.send_message(msg.channel_id, render_payload(targets, 0, targets.size()),
                    [&bot, targets, msg, render_payload](fluxerpp::models::Message status_msg, bool ok) mutable {
                        if (!ok) return;
                        std::thread([&bot, targets, status_msg, render_payload, msg]() mutable {
                            int completed = 0;
                            for (size_t idx = 0; idx < targets.size(); ++idx) {
                                targets[idx].status = "processing...";
                                bot.edit_message(status_msg.channel, status_msg.id, render_payload(targets, completed, targets.size()));

                                auto clone_res = bot.rest().post("/guilds/" + msg.server_id + "/emojis/clone",
                                                                 nlohmann::json{{"source_emoji_id", targets[idx].id}});
                                if (clone_res.success()) {
                                    if (!targets[idx].custom_name.empty() && clone_res.body.is_object() && clone_res.body.contains("id")) {
                                        std::string new_id = clone_res.body["id"].get<std::string>();
                                        bot.rest().patch("/guilds/" + msg.server_id + "/emojis/" + new_id,
                                                         nlohmann::json{{"name", targets[idx].custom_name}});
                                    }
                                    targets[idx].status = "success!";
                                } else {
                                    targets[idx].status = "failed (" + clone_res.error_message() + ")";
                                }

                                completed++;
                                bot.edit_message(status_msg.channel, status_msg.id, render_payload(targets, completed, targets.size()));
                                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                            }
                        }).detach();
                    });
            });
        };

        bot.register_command({
            "emoji",
            {"steal", "clone"},
            "clone custom emojis from other servers and add them to this server.",
            "",
            {"<emoji1>", "[emoji2]..."},
            callback,
            "Emoji"
        });
    }
};

} // namespace bronx::cogs
