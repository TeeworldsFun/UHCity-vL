/* (c) Paul 'Anime.pdf'. */
#include <game/server/gamecontext.h>
#include <iostream>
#include <sstream>
#include <cstdint>
#include <thread>

CGameContext *CGS;

CDiscordBot::CDiscordBot(CGameContext *GameServer)
{
    try
    {
        CGS = GameServer;
        m_Bot = new dpp::cluster(g_Config.m_SvDiscordToken);
        m_Bot->intents = dpp::intents::i_all_intents;
        std::istringstream iss(std::string(g_Config.m_SvDiscordChannel));
        iss >> m_Channel;
        dpp::cluster* bot_prt = m_Bot;
        dpp::snowflake* chann_ptr = &m_Channel;

        m_Bot->on_ready([bot_prt](const auto & event) {
            dbg_msg("Discord", "Logged in");
            bot_prt->set_presence(dpp::presence(dpp::ps_idle, dpp::at_game, "UH|City-vL"));
        });

        m_Bot->on_message_create([bot_prt, chann_ptr](const dpp::message_create_t &event) {
            if(*chann_ptr == event.msg.channel_id &&(!event.msg.author.is_bot() || !event.msg.content.empty()))
                CGS->SendChatFromDiscord(std::string("[Discord] " + ((event.msg.member.nickname.empty() == true) ? event.msg.author.username : event.msg.member.nickname) + ": " + event.msg.content).c_str());
        });
        std::thread aT([bot_prt]() { bot_prt->start(false); });

        aT.detach();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}

void CDiscordBot::LogChat(int Team, std::string Nickname, std::string Message)
{
    dpp::embed* embed;
    if(Team == -1)
    {
        embed = new dpp::embed();
        embed->set_color(0x00d4ff);
        embed->set_description("`[SPEC]` **" + Nickname +"**: "+Message);
    }else{
        embed = new dpp::embed();
        embed->set_color(0x00d4ff);
        embed->set_description("**" + Nickname +"**: "+Message);
    }
    dpp::message msg(m_Channel, *embed);
    m_Bot->message_create(msg);
}

void CDiscordBot::LogEnter(std::string Nickname)
{
    dpp::embed embed = dpp::embed().
        set_color(0x00ff11).
        set_description("**"+Nickname + "** has entered the game.");
    dpp::message msg(m_Channel, embed);
    m_Bot->message_create(msg);
}

void CDiscordBot::LogExit(std::string Nickname)
{ 
    dpp::embed embed = dpp::embed().
        set_color(0xff002e).
        set_description("**"+Nickname + "** has left the game.");
    dpp::message msg(m_Channel, embed);
    m_Bot->message_create(msg);
}

void CDiscordBot::SendChatTarget_Discord(std::string Text, std::string Desp)
{
    if(CGS->GetPlayerNum() <= 0)
        return;

    dpp::embed embed = dpp::embed().
        set_color(0x00ff11).
        set_title(Desp).
        set_description(Text);
    dpp::message msg(m_Channel, embed);
    m_Bot->message_create(msg);
}