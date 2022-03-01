/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/entities/projectile.h>
#include "hammerkill.h"

CHammerKill::CHammerKill(CGameWorld *pGameWorld, int Owner, int Victim)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE)
{
	m_Owner = Owner;
	m_Victim = Victim;
	m_VictimTick = 10*50;
	m_HammerCount = 0;
	m_Power = -2;

	GameWorld()->InsertEntity(this);
}

void CHammerKill::Reset()
{
	CCharacter *pVictim = GameServer()->GetPlayerChar(m_Victim);

	if(pVictim)
		pVictim->m_IsHammerKilled = false;

	GameServer()->m_World.DestroyEntity(this);
}

void CHammerKill::Tick()
{
	if(GameServer()->m_apPlayers[m_Owner]->m_onMonster)
		return;

	CCharacter *Victim = GameServer()->GetPlayerChar(m_Victim);
	CCharacter *Owner = GameServer()->GetPlayerChar(m_Owner);

	if (m_Power == 0)
		m_Power = 1;
		
	m_VictimTick-=m_Power;

	if(Server()->Tick() % Server()->TickSpeed() * 3 == 0)
		m_Power++;

	if(Victim && Owner && m_VictimTick)
	{
		if (m_HammerCount == 7)
		{
			GameServer()->CreateExplosion(Owner->m_Pos, m_Victim, WEAPON_HAMMER, false);
			Reset();
			return;
		}

		for(int i = 0; i <= 360; i+=36)
		{
			vec2 TempPos = Victim->m_Pos + normalize(GetDir(pi/180 * (i+m_VictimTick))) * (m_VictimTick/3);
			new CProjectile(GameWorld(), WEAPON_HAMMER,	m_Owner, TempPos, vec2(0, 0), 1, 1, 0, 0, -1, 0);
		}
		
		if((float)(m_VictimTick/3) < 28)
		{
			CCharacter *pChr = GameServer()->m_apPlayers[m_Owner]->GetCharacter();
			if (pChr)
			{
				pChr->m_EmoteType = EMOTE_HAPPY;
				pChr->m_EmoteStop = Server()->Tick() + Server()->TickSpeed();
			}

			Victim->Die(m_Owner, WEAPON_HAMMER);
			GameServer()->m_World.DestroyEntity(this);
			return;
		}
			
	}
	else
		Reset();	
}