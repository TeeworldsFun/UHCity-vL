/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SERVER_SERVER_H
#define ENGINE_SERVER_SERVER_H

#include <engine/server.h>
#include "engine/server/sql_connector.h"
#include "engine/server/sql_server.h"

class CSnapIDPool
{
	enum
	{
		MAX_IDS = 16 * 1024,
	};

	class CID
	{
	public:
		short m_Next;
		short m_State; // 0 = free, 1 = alloced, 2 = timed
		int m_Timeout;
	};

	CID m_aIDs[MAX_IDS];

	int m_FirstFree;
	int m_FirstTimed;
	int m_LastTimed;
	int m_Usage;
	int m_InUsage;

public:
	CSnapIDPool();

	void Reset();
	void RemoveFirstTimeout();
	int NewID();
	void TimeoutIDs();
	void FreeID(int ID);
};

class CServer : public IServer
{
	class IGameServer *m_pGameServer;
	class IConsole *m_pConsole;
	class IStorage *m_pStorage;
	CSqlServer *m_apSqlReadServers[MAX_SQLSERVERS];
	CSqlServer *m_apSqlWriteServers[MAX_SQLSERVERS];

public:
	class IGameServer *GameServer() { return m_pGameServer; }
	class IConsole *Console() { return m_pConsole; }
	class IStorage *Storage() { return m_pStorage; }

	enum
	{
		AUTHED_NO = 0,
		AUTHED_POLICE,
		AUTHED_MAPPER,
		AUTHED_MOD,
		AUTHED_ADMIN,
		AUTH_AMOUNT,

		MAX_RCONCMD_SEND = 16,
	};

	class CClient
	{
	public:
		enum
		{
			STATE_EMPTY = 0,
			STATE_AUTH,
			STATE_CONNECTING,
			STATE_READY,
			STATE_INGAME,
			STATE_BOT,

			SNAPRATE_INIT = 0,
			SNAPRATE_FULL,
			SNAPRATE_RECOVER
		};

		class CInput
		{
		public:
			int m_aData[MAX_INPUT_SIZE];
			int m_GameTick; // the tick that was chosen for the input
		};

		// connection state info
		int m_State;
		int m_Latency;
		int m_SnapRate;

		int m_LastAckedSnapshot;
		int m_LastInputTick;
		CSnapshotStorage m_Snapshots;

		CInput m_LatestInput;
		CInput m_aInputs[200]; // TODO: handle input better
		int m_CurrentInput;

		char m_aName[MAX_NAME_LENGTH];
		char m_aClan[MAX_CLAN_LENGTH];
		int m_Country;
		int m_Score;
		int m_Authed;
		int m_AuthTries;
		int m_AccID;

		struct
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
			unsigned long long int m_ExpPoints;

			// Rank
			bool m_Donor;
			bool m_VIP;

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
		} m_AccData;

		const IConsole::CCommandInfo *m_pRconCmdToSend;

		void Reset();

		int m_Client; // 64 clients

		int m_NextMapChunk;

		char m_aLanguage[16]; // NICE
	};

	virtual void Register(int ClientID, const char* pUsername, const char* pPassword);
	virtual void Login(int ClientID, const char* pUsername, const char* pPassword);

	CClient m_aClients[MAX_CLIENTS];
	int m_aIdMap[MAX_CLIENTS * VANILLA_MAX_CLIENTS];

	CSnapshotDelta m_SnapshotDelta;
	CSnapshotBuilder m_SnapshotBuilder;
	CSnapIDPool m_IDPool;
	CNetServer m_NetServer;
	CEcon m_Econ;

	IEngineMap *m_pMap;

	int64 m_GameStartTime;
	// int m_CurrentGameTick;
	int m_RunServer;
	int m_MapReload;
	int m_RconClientID;
	int m_RconAuthLevel;
	int m_PrintCBIndex;

	int64 m_Lastheartbeat;
	// static NETADDR4 master_server;

	char m_aCurrentMap[64];
	unsigned m_CurrentMapCrc;
	unsigned char *m_pCurrentMapData;
	int m_CurrentMapSize;

	CDemoRecorder m_DemoRecorder;
	CRegister m_Register;
	CMapChecker m_MapChecker;

	void BotJoin(int BotID, int BotMode);
	void BotJoin(int BotID, int BotMode, bool Puppy = false);

	CServer();

	int TrySetClientName(int ClientID, const char *pName);

	virtual void SetClientName(int ClientID, const char *pName);
	virtual void SetClientClan(int ClientID, char const *pClan);
	virtual void SetClientCountry(int ClientID, int Country);
	virtual void SetClientScore(int ClientID, int Score);
	virtual void SetClientAccID(int ClientID, int AccID);

	void Kick(int ClientID, const char *pReason);

	void DemoRecorder_HandleAutoStart();

	// int Tick()
	int64 TickStartTime(int Tick);
	// int TickSpeed()

	int Init();

	int AuthLvl(int ClientID);
	bool IsAuthed(int ClientID);
	// KlickFoots stuff
	bool IsPolice(int ClientID);
	bool IsMapper(int ClientID);
	bool IsMod(int ClientID);
	bool IsAdmin(int ClientID);
	void Logout(int ClientID);
	void Police(int ClientID, int Switch);
	void SetRconlvl(int ClientID, int Level);
	// Normales Zeugs
	int GetClientInfo(int ClientID, CClientInfo *pInfo);
	void GetClientAddr(int ClientID, NETADDR *pAddr);
	void GetClientAddr(int ClientID, char *pAddrStr, int Size);
	const char *ClientName(int ClientID);
	const char *ClientClan(int ClientID);
	int ClientCountry(int ClientID);
	bool ClientIngame(int ClientID);
	int ClientIdByName(const char *Name);

	virtual int SendMsg(CMsgPacker *pMsg, int Flags, int ClientID);
	int SendMsgEx(CMsgPacker *pMsg, int Flags, int ClientID, bool System);

	void DoSnapshot();

	static int NewClientCallback(int ClientID, void *pUser);
	static int DelClientCallback(int ClientID, const char *pReason, void *pUser);

	void SendMap(int ClientID);
	void SendMapData(int ClientID, int Chunk);
	void SendConnectionReady(int ClientID);
	void SendRconLine(int ClientID, const char *pLine);
	static void SendRconLineAuthed(const char *pLine, void *pUser);

	void SendRconCmdAdd(const IConsole::CCommandInfo *pCommandInfo, int ClientID);
	void SendRconCmdRem(const IConsole::CCommandInfo *pCommandInfo, int ClientID);
	void UpdateClientRconCommands();

	void ProcessClientPacket(CNetChunk *pPacket);

	void SendServerInfo(NETADDR *pAddr, int Token, bool Extended = false, int Offset = 0);
	void UpdateServerInfo();

	int BanAdd(NETADDR Addr, int Seconds, const char *pReason);
	int BanRemove(NETADDR Addr);

	void PumpNetwork();

	char *GetMapName();
	int LoadMap(const char *pMapName);

	void InitRegister(CNetServer *pNetServer, IEngineMasterServer *pMasterServer, IConsole *pConsole);
	int Run();

	static void ConKick(IConsole::IResult *pResult, void *pUser);
	static void ConBan(IConsole::IResult *pResult, void *pUser);
	static void ConUnban(IConsole::IResult *pResult, void *pUser);
	static void ConBans(IConsole::IResult *pResult, void *pUser);
	static void ConStatus(IConsole::IResult *pResult, void *pUser);
	static void ConShutdown(IConsole::IResult *pResult, void *pUser);
	static void ConRecord(IConsole::IResult *pResult, void *pUser);
	static void ConStopRecord(IConsole::IResult *pResult, void *pUser);
	static void ConMapReload(IConsole::IResult *pResult, void *pUser);
	static void ConchainSpecialInfoupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainMaxclientsperipUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainCommandUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainConsoleOutputLevelUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConAddSqlServer(IConsole::IResult *pResult, void *pUserData);

	void RegisterCommands();

	virtual int SnapNewID();
	virtual void SnapFreeID(int ID);
	virtual void *SnapNewItem(int Type, int ID, int Size);
	void SnapSetStaticsize(int ItemType, int Size);

	// 64 clients
	virtual int *GetIdMap(int ClientID);
	virtual void SetClient(int ClientID, int Client);
	virtual bool IsClient64(int ClientID) { return m_aClients[ClientID].m_Client != CLIENT_VANILLA; }

	virtual const char *GetClientLanguage(int ClientID);
	virtual void SetClientLanguage(int ClientID, const char *pLanguage);

	// SQL integrations
};

#endif
