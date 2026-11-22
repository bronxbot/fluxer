#pragma once
#include "fluxerpp/bot_module.h"
#include "fluxerpp/cluster.h"
#include "../emojis.h"

namespace bronx::cogs {

class VoiceModule : public fluxerpp::bot_module {
public:
    void register_handlers(fluxerpp::cluster& bot) override {
        bot.register_command({
            "join",
            {"vcjoin"},
            "join a voice channel.",
            "",
            {"[channel_id]"},
            [](fluxerpp::cluster& bot, const fluxerpp::events::Message& msg, const std::vector<std::string>& args) {
                if (msg.server_id.empty()) {
                    bot.send_reply(msg, bronx::emj_no + " Voice commands can only be used in a server.", true);
                    return;
                }
                std::string target_ch = args.empty() ? msg.channel_id : fluxerpp::utils::extract_ulid(args[0]);
                if (target_ch.empty()) target_ch = msg.channel_id;
                auto res = bot.rest().get_voice_call_info(target_ch);
                if (res.success()) {
                    bot.send_reply(msg, "🔊 Connected to voice channel <#" + target_ch + ">.", true);
                } else {
                    bot.send_reply(msg, bronx::emj_warning + " Voice streaming is not available for <#" + target_ch + "> (" + res.error_message() + ").", true);
                }
            },
            "Voice"
        });

        bot.register_command({
            "leave",
            {"vcleave", "disconnect"},
            "leave the active voice channel.",
            "",
            {},
            [](fluxerpp::cluster& bot, const fluxerpp::events::Message& msg, const std::vector<std::string>&) {
                bot.send_reply(msg, "👋 Disconnected from voice channel.", true);
            },
            "Voice"
        });
    }
};

} // namespace bronx::cogs
