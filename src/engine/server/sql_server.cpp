#include <base/system.h>
#include <engine/shared/protocol.h>
#include <engine/shared/config.h>

#include "sql_server.h"
//#include <cppconn/driver.h>
int CSqlServer::ms_NumReadServer = 0;
int CSqlServer::ms_NumWriteServer = 0;

/*
CSqlServer::CSqlServer(const char* pDatabase, const char* pPrefix, const char* pUser, const char* pPass, const char* pIp, int Port, bool ReadOnly, bool SetUpDb) :
		m_Port(Port),
		m_SetUpDB(SetUpDb)
*/
CSqlServer::CSqlServer(const char* pDatabase, const char* pPrefix, const char* pUser, const char* pPass, const char* pIp, int Port, bool ReadOnly) :
		m_Port(Port)
{
	str_copy(m_aDatabase, pDatabase, sizeof(m_aDatabase));
	str_copy(m_aPrefix, pPrefix, sizeof(m_aPrefix));
	str_copy(m_aUser, pUser, sizeof(m_aUser));
	str_copy(m_aPass, pPass, sizeof(m_aPass));
	str_copy(m_aIp, pIp, sizeof(m_aIp));

	m_pDriver = 0;
	m_pConnection = 0;
	m_pResults = 0;
	m_pStatement = 0;

	ReadOnly ? ms_NumReadServer++ : ms_NumWriteServer++;

	m_SqlLock = lock_create();
}

CSqlServer::~CSqlServer()
{
	Lock();
	try
	{
		if (m_pResults)
			delete m_pResults;
		if (m_pConnection)
			delete m_pConnection;
		dbg_msg("sql", "SQL connection disconnected");
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "ERROR: No SQL connection");
	}
	UnLock();
	lock_destroy(m_SqlLock);
}

bool CSqlServer::Connect()
{
	Lock();

	if (m_pDriver != NULL && m_pConnection != NULL)
	{
		try
		{
			// Connect to specific database
			m_pConnection->setSchema(m_aDatabase);
		}
		catch (sql::SQLException &e)
		{
			dbg_msg("sql", "MySQL Error: %s", e.what());
			dbg_msg("sql", "ERROR: SQL connection failed");
			UnLock();
			return false;
		}
		return true;
	}

	try
	{
		m_pDriver = 0;
		m_pConnection = 0;
		m_pStatement = 0;

		sql::ConnectOptionsMap connection_properties;
		connection_properties["hostName"]      = sql::SQLString(m_aIp);
		connection_properties["port"]          = m_Port;
		connection_properties["userName"]      = sql::SQLString(m_aUser);
		connection_properties["password"]      = sql::SQLString(m_aPass);
		connection_properties["OPT_CONNECT_TIMEOUT"] = 10;
		connection_properties["OPT_READ_TIMEOUT"] = 10;
		connection_properties["OPT_WRITE_TIMEOUT"] = 20;
		connection_properties["OPT_RECONNECT"] = true;

		// Create connection
		m_pDriver = get_driver_instance();
		m_pConnection = m_pDriver->connect(connection_properties);

		// Create Statement
		m_pStatement = m_pConnection->createStatement();

		// Connect to specific database
		m_pConnection->setSchema(m_aDatabase);
		dbg_msg("sql", "sql connection established");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: sql connection failed");
		UnLock();
		return false;
	}
	catch (const std::exception& ex)
	{
		// ...
		dbg_msg("sql", "1 %s",ex.what());

	}
	catch (const std::string& ex)
	{
		// ...
		dbg_msg("sql", "2 %s",ex.c_str());
	}
	catch( int )
	{
		dbg_msg("sql", "3 %s");
	}
	catch( float )
	{
		dbg_msg("sql", "4 %s");
	}

	catch( char[] )
	{
		dbg_msg("sql", "5 %s");
	}

	catch( char )
	{
		dbg_msg("sql", "6 %s");
	}
	catch (...)
	{
		dbg_msg("sql", "Unknown Error cause by the MySQL/C++ Connector, my advice compile server_debug and use it");

		dbg_msg("sql", "ERROR: sql connection failed");
		UnLock();
		return false;
	}
	UnLock();
	return false;
}

void CSqlServer::Disconnect()
{
	UnLock();
}

void CSqlServer::CreateTables()
{
	return;
}

void CSqlServer::executeSql(const char *pCommand)
{
	m_pStatement->execute(pCommand);
}

void CSqlServer::executeSqlQuery(const char *pQuery)
{
	if (m_pResults)
		delete m_pResults;

	// set it to 0, so exceptions raised from executeQuery can not make m_pResults point to invalid memory
	m_pResults = 0;
	m_pResults = m_pStatement->executeQuery(pQuery);
}

