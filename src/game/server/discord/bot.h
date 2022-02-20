#ifndef GAME_SERVER_DISCORD_BOT_H
#define GAME_SERVER_DISCORD_BOT_H

#include <dpp/dpp.h>

struct DiscordConfig
{
    std::string Token;
    dpp::snowflake ChannelID;
};

class CDiscordBot
{
public:
    CDiscordBot(CGameContext *GameServer, DiscordConfig Config);
    void LogChat(int Team, std::string Nickname, std::string Message);
    void LogEnter(std::string Nickname);
    void LogExit(std::string Nickname);
private:
    dpp::cluster *m_Bot;
    dpp::channel *m_Channel;
};

#endif