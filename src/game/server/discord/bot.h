#ifndef GAME_SERVER_DISCORD_BOT_H
#define GAME_SERVER_DISCORD_BOT_H

#include <dpp/dpp.h>

class CDiscordBot
{
public:
    CDiscordBot(CGameContext *GameServer);
    void LogChat(int Team, std::string Nickname, std::string Message);
    void LogEnter(std::string Nickname);
    void LogExit(std::string Nickname);
    void SendChatTarget_Discord(std::string Text, std::string Desp);
    void DiscordStatus();
private:
    dpp::cluster *m_Bot;
    dpp::snowflake m_Channel;
};

#endif