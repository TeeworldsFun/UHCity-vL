/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <string.h>
#include <fstream>
#include <engine/config.h>
#include "account.h"
#include "filesys.h"
#include "base/rapidjson/document.h"
#include "base/rapidjson/reader.h"
#include "base/rapidjson/writer.h"
#include "base/rapidjson/filereadstream.h"
#include "base/rapidjson/filewritestream.h"
#include "base/rapidjson/error/en.h"
#if defined(CONF_FAMILY_WINDOWS)
	#include <tchar.h>
	#include <direct.h>
#endif
#if defined(CONF_FAMILY_UNIX)
	#include <sys/types.h>
	#include <fcntl.h>
	#include <unistd.h>
#endif

using namespace rapidjson;

CAccount::CAccount(CPlayer *pPlayer, CGameContext *pGameServer)
{
   m_pPlayer = pPlayer;
   m_pGameServer = pGameServer;
}

/*
#ifndef GAME_VERSION_H
#define GAME_VERSION_H
#ifndef NON_HASED_VERSION
#include "generated/nethash.cpp"
#define GAME_VERSION "0.6.1"
#define GAME_NETVERSION "0.6 626fce9a778df4d4" //the std game version
#endif
#endif
*/

void CAccount::Login(char *Username, char *Password)
{
	char aBuf[256];
	if(m_pPlayer->m_AccData.m_UserID)
	{
		dbg_msg("account", "Account login failed ('%s' - Already logged in)", Username);
		GameServer()->SendChatTarget_Localization(m_pPlayer->GetCID(), CHATCATEGORY_INFO, _("Already logged in"));
		return;
	}
	
	GameServer()->Server()->Login(m_pPlayer->GetCID(), Username, Password);
	m_pPlayer->m_AccData = GameServer()->Server()->GetData(m_pPlayer->GetCID());
	if(!m_pPlayer->m_AccData.m_UserID)
	{
			GameServer()->SendChatTarget_Localization(m_pPlayer->GetCID(), CHATCATEGORY_INFO, _("Login failed"));
			return;
	}
	if(m_pPlayer->m_AccData.m_Donor)
		str_format(aBuf, sizeof(aBuf), "Donor '%s' logged in Account ID: %d and House ID: ", GameServer()->Server()->ClientName(m_pPlayer->GetCID()), m_pPlayer->m_AccData.m_UserID, m_pPlayer->m_AccData.m_HouseID);
	else
		str_format(aBuf, sizeof(aBuf), "Player '%s' logged in Account ID: %d", GameServer()->Server()->ClientName(m_pPlayer->GetCID()), m_pPlayer->m_AccData.m_UserID);
	GameServer()->SCT_Discord(aBuf, "Account");

	CCharacter *pOwner = GameServer()->GetPlayerChar(m_pPlayer->GetCID());

	if(pOwner)
	{
		if(pOwner->IsAlive())
			pOwner->Die(m_pPlayer->GetCID(), WEAPON_GAME);
	}
	 
	if(m_pPlayer->GetTeam() == TEAM_SPECTATORS)
		m_pPlayer->SetTeam(TEAM_RED);
  	
	dbg_msg("account", "Account login sucessful ('%s')", Username);
	GameServer()->SendChatTarget_Localization(m_pPlayer->GetCID(), CHATCATEGORY_INFO, _("Login succesful"));
 
	if (m_pPlayer->m_AccData.m_GunFreeze > 3) // Remove on acc reset
		m_pPlayer->m_AccData.m_GunFreeze = 3;
	return;
}

void CAccount::Register(char *Username, char *Password, char *TruePassword)
{
	//char aBuf[256];
	if(m_pPlayer->m_AccData.m_UserID) return GameServer()->SendChatTarget_Localization(m_pPlayer->GetCID(), CHATCATEGORY_INFO, _("Already logged in"));

	GameServer()->Server()->Register(m_pPlayer->GetCID(), Username, Password);	
	dbg_msg("account", "Registration succesful ('%s')", Username);
	GameServer()->SendChatTarget_Localization(m_pPlayer->GetCID(), CHATCATEGORY_INFO, _("Registration succesful - ('/login {str:Username} {str:Password}')"), "Username", Username, "Password", TruePassword, NULL);
	Login(Username, Password);
}

bool CAccount::Exists(const char *Username)
{
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "accounts/%s.acc", Username);

    if(FILE *Accfile = fopen(aBuf, "r"))
    {
        fclose(Accfile);
        return true;
    }
    return false;
}

void CAccount::Apply()
{
	GameServer()->Server()->UpdateData(m_pPlayer->GetCID(), m_pPlayer->m_AccData);
	/*
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "accounts/%s.acc", m_pPlayer->m_AccData.m_Username);
	std::remove(aBuf);
	StringBuffer strBuf;
    Writer<StringBuffer> writer(strBuf);
	FILE *Accfile;
	
	writer.StartObject();

    writer.Key("user");
    writer.StartObject();

    writer.Key("accdata");
    writer.StartObject();
    writer.Key("accid");
    writer.Int(m_pPlayer->m_AccData.m_UserID);
	writer.Key("username");
	writer.String(m_pPlayer->m_AccData.m_Username);
	writer.Key("password");
	writer.String(m_pPlayer->m_AccData.m_Password);
	writer.EndObject();

	writer.Key("general");
	writer.StartObject();
    writer.Key("money");
    writer.Int64(m_pPlayer->m_AccData.m_Money);
	writer.Key("health");
    writer.Int(m_pPlayer->m_AccData.m_Health<10?10:m_pPlayer->m_AccData.m_Health);
	writer.Key("armor");
    writer.Int(m_pPlayer->m_AccData.m_Armor<10?10:m_pPlayer->m_AccData.m_Armor);
	writer.Key("houseid");
    writer.Int(m_pPlayer->m_AccData.m_HouseID);
    writer.EndObject();

	writer.Key("level");
	writer.StartObject();
	writer.Key("normal");
    writer.Int(m_pPlayer->m_AccData.m_Level);
	writer.Key("hammer");
    writer.Int(m_pPlayer->m_AccData.m_LvlWeapon[WEAPON_HAMMER]);
    writer.Key("gun");
    writer.Int(m_pPlayer->m_AccData.m_LvlWeapon[WEAPON_GUN]);
	writer.Key("shotgun");
    writer.Int(m_pPlayer->m_AccData.m_LvlWeapon[WEAPON_SHOTGUN]);
	writer.Key("grenade");
    writer.Int(m_pPlayer->m_AccData.m_LvlWeapon[WEAPON_GRENADE]);
	writer.Key("rifle");
    writer.Int(m_pPlayer->m_AccData.m_LvlWeapon[WEAPON_RIFLE]);
    writer.EndObject();

	writer.Key("exp");
	writer.StartObject();
	writer.Key("normal");
	writer.Int64(m_pPlayer->m_AccData.m_ExpPoints);
	writer.Key("hammer");
    writer.Int(m_pPlayer->m_AccData.m_ExpWeapon[WEAPON_HAMMER]);
    writer.Key("gun");
    writer.Int(m_pPlayer->m_AccData.m_ExpWeapon[WEAPON_GUN]);
	writer.Key("shotung");
    writer.Int(m_pPlayer->m_AccData.m_ExpWeapon[WEAPON_SHOTGUN]);
	writer.Key("grenade");
    writer.Int(m_pPlayer->m_AccData.m_ExpWeapon[WEAPON_GRENADE]);
	writer.Key("rifle");
    writer.Int(m_pPlayer->m_AccData.m_ExpWeapon[WEAPON_RIFLE]);
    writer.EndObject();

	writer.Key("auth");
	writer.StartObject();
	writer.Key("authlvl");
	writer.Int(GameServer()->Server()->AuthLvl(m_pPlayer->GetCID()));
	writer.Key("vip");
	writer.Int(m_pPlayer->m_AccData.m_VIP);
	writer.EndObject();

	writer.Key("donor");
	writer.Int(m_pPlayer->m_AccData.m_Donor);

	writer.Key("hammerexplode");
	writer.Int(m_pPlayer->m_AccData.m_HammerExplode);

	writer.Key("language");
	writer.String(m_pPlayer->GetLanguage());

	writer.Key("event");
	writer.StartObject();
	writer.Key("bounty");
	writer.Int64(m_pPlayer->m_AccData.m_Bounty);
	writer.EndObject();

    writer.Key("items");
    writer.StartObject();

	writer.Key("basic");
	writer.StartObject();
	writer.Key("allweapons");
	writer.Int(m_pPlayer->m_AccData.m_AllWeapons);
	writer.Key("healthregen");
	writer.Int(m_pPlayer->m_AccData.m_HealthRegen);
	writer.Key("infinityammo");
	writer.Int(m_pPlayer->m_AccData.m_InfinityAmmo);
	writer.Key("infinityjumps");
	writer.Int(m_pPlayer->m_AccData.m_InfinityJumps);
	writer.Key("fastreload");
	writer.Int(m_pPlayer->m_AccData.m_FastReload);
	writer.Key("noselfdmg");
	writer.Int(m_pPlayer->m_AccData.m_NoSelfDMG);
	writer.EndObject();
    writer.Key("gun");
    writer.StartObject();
	writer.Key("gunspread");
    writer.Int(m_pPlayer->m_AccData.m_GunSpread);
	writer.Key("gunexplode");
    writer.Int(m_pPlayer->m_AccData.m_GunExplode);
    writer.Key("freezegun");
    writer.Int(m_pPlayer->m_AccData.m_GunFreeze);
    writer.EndObject();

	writer.Key("shotgun");
    writer.StartObject();
	writer.Key("shotgunspread");
    writer.Int(m_pPlayer->m_AccData.m_ShotgunSpread);
	writer.Key("shotgunexplode");
    writer.Int(m_pPlayer->m_AccData.m_ShotgunExplode);
    writer.Key("shotgunstars");
    writer.Int(m_pPlayer->m_AccData.m_ShotgunStars);
    writer.EndObject();

	writer.Key("grenade");
    writer.StartObject();
	writer.Key("grenadespread");
    writer.Int(m_pPlayer->m_AccData.m_GunSpread);
	writer.Key("grenadebounce");
    writer.Int(m_pPlayer->m_AccData.m_GrenadeBounce);
    writer.Key("grenademine");
    writer.Int(m_pPlayer->m_AccData.m_GrenadeMine);
    writer.EndObject();

	writer.Key("rifle");
    writer.StartObject();
	writer.Key("riflespread");
    writer.Int(m_pPlayer->m_AccData.m_RifleSpread);
	writer.Key("rifleswap");
    writer.Int(m_pPlayer->m_AccData.m_RifleSwap);
    writer.Key("rifleplasma");
    writer.Int(m_pPlayer->m_AccData.m_RiflePlasma);
    writer.EndObject();

	writer.Key("hammer");
    writer.StartObject();
	writer.Key("hammerwalls");
    writer.Int(m_pPlayer->m_AccData.m_HammerWalls);
	writer.Key("hammershot");
    writer.Int(m_pPlayer->m_AccData.m_HammerShot);
    writer.Key("hammerkill");
    writer.Int(m_pPlayer->m_AccData.m_HammerKill);
	writer.Key("portal");
    writer.Int(m_pPlayer->m_AccData.m_Portal);
    writer.EndObject();

	writer.Key("ninja");
    writer.StartObject();
	writer.Key("ninjapermanent");
    writer.Int(m_pPlayer->m_AccData.m_NinjaPermanent);
	writer.Key("ninjastart");
    writer.Int(m_pPlayer->m_AccData.m_NinjaStart);
    writer.Key("ninjaswitch");
    writer.Int(m_pPlayer->m_AccData.m_NinjaSwitch);
	writer.Key("fly");
    writer.Int(m_pPlayer->m_AccData.m_NinjaFly);
	writer.Key("bomber");
    writer.Int(m_pPlayer->m_AccData.m_NinjaBomber);
    writer.EndObject();

	writer.Key("hook");
	writer.StartObject();
	writer.Key("endless");
    writer.Int(m_pPlayer->m_AccData.m_EndlessHook);
	writer.Key("heal");
    writer.Int(m_pPlayer->m_AccData.m_HealHook);
	writer.Key("boost");
    writer.Int(m_pPlayer->m_AccData.m_BoostHook);
	writer.EndObject();

	writer.Key("special");
	writer.StartObject();
	writer.Key("pushaura");
    writer.Int(m_pPlayer->m_AccData.m_PushAura);
	writer.Key("pullaura");
    writer.Int(m_pPlayer->m_AccData.m_PullAura);
	writer.EndObject();

    writer.EndObject();
    writer.EndObject();
    writer.EndObject();

	Accfile = fopen(aBuf,"a+");
	fputs(strBuf.GetString(), Accfile);
	fclose(Accfile);
	*/
}

void CAccount::Reset()
{
	if (m_pPlayer->m_AccData.m_Bounty)
		GameServer()->RemoveFromBountyList(m_pPlayer->GetCID());

	str_copy(m_pPlayer->m_AccData.m_Username, "", 32);
	str_copy(m_pPlayer->m_AccData.m_Password, "", 32);
	m_pPlayer->m_AccData.m_UserID = 0;
	
	m_pPlayer->m_AccData.m_HouseID = 0;
	m_pPlayer->m_AccData.m_Money = 0;
	m_pPlayer->m_AccData.m_Health = 10;
	m_pPlayer->m_AccData.m_Armor = 10;
	m_pPlayer->SetLanguage("en");

	m_pPlayer->m_AccData.m_Donor = 0;
	m_pPlayer->m_AccData.m_VIP = 0;

	m_pPlayer->m_AccData.m_Level = 1;
	m_pPlayer->m_AccData.m_ExpPoints = 0;

	for (int i = 0; i < 5; i++) {
		m_pPlayer->m_AccData.m_LvlWeapon[i] = 0;
		m_pPlayer->m_AccData.m_ExpWeapon[i] = 0;
	}
		
	m_pPlayer->m_AccData.m_Bounty = 0;

	m_pPlayer->m_AccData.m_Arrested = 0;

	m_pPlayer->m_AccData.m_AllWeapons = 0;
	m_pPlayer->m_AccData.m_HealthRegen = 0;
	m_pPlayer->m_AccData.m_InfinityAmmo = 0;
	m_pPlayer->m_AccData.m_InfinityJumps = 0;
	m_pPlayer->m_AccData.m_FastReload = 0;
	m_pPlayer->m_AccData.m_NoSelfDMG = 0;

	m_pPlayer->m_AccData.m_GrenadeSpread = 0;
	m_pPlayer->m_AccData.m_GrenadeBounce = 0;
	m_pPlayer->m_AccData.m_GrenadeMine = 0;

	m_pPlayer->m_AccData.m_ShotgunSpread = 0;
	m_pPlayer->m_AccData.m_ShotgunExplode = 0;
	m_pPlayer->m_AccData.m_ShotgunStars = 0;

	m_pPlayer->m_AccData.m_RifleSpread = 0;
	m_pPlayer->m_AccData.m_RifleSwap = 0;
	m_pPlayer->m_AccData.m_RiflePlasma = 0;

	m_pPlayer->m_AccData.m_GunSpread = 0;
	m_pPlayer->m_AccData.m_GunExplode = 0;
	m_pPlayer->m_AccData.m_GunFreeze = 0;

	m_pPlayer->m_AccData.m_HammerWalls = 0;
	m_pPlayer->m_AccData.m_HammerShot = 0;
	m_pPlayer->m_AccData.m_HammerKill = 0;
	m_pPlayer->m_AccData.m_HammerExplode = 0;

	m_pPlayer->m_AccData.m_NinjaPermanent = 0;
	m_pPlayer->m_AccData.m_NinjaStart = 0;
	m_pPlayer->m_AccData.m_NinjaSwitch = 0;
	m_pPlayer->m_AccData.m_NinjaFly = 0;
	m_pPlayer->m_AccData.m_NinjaBomber = 0;

	m_pPlayer->m_AccData.m_HealHook = 0;
	m_pPlayer->m_AccData.m_BoostHook = 0;
	m_pPlayer->m_AccData.m_EndlessHook = 0;

	m_pPlayer->m_AccData.m_Portal = 0;
	m_pPlayer->m_AccData.m_PushAura = 0;
	m_pPlayer->m_AccData.m_PullAura = 0;
	

	if (GameServer()->m_aPortals[m_pPlayer->GetCID()])
		GameServer()->m_aPortals[m_pPlayer->GetCID()]->Reset();

	GameServer()->Server()->Logout(m_pPlayer->GetCID());
}

void CAccount::Delete()
{
	char aBuf[128];
	if(m_pPlayer->m_AccData.m_UserID)
	{
		Reset();
		
		dbg_msg("account", "Account deleted ('%s')", m_pPlayer->m_AccData.m_Username);
		GameServer()->SendChatTarget_Localization(m_pPlayer->GetCID(), CHATCATEGORY_INFO, _("Account deleted!"));
	}
	else
		GameServer()->SendChatTarget_Localization(m_pPlayer->GetCID(), CHATCATEGORY_INFO, _("Please, login to delete your account"));
}

void CAccount::NewPassword(char *NewPassword)
{
	char aBuf[128];
	if(!m_pPlayer->m_AccData.m_UserID)
	{
		GameServer()->SendChatTarget_Localization(m_pPlayer->GetCID(), CHATCATEGORY_INFO, _("Please, login to change the password"));
		return;
	}

	if(strlen(NewPassword) > 15 || !strlen(NewPassword))
	{
		str_format(aBuf, sizeof(aBuf), "Password too %s!", strlen(NewPassword)?"long":"short");
		GameServer()->SendChatTarget_Localization(m_pPlayer->GetCID(), CHATCATEGORY_DEFAULT, _("Password too {str:ls}!"), "ls", strlen(NewPassword)?"long":"short", NULL);
		return;
    }

	str_copy(m_pPlayer->m_AccData.m_Password, NewPassword, 32);
	
	dbg_msg("account", "Password changed - ('%s')", m_pPlayer->m_AccData.m_Username);
	GameServer()->SendChatTarget_Localization(m_pPlayer->GetCID(), CHATCATEGORY_INFO, _("Password successfully changed!"));
}

int CAccount::NextID()
{
	FILE *Accfile;
	int UserID = 1;
	char aBuf[32];
	char AccUserID[32];

	str_copy(AccUserID, "accounts/++UserIDs++.acc", sizeof(AccUserID));

	if(Exists("++UserIDs++"))
	{
		Accfile = fopen(AccUserID, "r");
		fscanf(Accfile, "%d", &UserID);
		fclose(Accfile);

		std::remove(AccUserID);

		Accfile = fopen(AccUserID, "a+");
		str_format(aBuf, sizeof(aBuf), "%d", UserID+1);
		fputs(aBuf, Accfile);
		fclose(Accfile);

		return UserID+1;
	}
	else
	{
		Accfile = fopen(AccUserID, "a+");
		str_format(aBuf, sizeof(aBuf), "%d", UserID);
		fputs(aBuf, Accfile);
		fclose(Accfile);
	}

	return 1;
}

bool CAccount::OldLogin(char *Username, char *Password)
{
	char aBuf[128];

	str_format(aBuf, sizeof aBuf, "+%s", Username);

	if(!Exists(aBuf))
	{
		return false;
	}

	str_format(aBuf, sizeof(aBuf), "accounts/+%s.acc", Username);

	char AccUsername[32];
	char AccPassword[32];
	char AccRcon[32];
	int AccID;
 
	FILE *Accfile;
	Accfile = fopen(aBuf, "r");
	fscanf(Accfile, "%s\n%s\n%s\n%d", AccUsername, AccPassword, AccRcon, &AccID);
	fclose(Accfile);

	for(int i = 0; i < MAX_SERVER; i++)
	{
		for(int j = 0; j < MAX_CLIENTS; j++)
		{
			if(GameServer()->m_apPlayers[j] && GameServer()->m_apPlayers[j]->m_AccData.m_UserID == AccID)
			{
				dbg_msg("account", "Account login failed ('%s' - already in use (local))", Username);
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Account already in use");
				return true;
			}

			if(!GameServer()->m_aaExtIDs[i][j])
				continue;

			if(AccID == GameServer()->m_aaExtIDs[i][j])
			{
				dbg_msg("account", "Account login failed ('%s' - already in use (extern))", Username);
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Account already in use");
				return true;
			}
		}
	}

	if(strcmp(Username, AccUsername))
	{
		dbg_msg("account", "Account login failed ('%s' - Wrong username)", Username);
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Wrong username or password");
		return true;
	}

	if(strcmp(Password, AccPassword))
	{
		dbg_msg("account", "Account login failed ('%s' - Wrong password)", Username);
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Wrong username or password");
		return true;
	}

	Accfile = fopen(aBuf, "r"); 
	
	char tmpBuf[128];

	fscanf(Accfile, "%s\n%s\n%s\n%d\n\n\n%d\n%d\n%d\n%d\n%d\n\n%d\n%d\n%d\n\n%d\n%d\n%d\n%d\n%d\n%d\n\n%d\n%d\n%d\n\n%d\n%d\n%d\n\n%d\n%d\n%d\n\n%d\n%d\n%d\n\n%d\n%d\n%d\n\n%d\n%d\n%d\n%d\n%d", 
		m_pPlayer->m_AccData.m_Username, // Done
		m_pPlayer->m_AccData.m_Password, // Done
		tmpBuf,
		&m_pPlayer->m_AccData.m_UserID, // Done

		&m_pPlayer->m_AccData.m_HouseID,
 		&m_pPlayer->m_AccData.m_Money, // Done
		&m_pPlayer->m_AccData.m_Health, // Done
		&m_pPlayer->m_AccData.m_Armor, // Done
		&m_pPlayer->m_Score, // Done

		&m_pPlayer->m_AccData.m_Donor,
		&m_pPlayer->m_AccData.m_VIP, // Done
		&m_pPlayer->m_AccData.m_Arrested, // Done

		&m_pPlayer->m_AccData.m_AllWeapons, // Done
		&m_pPlayer->m_AccData.m_HealthRegen, // Done
		&m_pPlayer->m_AccData.m_InfinityAmmo, // Done
		&m_pPlayer->m_AccData.m_InfinityJumps, // Done
		&m_pPlayer->m_AccData.m_FastReload, // Done
		&m_pPlayer->m_AccData.m_NoSelfDMG, // Done

		&m_pPlayer->m_AccData.m_GrenadeSpread, // Done 
		&m_pPlayer->m_AccData.m_GrenadeBounce, // Done
		&m_pPlayer->m_AccData.m_GrenadeMine,

		&m_pPlayer->m_AccData.m_ShotgunSpread, // Done
		&m_pPlayer->m_AccData.m_ShotgunExplode, // Done
		&m_pPlayer->m_AccData.m_ShotgunStars,

		&m_pPlayer->m_AccData.m_RifleSpread, // Done
		&m_pPlayer->m_AccData.m_RifleSwap, // Done
		&m_pPlayer->m_AccData.m_RiflePlasma, // Done

		&m_pPlayer->m_AccData.m_GunSpread, // Done
		&m_pPlayer->m_AccData.m_GunExplode, // Done
		&m_pPlayer->m_AccData.m_GunFreeze, // Done

		&m_pPlayer->m_AccData.m_HammerWalls, // Done
		&m_pPlayer->m_AccData.m_HammerShot, // Done
		&m_pPlayer->m_AccData.m_HammerKill, // Done
		&m_pPlayer->m_AccData.m_HammerExplode,

		&m_pPlayer->m_AccData.m_NinjaPermanent, // Done
		&m_pPlayer->m_AccData.m_NinjaStart, // Done
		&m_pPlayer->m_AccData.m_NinjaSwitch, // Done

		&m_pPlayer->m_AccData.m_Level,
		&m_pPlayer->m_AccData.m_ExpPoints); 

	fclose(Accfile);

	m_pPlayer->m_AccData.m_Bounty = 0;

	CCharacter *pOwner = GameServer()->GetPlayerChar(m_pPlayer->GetCID());

	if(pOwner)
	{
		if(pOwner->IsAlive())
			pOwner->Die(m_pPlayer->GetCID(), WEAPON_GAME);
	}
	 
	if(m_pPlayer->GetTeam() == TEAM_SPECTATORS)
		m_pPlayer->SetTeam(TEAM_RED);
  	
	dbg_msg("account", "Account login sucessful ('%s')", Username);
	GameServer()->SendChatTarget_Localization(m_pPlayer->GetCID(), CHATCATEGORY_INFO, _("Login succesful"));
 
	if (m_pPlayer->m_AccData.m_GunFreeze > 3) // Remove on acc reset
		m_pPlayer->m_AccData.m_GunFreeze = 3;
	std::remove(aBuf);
	
	return true;
}

void CAccount::SetAuth(char *Username, int lvl) {
	char aBuf[512];
	char AccText[65536];
	FILE *Accfile;

	if (GameServer()->Server()->AuthLvl(m_pPlayer->GetCID()) == lvl)
		return;

	if (!Exists(Username))
		return;

	str_format(aBuf, sizeof(aBuf), "accounts/%s.acc", Username);

	Accfile = fopen(aBuf, "r");
	fscanf(Accfile, "%s\n", AccText);
	fclose(Accfile);
	
	Document AccD;
	StringBuffer strBuf;
	Writer<StringBuffer> writer(strBuf);
	ParseResult res = AccD.Parse(AccText);

	if (res.IsError()) {
		GameServer()->SendChatTarget_Localization(m_pPlayer->GetCID(), CHATCATEGORY_INFO, _(" Parse Error: Please contact UrinStone to get this fixed."));
		dbg_msg("account", "parse error");
		return;
	}

	tl_assert(AccD.IsObject());

	AccD["user"]["auth"]["authlvl"].SetInt(lvl);

	AccD.Accept(writer);
	std::remove(aBuf);
	Accfile = fopen(aBuf, "a+");
	fscanf(Accfile, "%s\n", AccText);
	fclose(Accfile);
}
