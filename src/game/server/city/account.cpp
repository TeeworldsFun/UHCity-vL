/* SQL class 0.5 by Sushi */
/* SQL class 0.6 by FFS   */

#include "../gamecontext.h"

#include <engine/shared/config.h>

static LOCK sql_lock = 0;
class CGameContext *m_pGameServer;
CGameContext *GameServer() { return m_pGameServer; }

CSQL::CSQL(class CGameContext *pGameServer)
{
	if(sql_lock == 0)
		sql_lock = lock_create();

	m_pGameServer = pGameServer;
		
	// set database info
	database = g_Config.m_SvSqlDatabase;
	prefix = g_Config.m_SvSqlPrefix;
	user = g_Config.m_SvSqlUser;
	pass = g_Config.m_SvSqlPw;
	ip = g_Config.m_SvSqlIp;
	port = g_Config.m_SvSqlPort;
}

bool CSQL::connect()
{
	try 
	{
		// Create connection
		driver = get_driver_instance();
		char buf[256];
		str_format(buf, sizeof(buf), "tcp://%s:%d", ip, port);
		connection = driver->connect(buf, user, pass);
		
		// Create Statement
		statement = connection->createStatement();
		
		// Create database if not exists
		str_format(buf, sizeof(buf), "CREATE DATABASE IF NOT EXISTS %s", database);
		statement->execute(buf);
		
		// Connect to specific database
		connection->setSchema(database);
		dbg_msg("SQL", "SQL connection established");
		return true;
	} 
	catch (sql::SQLException &e)
	{
		dbg_msg("SQL", "ERROR: SQL connection failed");
		return false;
	}
}

void CSQL::disconnect()
{
	try
	{
		delete connection;
		dbg_msg("SQL", "SQL connection disconnected");
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("SQL", "ERROR: No SQL connection");
	}
}

// create tables... should be done only once
void CSQL::create_tables()
{
	// create connection
	if(connect())
	{
		try
		{
			// create tables
			char buf[2048];
			str_format(buf, sizeof(buf), 
			"CREATE TABLE IF NOT EXISTS %s_Account "
			"(UserID INT AUTO_INCREMENT PRIMARY KEY, "
			"Username VARCHAR(31) NOT NULL, "
			"Password VARCHAR(32) NOT NULL, "
			"HouseID INT DEFAULT 0, "
			"Exp BIGINT DEFAULT 0, "
			"Level BIGINT DEFAULT 0, "
			"Health INT DEFAULT 10, "
			"Armor INT DEFAULT 10, "
			"Language VARCHAR(10) DEFAULT 'en', "
			"VIP TINYINT DEFAULT 0, "
			"Donor TINYINT DEFAULT 0, "
			"Level BIGINT DEFAULT 0, "
			"Gun BIGINT DEFAULT 0, "
			"Shotgun BIGINT DEFAULT 0, "
			"Grenade BIGINT DEFAULT 0, "
			"Rifle BIGINT DEFAULT 0, "
			"Hammer BIGINT DEFAULT 0, "
			"Bounty BIGINT DEFAULT 0, "	
			"Arrested BIGINT DEFAULT 0, "
			"AllWeapons INT DEFAULT 0, "
			"HealthRegen INT DEFAULT 0, "
			"InfinityAmmo INT DEFAULT 0, "
			"InfinityJumps INT DEFAULT 0, "
			"FastReload INT DEFAULT 0, "
			"NoSelfDMG INT DEFAULT 0, "
			"GrenadeSpread INT DEFAULT 0, "
			"GrenadeBounce INT DEFAULT 0, "
			"GrenadeMine INT DEFAULT 0, "
			"ShotgunSpread INT DEFAULT 0, "
			"ShotgunExplode INT DEFAULT 0, "
			"ShotgunStars INT DEFAULT 0, "
			"RifleSpread INT DEFAULT 0, "
			"RifleSwap INT DEFAULT 0, "
			"RiflePlasma INT DEFAULT 0, "
			"GunSpread INT DEFAULT 0, "
			"GunExplode INT DEFAULT 0, "
			"GunFreeze INT DEFAULT 0, "
			"HammerWalls INT DEFAULT 0, "
			"HammerShot INT DEFAULT 0, "
			"HammerKill INT DEFAULT 0, "
			"HammerExplode INT DEFAULT 0, "
			"NinjaPermanent INT DEFAULT 0, "
			"NinjaStart INT DEFAULT 0, "
			"NinjaSwitch INT DEFAULT 0, "
			"NinjaFly INT DEFAULT 0, "
			"NinjaBomber INT DEFAULT 0, "
			"HealHook INT DEFAULT 0, "
			"BoostHook INT DEFAULT 0, "
			"EndlessHook INT DEFAULT 0, "
			"Portal INT DEFAULT 0, "
			"PushAura INT DEFAULT 0, "
			"PullAura INT DEFAULT 0, "			
			"Money BIGINT DEFAULT 0);", prefix);
			statement->execute(buf);
			dbg_msg("SQL", "Tables were created successfully");

			// delete statement
			delete statement;
		}
		catch (sql::SQLException &e)
		{
			dbg_msg("SQL", "ERROR: Tables were NOT created");
		}
		
		// disconnect from database
		disconnect();
	}	
}

// create Account
static void create_account_thread(void *user)
{
	lock_wait(sql_lock);
	
	CSqlData *Data = (CSqlData *)user;
	
	if(GameServer()->m_apPlayers[Data->m_ClientID])
	{
		// Connect to database
		if(Data->m_SqlData->connect())
		{
			try
			{
				// check if allready exists
				char buf[512];
				str_format(buf, sizeof(buf), "SELECT * FROM %s_Account WHERE Username='%s';", Data->m_SqlData->prefix, Data->name);
				Data->m_SqlData->results = Data->m_SqlData->statement->executeQuery(buf);
				if(Data->m_SqlData->results->next())
				{
					// Account found
					dbg_msg("SQL", "Account '%s' allready exists", Data->name);
					
					GameServer()->SendChatTarget(Data->m_ClientID, "This acoount allready exists!");
				}
				else
				{
					// create Account \o/
					str_format(buf, sizeof(buf), "INSERT INTO %s_Account(Username, Password) VALUES ('%s', '%s');", 
					Data->m_SqlData->prefix, 
					Data->name, Data->pass);
					
					Data->m_SqlData->statement->execute(buf);
					dbg_msg("SQL", "Account '%s' was successfully created", Data->name);
					
					GameServer()->SendChatTarget(Data->m_ClientID, "Acoount was created successfully.");
					GameServer()->SendChatTarget(Data->m_ClientID, "You may login now. (/login <user> <pass>)");
				}
				
				// delete statement
				delete Data->m_SqlData->statement;
				delete Data->m_SqlData->results;
			}
			catch (sql::SQLException &e)
			{
				dbg_msg("SQL", "ERROR: Could not create Account");
			}
			
			// disconnect from database
			Data->m_SqlData->disconnect();
		}
	}
	
	delete Data;
	
	lock_release(sql_lock);
}

void CSQL::create_account(const char* name, const char* pass, int m_ClientID)
{
	CSqlData *tmp = new CSqlData();
	str_copy(tmp->name, name, sizeof(tmp->name));
	str_copy(tmp->pass, pass, sizeof(tmp->pass));
	tmp->m_ClientID = m_ClientID;
	tmp->m_SqlData = this;
	
	void *register_thread = thread_init(create_account_thread, tmp);
#if defined(CONF_FAMILY_UNIX)
	pthread_detach((pthread_t)register_thread);
#endif
}

// change password
static void change_password_thread(void *user)
{
	lock_wait(sql_lock);
	
	CSqlData *Data = (CSqlData *)user;
	
	// Connect to database
	if(Data->m_SqlData->connect())
	{
		try
		{
			// Connect to database
			Data->m_SqlData->connect();
			
			// check if Account exists
			char buf[512];
			str_format(buf, sizeof(buf), "SELECT * FROM %s_Account WHERE UserID='%d';", Data->m_SqlData->prefix, Data->UserID[Data->m_ClientID]);
			Data->m_SqlData->results = Data->m_SqlData->statement->executeQuery(buf);
			if(Data->m_SqlData->results->next())
			{
				// update Account data
				str_format(buf, sizeof(buf), "UPDATE %s_Account SET Password='%s' WHERE UserID='%d'", Data->m_SqlData->prefix, Data->pass, Data->UserID[Data->m_ClientID]);
				Data->m_SqlData->statement->execute(buf);
				
				// get Account name from database
				str_format(buf, sizeof(buf), "SELECT name FROM %s_Account WHERE UserID='%d';", Data->m_SqlData->prefix, Data->UserID[Data->m_ClientID]);
				
				// create results
				Data->m_SqlData->results = Data->m_SqlData->statement->executeQuery(buf);

				// jump to result
				Data->m_SqlData->results->next();
				
				// finally the name is there \o/
				char acc_name[32];
				str_copy(acc_name, Data->m_SqlData->results->getString("Username").c_str(), sizeof(acc_name));	
				dbg_msg("SQL", "Account '%s' changed password.", acc_name);
				
				// Success
				str_format(buf, sizeof(buf), "Successfully changed your password to '%s'.", Data->pass);
				GameServer()->SendBroadcast(buf, Data->m_ClientID);
				GameServer()->SendChatTarget(Data->m_ClientID, buf);
			}
			else
				dbg_msg("SQL", "Account seems to be deleted");
			
			// delete statement and results
			delete Data->m_SqlData->statement;
			delete Data->m_SqlData->results;
		}
		catch (sql::SQLException &e)
		{
			dbg_msg("SQL", "ERROR: Could not update Account");
		}
		
		// disconnect from database
		Data->m_SqlData->disconnect();
	}
	
	delete Data;
	
	lock_release(sql_lock);
}

void CSQL::change_password(int m_ClientID, const char* new_pass)
{
	CSqlData *tmp = new CSqlData();
	tmp->m_ClientID = m_ClientID;
	tmp->UserID[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_UserID;
	str_copy(tmp->pass, new_pass, sizeof(tmp->pass));
	tmp->m_SqlData = this;
	
	void *change_pw_thread = thread_init(change_password_thread, tmp);
#if defined(CONF_FAMILY_UNIX)
	pthread_detach((pthread_t)change_pw_thread);
#endif
}

// login stuff
static void login_thread(void *user)
{
	lock_wait(sql_lock);
	
	CSqlData *Data = (CSqlData *)user;
	
	if(GameServer()->m_apPlayers[Data->m_ClientID] && !GameServer()->m_apPlayers[Data->m_ClientID]->m_AccData.m_UserID)
	{
		// Connect to database
		if(Data->m_SqlData->connect())
		{
			try
			{		
				// check if Account exists
				char buf[1024];
				str_format(buf, sizeof(buf), "SELECT * FROM %s_Account WHERE Username='%s';", Data->m_SqlData->prefix, Data->name);
				Data->m_SqlData->results = Data->m_SqlData->statement->executeQuery(buf);
				if(Data->m_SqlData->results->next())
				{
					// check for right pw and get data
					str_format(buf, sizeof(buf), "SELECT UserID, "
					"Level, Exp, Money "
					"FROM %s_Account WHERE Username='%s' AND Password='%s';", Data->m_SqlData->prefix, Data->name, Data->pass);
					
					// create results
					Data->m_SqlData->results = Data->m_SqlData->statement->executeQuery(buf);
					
					// if match jump to it
					if(Data->m_SqlData->results->next())
					{
						// never use player directly!
						// finally save the result to AccountData() \o/

						// check if Account allready is logged in
						for(int i = 0; i < MAX_CLIENTS; i++)
						{
							if(GameServer()->m_apPlayers[Data->m_ClientID]->m_AccData.m_UserID == Data->m_SqlData->results->getInt("UserID"))
							{
								dbg_msg("SQL", "Account '%s' already is logged in", Data->name);
								
								GameServer()->SendChatTarget(Data->m_ClientID, "This Account is already logged in.");
								
								// delete statement and results
								delete Data->m_SqlData->statement;
								delete Data->m_SqlData->results;
								
								// disconnect from database
								Data->m_SqlData->disconnect();
								
								// delete Data
								delete Data;
	
								// release lock
								lock_release(sql_lock);
								
								return;
							}
						}

						GameServer()->m_apPlayers[Data->m_ClientID]->m_AccData.m_UserID = Data->m_SqlData->results->getInt("UserID");
						GameServer()->m_apPlayers[Data->m_ClientID]->m_AccData.m_ExpPoints = (float)Data->m_SqlData->results->getDouble("Exp");
						GameServer()->m_apPlayers[Data->m_ClientID]->m_AccData.m_Money = Data->m_SqlData->results->getInt("Money");

						// login should be the last thing
						dbg_msg("SQL", "Account '%s' logged in sucessfully", Data->name);
						
						GameServer()->SendChatTarget(Data->m_ClientID, "You are now logged in.");
						char buf[512];
						str_format(buf, sizeof(buf), "Welcome %s!", Data->name);
						GameServer()->SendBroadcast(buf, Data->m_ClientID);
					}
					else
					{
						// wrong password
						dbg_msg("SQL", "Account '%s' is not logged in due to wrong password", Data->name);
						
						GameServer()->SendChatTarget(Data->m_ClientID, "The password you entered is wrong.");
					}
				}
				else
				{
					// no Account
					dbg_msg("SQL", "Account '%s' does not exists", Data->name);
					
					GameServer()->SendChatTarget(Data->m_ClientID, "This Account does not exists.");
					GameServer()->SendChatTarget(Data->m_ClientID, "Please register first. (/register <user> <pass>)");
				}
				
				// delete statement and results
				delete Data->m_SqlData->statement;
				delete Data->m_SqlData->results;
			}
			catch (sql::SQLException &e)
			{
				dbg_msg("SQL", "ERROR: Could not login Account");
			}
			
			// disconnect from database
			Data->m_SqlData->disconnect();
		}
	}
	
	delete Data;
	
	lock_release(sql_lock);
}

void CSQL::login(const char* name, const char* pass, int m_ClientID)
{
	CSqlData *tmp = new CSqlData();
	str_copy(tmp->name, name, sizeof(tmp->name));
	str_copy(tmp->pass, pass, sizeof(tmp->pass));
	tmp->m_ClientID = m_ClientID;
	tmp->m_SqlData = this;
	
	void *login_account_thread = thread_init(login_thread, tmp);
#if defined(CONF_FAMILY_UNIX)
	pthread_detach((pthread_t)login_account_thread);
#endif
}

// update stuff
static void update_thread(void *user)
{
	lock_wait(sql_lock);
	
	CSqlData *Data = (CSqlData *)user;
	
	// Connect to database
	if(Data->m_SqlData->connect())
	{
		try
		{
			// check if Account exists
			char buf[1024];
			str_format(buf, sizeof(buf), "SELECT * FROM %s_Account WHERE UserID='%d';", Data->m_SqlData->prefix, Data->UserID[Data->m_ClientID]);
			Data->m_SqlData->results = Data->m_SqlData->statement->executeQuery(buf);
			if(Data->m_SqlData->results->next())
			{
				// update account data	
					str_format(buf, sizeof(buf), "UPDATE %s_Account SET "
					"HouseID='%d', "
					"Money='%d', "
					"Health='%d', "
					"Armor='%d', "
					"Donor='%d', "
					"VIP='%d', "
					"Level='%d', "
					"Exp='%d', "
					"Hammer='%d', "
					"Gun='%d', "
					"Shotgun='%d', "
					"Grenade='%d', "
					"Rifle='%d', "
					"Bounty='%d', "
					"Arrested='%d', "
					"AllWeapons='%d', "
					"HealthRegen='%d', "
					"InfinityAmmo='%d', "
					"InfinityJumps='%d', "
					"FastReload='%d', "
					"NoSelfDMG='%d', "
					"GrenadeSpread='%d', "
					"GrenadeBounce='%d', "
					"GrenadeMine='%d', "
					"ShotgunSpread='%d', "
					"ShotgunExplode='%d', "
					"ShotgunStars='%d', "
					"RifleSpread='%d', "
					"RifleSwap='%d', "
					"RiflePlasma='%d', "
					"GunSpread='%d', "
					"GunExplode='%d', "
					"GunFreeze='%d', "
					"HammerWalls='%d', "
					"HammerShot='%d', "
					"HammerKill='%d', "
					"HammerExplode='%d', "
					"NinjaPermanent='%d', "
					"NinjaStart='%d', "
					"NinjaSwitch='%d', "
					"NinjaFly='%d', "
					"NinjaBomber='%d', "
					"HealHook='%d', "
					"BoostHook='%d', "
					"EndlessHook='%d', "
					"Portal='%d', "
					"PushAura='%d', "
					"PullAura='%d' "
					"Language='%s' "
					"FlameThrower='%s' "
					"WHERE UserID='%d'", 
					Data->m_SqlData->prefix, 
					Data->m_HouseID, 
					Data->m_Money, 
					Data->m_Health, 
					Data->m_Armor, 
					Data->m_Donor, 
					Data->m_VIP, 
					Data->m_Level, 
					Data->m_ExpPoints, 
					Data->m_LvlHammer, 
					Data->m_LvlGun, 
					Data->m_LvlShotgun, 
					Data->m_LvlGrenade, 
					Data->m_LvlRifle, 
					Data->m_Bounty, 
					Data->m_Arrested, 
					Data->m_AllWeapons, 
					Data->m_HealthRegen, 
					Data->m_InfinityAmmo, 
					Data->m_InfinityJumps, 
					Data->m_FastReload, 
					Data->m_NoSelfDMG, 
					Data->m_GrenadeSpread, 
					Data->m_GrenadeBounce, 
					Data->m_GrenadeMine, 
					Data->m_ShotgunSpread, 
					Data->m_ShotgunExplode, 
					Data->m_ShotgunStars, 
					Data->m_RifleSpread, 
					Data->m_RifleSwap, 
					Data->m_RiflePlasma, 
					Data->m_GunSpread, 
					Data->m_GunExplode, 
					Data->m_GunFreeze, 
					Data->m_HammerWalls, 
					Data->m_HammerShot, 
					Data->m_HammerKill, 
					Data->m_HammerExplode, 
					Data->m_NinjaPermanent, 
					Data->m_NinjaStart, 
					Data->m_NinjaSwitch, 
					Data->m_NinjaFly, 
					Data->m_NinjaBomber, 
					Data->m_HealHook, 
					Data->m_BoostHook, 
					Data->m_EndlessHook, 
					Data->m_Portal, 
					Data->m_PushAura, 
					Data->m_PullAura, 
					Data->m_aLanguage, 
					Data->m_FlameThrower, 
					Data->UserID);
				Data->m_SqlData->statement->execute(buf);
				
				// get Account name from database
				str_format(buf, sizeof(buf), "SELECT name FROM %s_Account WHERE UserID='%d';", Data->m_SqlData->prefix, Data->UserID[Data->m_ClientID]);
				
				// create results
				Data->m_SqlData->results = Data->m_SqlData->statement->executeQuery(buf);

				// jump to result
				Data->m_SqlData->results->next();
				
				// finally the nae is there \o/
				char acc_name[32];
				str_copy(acc_name, Data->m_SqlData->results->getString("name").c_str(), sizeof(acc_name));	
				dbg_msg("SQL", "Account '%s' was saved successfully", acc_name);
			}
			else
				dbg_msg("SQL", "Account seems to be deleted");
			
			// delete statement and results
			delete Data->m_SqlData->statement;
			delete Data->m_SqlData->results;
		}
		catch (sql::SQLException &e)
		{
			dbg_msg("SQL", "ERROR: Could not update Account");
		}
		
		// disconnect from database
		Data->m_SqlData->disconnect();
	}
	
	delete Data;
	
	lock_release(sql_lock);
}

void CSQL::update(int m_ClientID)
{
	CSqlData *tmp = new CSqlData();
	tmp->m_ClientID = m_ClientID;
	tmp->UserID[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_UserID;
	tmp->m_ExpPoints[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_ExpPoints;
	tmp->m_Money[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_Money;
	tmp->m_Level[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_Level;
	tmp->m_AllWeapons[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_AllWeapons;
	tmp->m_Armor[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_Armor;
	tmp->m_Arrested[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_Arrested;
	tmp->m_BoostHook[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_BoostHook;
	tmp->m_Bounty[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_Bounty;
	tmp->m_Donor[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_Donor;
	tmp->m_EndlessHook[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_EndlessHook;
	tmp->m_ExpPoints[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_ExpPoints;
	tmp->m_FastReload[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_FastReload;
	tmp->m_FlameThrower[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_FlameThrower;
	tmp->m_GrenadeBounce[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_GrenadeBounce;
	tmp->m_GrenadeMine[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_GrenadeMine;
	tmp->m_GrenadeSpread[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_GrenadeSpread;
	tmp->m_GunExplode[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_GunExplode;
	tmp->m_GunFreeze[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_GunFreeze;
	tmp->m_GunSpread[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_GunSpread;
	tmp->m_ShotgunExplode[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_ShotgunExplode;
	tmp->m_ShotgunSpread[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_ShotgunSpread;
	tmp->m_ShotgunStars[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_ShotgunStars;
	tmp->m_HammerExplode[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_HammerExplode;
	tmp->m_HammerKill[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_HammerKill;
	tmp->m_HammerShot[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_HammerShot;
	tmp->m_HammerWalls[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_HammerWalls;
	tmp->m_HealHook[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_HealHook;
	tmp->m_Health[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_Health;
	tmp->m_HealthRegen[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_HealthRegen;
	tmp->m_HouseID[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_HouseID;
	tmp->m_InfinityAmmo[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_InfinityAmmo;
	tmp->m_InfinityJumps[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_InfinityAmmo;
	tmp->m_NinjaBomber[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_NinjaBomber;
	tmp->m_NinjaFly[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_NinjaFly;
	tmp->m_NinjaPermanent[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_NinjaPermanent;
	tmp->m_NinjaStart[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_NinjaStart;
	tmp->m_NinjaSwitch[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_NinjaSwitch;
	tmp->m_VIP[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_VIP;
	tmp->m_NoSelfDMG[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_NoSelfDMG;
	tmp->m_Portal[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_Portal;
	tmp->m_PullAura[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_PullAura;
	tmp->m_PushAura[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_PushAura;
	tmp->m_RiflePlasma[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_RiflePlasma;
	tmp->m_RifleSpread[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_RifleSpread;
	tmp->m_RifleSwap[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_RifleSwap;
	tmp->m_LvlGun[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_LvlWeapon[WEAPON_GUN];
	tmp->m_LvlShotgun[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_LvlWeapon[WEAPON_SHOTGUN];
	tmp->m_LvlGrenade[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_LvlWeapon[WEAPON_GRENADE];
	tmp->m_LvlRifle[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_LvlWeapon[WEAPON_RIFLE];
	tmp->m_LvlHammer[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->m_AccData.m_LvlWeapon[WEAPON_HAMMER];
	tmp->m_aLanguage[m_ClientID] = GameServer()->m_apPlayers[m_ClientID]->GetLanguage();


		
	tmp->m_SqlData = this;
	
	void *update_account_thread = thread_init(update_thread, tmp);
#if defined(CONF_FAMILY_UNIX)
	pthread_detach((pthread_t)update_account_thread);
#endif
}

// update all
void CSQL::update_all()
{
	lock_wait(sql_lock);
	
	// Connect to database
	if(connect())
	{
		try
		{
			char buf[512];
			char acc_name[32];
			for(int i = 0; i < MAX_PLAYERS; i++)
			{
				if(!GameServer()->m_apPlayers[i])
					continue;
				
				if(!GameServer()->m_apPlayers[i]->m_AccData.m_UserID)
					continue;
				
				// check if Account exists
				str_format(buf, sizeof(buf), "SELECT * FROM %s_Account WHERE UserID='%d';", prefix, GameServer()->m_apPlayers[i]->m_AccData.m_UserID);
				results = statement->executeQuery(buf);
				if(results->next())
				{
					// update account data	
					str_format(buf, sizeof(buf), "UPDATE %s_Account SET "
					"HouseID='%d', "
					"Money='%d', "
					"Health='%d', "
					"Armor='%d', "
					"Donor='%d', "
					"VIP='%d', "
					"Level='%d', "
					"Exp='%d', "
					"Hammer='%d', "
					"Gun='%d', "
					"Shotgun='%d', "
					"Grenade='%d', "
					"Rifle='%d', "
					"Bounty='%d', "
					"Arrested='%d', "
					"AllWeapons='%d', "
					"HealthRegen='%d', "
					"InfinityAmmo='%d', "
					"InfinityJumps='%d', "
					"FastReload='%d', "
					"NoSelfDMG='%d', "
					"GrenadeSpread='%d', "
					"GrenadeBounce='%d', "
					"GrenadeMine='%d', "
					"ShotgunSpread='%d', "
					"ShotgunExplode='%d', "
					"ShotgunStars='%d', "
					"RifleSpread='%d', "
					"RifleSwap='%d', "
					"RiflePlasma='%d', "
					"GunSpread='%d', "
					"GunExplode='%d', "
					"GunFreeze='%d', "
					"HammerWalls='%d', "
					"HammerShot='%d', "
					"HammerKill='%d', "
					"HammerExplode='%d', "
					"NinjaPermanent='%d', "
					"NinjaStart='%d', "
					"NinjaSwitch='%d', "
					"NinjaFly='%d', "
					"NinjaBomber='%d', "
					"HealHook='%d', "
					"BoostHook='%d', "
					"EndlessHook='%d', "
					"Portal='%d', "
					"PushAura='%d', "
					"PullAura='%d' "
					"Language='%s' "
					"FlameThrower='%s' "
					"WHERE UserID='%d'", 
					prefix, 
					GameServer()->m_apPlayers[i]->m_AccData.m_HouseID, 
					GameServer()->m_apPlayers[i]->m_AccData.m_Money, 
					GameServer()->m_apPlayers[i]->m_AccData.m_Health, 
					GameServer()->m_apPlayers[i]->m_AccData.m_Armor, 
					GameServer()->m_apPlayers[i]->m_AccData.m_Donor, 
					GameServer()->m_apPlayers[i]->m_AccData.m_VIP, 
					GameServer()->m_apPlayers[i]->m_AccData.m_Level, 
					GameServer()->m_apPlayers[i]->m_AccData.m_ExpPoints, 
					GameServer()->m_apPlayers[i]->m_AccData.m_LvlWeapon[WEAPON_HAMMER], 
					GameServer()->m_apPlayers[i]->m_AccData.m_LvlWeapon[WEAPON_GUN], 
					GameServer()->m_apPlayers[i]->m_AccData.m_LvlWeapon[WEAPON_SHOTGUN], 
					GameServer()->m_apPlayers[i]->m_AccData.m_LvlWeapon[WEAPON_GRENADE], 
					GameServer()->m_apPlayers[i]->m_AccData.m_LvlWeapon[WEAPON_RIFLE], 
					GameServer()->m_apPlayers[i]->m_AccData.m_Bounty, 
					GameServer()->m_apPlayers[i]->m_AccData.m_Arrested, 
					GameServer()->m_apPlayers[i]->m_AccData.m_AllWeapons, 
					GameServer()->m_apPlayers[i]->m_AccData.m_HealthRegen, 
					GameServer()->m_apPlayers[i]->m_AccData.m_InfinityAmmo, 
					GameServer()->m_apPlayers[i]->m_AccData.m_InfinityJumps, 
					GameServer()->m_apPlayers[i]->m_AccData.m_FastReload, 
					GameServer()->m_apPlayers[i]->m_AccData.m_NoSelfDMG, 
					GameServer()->m_apPlayers[i]->m_AccData.m_GrenadeSpread, 
					GameServer()->m_apPlayers[i]->m_AccData.m_GrenadeBounce, 
					GameServer()->m_apPlayers[i]->m_AccData.m_GrenadeMine, 
					GameServer()->m_apPlayers[i]->m_AccData.m_ShotgunSpread, 
					GameServer()->m_apPlayers[i]->m_AccData.m_ShotgunExplode, 
					GameServer()->m_apPlayers[i]->m_AccData.m_ShotgunStars, 
					GameServer()->m_apPlayers[i]->m_AccData.m_RifleSpread, 
					GameServer()->m_apPlayers[i]->m_AccData.m_RifleSwap, 
					GameServer()->m_apPlayers[i]->m_AccData.m_RiflePlasma, 
					GameServer()->m_apPlayers[i]->m_AccData.m_GunSpread, 
					GameServer()->m_apPlayers[i]->m_AccData.m_GunExplode, 
					GameServer()->m_apPlayers[i]->m_AccData.m_GunFreeze, 
					GameServer()->m_apPlayers[i]->m_AccData.m_HammerWalls, 
					GameServer()->m_apPlayers[i]->m_AccData.m_HammerShot, 
					GameServer()->m_apPlayers[i]->m_AccData.m_HammerKill, 
					GameServer()->m_apPlayers[i]->m_AccData.m_HammerExplode, 
					GameServer()->m_apPlayers[i]->m_AccData.m_NinjaPermanent, 
					GameServer()->m_apPlayers[i]->m_AccData.m_NinjaStart, 
					GameServer()->m_apPlayers[i]->m_AccData.m_NinjaSwitch, 
					GameServer()->m_apPlayers[i]->m_AccData.m_NinjaFly, 
					GameServer()->m_apPlayers[i]->m_AccData.m_NinjaBomber, 
					GameServer()->m_apPlayers[i]->m_AccData.m_HealHook, 
					GameServer()->m_apPlayers[i]->m_AccData.m_BoostHook, 
					GameServer()->m_apPlayers[i]->m_AccData.m_EndlessHook, 
					GameServer()->m_apPlayers[i]->m_AccData.m_Portal, 
					GameServer()->m_apPlayers[i]->m_AccData.m_PushAura, 
					GameServer()->m_apPlayers[i]->m_AccData.m_PullAura, 
					GameServer()->m_apPlayers[i]->GetLanguage(), 
					GameServer()->m_apPlayers[i]->m_AccData.m_FlameThrower, 
					GameServer()->m_apPlayers[i]->m_AccData.m_UserID);
					statement->execute(buf);

					// get Account name from database
					str_format(buf, sizeof(buf), "SELECT name FROM %s_Account WHERE UserID='%d';", prefix, GameServer()->m_apPlayers[i]->m_AccData.m_UserID);
					
					// create results
					results = statement->executeQuery(buf);

					// jump to result
					results->next();
					
					// finally the name is there \o/	
					str_copy(acc_name, results->getString("name").c_str(), sizeof(acc_name));	
					dbg_msg("SQL", "Account '%s' was saved successfully", acc_name);
				}
				else
					dbg_msg("SQL", "Account seems to be deleted");
				
				// delete results
				delete results;
			}
			
			// delete statement
			delete statement;
		}
		catch (sql::SQLException &e)
		{
			dbg_msg("SQL", "ERROR: Could not update Account");
		}
		
		// disconnect from database
		disconnect();
	}

	lock_release(sql_lock);
}
	
	
