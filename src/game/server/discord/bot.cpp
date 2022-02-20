#include <game/server/gamecontext.h>

CGameContext *CGS;

CDiscordBot::CDiscordBot(CGameContext *GameServer, DiscordConfig Config)
{
    CGS = GameServer;
    m_Bot = new dpp::cluster(Config.token);

    bot->on_ready([&bot](const auto & event) {
        dbg_msg("Discord", "Logged in as %s", bot.me.username);
    });

    bot->on_message_create([&bot](const auto & event) {
        std::string Content { event.msg.content };
        
        //if(CGS && m_Channel && !Object.msg.is_bot())
            //CGS->SendChatFromDiscord(Object.msg.author.username.c_str(), Content.c_str());
    });

    m_Bot->start(false);
}

void CDiscordBot::LogChat(int Team, std::string Nickname, std::string Message)
{
    if(!m_Channel || !m_Bot)
        return;

    dpp::message msg;
    if(Team == TEAM_RED)
        msg(g_Config.m_SvDiscordChannel, "**[RED] " + Nickname + "**: " + Message);
    else if(Team == TEAM_BLUE)
        msg(g_Config.m_SvDiscordChannel, "**[BLUE] " + Nickname + "**: " + Message);
    else if(Team == TEAM_SPECTATORS)
        msg(g_Config.m_SvDiscordChannel, "**[SPEC] " + Nickname + "**: " + Message);
    else
        msg(g_Config.m_SvDiscordChannel, "**" + Nickname + "**: " + Message);
    dpp::message_create_t::send(msg);
}

void CDiscordBot::LogEnter(std::string Nickname)
{
    if(!m_Channel || !m_Bot)
        return;
    
    msg(g_Config.m_SvDiscordChannel, "`##` **" + Nickname + "** has entered the game. `##`");
    dpp::message_create_t::send(msg);
}

void CDiscordBot::LogExit(std::string Nickname)
{
    if(!m_Channel || !m_Bot)
        return;
    
    msg(g_Config.m_SvDiscordChannel, "`##` **" + Nickname + "** has left the game. `##`");
    dpp::message_create_t::send(msg);
}