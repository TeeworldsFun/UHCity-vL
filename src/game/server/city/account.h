/* SQL Class 0.5 by Sushi */
/* SQL Class 0.6 by FFS*/

#include <engine/shared/protocol.h>

#include <mysql_connection.h>
	
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

class CSQL
{
public:
	CSQL(class CGameContext *pGameServer);

	sql::Driver *driver;
	sql::Connection *connection;
	sql::Statement *statement;
	sql::ResultSet *results;
	
	// copy of config vars
	const char* database;
	const char* prefix;
	const char* user;
	const char* pass;
	const char* ip;
	int port;
	
	bool connect();
	void disconnect();
	
	void create_tables();
	void create_account(const char* name, const char* pass, int client_id);
	void delete_account(const char* name);
	void delete_account(int client_id);
	void change_password(int client_id, const char* new_pass);

	void login(const char* name, const char* pass, int client_id);
	void update(int client_id);
	void update_all();

/*	static void change_password_thread(void *user);
	static void login_thread(void *user);
	static void update_thread(void *user);
	static void create_account_thread(void *user);*/
};

struct CSqlData
{
	CSQL *m_SqlData;
	char name[32];
	char pass[32];
	const char*  m_aLanguage[MAX_CLIENTS];
	int m_ClientID;
	int UserID[MAX_CLIENTS];
	
	unsigned int m_Level[MAX_CLIENTS];
	unsigned long long int m_ExpPoints[MAX_CLIENTS];
	
	// basic
	unsigned long long m_Money[MAX_CLIENTS];
	int m_Health[MAX_CLIENTS];
	int m_Armor[MAX_CLIENTS];	
	int m_Kills[MAX_CLIENTS];
	int m_HouseID[MAX_CLIENTS];

	// Rank
	int m_Donor[MAX_CLIENTS];
	int m_VIP[MAX_CLIENTS];

	// Event
	long long m_Bounty[MAX_CLIENTS];

	// Punishments
	int m_Arrested[MAX_CLIENTS];

	// Player
	int m_AllWeapons[MAX_CLIENTS];
	int m_HealthRegen[MAX_CLIENTS];
	int m_InfinityAmmo[MAX_CLIENTS];
	int m_InfinityJumps[MAX_CLIENTS];
	int m_FastReload[MAX_CLIENTS];
	int m_NoSelfDMG[MAX_CLIENTS];
	int m_Portal[MAX_CLIENTS];

	// Weapons
	int m_GrenadeSpread[MAX_CLIENTS];
	int m_GrenadeBounce[MAX_CLIENTS];
	int m_GrenadeMine[MAX_CLIENTS];
	int m_FlameThrower[MAX_CLIENTS];

	int m_ShotgunSpread[MAX_CLIENTS];
	int m_ShotgunExplode[MAX_CLIENTS];
	int m_ShotgunStars[MAX_CLIENTS];

	int m_RifleSpread[MAX_CLIENTS];
	int m_RifleSwap[MAX_CLIENTS];
	int m_RiflePlasma[MAX_CLIENTS];

	int m_GunSpread[MAX_CLIENTS];
	int m_GunExplode[MAX_CLIENTS];
	int m_GunFreeze[MAX_CLIENTS];

	int m_HammerWalls[MAX_CLIENTS];
	int m_HammerShot[MAX_CLIENTS];
	int m_HammerKill[MAX_CLIENTS];
	int m_HammerExplode[MAX_CLIENTS];

	int m_NinjaPermanent[MAX_CLIENTS];
	int m_NinjaStart[MAX_CLIENTS];
	int m_NinjaSwitch[MAX_CLIENTS];
	int m_NinjaFly[MAX_CLIENTS];
	int m_NinjaBomber[MAX_CLIENTS];

	int m_EndlessHook[MAX_CLIENTS];
	int m_HealHook[MAX_CLIENTS];
	int m_BoostHook[MAX_CLIENTS];

	int m_PushAura[MAX_CLIENTS];
	int m_PullAura[MAX_CLIENTS];

	unsigned int m_LvlGun[MAX_CLIENTS];
	unsigned int m_LvlShotgun[MAX_CLIENTS];
	unsigned int m_LvlGrenade[MAX_CLIENTS];
	unsigned int m_LvlRifle[MAX_CLIENTS];
	unsigned int m_LvlHammer[MAX_CLIENTS];
};

struct CAccountData
{
	int UserID[MAX_CLIENTS];
	
	bool logged_in[MAX_CLIENTS];

	float exp[MAX_CLIENTS];
	int money[MAX_CLIENTS];
};
