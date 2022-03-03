/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SERVER_H
#define ENGINE_SERVER_H
#include "kernel.h"
#include "message.h"

#include <engine/shared/protocol.h>
#include <game/generated/protocol.h>

enum
{
	LEVEL,
	EXP,
	MONEY,
	DONATE,
	DGOLD,
};

enum
{
	CHATCATEGORY_DEFAULT=0,
	CHATCATEGORY_INFO,
	CHATCATEGORY_JOIN,
	CHATCATEGORY_GOOD,
};

enum
{
		e_UserID,
		e_Username,
		e_Password,
		e_RconPassword,

		e_Money,
		e_Health,
		e_Armor,
		e_Kills,
		e_HouseID,

		e_Level,
		e_ExpPoints,

		e_Donor,
		e_VIP,

		e_Bounty,

		e_Arrested,

		e_AllWeapons,
		e_HealthRegen,
		e_InfinityAmmo,
		e_InfinityJumps,
		e_FastReload,
		e_NoSelfDMG,
		e_Portal,

		e_HammerLvl,
        e_GunLvl,
        e_ShotgunLvl,
        e_GrenadeLvl,
        e_RifleLvl,

        e_HammerExp,
        e_GunExp,
        e_ShotgunExp,
        e_GrenadeExp,
        e_RifleExp,


		e_GrenadeSpread,
		e_GrenadeBounce,
		e_GrenadeMine,

		e_ShotgunSpread,
		e_ShotgunExplode,
		e_ShotgunStars,

		e_RifleSpread,
		e_RifleSwap,
		e_RiflePlasma,

		e_GunSpread,
		e_GunExplode,
		e_GunFreeze,

		e_HammerWalls,
		e_HammerShot,
		e_HammerKill,
		e_HammerExplode,

		e_NinjaPermanent,
		e_NinjaStart,
		e_NinjaSwitch,
		e_NinjaFly,
		e_NinjaBomber,

		e_EndlessHook,
		e_HealHook,
		e_BoostHook,

		e_PushAura,
		e_PullAura,
		e_ValueCount
};

struct _m_AccData
{
	// Main
	int m_UserID;
	char m_Username[32];
	char m_Password[32];
	char m_RconPassword[32];

	// basic
	unsigned long long m_Money;
	int m_Health;
	int m_Armor;
	int m_Kills;
	int m_HouseID;

	// levels
	unsigned int m_Level;
	unsigned long long m_ExpPoints;

	// Rank
	int m_Donor;
	int m_VIP;

	// Event
	long long int m_Bounty;

	// Punishments
	int m_Arrested;

	// Player
	int m_AllWeapons;
	int m_HealthRegen;
	int m_InfinityAmmo;
	int m_InfinityJumps;
	int m_FastReload;
	int m_NoSelfDMG;
	int m_Portal;

	// Weapons
	unsigned int m_LvlWeapon[5];
	unsigned int m_ExpWeapon[5];

	int m_GrenadeSpread;
	int m_GrenadeBounce;
	int m_GrenadeMine;

	int m_ShotgunSpread;
	int m_ShotgunExplode;
	int m_ShotgunStars;

	int m_RifleSpread;
	int m_RifleSwap;
	int m_RiflePlasma;

	int m_GunSpread;
	int m_GunExplode;
	int m_GunFreeze;

	int m_HammerWalls;
	int m_HammerShot;
	int m_HammerKill;
	int m_HammerExplode;

	int m_NinjaPermanent;
	int m_NinjaStart;
	int m_NinjaSwitch;
	int m_NinjaFly;
	int m_NinjaBomber;

	int m_EndlessHook;
	int m_HealHook;
	int m_BoostHook;

	int m_PushAura;
	int m_PullAura;
};

const char VarName[58][32] =
	{
		"UserID",
		"Username",
		"Password",
		"RconPassword",
		"Money",
		"Health",
		"Armor",
		"Kills",
		"HouseID",
		"Level",
		"ExpPoints",
		"Donor",
		"VIP",
		"Bounty",
		"Arrested",
		"AllWeapons",
		"HealthRegen",
		"InfinityAmmo",
		"InfinityJumps",
		"FastReload",
		"NoSelfDMG",
		"Portal",
		"HammerLvl",
		"GunLvl",
		"ShotgunLvl",
		"GrenadeLvl",
		"RifleLvl",
		"HammerExp",
		"GunExp",
		"ShotgunExp",
		"GrenadeExp",
		"RifleExp",
		"GrenadeSpread",
		"GrenadeBounce",
		"GrenadeMine",
		"ShotgunSpread",
		"ShotgunExplode",
		"ShotgunStars",
		"RifleSpread",
		"RifleSwap",
		"RiflePlasma",
		"GunSpread",
		"GunExplode",
		"GunFreeze",
		"HammerWalls",
		"HammerShot",
		"HammerKill",
		"HammerExplode",
		"NinjaPermanent",
		"NinjaStart",
		"NinjaSwitch",
		"NinjaFly",
		"NinjaBomber",
		"EndlessHook",
		"HealHook",
		"BoostHook",
		"PushAura",
		"PullAura"};
class IServer : public IInterface
{
	MACRO_INTERFACE("server", 0)
protected:
	int m_CurrentGameTick;
	int m_TickSpeed;

public:
	class CLocalization* m_pLocalization;

public:
	/*
		Structure: CClientInfo
	*/
	struct CClientInfo
	{
		const char *m_pName;
		int m_Latency;

		int m_Client;
	};

	enum
	{
		CLIENT_VANILLA = 0,
		CLIENT_DDNET,
		CLIENT_KCLIENT,
		CLIENT_CUSTOM, // some custom client with 64p which we don't know
		NUM_CLIENT_TYPES,
	};

	virtual ~IServer() {};

	inline class CLocalization* Localization() { return m_pLocalization; }

	int Tick() const { return m_CurrentGameTick; }
	int TickSpeed() const { return m_TickSpeed; }

	virtual const char *ClientName(int ClientID) = 0;
	virtual const char *ClientClan(int ClientID) = 0;
	virtual int ClientCountry(int ClientID) = 0;
	virtual int ClientIdByName(const char* Name) = 0;
	virtual bool ClientIngame(int ClientID) = 0;
	virtual int GetClientInfo(int ClientID, CClientInfo *pInfo) = 0;
	virtual void GetClientAddr(int ClientID, NETADDR *pAddr) = 0;
	virtual void GetClientAddr(int ClientID, char *pAddrStr, int Size) = 0;

	virtual int SendMsg(CMsgPacker *pMsg, int Flags, int ClientID) = 0;

	template<class T>
	int SendPackMsg(T *pMsg, int Flags, int ClientID)
	{
		int result = 0;
		T tmp;
		if (ClientID == -1)
		{
			for(int i = 0; i < MAX_PLAYERS; i++)
				if(ClientIngame(i))
				{
					mem_copy(&tmp, pMsg, sizeof(T));
					result = SendPackMsgTranslate(&tmp, Flags, i);
				}
		} else 
		{
			mem_copy(&tmp, pMsg, sizeof(T));
			result = SendPackMsgTranslate(&tmp, Flags, ClientID);
		}
		return result;
	}

	template<class T>
	int SendPackMsgTranslate(T* pMsg, int Flags, int ClientID)
	{
		return SendPackMsgOne(pMsg, Flags, ClientID);
	}

	int SendPackMsgTranslate(CNetMsg_Sv_Emoticon* pMsg, int Flags, int ClientID)
	{
		return Translate(pMsg->m_ClientID, ClientID) && SendPackMsgOne(pMsg, Flags, ClientID);
	}

	char aMsgBuf[1024];
	int SendPackMsgTranslate(CNetMsg_Sv_Chat* pMsg, int Flags, int ClientID)
	{
		if (pMsg->m_ClientID >= 0 && !Translate(pMsg->m_ClientID, ClientID))
		{
			str_format(aMsgBuf, sizeof(aMsgBuf), "%s: %s", ClientName(pMsg->m_ClientID), pMsg->m_pMessage);
			pMsg->m_pMessage = aMsgBuf;
			pMsg->m_ClientID = VANILLA_MAX_CLIENTS - 1;
		}

		return SendPackMsgOne(pMsg, Flags, ClientID);
	}

	int SendPackMsgTranslate(CNetMsg_Sv_KillMsg* pMsg, int Flags, int ClientID)
	{
		if (!Translate(pMsg->m_Victim, ClientID)) return 0;
		if (!Translate(pMsg->m_Killer, ClientID)) pMsg->m_Killer = pMsg->m_Victim;
		return SendPackMsgOne(pMsg, Flags, ClientID);
	}

	template<class T>
	int SendPackMsgOne(T* pMsg, int Flags, int ClientID)
	{
		dbg_assert(ClientID != -1, "SendPackMsgOne called with -1");

		CMsgPacker Packer(pMsg->MsgID());
		if (pMsg->Pack(&Packer))
			return -1;

		return SendMsg(&Packer, Flags, ClientID);
	}

	bool Translate(int& Target, int ClientID)
	{
		CClientInfo Info;
		GetClientInfo(ClientID, &Info);
		if (Info.m_Client != CLIENT_VANILLA)
			return true;

		int* pMap = GetIdMap(ClientID);
		bool Found = false;
		for (int i = 0; i < VANILLA_MAX_CLIENTS; i++)
		{
			if (Target == pMap[i])
			{
				Target = i;
				Found = true;
				break;
			}
		}

		return Found;
	}

	bool ReverseTranslate(int& Target, int ClientID)
	{
		CClientInfo Info;
		GetClientInfo(ClientID, &Info);
		if (Info.m_Client != CLIENT_VANILLA)
			return true;

		int* pMap = GetIdMap(ClientID);
		if (pMap[Target] == -1)
			return false;

		Target = pMap[Target];
		return true;
	}

	virtual void BotJoin(int BotID, int BotMode, bool Puppy = false) = 0;


	virtual void SetClientName(int ClientID, char const *pName) = 0;
	virtual void SetClientClan(int ClientID, char const *pClan) = 0;
	virtual void SetClientCountry(int ClientID, int Country) = 0;
	virtual void SetClientScore(int ClientID, int Score) = 0;
	virtual void Logout(int ClientID) = 0;
	virtual void Police(int ClientID,int Switch) = 0;
	virtual void SetRconlvl(int ClientID,int Level) = 0;
	virtual void SetClientAccID(int ClientID, int AccID) = 0;

	virtual int SnapNewID() = 0;
	virtual void SnapFreeID(int ID) = 0;
	virtual void *SnapNewItem(int Type, int ID, int Size) = 0;

	virtual void SnapSetStaticsize(int ItemType, int Size) = 0;

	//KlickFoots stuff
	virtual bool IsAdmin(int ClientID) = 0;
	virtual bool IsMod(int ClientID) = 0;
	virtual bool IsMapper(int ClientID) = 0;
	virtual bool IsPolice(int ClientID) = 0;

	//Normales zeugs
	virtual bool IsAuthed(int ClientID) = 0;
	virtual int AuthLvl(int ClientID) = 0;
	virtual void Kick(int ClientID, const char *pReason) = 0;

	// 64 clients
	virtual int* GetIdMap(int ClientID) = 0;
	virtual void SetClient(int ClientID, int Client) = 0;
	virtual bool IsClient64(int ClientID) = 0;

	virtual void DemoRecorder_HandleAutoStart() = 0;
	
	// Localization system
	virtual const char* GetClientLanguage(int ClientID) = 0;
	virtual void SetClientLanguage(int ClientID, const char* pLanguage) = 0;

	// SQL integrations
	// virtual void GetData_Server(int ClientID) = 0;
	virtual _m_AccData GetData(int ClientID) = 0;
	virtual void UpdateData(int ClientID, _m_AccData AccData) = 0;
	virtual void UpdateData_Server(int ClientID) = 0;
	virtual void Register(int ClientID, const char* pUsername, const char* pPassword) = 0;
	virtual void Login(int ClientID, const char* pUsername, const char* pPassword) = 0;
};

class IGameServer : public IInterface
{
	MACRO_INTERFACE("gameserver", 0)
protected:
public:
	virtual void OnInit() = 0;
	virtual void OnConsoleInit() = 0;
	virtual void OnShutdown() = 0;

	virtual void OnTick() = 0;
	virtual void OnPreSnap() = 0;
	virtual void OnSnap(int ClientID) = 0;
	virtual void OnPostSnap() = 0;

	virtual void OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID) = 0;

	virtual void OnClientConnected(int ClientID) = 0;
	virtual void OnClientEnter(int ClientID) = 0;
	virtual void OnClientDrop(int ClientID, const char *pReason) = 0;
	virtual void OnClientDirectInput(int ClientID, void *pInput) = 0;
	virtual void OnClientPredictedInput(int ClientID, void *pInput) = 0;

	virtual bool IsClientReady(int ClientID) = 0;
	virtual bool IsClientPlayer(int ClientID) = 0;

	virtual const char *GameType() = 0;
	virtual const char *Version() = 0;
	virtual const char *NetVersion() = 0;

	virtual void SendBroadcast_Localization(int To, int Priority, int LifeSpan, const char* pText, ...) = 0;
	virtual void SendBroadcast_Localization_P(int To, int Priority, int LifeSpan, int Number, const char* pText, ...) = 0;
	virtual void SendChatTarget(int To, const char* pText) = 0;
	virtual void SCT_Discord(const char *pText, const char *Desp) = 0;
	virtual void SendChatTarget_Localization(int To, int Category, const char* pText, ...) = 0;
	virtual void SendChatTarget_Localization_P(int To, int Category, int Number, const char* pText, ...) = 0;
};

extern IGameServer *CreateGameServer();
#endif
