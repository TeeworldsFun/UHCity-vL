#ifndef ENGINE_SERVER_SQL_JOB_H
#define ENGINE_SERVER_SQL_JOB_H

#include <base/tl/array.h>
#include <game/server/player.h>
#include "sql_string_helpers.h"
#include "sql_connector.h"

class CSqlJob
{
	class IGameServer *m_pGameServer;
protected:
	bool m_ReadOnly;
	int m_Instance;

public:
	array<CSqlJob*> m_QueuedJobs;
	
public:
	virtual ~CSqlJob();

	class IGameServer *GameServer() { return m_pGameServer; }

	void StartReadOnly();
	void Start(bool ReadOnly=false);
	static void Exec(void* pDataSelf);
	
	void AddQueuedJob(CSqlJob* pJob);
	virtual void* GenerateChildData() { return 0x0; };
	virtual void ProcessParentData(void* pData) {};
	
	virtual bool Job(CSqlServer* pSqlServer) = 0;
	virtual void CleanInstanceRef() {}
	
	int GetInstance() { return m_Instance; }
};

#endif