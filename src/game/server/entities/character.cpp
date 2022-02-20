/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <new>
#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <game/mapitems.h>

#include "character.h"
#include "laser.h"
#include "projectile.h"
#include "game/server/city/items/plasma.h"
#include "game/server/city/items/hammerkill.h"
#include "game/server/city/wall.h"
#include "game/server/city/gui.h"
#include "game/server/city/items/crown.h"
#include "game/server/city/healstate.h"
#include "game/server/city/items/gravaura.h"
#include "game/server/city/transfer.h"
#include "game/server/city/entities/spawnprotect.h"

//input count
struct CInputCount
{
	int m_Presses;
	int m_Releases;
};

CInputCount CountInput(int Prev, int Cur)
{
	CInputCount c = {0, 0};
	Prev &= INPUT_STATE_MASK;
	Cur &= INPUT_STATE_MASK;
	int i = Prev;

	while(i != Cur)
	{
		i = (i+1)&INPUT_STATE_MASK;
		if(i&1)
			c.m_Presses++;
		else
			c.m_Releases++;
	}

	return c;
}

MACRO_ALLOC_POOL_ID_IMPL(CCharacter, MAX_CLIENTS)

// Character, "physical" player's part
CCharacter::CCharacter(CGameWorld *pWorld)
: CEntity(pWorld, CGameWorld::ENTTYPE_CHARACTER)
{
	m_ProximityRadius = ms_PhysSize;
	m_Health = 0;
	m_Armor = 0;
}

void CCharacter::Reset()
{
	Destroy();
}

bool CCharacter::Spawn(CPlayer *pPlayer, vec2 Pos)
{
	m_EmoteStop = 1;
	m_LastAction = -1;
	m_ActiveWeapon = pPlayer->m_Insta ? WEAPON_RIFLE:WEAPON_GUN;

	if(pPlayer->m_Insta)
		m_aWeapons[WEAPON_RIFLE].m_Ammo = 10;

	m_LastWeapon = WEAPON_HAMMER;
	m_QueuedWeapon = -1;

	m_pPlayer = pPlayer;
	m_Pos = Pos;
	m_DesiredPos = Pos;
	m_LastWeapon = WEAPON_HAMMER;

	m_Core.Reset();
	m_Core.Init(&GameServer()->m_World.m_Core, GameServer()->Collision());
	m_Core.m_Pos = m_Pos;
	m_Core.m_ClientID = m_pPlayer->GetCID();
	m_Core.m_Afk = GetPlayer()->m_Afk;
	GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = &m_Core;

	m_ReckoningTick = 0;
	mem_zero(&m_SendCore, sizeof(m_SendCore));
	mem_zero(&m_ReckoningCore, sizeof(m_ReckoningCore));

	GameServer()->m_World.InsertEntity(this);
	m_Alive = true;

	// City
	if(!m_pPlayer->m_Insta)
	{
		m_Protected = true;
		m_Core.m_Protected = true;
		new CSpawProtect(GameWorld(), m_pPlayer->GetCID());
	}

	m_InstaKills = 0;
	m_GravityY = 0.5;
	m_GravityX = 0;
	m_ShopGroup = ITEM_GENERAL;
	m_ShopPage = 0;
	m_Walls = 0;
	m_Plasma = 0;
	m_FreezeEnd = false;
	m_HammerPos1 = vec2(0, 0);
	m_HammerPos2 = vec2(0, 0);
	m_GunFreezeCooldown = 0;
	m_ExternalHeal = 0;
	m_LastHooked = -1;
	m_LastAtkID = -1;
	m_LastAtkWeapon = WEAPON_GAME;
	m_LastAtkIDTimer = 0;

	if (GetPlayer()->m_AccData.m_EndlessHook)
		m_Core.m_EndlessHook = true;

	m_SpawnProtection = Server()->Tick();

	new CGui(GameWorld(), m_pPlayer->GetCID());
	new CCrown(GameWorld(), m_pPlayer->GetCID());
	new CHealstate(GameWorld(), m_pPlayer->GetCID());

	m_pPlayer->m_Crown = true;

	m_GameZone = false;
	m_IsHammerKilled = false;

	GameServer()->m_TeleNR[m_pPlayer->GetCID()] = 0;
	m_Home = 0;

	GameServer()->m_pController->OnCharacterSpawn(this);
	GameServer()->SendTuningParams(m_pPlayer->GetCID());
	return true;
}

void CCharacter::Destroy()
{
	GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	m_Alive = false;
}

void CCharacter::SetWeapon(int W)
{
	if(W == m_ActiveWeapon || (m_pPlayer->m_Insta && m_ActiveWeapon == WEAPON_RIFLE))
		return;

	m_LastWeapon = m_ActiveWeapon;
	m_QueuedWeapon = -1;
	m_ActiveWeapon = W;
	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SWITCH);

	if(m_ActiveWeapon < 0 || m_ActiveWeapon >= NUM_WEAPONS)
		m_ActiveWeapon = 0;
}

bool CCharacter::IsGrounded()
{
	if (m_Core.m_IgnoreGround)
		return false;
	if (GameServer()->Collision()->CheckPoint(m_Pos.x+m_ProximityRadius/2, m_Pos.y+m_ProximityRadius/2+5))
		return true;
	if (GameServer()->Collision()->CheckPoint(m_Pos.x-m_ProximityRadius/2, m_Pos.y+m_ProximityRadius/2+5))
		return true;
	return false;
}

// City
void CCharacter::Tele()
{
	vec2 TelePos = m_Pos + vec2(m_Input.m_TargetX,m_Input.m_TargetY);

	if (!GameServer()->Collision()->IsTile(TelePos, TILE_SOLID) || m_pPlayer->m_God)
	{
		float Dist = distance(TelePos, m_Pos);

		if (Dist > 500) {
			GameServer()->SendChatTarget_Localization(m_pPlayer->GetCID(), CHATCATEGORY_INFO, _("Out of Range"));
			return;
		}

		for (int i = 1; i < Dist; i += 32)
		{
			vec2 TestPos = m_Pos + normalize(TelePos - m_Pos) * i;
			if (!m_pPlayer->m_God)
			{
				if (GameServer()->Collision()->IsTile(TestPos, TILE_ANTI_TELE)
					|| GameServer()->Collision()->IsTile(TestPos, TILE_POLICE)
					|| GameServer()->Collision()->IsTile(TestPos, TILE_ADMIN)
					|| GameServer()->Collision()->IsTile(TestPos, TILE_DONOR))
				{
					GameServer()->SendChatTarget_Localization(m_pPlayer->GetCID(), CHATCATEGORY_INFO, _("You can't teleport there."));
					return;
				}
			}
		}

		if (Protected())
		{
			GameServer()->SendChatTarget_Localization(m_pPlayer->GetCID(), CHATCATEGORY_INFO, _("You can't tele while being protected"));
			return;
		}
	}
	else
		return;

	m_Core.m_Pos = TelePos;
	CNetEvent_Spawn* pEvent = (CNetEvent_Spawn*)GameServer()->m_Events.Create(NETEVENTTYPE_SPAWN, sizeof(CNetEvent_Spawn));
	if (pEvent)
	{
		pEvent->m_X = (int)TelePos.x;
		pEvent->m_Y = (int)TelePos.y;
	}
}

void CCharacter::SaveLoad(int Value)
{
	if(!Value)
	{
		if(!GameServer()->Collision()->CheckPoint(m_Pos))
			m_SavePos = m_Pos;
	}
	else if(m_SavePos == vec2(0,0))
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Set a position first"); 
		return;
	}
	
	if (!Protected()) {
		m_Core.m_Pos = m_SavePos;

		CNetEvent_Spawn* pEvent = (CNetEvent_Spawn*)GameServer()->m_Events.Create(NETEVENTTYPE_SPAWN, sizeof(CNetEvent_Spawn));

		if (pEvent)
		{
			pEvent->m_X = m_SavePos.x;
			pEvent->m_Y = m_SavePos.y;
		}
	}
}

void CCharacter::Move(int dir) 
{
	int toX = 0;
	int toY = 0;

	switch (dir)
	{
		case 0:
			toY = -32;
			break;
		case 1:
			toX = -32;
			break;
		case 2:
			toY = 32;
			break;
		case 3:
			toX = 32;
			break;
		default:
			break;
	}

	vec2 movePos = m_Pos + vec2(toX, toY);

	if (GameServer()->Collision()->IsTile(movePos, TILE_ANTI_TELE)
		|| (GameServer()->Collision()->IsTile(movePos, TILE_DEATH))
		|| (GameServer()->Collision()->IsTile(movePos, TILE_FREEZE))
		|| (GameServer()->Collision()->IsTile(movePos, TILE_POLICE) && !Server()->IsAuthed(m_pPlayer->GetCID()))
		|| (GameServer()->Collision()->IsTile(movePos, TILE_ADMIN) && !Server()->IsAdmin(m_pPlayer->GetCID()))
		|| (GameServer()->Collision()->IsTile(movePos, TILE_DONOR) && !m_pPlayer->m_AccData.m_Donor)
		|| (GameServer()->Collision()->IsTile(movePos, TILE_MONEY_DONOR) && !m_pPlayer->m_AccData.m_Donor))
		return;

	m_Core.m_Pos = movePos;
}

void CCharacter::Buy(const char *Name, int *Upgrade, long long unsigned Price, int Click, int Max)
{
	char aBuf[128];
	char numBuf[2][16];
	const char* pLanguage = GetPlayer()->GetLanguage();
	dynamic_string Buffer;

	Price = floor(Price * pow(*Upgrade + 1, 2.0) * pow(*Upgrade + 1, -1.7));

	if(Click == 1)
	{
		if(*Upgrade < Max)
		{
			if(m_pPlayer->m_AccData.m_Money >= Price)
			{
				if(Server()->Tick() - m_BuyTick > 50)
				{
					*Upgrade += 1;
					m_pPlayer->m_AccData.m_Money -= Price;
					str_format(aBuf, sizeof(aBuf), "%s (%d/%d)", Name, *Upgrade, Max);

					if(m_pPlayer->m_AccData.m_UserID)
						m_pPlayer->m_pAccount->Apply();

					m_BuyTick = Server()->Tick();
					GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
					GameServer()->FormatInt(m_pPlayer->m_AccData.m_Money, numBuf[0]);
					Server()->Localization()->Format_L(Buffer, pLanguage, _("Money: {str:m}$"), "m", numBuf[0], NULL);
					SendBroadcast(Buffer.buffer(), m_pPlayer->GetCID());
				}
			}
			else
			{
				GameServer()->FormatInt(Price, numBuf[0]);
				GameServer()->FormatInt(m_pPlayer->m_AccData.m_Money, numBuf[1]);
				Server()->Localization()->Format_L(Buffer, pLanguage, _("Not enough money\n{str:Name}: {str:nb0}$\nMoney: {str:nb1}$"), "Name", Name, "nb0", numBuf[0], "nb1", numBuf[1], NULL);
				SendBroadcast(Buffer.buffer(), m_pPlayer->GetCID());
			}
		}
		else
		{
			Server()->Localization()->Format_L(Buffer, pLanguage, _("Maximum '{str:Name}' ({int:Upgr}/{int:Max})"), "Name", Name, "Upgr", &*Upgrade, "Max", &Max, NULL);
			SendBroadcast(Buffer.buffer(), m_pPlayer->GetCID());
		}
	}
	else if(Click == 2)
	{
		GameServer()->FormatInt(Price, numBuf[0]);
		GameServer()->FormatInt(m_pPlayer->m_AccData.m_Money, numBuf[1]);
		Server()->Localization()->Format_L(Buffer, pLanguage, _("{str:Name} ({int:upgr}/{int:max})\nCost: {str:cost}$\nMoney: {str:money}$"), 
		"Name", Name, 
		"upgr", &*Upgrade, 
		"max", &Max, 
		"cost", numBuf[0], 
		"money", numBuf[1], NULL);
		SendBroadcast(Buffer.buffer(), m_pPlayer->GetCID());
	}
}

int CCharacter::ActiveWeapon()
{
	return m_ActiveWeapon;
}

int CCharacter::MouseEvent(vec2 Pos)
{
	if(distance(Pos, m_Pos+vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY)) < 64)
	{
		if(m_Input.m_Hook)
		{
			m_Input.m_Hook = 0;
			return 1;
		}

		if((m_Input.m_Fire&1) != 0)
			return -1;

		return 2;
	}
	
	return 0;
}

void CCharacter::HandleNinja()
{
	if(m_ActiveWeapon != WEAPON_NINJA)
		return;

	if(m_Water || m_SingleWater)
		return;

	// City
	if(!m_pPlayer->m_AccData.m_NinjaPermanent && (Server()->Tick() - m_Ninja.m_ActivationTick) > (g_pData->m_Weapons.m_Ninja.m_Duration * Server()->TickSpeed() / 1000) && !m_Frozen)
	{
		// time's up, return
		m_aWeapons[WEAPON_NINJA].m_Got = false;

		m_ActiveWeapon = m_FreezeEnd ? m_FreezeWeapon : m_LastWeapon;

		SetWeapon(m_ActiveWeapon);
		return;
	}

	if(m_Frozen)
		return;

	if (Protected())
		m_Ninja.m_CurrentMoveTime = 0;

	// force ninja Weapon
	SetWeapon(WEAPON_NINJA);

	m_Ninja.m_CurrentMoveTime--;

	if (m_Ninja.m_CurrentMoveTime == 0)
	{
		// reset velocity
		m_Core.m_Vel = m_Ninja.m_ActivationDir*m_Ninja.m_OldVelAmount;
	}

	if (m_Ninja.m_CurrentMoveTime > 0)
	{
		if (m_pPlayer->m_NinjaBomber && m_Pos.x != m_DesiredPos.x) {
			vec2 Direction = vec2(0, 1);
			vec2 ProjStartPos = m_Pos+Direction*m_ProximityRadius*0.75f;

			CMsgPacker Msg(NETMSGTYPE_SV_EXTRAPROJECTILE);
			CProjectile *pProj = new CProjectile(GameWorld(), WEAPON_GRENADE,
			m_pPlayer->GetCID(),
			ProjStartPos,
			Direction,
			(int)(Server()->TickSpeed()*GameServer()->Tuning()->m_GrenadeLifetime),
			1, true, 0, SOUND_GRENADE_EXPLODE, WEAPON_GRENADE);

			// pack the Projectile and send it to the client Directly
			CNetObj_Projectile p;
			pProj->FillInfo(&p);

			
			Msg.AddInt(1);
			for(unsigned i = 0; i < sizeof(CNetObj_Projectile)/sizeof(int); i++)
				Msg.AddInt(((int *)&p)[i]);
		}
		
		// Set velocity
		m_Core.m_Vel = m_Ninja.m_ActivationDir * g_pData->m_Weapons.m_Ninja.m_Velocity;
		vec2 OldPos = m_Pos;
		GameServer()->Collision()->MoveBox(&m_Core.m_Pos, &m_Core.m_Vel, vec2(m_ProximityRadius, m_ProximityRadius), 0.f);

		// reset velocity so the client doesn't predict stuff
		m_Core.m_Vel = vec2(0.f, 0.f);

		// check if we Hit anything along the way
		{
			CCharacter *aEnts[MAX_CLIENTS];
			vec2 Dir = m_Pos - OldPos;
			float Radius = m_ProximityRadius * 2.0f;
			vec2 Center = OldPos + Dir * 0.5f;
			int Num = GameServer()->m_World.FindEntities(Center, Radius, (CEntity**)aEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

			for (int i = 0; i < Num; ++i)
			{
				if (aEnts[i] == this)
					continue;

				// make sure we haven't Hit this object before
				bool bAlreadyHit = false;
				for (int j = 0; j < m_NumObjectsHit; j++)
				{
					if (m_apHitObjects[j] == aEnts[i])
						bAlreadyHit = true;
				}
				if (bAlreadyHit)
					continue;

				// check so we are sufficiently close
				if (distance(aEnts[i]->m_Pos, m_Pos) > (m_ProximityRadius * 2.0f))
					continue;

				// Hit a player, give him damage and stuffs...
				GameServer()->CreateSound(aEnts[i]->m_Pos, SOUND_NINJA_HIT);
				// set his velocity to fast upward (for now)
				if(m_NumObjectsHit < 10)
					m_apHitObjects[m_NumObjectsHit++] = aEnts[i];

				aEnts[i]->TakeDamage(vec2(0, -10.0f), g_pData->m_Weapons.m_Ninja.m_pBase->m_Damage, m_pPlayer->GetCID(), WEAPON_NINJA);
			}
		}

		return;
	}

	return;
}

void CCharacter::DoWeaponSwitch()
{
	// City
	// make sure we can switch
	if(m_ReloadTimer != 0 || m_QueuedWeapon == -1 || (m_aWeapons[WEAPON_NINJA].m_Got && !m_pPlayer->m_AccData.m_NinjaSwitch) || m_Frozen)
		return;

	// switch Weapon
	SetWeapon(m_QueuedWeapon);
}

void CCharacter::HandleWeaponSwitch()
{
	int WantedWeapon = m_ActiveWeapon;
	if(m_QueuedWeapon != -1)
		WantedWeapon = m_QueuedWeapon;

	// select Weapon
	int Next = CountInput(m_LatestPrevInput.m_NextWeapon, m_LatestInput.m_NextWeapon).m_Presses;
	int Prev = CountInput(m_LatestPrevInput.m_PrevWeapon, m_LatestInput.m_PrevWeapon).m_Presses;

	if(Next < 128) // make sure we only try sane stuff
	{
		while(Next) // Next Weapon selection
		{
			WantedWeapon = (WantedWeapon+1)%NUM_WEAPONS;
			if(m_aWeapons[WantedWeapon].m_Got)
				Next--;
		}
	}

	if(Prev < 128) // make sure we only try sane stuff
	{
		while(Prev) // Prev Weapon selection
		{
			WantedWeapon = (WantedWeapon-1)<0?NUM_WEAPONS-1:WantedWeapon-1;
			if(m_aWeapons[WantedWeapon].m_Got)
				Prev--;
		}
	}

	// Direct Weapon selection
	if(m_LatestInput.m_WantedWeapon)
		WantedWeapon = m_Input.m_WantedWeapon-1;

	// check for insane values
	if(WantedWeapon >= 0 && WantedWeapon < NUM_WEAPONS && WantedWeapon != m_ActiveWeapon && m_aWeapons[WantedWeapon].m_Got)
		m_QueuedWeapon = WantedWeapon;

	DoWeaponSwitch();
}

void CCharacter::SwitchShop(int Value) {
	if(GameServer()->Collision()->TileShop(m_Pos))
	{
		m_Shop++;

		if (m_Shop < ITEM_HAMMER)
			m_Shop = ITEM_JUMP;
		else if (m_Shop > ITEM_JUMP)
			m_Shop = ITEM_HAMMER;
		else if (m_Shop == ITEM_HAMMER)
        	m_ShopGroup = ITEM_HAMMER;
		else if (m_Shop == ITEM_GUN)
        	m_ShopGroup = ITEM_GUN;
	    else if (m_Shop == ITEM_SHOTGUN)
    	    m_ShopGroup = ITEM_SHOTGUN;
    	else if (m_Shop == ITEM_GRENADE)
        	m_ShopGroup = ITEM_GRENADE;
	    else if (m_Shop == ITEM_RIFLE)
	        m_ShopGroup = ITEM_RIFLE;
    	else if (m_Shop == ITEM_NINJA)
        	m_ShopGroup = ITEM_NINJA;
	    else if (m_Shop == ITEM_GENERAL)
			m_ShopGroup = ITEM_GENERAL;
		else if (m_Shop == ITEM_HOOK)
        	m_ShopGroup = ITEM_HOOK;
		else if (m_Shop == ITEM_JUMP)
        	m_ShopGroup = ITEM_JUMP;

		return;
	}
}

void CCharacter::ChangeUpgrade(int Weapon, int Value)
{	
	m_pPlayer->m_AciveUpgrade[Weapon] = Value == m_pPlayer->m_AciveUpgrade[Weapon] ? 0 : Value;
}

void CCharacter::FireWeapon()
{
	if(m_ReloadTimer != 0)
		return;

	DoWeaponSwitch();
	vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));

	// City
	if(!Direction.x && !Direction.y)
		return;

	bool FullAuto = false;
	
	// City
	if(m_ActiveWeapon == WEAPON_GRENADE || m_ActiveWeapon == WEAPON_SHOTGUN || m_ActiveWeapon == WEAPON_RIFLE)
		FullAuto = true;
	
	if(m_pPlayer->m_AciveUpgrade[m_ActiveWeapon])
		FullAuto = false;

	if(m_pPlayer->m_AccData.m_FastReload && !m_GameZone)
		FullAuto = true;

	// check if we gonna fire
	bool WillFire = false;
	if(CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses)
		WillFire = true;

	if(FullAuto && (m_LatestInput.m_Fire&1) && m_aWeapons[m_ActiveWeapon].m_Ammo)
		WillFire = true;

	if(!WillFire)
		return;

	// check for ammo
	if(!m_aWeapons[m_ActiveWeapon].m_Ammo)
	{
		// 125ms is a magical limit of how fast a human can click
		m_ReloadTimer = 125 * Server()->TickSpeed() / 1000;
		GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO);
		return;
	}

	vec2 ProjStartPos = m_Pos+Direction*m_ProximityRadius*0.75f;

	// City
	if(m_Protected && m_ActiveWeapon != WEAPON_NINJA && m_ActiveWeapon != WEAPON_RIFLE && !m_GameZone)
		return;

	switch(m_ActiveWeapon)
	{
		case WEAPON_HAMMER:
		{
			// reset objects Hit
			m_NumObjectsHit = 0;
			GameServer()->CreateSound(m_Pos, SOUND_HAMMER_FIRE);

			CCharacter *apEnts[MAX_CLIENTS];
			int Hits = 0;
			int Num = GameServer()->m_World.FindEntities(ProjStartPos, m_ProximityRadius*0.5f, (CEntity**)apEnts,
														MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

			for (int i = 0; i < Num; ++i)
			{
				CCharacter *pTarget = apEnts[i];

				if ((pTarget == this) || GameServer()->Collision()->IntersectLine(ProjStartPos, pTarget->m_Pos, NULL, NULL) || pTarget->GetPlayer()->m_Insta)
					continue;

				// set his velocity to fast upward (for now)
				if(length(pTarget->m_Pos-ProjStartPos) > 0.0f)
					GameServer()->CreateHammerHit(pTarget->m_Pos-normalize(pTarget->m_Pos-ProjStartPos)*m_ProximityRadius*0.5f);
				else
					GameServer()->CreateHammerHit(ProjStartPos);

				vec2 Dir;
				if (length(pTarget->m_Pos - m_Pos) > 0.0f)
					Dir = normalize(pTarget->m_Pos - m_Pos);
				else
					Dir = vec2(0.f, -1.f);

				if (m_pPlayer->m_AciveUpgrade[m_ActiveWeapon] == 3 && !GameServer()->HasDmgDisabled(m_pPlayer->GetCID(), pTarget->m_pPlayer->GetCID())) {
					new CHammerKill(GameWorld(), m_pPlayer->GetCID(), pTarget->GetPlayer()->GetCID());
					pTarget->m_IsHammerKilled = true;
				}

				pTarget->TakeDamage(vec2(0.f, -1.f) + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f, g_pData->m_Weapons.m_Hammer.m_pBase->m_Damage,
					m_pPlayer->GetCID(), m_ActiveWeapon);
				Hits++;

				if(pTarget->m_GameZone && m_GameZone && pTarget->m_Frozen)
					pTarget->Unfreeze();

			}
			
			// if we Hit anything, we have to wait for the reload
			if(Hits)
				m_ReloadTimer = Server()->TickSpeed()/3;

			if(m_pPlayer->m_AccData.m_HammerWalls > m_Walls && m_pPlayer->m_AciveUpgrade[m_ActiveWeapon] == UPGRADE_HAMMERWALLS && !Hits && !m_GameZone)
			{
				m_ReloadTimer = Server()->TickSpeed()/3;

				if(!m_HammerPos1.x && !m_HammerPos1.y)
					m_HammerPos1 = m_Pos;
				else
				{
					//vec2 Pos2 = m_Pos+vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY);
					if(m_HammerPos1.x != m_Pos.x || m_HammerPos1.y != m_Pos.y)
					{
	
						if(!GameServer()->Collision()->IntersectLine(m_HammerPos1, m_Pos, &m_HammerPos2, NULL))
							m_HammerPos2 = m_Pos;

						if(distance(m_HammerPos1, m_HammerPos2) > 1000)
							m_HammerPos2 = m_HammerPos1 + normalize(m_HammerPos2 - m_HammerPos1)*1000;

						new CWall(GameWorld(), m_HammerPos1, m_HammerPos2, m_pPlayer->GetCID());
						m_Walls++;
					}

					m_HammerPos1 = vec2(0, 0);
					m_HammerPos2 = vec2(0, 0);
				}
			}
			else if (m_pPlayer->m_AccData.m_HammerShot && m_pPlayer->m_AciveUpgrade[m_ActiveWeapon] == UPGRADE_HAMMERSHOT && !m_GameZone)
			{
				NewPlasma();
				m_ReloadTimer = Server()->TickSpeed() / 3;
			}
			else if (m_pPlayer->m_AccData.m_HammerKill && m_pPlayer->m_AciveUpgrade[m_ActiveWeapon] == UPGRADE_HAMMERKILL && !m_GameZone)
			{
				m_ReloadTimer = Server()->TickSpeed() / 3;
			} else if (m_pPlayer->m_AccData.m_Portal && m_pPlayer->m_AciveUpgrade[m_ActiveWeapon] == UPGRADE_PORTAL && !m_GameZone)
			{
				m_ReloadTimer = Server()->TickSpeed()/3;

				if (!m_PortalPos[0].x && !m_PortalPos[0].y) {
					m_PortalPos[0] = m_Pos;
				} else if (!m_PortalPos[1].x && !m_PortalPos[1].y) {
					m_PortalPos[1] = m_Pos;

					GameServer()->m_aPortals[m_pPlayer->GetCID()] = new CPortal(GameWorld(), m_pPlayer->GetCID(), m_PortalPos[0], m_PortalPos[1]);
				} else {
					m_PortalPos[0] = vec2(0, 0);
					m_PortalPos[1] = vec2(0, 0);

					GameServer()->m_aPortals[m_pPlayer->GetCID()]->Reset();
				}
			}
		} break;

		case WEAPON_GUN:
		{
			CMsgPacker Msg(NETMSGTYPE_SV_EXTRAPROJECTILE);

			if(!m_pPlayer->m_AccData.m_GunSpread || m_GameZone)
			{
				CProjectile *pProj = new CProjectile(GameWorld(), WEAPON_GUN,
				m_pPlayer->GetCID(),
				ProjStartPos,
				Direction,
				(int)(Server()->TickSpeed()*GameServer()->Tuning()->m_GunLifetime),
				1, m_GameZone?0:m_pPlayer->m_AccData.m_GunExplode, 0, -1, WEAPON_GUN);

				// pack the Projectile and send it to the client Directly
				CNetObj_Projectile p;
				pProj->FillInfo(&p);

			
				Msg.AddInt(1);
				for(unsigned i = 0; i < sizeof(CNetObj_Projectile)/sizeof(int); i++)
					Msg.AddInt(((int *)&p)[i]);

			}
			else
			{
				int ShotSpread = m_pPlayer->m_AccData.m_GunSpread;

				if(m_pPlayer->m_onMonster)
					ShotSpread = 0;

				//CMsgPacker Msg(NETMSGTYPE_SV_EXTRAPROJECTILE);
				Msg.AddInt(ShotSpread*2+1);

				float Spreading[12*2+1];

				for(int i = 0; i < 12*2+1; i++)
				{
					Spreading[i] = -1.260f + 0.070f * i;
				}
			
				for(int i = -ShotSpread; i <= ShotSpread; ++i)
				{
					float a = GetAngle(Direction);
					a += Spreading[i+18];
					float v = 1-(absolute(i)/(float)ShotSpread);

					float Speed = mix((float)GameServer()->Tuning()->m_ShotgunSpeeddiff, 1.0f, v);
					CProjectile *pProj = new CProjectile(GameWorld(), WEAPON_GUN,
						m_pPlayer->GetCID(),
						ProjStartPos,
						vec2(cosf(a), sinf(a))*Speed,
						(int)(Server()->TickSpeed()*GameServer()->Tuning()->m_GunLifetime),
						1, m_pPlayer->m_AccData.m_GunExplode, 0, -1, WEAPON_GUN);

					// pack the Projectile and send it to the client DirectlyPikotee
					CNetObj_Projectile p;
					pProj->FillInfo(&p);

					for(unsigned i = 0; i < sizeof(CNetObj_Projectile)/sizeof(int); i++)
						Msg.AddInt(((int *)&p)[i]);
				}
			}

			Server()->SendMsg(&Msg, 0, m_pPlayer->GetCID());

			GameServer()->CreateSound(m_Pos, SOUND_GUN_FIRE);
		} break;

		case WEAPON_SHOTGUN:
		{
			if(!m_GameZone)
			{
				int ShotSpread = 2+m_pPlayer->m_AccData.m_ShotgunSpread;

				if(m_pPlayer->m_onMonster)
					ShotSpread = 2;

				if(m_GameZone)
					ShotSpread = 2;

				CMsgPacker Msg(NETMSGTYPE_SV_EXTRAPROJECTILE);
				Msg.AddInt(ShotSpread*2+1);

				float Spreading[18*2+1];

				for(int i = 0; i < 18*2+1; i++)
				{
					Spreading[i] = -1.260f + 0.070f * i;
				}
			

				for(int i = -ShotSpread; i <= ShotSpread; ++i)
				{
					float a = GetAngle(Direction);
					a += Spreading[i+18];
					float v = 1-(absolute(i)/(float)ShotSpread);
					float Speed = mix((float)GameServer()->Tuning()->m_ShotgunSpeeddiff, 1.0f, v);
					CProjectile *pProj = new CProjectile(GameWorld(), WEAPON_SHOTGUN,
						m_pPlayer->GetCID(),
						ProjStartPos,
						vec2(cosf(a), sinf(a))*Speed,
						(int)(Server()->TickSpeed()*GameServer()->Tuning()->m_ShotgunLifetime),
						1, m_GameZone?0:m_pPlayer->m_AccData.m_ShotgunExplode, 0, -1, WEAPON_SHOTGUN);

					// pack the Projectile and send it to the client DirectlyPikotee
					CNetObj_Projectile p;
					pProj->FillInfo(&p);

					for(unsigned i = 0; i < sizeof(CNetObj_Projectile)/sizeof(int); i++)
						Msg.AddInt(((int *)&p)[i]);
				}

				Server()->SendMsg(&Msg, 0,m_pPlayer->GetCID());
			}
			else
				new CLaser(GameWorld(), m_Pos, Direction, GameServer()->Tuning()->m_LaserReach, m_pPlayer->GetCID(), WEAPON_SHOTGUN);

			GameServer()->CreateSound(m_Pos, SOUND_SHOTGUN_FIRE);
		} break;

		case WEAPON_GRENADE:
		{
			CMsgPacker Msg(NETMSGTYPE_SV_EXTRAPROJECTILE);
			if(!m_pPlayer->m_AccData.m_GrenadeSpread || m_GameZone)
			{
				CProjectile *pProj = new CProjectile(GameWorld(), WEAPON_GRENADE,
				m_pPlayer->GetCID(),
				ProjStartPos,
				Direction,
				(int)(Server()->TickSpeed()*GameServer()->Tuning()->m_GrenadeLifetime),
				1, true, 0, SOUND_GRENADE_EXPLODE, WEAPON_GRENADE);

				// pack the Projectile and send it to the client Directly
				CNetObj_Projectile p;
				pProj->FillInfo(&p);

				
				Msg.AddInt(1);
				for(unsigned i = 0; i < sizeof(CNetObj_Projectile)/sizeof(int); i++)
					Msg.AddInt(((int *)&p)[i]);

			}
			else
			{
				int ShotSpread = m_pPlayer->m_AccData.m_GrenadeSpread;

				if(m_pPlayer->m_onMonster)
					ShotSpread = 0;

				//CMsgPacker Msg(NETMSGTYPE_SV_EXTRAPROJECTILE);
				Msg.AddInt(ShotSpread*2+1);

				float Spreading[18*2+1];

				for(int i = 0; i < 18*2+1; i++)
				{
					Spreading[i] = -1.260f + 0.070f * i;
				}
			
				for(int i = -ShotSpread; i <= ShotSpread; ++i)
				{
					float a = GetAngle(Direction);
					a += Spreading[i+18];
					float v = 1-(absolute(i)/(float)ShotSpread);

					float Speed = mix((float)GameServer()->Tuning()->m_ShotgunSpeeddiff, 1.0f, v);
					CProjectile *pProj = new CProjectile(GameWorld(), WEAPON_GRENADE,
						m_pPlayer->GetCID(),
						ProjStartPos,
						vec2(cosf(a), sinf(a))*Speed,
						(int)(Server()->TickSpeed()*GameServer()->Tuning()->m_GrenadeLifetime),
						1, true, 0, SOUND_GRENADE_EXPLODE, WEAPON_GRENADE);

					// pack the Projectile and send it to the client DirectlyPikotee
					CNetObj_Projectile p;
					pProj->FillInfo(&p);

					for(unsigned i = 0; i < sizeof(CNetObj_Projectile)/sizeof(int); i++)
						Msg.AddInt(((int *)&p)[i]);
				}
			}

			Server()->SendMsg(&Msg, 0, m_pPlayer->GetCID());

			GameServer()->CreateSound(m_Pos, SOUND_GRENADE_FIRE);
		} break;

		case WEAPON_RIFLE:
		{
			if(m_Protected && !m_JailRifle)
				return;

			if(!m_pPlayer->m_Insta && !m_GameZone && !m_JailRifle)
			{
				if(m_pPlayer->m_AccData.m_RiflePlasma && m_pPlayer->m_AciveUpgrade[m_ActiveWeapon] == UPGRADE_RIFLEPLASMA)
				{
					m_ReloadTimer = Server()->TickSpeed()/3;
					new CPlasma(GameWorld(), WEAPON_RIFLE, m_Pos, m_pPlayer->GetCID(), Direction);
				} else if(m_pPlayer->m_AccData.m_RifleSwap && m_pPlayer->m_AciveUpgrade[m_ActiveWeapon] == UPGRADE_RIFLESWAP)
				{
					FullAuto = false;
					m_ReloadTimer = Server()->TickSpeed();

					float a = GetAngle(Direction);
					new CLaser(GameWorld(), m_Pos, vec2(cosf(a), sinf(a)), GameServer()->Tuning()->m_LaserReach, m_pPlayer->GetCID(), WEAPON_RIFLE);
				}
				else if(m_pPlayer->m_AccData.m_RifleSpread && !m_pPlayer->m_AciveUpgrade[m_ActiveWeapon])
				{
					int ShotSpread = m_pPlayer->m_AccData.m_RifleSpread;
					if(m_pPlayer->m_onMonster)
						ShotSpread = 0;
					float Spreading[18*2+1];

					for(int i = 0; i < 18*2+1; i++)
					{
						Spreading[i] = -1.260f + 0.070f * i;
					}
			
					for(int i = -ShotSpread; i <= ShotSpread; ++i)
					{
						float a = GetAngle(Direction);
						a += Spreading[i+18];
						new CLaser(GameWorld(), m_Pos, vec2(cosf(a), sinf(a)), GameServer()->Tuning()->m_LaserReach, m_pPlayer->GetCID(), WEAPON_RIFLE);
					}
				}
				else
					new CLaser(GameWorld(), m_Pos, Direction, GameServer()->Tuning()->m_LaserReach, m_pPlayer->GetCID(), WEAPON_RIFLE);
				
				GameServer()->CreateSound(m_Pos, SOUND_RIFLE_FIRE);
			}
			else
				new CLaser(GameWorld(), m_Pos, Direction, GameServer()->Tuning()->m_LaserReach, m_pPlayer->GetCID(), WEAPON_RIFLE);

		} break;

		case WEAPON_NINJA:
		{
			if(!m_Water && !m_SingleWater && !m_GameZone)
			{
				// reset Hit objects
				m_NumObjectsHit = 0;

				m_Ninja.m_ActivationDir = Direction;
				m_Ninja.m_CurrentMoveTime = g_pData->m_Weapons.m_Ninja.m_Movetime * Server()->TickSpeed() / 1000;
				m_Ninja.m_OldVelAmount = length(m_Core.m_Vel);

				GameServer()->CreateSound(m_Pos, SOUND_NINJA_FIRE);
			}
		} break;

	}

	m_AttackTick = Server()->Tick();

	if(m_aWeapons[m_ActiveWeapon].m_Ammo > 0 && !m_pPlayer->m_AccData.m_InfinityAmmo && !m_pPlayer->m_Insta && !m_GameZone && !m_pPlayer->m_Zomb) // -1 == unlimited
		m_aWeapons[m_ActiveWeapon].m_Ammo--;

	if(!m_ReloadTimer)
	{
		if(!m_pPlayer->m_Insta && !m_GameZone) {
			float LvlSpeed = 1;
			if (m_ActiveWeapon >= 0 && m_ActiveWeapon <= WEAPON_RIFLE)
				LvlSpeed = (m_pPlayer->m_AccData.m_LvlWeapon[m_ActiveWeapon]/30) + 1;

			if (LvlSpeed > (float)g_Config.m_SvWLvlSpeedMax)
				LvlSpeed = (float)g_Config.m_SvWLvlSpeedMax;

			if (m_ActiveWeapon == WEAPON_GUN)
				LvlSpeed /= 2;

			m_ReloadTimer = g_pData->m_Weapons.m_aId[m_ActiveWeapon].m_Firedelay 
				/ LvlSpeed * Server()->TickSpeed() / 1000;
			
			if (m_pPlayer->m_NinjaFly && m_ActiveWeapon == WEAPON_NINJA)
				m_ReloadTimer = 5;
		}
		else
			m_ReloadTimer = g_pData->m_Weapons.m_aId[m_ActiveWeapon].m_Firedelay * Server()->TickSpeed()/1000;
	}
}

// City

int CCharacter::NewPlasma()
{
	if(m_Plasma > m_pPlayer->m_AccData.m_HammerShot)
		return 0;

	int Num = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CCharacter *VictimChr = GameServer()->GetPlayerChar(i);

		if(!VictimChr || VictimChr == this || VictimChr->Protected() || VictimChr->GetPlayer()->m_God)
			continue;
		
		if(!GameServer()->Collision()->IntersectLine(m_Pos, VictimChr->m_Pos, 0x0, 0x0))
		{
			new CPlasma(GameWorld(), WEAPON_HAMMER, m_Pos, m_pPlayer->GetCID(), i);
			Num++;
		}
	}

	return Num;
}

void CCharacter::HealthRegeneration()
{
	if (m_Health != 0 
		&& !m_pPlayer->m_AccData.m_HealthRegen
		&& Server()->Tick() % 50 == 0) {

		if(m_Health < m_pPlayer->m_AccData.m_Health)
			IncreaseHealth(1 + m_ExternalHeal);
		else if(m_Armor < m_pPlayer->m_AccData.m_Armor)
			IncreaseArmor(1 + m_ExternalHeal);

	} else if (m_Health != 0 && m_pPlayer->m_AccData.m_HealthRegen && !(Server()->Tick() % (int)(251 - m_pPlayer->m_AccData.m_HealthRegen*10))) {

		if(m_Health < m_pPlayer->m_AccData.m_Health)
			IncreaseHealth(1 + m_ExternalHeal);
		else if(m_Armor < m_pPlayer->m_AccData.m_Armor)
			IncreaseArmor(1 + m_ExternalHeal);
	}

}

void CCharacter::HandleWeapons()
{
	//ninja
	HandleNinja();

	if(m_Frozen)
		return;

	// check reload timer
	if(m_ReloadTimer)
	{
		m_ReloadTimer--;
		return;
	}

	// fire Weapon, if wanted
	FireWeapon();

	// ammo regen
	int AmmoRegenTime = g_pData->m_Weapons.m_aId[m_ActiveWeapon].m_Ammoregentime;
	if(AmmoRegenTime && !m_pPlayer->m_AccData.m_InfinityAmmo && !m_pPlayer->m_Zomb)
	{
		// If equipped and not active, regen ammo?
		if (m_ReloadTimer <= 0)
		{
			if (m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart < 0)
				m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart = Server()->Tick();

			if ((Server()->Tick() - m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart) >= AmmoRegenTime * Server()->TickSpeed() / 1000)
			{
				// Add some ammo
				m_aWeapons[m_ActiveWeapon].m_Ammo = min(m_aWeapons[m_ActiveWeapon].m_Ammo + 1, 10);
				m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart = -1;
			}
		}
		else
		{
			m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart = -1;
		}
	}
	return;
}

bool CCharacter::GiveWeapon(int Weapon, int Ammo)
{
	if(m_aWeapons[Weapon].m_Ammo < g_pData->m_Weapons.m_aId[Weapon].m_Maxammo || !m_aWeapons[Weapon].m_Got)
	{
		m_aWeapons[Weapon].m_Got = true;
		m_aWeapons[Weapon].m_Ammo = min(g_pData->m_Weapons.m_aId[Weapon].m_Maxammo, Ammo);
		return true;
	}
	return false;
}

void CCharacter::GiveNinja()
{
	m_Ninja.m_ActivationTick = Server()->Tick();
	m_aWeapons[WEAPON_NINJA].m_Got = true;
	m_aWeapons[WEAPON_NINJA].m_Ammo = -1;
	if (m_ActiveWeapon != WEAPON_NINJA)
		m_LastWeapon = m_ActiveWeapon;
	m_ActiveWeapon = WEAPON_NINJA;

	GameServer()->CreateSound(m_Pos, SOUND_PICKUP_NINJA);
}

void CCharacter::SetEmote(int Emote, int Tick)
{
	m_EmoteType = Emote;
	m_EmoteStop = Tick;
}

void CCharacter::OnPredictedInput(CNetObj_PlayerInput *pNewInput)
{
	// check for changes
	if(mem_comp(&m_Input, pNewInput, sizeof(CNetObj_PlayerInput)) != 0)
		m_LastAction = Server()->Tick();

	// copy new input
	mem_copy(&m_Input, pNewInput, sizeof(m_Input));
	m_NumInputs++;

	// or are not allowed to aim in the center
	if(m_Input.m_TargetX == 0 && m_Input.m_TargetY == 0)
		m_Input.m_TargetY = -1;
}

void CCharacter::OnDirectInput(CNetObj_PlayerInput *pNewInput)
{
	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
	mem_copy(&m_LatestInput, pNewInput, sizeof(m_LatestInput));

		// it is not allowed to aim in the center
	if(m_LatestInput.m_TargetX == 0 && m_LatestInput.m_TargetY == 0)
		m_LatestInput.m_TargetY = -1;

	if(m_NumInputs > 2 && m_pPlayer->GetTeam() != TEAM_SPECTATORS)
	{
		HandleWeaponSwitch();
		FireWeapon();
	}

	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
}

// calculate exp needed to level up
unsigned long long CCharacter::calcExp(int level)
{
  unsigned long long exp = 3000;
  if(level >= 800)
    exp = ~0;
  else
    exp += ((10.0 * level + pow(2, level/25.0)) + pow(level, 1.8)) * pow(level, 1.7) - 150;
  return exp;
}

void CCharacter::ResetInput()
{
	m_Input.m_Direction = 0;
	m_Input.m_Hook = 0;
	// simulate releasing the fire button
	if((m_Input.m_Fire&1) != 0)
		m_Input.m_Fire++;
	m_Input.m_Fire &= INPUT_STATE_MASK;
	m_Input.m_Jump = 0;
	m_LatestPrevInput = m_LatestInput = m_Input;
}

void CCharacter::Booster()
{
	// Booster
	if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOST_DOWN)) {
		m_OnGavityZone = true;
		m_Core.m_Vel.y += 1;
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOST_UP)) {
		m_OnGavityZone = true;
		m_Core.m_Vel.y += -1;
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOST_RIGHT)) {
		m_Core.m_IgnoreGround = true;
		m_Core.m_Vel.x += 1;
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOST_LEFT)) {
		m_Core.m_IgnoreGround = true;
		m_Core.m_Vel.x += -1;
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOST_HORI) && m_Core.m_Vel.x) {
		m_Core.m_IgnoreGround = true;
		m_Core.m_Vel.x += m_Core.m_Vel.x > 0 ? 1.5 : -1.5;
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOST_VERT) && m_Core.m_Vel.y) {
		m_OnGavityZone = true;
		m_Core.m_Vel.y += m_Core.m_Vel.y > 0 ? 1.5 : -1.5;
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOST_ALL))
	{
		if(m_Core.m_Vel.x > 0.0001f)
			m_Core.m_Vel.x += m_Core.m_Vel.x > 0 ? 1 : -1;

		if(m_Core.m_Vel.y)
			m_Core.m_Vel.y += m_Core.m_Vel.y > 0 ? 1 : -1;
	}

	// Booster Wall
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOSTW_DOWN))
	{
		m_Core.m_Vel.y += 1;
		m_Core.m_Vel.x = 0;
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOSTW_UP))
	{
		m_Core.m_Vel.y += -1;
		m_Core.m_Vel.x = 0;
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOSTW_RIGHT))
	{
		m_Core.m_Vel.x += 1;
		m_Core.m_Vel.y = 0;
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOSTW_LEFT))
	{
		m_Core.m_Vel.x += -1;
		m_Core.m_Vel.y = 0;
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOSTW_HORI) && m_Core.m_Vel.x)
	{
		m_Core.m_Vel.x = m_Core.m_Vel.x > 0 ? 1 : -1;
		m_Core.m_Vel.y = 0;
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOSTW_VERT) && m_Core.m_Vel.y)
	{
		m_Core.m_Vel.y = m_Core.m_Vel.y > 0 ? 1 : -1;
		m_Core.m_Vel.x = 0;
	}

	// Fast Booster
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOSTF_DOWN)) {
		m_OnGavityZone = true;
		m_Core.m_Vel.y += 2;
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOSTF_UP)) {
		m_OnGavityZone = true;
		m_Core.m_Vel.y += -2;
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOSTF_RIGHT)) {
		m_Core.m_IgnoreGround = true;
		m_Core.m_Vel.x += 2;
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOSTF_LEFT)) {
		m_Core.m_IgnoreGround = true;
		m_Core.m_Vel.x += -2;
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOSTF_HORI) && m_Core.m_Vel.x) {
		m_OnGavityZone = true;
		m_Core.m_Vel.x = m_Core.m_Vel.x > 0 ? 2 : -2;
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOSTF_VERT) && m_Core.m_Vel.y) {
		m_Core.m_IgnoreGround = true;
		m_Core.m_Vel.y = m_Core.m_Vel.y > 0 ? 2 : -2;
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOSTF_ALL))
	{
		if(m_Core.m_Vel.x > 0.0001f)
			m_Core.m_Vel.x = m_Core.m_Vel.x > 0 ? 2 : -2;

		if(m_Core.m_Vel.y)
			m_Core.m_Vel.y = m_Core.m_Vel.y > 0 ? 2 : -2;
	}

	// Fast Booster Wall
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOSTFW_DOWN))
	{
		m_OnGavityZone = true;
		m_Core.m_Vel.y += 2;
		m_Core.m_Vel.x = 0;
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOSTFW_UP))
	{
		m_OnGavityZone = true;
		m_Core.m_Vel.y += -2;
		m_Core.m_Vel.x = 0;
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOSTFW_RIGHT))
	{
		m_Core.m_IgnoreGround = true;
		m_Core.m_Vel.x += 2;
		m_Core.m_Vel.y = 0;
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOSTFW_LEFT))
	{
		m_Core.m_IgnoreGround = true;
		m_Core.m_Vel.x += -2;
		m_Core.m_Vel.y = 0;
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOSTFW_HORI) && m_Core.m_Vel.x)
	{
		m_Core.m_Vel.x = m_Core.m_Vel.x > 0 ? 2 : -2;
		m_Core.m_Vel.y = -GameServer()->Tuning()->m_Gravity;
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_BOOSTFW_VERT) && m_Core.m_Vel.y)
	{
		m_Core.m_Vel.y = m_Core.m_Vel.y > 0 ? 2 : -2;
		m_Core.m_Vel.x = 0;
	} else {
		m_OnGavityZone = false;
		m_Core.m_IgnoreGround = false;
	}
		
	// Rankzones
	if (GameServer()->Collision()->IsTile(m_Pos, TILE_POLICE) && Server()->AuthLvl(m_pPlayer->GetCID()) < 1)
	{
		SendBroadcast("Policezone - Acces denied", m_pPlayer->GetCID());
		Die(m_pPlayer->GetCID(), WEAPON_WORLD);
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_DONOR) && !m_pPlayer->m_AccData.m_Donor && Server()->AuthLvl(m_pPlayer->GetCID()) < 2)
	{
		SendBroadcast("Donorzone - Acces denied", m_pPlayer->GetCID());
		Die(m_pPlayer->GetCID(), WEAPON_WORLD);
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_ADMIN) && Server()->AuthLvl(m_pPlayer->GetCID()) < 2)
	{
		SendBroadcast("Adminzone - Acces denied", m_pPlayer->GetCID());
		Die(m_pPlayer->GetCID(), WEAPON_WORLD);
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_VIP) && !m_pPlayer->m_AccData.m_VIP && Server()->AuthLvl(m_pPlayer->GetCID()) < 2)
	{
		SendBroadcast("VIPzone - Acces denied", m_pPlayer->GetCID());
		Die(m_pPlayer->GetCID(), WEAPON_WORLD);
	}
	//Water zeugs :D



	if(GameServer()->Collision()->IsTile(m_Pos, TILE_WATER))
	{
		if(m_Core.m_Vel.y > 10)
			m_Core.m_Vel.y = 7;

		m_SingleWater = true;
	}
	else
	{
		if(m_SingleWater && !m_Frozen)
			m_Core.m_Jumped &= ~2;
		m_SingleWater = false;
	}
			
	if(m_Water || m_SingleWater)
	{
		if(GameServer()->Collision()->IsTile(vec2(m_Pos.x, m_Pos.y-32), TILE_WATER))
		{
			if(m_Core.m_Vel.y > -0.45f)
				//m_Core.m_Vel.x /= 1.1f;
				m_Core.m_Vel.y -= 0.45f;
			else
				m_Core.m_Vel.y = -0.45f;
		}

		if(m_Input.m_Jump)
			m_Core.m_Vel.y = -3;
		
		m_Core.m_Vel.x /= 2;
	
	}
	
	if(m_Water || m_SingleWater || m_Space || m_SingleSpace)
	{
		m_Luft++;

		if(m_Luft >= 50 && m_pPlayer->m_AccData.m_HealthRegen == 0)
		{
			TakeDamage(vec2(0,0), 1, -1, WEAPON_GAME);
		m_Luft = 1;

		if(m_Health <= 0)
			Die(m_pPlayer->GetCID(), WEAPON_WORLD);
		}
	}
	else
		m_Luft = 0;

	//Insta <3
	if(GameServer()->Collision()->IsTile(m_Pos, TILE_INSTA_START))
	{
		if(!g_Config.m_EnableInstagib && m_pPlayer)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Instagib are not allow");
			Die(-1, WEAPON_GAME);
		}
		if(!m_pPlayer->m_Insta)
		{
			SendBroadcast("Entered insta zone (/instagib to quit)", m_pPlayer->GetCID());
			m_pPlayer->m_Insta = true;
			//m_ActiveWeapon = WEAPON_RIFLE;
			SetWeapon(WEAPON_RIFLE);
			m_aWeapons[WEAPON_RIFLE].m_Ammo = 10;
			m_Protected = false;
		}
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_INSTA_STOP))
	{
		if(m_pPlayer->m_Insta)
		{
			SendBroadcast("Left insta zone", m_pPlayer->GetCID());
			m_pPlayer->m_Insta = false;
			m_ActiveWeapon = WEAPON_GUN;
		}
	}

	//DONOR MONEY TILE!!! :D <3
	if(GameServer()->Collision()->IsTile(m_Pos, TILE_MONEY_DONOR))
	{
		if(m_pPlayer->m_AccData.m_Donor)
		{
			if(Server()->Tick()%50 == 0)
			{
				char aBuf[256];
				char numBuf[4][16];
				char progressBuf[128];
				
				int barWidth = 40;
				int Money = 1000;
				int ExpPoints = 10000;
	
				if(Money && ExpPoints)
				{
					long long unsigned NeededExp = calcExp(m_pPlayer->m_AccData.m_Level);

					double progress = (double)m_pPlayer->m_AccData.m_ExpPoints / (double)NeededExp;
					int pos = barWidth * progress;
					int percent = floor(progress * 100);
					int Multiplier = 1;

					progressBuf[0] = '[';
					for (int i = 0; i < barWidth; ++i) {
						if (i < pos) progressBuf[i+1] = ':';
						else progressBuf[i+1] = ' ';
					}
					progressBuf[barWidth+1] = ']';
					progressBuf[barWidth+2] = '\0';

					GameServer()->FormatInt(m_pPlayer->m_AccData.m_Money, numBuf[0]);
					GameServer()->FormatInt(Money, numBuf[1]);
					GameServer()->FormatInt(m_pPlayer->m_AccData.m_ExpPoints, numBuf[2]);
					GameServer()->FormatInt(ExpPoints, numBuf[3]);

					if (m_pPlayer->m_AccData.m_Donor)
						Multiplier = 6;
					else if (m_pPlayer->m_AccData.m_VIP)
						Multiplier = 3;

					if (GameServer()->GameEvent()->m_Multiplier > 1)
						Multiplier += GameServer()->GameEvent()->m_Multiplier;
					

					if (Multiplier > 1) {
						str_format(aBuf, sizeof(aBuf), "Money: %s$ | +%s$ x%d\nExp: %sep | +%sep x%d\n%s %i%%",
							numBuf[0], numBuf[1], Multiplier, numBuf[2], numBuf[3], Multiplier, progressBuf, percent);

						Money *= Multiplier;
						ExpPoints *= Multiplier;
					} else
						str_format(aBuf, sizeof(aBuf), "Money: %s$ | +%s$\nExp: %sep | +%sep\n%s %i%%",
							numBuf[0], numBuf[1], numBuf[2], numBuf[3], progressBuf, percent);
						

					m_pPlayer->m_AccData.m_Money += Money;
					m_pPlayer->m_AccData.m_ExpPoints += ExpPoints;
					GameServer()->MoneyCollector()->AddMoney(Money);

					SendBroadcast(aBuf, m_pPlayer->GetCID());

					if ( m_pPlayer->m_AccData.m_ExpPoints >= NeededExp)
					{
						m_pPlayer->m_AccData.m_ExpPoints = (m_pPlayer->m_AccData.m_ExpPoints - NeededExp) % NeededExp;
						m_pPlayer->m_AccData.m_Level++;
						m_pPlayer->m_Score = m_pPlayer->m_AccData.m_Level;

						char aBuf[256];
						GameServer()->SendChatTarget_Localization(m_pPlayer->GetCID(), CHATCATEGORY_INFO, _("You leveled up!"));
					}
				}
			}
		}
		else
		{
			SendBroadcast("Donor Money zone - Acces denied", m_pPlayer->GetCID());
			Die(m_pPlayer->GetCID(), WEAPON_WORLD);
		}
	
	}
	if(GameServer()->Collision()->IsTile(m_Pos, TILE_LIFE))
	{
		char aBuf[50];
		m_NeedArmor = GetPlayer()->m_AccData.m_Armor;
		m_NeedHealth = GetPlayer()->m_AccData.m_Health;
		m_ArmorCost = m_NeedArmor -= m_Armor;
		m_HealthCost = m_NeedHealth -= m_Health;
		m_LifeCost = (m_ArmorCost+m_HealthCost)*50;
		if(m_LifeCost > 0)
		{
				if(GetPlayer()->m_AccData.m_Money >= (long long unsigned)m_LifeCost)
				{
					m_Armor += m_NeedArmor;
					m_Health += m_NeedHealth;

					GetPlayer()->m_AccData.m_Money -= m_LifeCost;
					str_format(aBuf, sizeof(aBuf), "Money: %d$ | -%d$\nHealth: %d\nArmor: %d", m_pPlayer->m_AccData.m_Money, m_LifeCost, m_Health, m_Armor);
					SendBroadcast(aBuf, m_pPlayer->GetCID());
				}
			
			else
			{
				GameServer()->SendBroadcast_Localization(m_pPlayer->GetCID(), BROADCAST_PRIORITY_INTERFACE, BROADCAST_DURATION_REALTIME, _("You need {int:cost} TC"), "cost", &m_LifeCost, NULL);
			}
		}
	}
}

void CCharacter::SetPosition(vec2 Pos)
{
	if(m_Input.m_Hook)
		m_Input.m_Hook = 0;

	m_Core.m_Pos = Pos;
	GameServer()->m_TeleNR[m_pPlayer->GetCID()] = 0;
	m_Home = 0;
}

void CCharacter::Freeze(int Seconds)
{
	m_EmoteType = EMOTE_BLINK;
	m_EmoteStop = Server()->Tick() + Seconds;
	
	if(!m_Frozen)
		m_FreezeWeapon = m_ActiveWeapon;

	m_Frozen = Seconds;
	m_LastWeapon = ActiveWeapon();
	m_aWeapons[WEAPON_NINJA].m_Ammo = 0;
	SetWeapon(WEAPON_NINJA);
}

void CCharacter::Unfreeze()
{
	//m_EmoteType = EMOTE_NORMAL;
	m_EmoteStop = Server()->Tick();
	m_Frozen = m_Frozen ? 1 : 0;
	SetWeapon(m_LastWeapon);
}

void CCharacter::Transfer(int Value)
{
	char aBuf[256];
	char numBuf[2][32];

	if(Value <= 0)
	{
		if(!Value)
			GameServer()->SendChatTarget_Localization(m_pPlayer->GetCID(), CHATCATEGORY_INFO, _("Ever seen a 0$ bill?"));
		else
			GameServer()->SendChatTarget_Localization(m_pPlayer->GetCID(), CHATCATEGORY_INFO, _("Are you trying to cheat money?"));
			
		return;
	}
	else if(m_pPlayer->m_AccData.m_Money < (long long unsigned)Value)
	{
		GameServer()->SendChatTarget_Localization(m_pPlayer->GetCID(), CHATCATEGORY_INFO, _("Not enough money."));
		return;
	}
	else if (m_Transfers > 5) {
		GameServer()->SendChatTarget_Localization(m_pPlayer->GetCID(), CHATCATEGORY_INFO, _("Too many transfers"));
		return;
	}
	else
	{
		vec2 SnapPos = vec2(m_Pos.x+m_LatestInput.m_TargetX,m_Pos.y+m_LatestInput.m_TargetY);

		if(GameServer()->Collision()->CheckPoint(SnapPos))
		{
			GameServer()->SendChatTarget_Localization(m_pPlayer->GetCID(), CHATCATEGORY_INFO, _("The wall doesn't need money..."));
			return;
		}

		/*for(int i = 0; i < 4; i++)
		{
			vec2 TestPos = SnapPos;

			if(!(i%2))
				TestPos.x = (int)m_Pos.x + TestLength * (i==0?1:-1);
			else
				TestPos.y = (int)m_Pos.y + TestLength * (i==3?1:-1);	
			
			if(GameServer()->Collision()->CheckPoint(SnapPos))
			{
				SendBroadcast("The wall doesn't need money...", m_pPlayer->GetCID());
				return;
			}
		}*/

		m_pPlayer->m_AccData.m_Money -= Value;
		new CTransfer(GameWorld(), Value, SnapPos, this);

		GameServer()->FormatInt(m_pPlayer->m_AccData.m_Money, numBuf[0]);
		GameServer()->FormatInt(Value, numBuf[1]);

		str_format(aBuf, sizeof(aBuf), "Your money (%s$) | -%s$", numBuf[0], numBuf[1]);
		SendBroadcast(aBuf, m_pPlayer->GetCID());
	}
}

void CCharacter::HandleCity()
{
	HealthRegeneration();

	if(GameServer()->Collision()->IsTile(m_Pos, TILE_PROTECT))
	{
		if (!(m_Protected || m_Core.m_Protected)) {
			GameServer()->SendBroadcast_Localization(m_pPlayer->GetCID(), BROADCAST_PRIORITY_INTERFACE, BROADCAST_DURATION_REALTIME, _("Protected zone entered"));
			m_Protected = true;
			m_Core.m_Protected = true;
			GameServer()->SendTuningParams(m_pPlayer->GetCID());
			new CSpawProtect(GameWorld(), m_pPlayer->GetCID());
		}
	}
	else if (m_Protected || m_Core.m_Protected)
	{
		GameServer()->SendBroadcast_Localization(m_pPlayer->GetCID(), BROADCAST_PRIORITY_INTERFACE, BROADCAST_DURATION_REALTIME, _("Protected zone left"));
		m_Protected = false;
		m_Core.m_Protected = false;
		GameServer()->SendTuningParams(m_pPlayer->GetCID());
	}

	if(GameServer()->Collision()->IsTile(m_Pos, TILE_GAMEZONE_START) && !m_GameZone)
	{
		GameServer()->SendBroadcast_Localization(m_pPlayer->GetCID(), BROADCAST_PRIORITY_INTERFACE, BROADCAST_DURATION_REALTIME, _("Gamezone entered"));
		m_GameZone = true;
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_GAMEZONE_END) && m_GameZone)
	{
		GameServer()->SendBroadcast_Localization(m_pPlayer->GetCID(), BROADCAST_PRIORITY_INTERFACE, BROADCAST_DURATION_REALTIME, _("Gamezone left"));
		m_GameZone = false;
	}

	if (GameServer()->Collision()->IsTile(m_Pos, TILE_MONEYCOLLECTOR)) {
		char aBuf[256], numBuf[2][32];
		GameServer()->FormatInt(GameServer()->MoneyCollector()->m_Price, numBuf[0]);
		GameServer()->FormatInt(GameServer()->MoneyCollector()->m_Money, numBuf[1]);
		GameServer()->SendBroadcast_Localization(m_pPlayer->GetCID(), BROADCAST_PRIORITY_INTERFACE, BROADCAST_DURATION_REALTIME, _("~ Money Collector ~\nHolder: {str:h}\nPrice: {str:p}$\nPot: {str:po}$\n/mchelp"), 
		"h", GameServer()->MoneyCollector()->m_aHolderName,
		"p", numBuf[0], "po", numBuf[1], NULL);
	}

	if (m_InRace) {
		char aBuf[128];
		int diff = (Server()->Tick() - m_RaceTime) * 2;

		int min = diff / (100 * 60);
		int sec = (diff / 100) % 60;
		int mili = diff % 100;
		
		if (!min)
			str_format(aBuf, sizeof aBuf, "%d.%d seconds", sec, mili);
		else 
			str_format(aBuf, sizeof aBuf, "%d : %d.%d sec", min, sec, mili);

		SendBroadcast(aBuf, m_pPlayer->GetCID());
	}

	if (GameServer()->Collision()->IsTile(m_Pos, TILE_RACE_START)) {
		m_GameZone = true;
		m_InRace = true;
		m_RaceTime = Server()->Tick();
	} else if(GameServer()->Collision()->IsTile(m_Pos, TILE_RACE_END) && m_InRace) {
		m_GameZone = false;
		m_InRace = false;
		char numBuf[16];

		char aBuf[128];
		int diff = (Server()->Tick() - m_RaceTime) * 2;

		int min = diff / (100 * 60);
		int sec = (diff / 100) % 60;
		int mili = diff % 100;
		
		if (!min)
			str_format(aBuf, sizeof aBuf, "%s completed the race in %d.%d seconds", Server()->ClientName(m_pPlayer->GetCID()), sec, mili);
		else 
			str_format(aBuf, sizeof aBuf, "%s completed the race in %d minutes and %d.%d seconds", Server()->ClientName(m_pPlayer->GetCID()), min, sec, mili);

		int Reward = 500000 - diff*50;
		m_pPlayer->m_AccData.m_Money += Reward;

		if (Reward > 0)
			GameServer()->MoneyCollector()->AddMoney(Reward);

		GameServer()->SendChat(-1, GameServer()->CHAT_ALL, aBuf);

		if (Reward > 0) {
			GameServer()->FormatInt(Reward, numBuf);
			str_format(aBuf, sizeof aBuf, "You got a Reward of %s$", numBuf);
		} else {
			GameServer()->FormatInt(-Reward, numBuf);
			str_format(aBuf, sizeof aBuf, "You got a Reward of -%s$", numBuf);
		}
		
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
	}

	if(m_ActiveWeapon != WEAPON_RIFLE && m_pPlayer->m_Insta)
		SetWeapon(WEAPON_RIFLE);

	Booster();

	if (GameServer()->Collision()->IsTile(m_Pos, TILE_TRAINER)) {
		GameServer()->SendBroadcast_Localization(m_pPlayer->GetCID(), BROADCAST_PRIORITY_INTERFACE, BROADCAST_DURATION_REALTIME, _("Welcome to the coach!\nWrite /coach for more information"));
	}

	if(GameServer()->Collision()->IsTile(m_Pos, TILE_SPACE_GRAVITY)) {
		if (m_GravityY != 0.2) {
			m_GravityY = 0.2;
			GameServer()->SendTuningParams(m_pPlayer->GetCID());
		}
	}

	if(GameServer()->Collision()->IsTile(m_Pos, TILE_NORMAL_GRAVITY)) {
		if (m_GravityY != 0.5) {
			m_GravityY = 0.5;
			GameServer()->SendTuningParams(m_pPlayer->GetCID());
		}
	}

	if(GameServer()->Collision()->IsTile(m_Pos, TILE_SINGLE_FREEZE))
	{
		Freeze(3*50);
	}
	else if(GameServer()->Collision()->IsTile(m_Pos, TILE_UNFREEZE))
		Unfreeze();

	if(m_Invisible && m_pPlayer->m_Score > 20)
	{
		SendBroadcast("Invisibility disabled, Score > 20", m_pPlayer->GetCID());
		m_Invisible = 0;
	}

	if (GameServer()->Collision()->IsTile(m_Pos, ENTITY_AFK + ENTITY_OFFSET)) {
		if (!m_Core.m_Afk)
			

		if (!GetPlayer()->m_Afk) {
			GetPlayer()->m_Afk = true;
			new CSpawProtect(GameWorld(), m_pPlayer->GetCID());
			GameServer()->SendTuningParams(GetPlayer()->GetCID());
		}
		
		SendBroadcast("AFK Zone", m_pPlayer->GetCID());
	} else {
		if (GetPlayer()->m_Afk) {
			m_Core.m_Afk = false;
			GetPlayer()->m_Afk = false;
			GameServer()->SendTuningParams(GetPlayer()->m_Afk);
		}
	}

	if (GameServer()->ValidID(m_Core.m_HookedPlayer) && !(m_GameZone || m_pPlayer->m_Insta)) { // hook items
		if (m_pPlayer->m_AciveUpgrade[ITEM_HOOK] == UPGRADE_HEALHOOK) {
			CCharacter *pChr = GameServer()->m_apPlayers[m_Core.m_HookedPlayer]->GetCharacter();

			if (!pChr)
				return;

			if (m_LastHooked != m_Core.m_HookedPlayer) {
				pChr->m_ExternalHeal += m_pPlayer->m_AccData.m_HealHook;
				m_LastHooked = m_Core.m_HookedPlayer;
			}
		} else if (m_pPlayer->m_AciveUpgrade[ITEM_HOOK] == UPGRADE_BOOSTHOOK) {
			CCharacter *pChr = GameServer()->m_apPlayers[m_Core.m_HookedPlayer]->GetCharacter();

			if (!pChr) return;
			m_Core.m_EndlessHook = false;
			pChr->m_Core.m_Vel += m_Core.m_HookDir * g_Config.m_SvBoostHookStr;
		}
	} else {
		
		if (GameServer()->ValidID(m_LastHooked)) {
			CCharacter *pChr = GameServer()->m_apPlayers[m_LastHooked]->GetCharacter();

			if (!pChr)
				return;

			if (pChr->m_ExternalHeal)
				pChr->m_ExternalHeal -= m_pPlayer->m_AccData.m_HealHook;

			m_LastHooked = -1;
		}
		if (m_pPlayer->m_AccData.m_EndlessHook)
			m_Core.m_EndlessHook = true;
	}

	if(Server()->Tick()%50 == 0)
	{
		int Money = GameServer()->Collision()->TileMoney(m_Pos.x, m_Pos.y);
		int ExpPoints = Money * 10;
	
		char aBuf[256];
		char numBuf[4][16];
		char progressBuf[128];
		
		int barWidth = 40;
		
		if(Money && ExpPoints)
		{
			int NeededExp = calcExp(m_pPlayer->m_AccData.m_Level);

			double progress = (double)m_pPlayer->m_AccData.m_ExpPoints / (double)NeededExp;
			int pos = barWidth * progress;
			int percent = floor(progress * 100);
			int Multiplier = 1;

			progressBuf[0] = '[';
			for (int i = 0; i < barWidth; ++i) {
				if (i < pos) progressBuf[i+1] = ':';
				else progressBuf[i+1] = ' ';
			}
			progressBuf[barWidth+1] = ']';
			progressBuf[barWidth+2] = '\0';

			GameServer()->FormatInt(m_pPlayer->m_AccData.m_Money, numBuf[0]);
			GameServer()->FormatInt(Money, numBuf[1]);
			GameServer()->FormatInt(m_pPlayer->m_AccData.m_ExpPoints, numBuf[2]);
			GameServer()->FormatInt(ExpPoints, numBuf[3]);

			if (m_pPlayer->m_AccData.m_Donor)
				Multiplier = 5;
			else if (m_pPlayer->m_AccData.m_VIP)
				Multiplier = 3;

			if (GameServer()->GameEvent()->m_Multiplier > 1)
				Multiplier += GameServer()->GameEvent()->m_Multiplier;
			
			if (Multiplier > 1) {
				str_format(aBuf, sizeof(aBuf), "Money: %s$ | +%s$ x%d\nExp: %sep | +%sep x%d\n%s %i%%",
					numBuf[0], numBuf[1], Multiplier, numBuf[2], numBuf[3], Multiplier, progressBuf, percent);

				Money *= Multiplier;
				ExpPoints *= Multiplier;
			} else
				str_format(aBuf, sizeof(aBuf), "Money: %s$ | +%s$\nExp: %sep | +%sep\n%s %i%%",
					numBuf[0], numBuf[1], numBuf[2], numBuf[3], progressBuf, percent);

			m_pPlayer->m_AccData.m_Money += Money;
			m_pPlayer->m_AccData.m_ExpPoints += ExpPoints;
			GameServer()->MoneyCollector()->AddMoney(Money);

			SendBroadcast(aBuf, m_pPlayer->GetCID());

			if ( m_pPlayer->m_AccData.m_ExpPoints >= calcExp(m_pPlayer->m_AccData.m_Level))
			{
				m_pPlayer->m_AccData.m_ExpPoints = 0;
				m_pPlayer->m_AccData.m_Level++;
				m_pPlayer->m_Score = m_pPlayer->m_AccData.m_Level;

				char aBuf[256];
				GameServer()->SendChatTarget_Localization(m_pPlayer->GetCID(), CHATCATEGORY_INFO, _("You leveled up!"));
			}
		}

		if (m_LastAtkIDTimer) {
			m_LastAtkIDTimer--;

			if (!m_LastAtkIDTimer) {
				m_LastAtkID = -1;
				m_LastAtkWeapon = WEAPON_GAME;
			}
				
		}

		if(m_pPlayer->m_AccData.m_UserID)
			m_pPlayer->m_pAccount->Apply();	
	}

}

bool CCharacter::Protected()
{
	if(m_GameZone
	|| m_Protected
	|| m_SpawnProtection + 3 * Server()->TickSpeed() > Server()->Tick()
	|| GameServer()->Collision()->IsTile(m_Pos, ENTITY_AFK + ENTITY_OFFSET))
		return true;

	return false;
}

void CCharacter::AddGravAura() {
	new CGravAura(GameWorld(), m_pPlayer->GetCID());
}

void CCharacter::Tick()
{
	Booster();
	if(m_Health <= 0)
	{	
		DieFromMonster(WEAPON_GAME);
		Die(m_pPlayer->GetCID(), WEAPON_WORLD);
	}

	DoZombieMovement();
	if(m_pPlayer->m_ForceBalanced)
	{
		char Buf[128];
		str_format(Buf, sizeof(Buf), "You were moved to %s due to team balancing", GameServer()->m_pController->GetTeamName(m_pPlayer->GetTeam()));
		SendBroadcast(Buf, m_pPlayer->GetCID());

		m_pPlayer->m_ForceBalanced = false;
	}

	if (!m_OnGavityZone) {
		if (m_GravityY == 0.5)
			m_Core.m_Vel.y += GameServer()->Tuning()->m_Gravity;
		else {
			m_Core.m_Vel.y += m_GravityY;
			m_Core.m_Vel.x += m_GravityX;
		}
	}
	
	// City
	HandleCity();

	if(!m_Frozen && !m_pPlayer->m_Insta && !m_GameZone && !m_Water && !m_SingleWater)
	{
		if(m_pPlayer->m_AccData.m_InfinityJumps == 2 && m_pPlayer->m_AciveUpgrade[ITEM_HOOK] == UPGRADE_FLY)
		{
			m_Core.m_Fly = true;
			if (m_Input.m_Hook && m_GravityY != -0.3)
			{
				m_Core.m_Vel += ((normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY))) * max(0.001f, 1.5f));
				
			}
			else if (!m_Input.m_Hook && m_GravityY < 0) {
				
				m_GravityY = 0.5;
				GameServer()->SendTuningParams(m_pPlayer->GetCID());
			}
			else
				m_Core.m_Fly = false;
		}
		else if(m_pPlayer->m_AccData.m_InfinityJumps >= 1)
			m_Core.m_Jumped &= ~2;
		else
			m_Core.m_Fly = false;
	}

	if(m_Frozen)
	{
		if(m_Frozen%50==0 && m_FreezeTick+25 < Server()->Tick())
		{
			m_FreezeTick = Server()->Tick();
			GameServer()->CreateDamageInd(m_Pos, GetAngle(vec2(0, 1)), (int)(m_Frozen/50));
		}

		ResetInput();
		m_Frozen--;

		if(!m_Frozen)
			m_FreezeEnd = true;
	} else {
		if (Server()->Tick() % 50 == 0 && m_GunFreezeCooldown)
			m_GunFreezeCooldown--;
	}
	
	m_Core.m_Input = m_Input;
	m_Core.Tick(true);

	// handle death-tiles and leaving gamelayer
	if(GameServer()->Collision()->GetCollisionAt(m_Pos.x+m_ProximityRadius/3.f, m_Pos.y-m_ProximityRadius/3.f)&CCollision::COLFLAG_DEATH ||
		GameServer()->Collision()->GetCollisionAt(m_Pos.x+m_ProximityRadius/3.f, m_Pos.y+m_ProximityRadius/3.f)&CCollision::COLFLAG_DEATH ||
		GameServer()->Collision()->GetCollisionAt(m_Pos.x-m_ProximityRadius/3.f, m_Pos.y-m_ProximityRadius/3.f)&CCollision::COLFLAG_DEATH ||
		GameServer()->Collision()->GetCollisionAt(m_Pos.x-m_ProximityRadius/3.f, m_Pos.y+m_ProximityRadius/3.f)&CCollision::COLFLAG_DEATH ||
		GameLayerClipped(m_Pos))
	{
		Die(m_pPlayer->GetCID(), WEAPON_WORLD);
	}

	// handle Weapons	
	HandleWeapons();

	if(m_FreezeEnd)
	{
		if(m_pPlayer->m_AccData.m_NinjaPermanent || (Server()->Tick() - m_Ninja.m_ActivationTick) < (g_pData->m_Weapons.m_Ninja.m_Duration * Server()->TickSpeed() / 1000))
		{
			m_aWeapons[WEAPON_NINJA].m_Ammo = -1;
			m_aWeapons[WEAPON_NINJA].m_Got = true;

			if(m_FreezeWeapon == WEAPON_NINJA)
			{
				SetWeapon(m_LastWeapon);
				m_LastWeapon = m_ActiveWeapon?WEAPON_HAMMER:WEAPON_GUN;
			}
		}
		
		m_FreezeEnd = false;
	}

	if (Server()->Tick() % 50 == 0 && m_Transfers > 0)
		m_Transfers--;
	
	
	// Previnput
	m_PrevInput = m_Input;
	return;
}

void CCharacter::TickDefered()
{
	m_DesiredPos = m_Core.m_Pos;

	// advance the dummy
	{
		CWorldCore TempWorld;
		m_ReckoningCore.Init(&TempWorld, GameServer()->Collision());
		m_ReckoningCore.Tick(false);
		m_ReckoningCore.Move();
		m_ReckoningCore.Quantize();
	}

	//lastsentcore
	vec2 StartPos = m_Core.m_Pos;
	vec2 StartVel = m_Core.m_Vel;
	bool StuckBefore = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));

	m_Core.Move();
	bool StuckAfterMove = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));
	m_Core.Quantize();
	bool StuckAfterQuant = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));
	m_Pos = m_Core.m_Pos;

	if(!StuckBefore && (StuckAfterMove || StuckAfterQuant))
	{
		// Hackish solution to get rid of strict-aliasing warning
		union
		{
			float f;
			unsigned u;
		}StartPosX, StartPosY, StartVelX, StartVelY;

		StartPosX.f = StartPos.x;
		StartPosY.f = StartPos.y;
		StartVelX.f = StartVel.x;
		StartVelY.f = StartVel.y;

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "STUCK!!! %d %d %d %f %f %f %f %x %x %x %x",
			StuckBefore,
			StuckAfterMove,
			StuckAfterQuant,
			StartPos.x, StartPos.y,
			StartVel.x, StartVel.y,
			StartPosX.u, StartPosY.u,
			StartVelX.u, StartVelY.u);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}

	int Events = m_Core.m_TriggeredEvents;
	int Mask = CmaskAllExceptOne(m_pPlayer->GetCID());

	if(Events&COREEVENT_GROUND_JUMP) GameServer()->CreateSound(m_Pos, SOUND_PLAYER_JUMP, Mask);

	if(Events&COREEVENT_HOOK_ATTACH_PLAYER) GameServer()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_PLAYER, CmaskAll());
	if(Events&COREEVENT_HOOK_ATTACH_GROUND) GameServer()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_GROUND, Mask);
	if(Events&COREEVENT_HOOK_HIT_NOHOOK) GameServer()->CreateSound(m_Pos, SOUND_HOOK_NOATTACH, Mask);


	if(m_pPlayer->GetTeam() == TEAM_SPECTATORS)
	{
		m_Pos.x = m_Input.m_TargetX;
		m_Pos.y = m_Input.m_TargetY;
	}

	// update the m_SendCore if needed
	{
		CNetObj_Character Predicted;
		CNetObj_Character Current;
		mem_zero(&Predicted, sizeof(Predicted));
		mem_zero(&Current, sizeof(Current));
		m_ReckoningCore.Write(&Predicted);
		m_Core.Write(&Current);

		// only allow dead reackoning for a top of 3 seconds
		if(m_ReckoningTick+Server()->TickSpeed()*3 < Server()->Tick() || mem_comp(&Predicted, &Current, sizeof(CNetObj_Character)) != 0)
		{
			m_ReckoningTick = Server()->Tick();
			m_SendCore = m_Core;
			m_ReckoningCore = m_Core;
		}
	}
}

bool CCharacter::IncreaseHealth(int Amount)
{
	if(m_Health >= m_pPlayer->m_AccData.m_Health)
		return false;
	m_Health = clamp(m_Health+Amount, 0, m_pPlayer->m_AccData.m_Health);
	return true;
}

bool CCharacter::IncreaseArmor(int Amount)
{
	if(m_Armor >= m_pPlayer->m_AccData.m_Armor)
		return false;
	m_Armor = clamp(m_Armor+Amount, 0, m_pPlayer->m_AccData.m_Armor);
	return true;
}

void CCharacter::Die(int Killer, int Weapon)
{
	if (!m_Alive)
		return;

	if ((!GameServer()->ValidID(Killer) || Killer == m_pPlayer->GetCID()) && GameServer()->ValidID(m_LastAtkID)) {
		Killer = m_LastAtkID;
		Weapon = m_LastAtkWeapon;
	}

	CCharacter *pKiller = GameServer()->GetPlayerChar(Killer);
	if(!pKiller)
		return;

	if(Weapon >= 0 && ((Protected() && !pKiller->m_JailRifle) || (m_pPlayer->m_God && !pKiller->m_JailRifle)))
		return;

	if (Killer != m_pPlayer->GetCID())
		pKiller->AddExp(Weapon);

	// we got to wait 0.5 secs before respawning
	m_pPlayer->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()/2;
	int ModeSpecial = GameServer()->m_pController->OnCharacterDeath(this, GameServer()->m_apPlayers[Killer], Weapon);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "kill killer='%d:%s' victim='%d:%s' weapon=%d special=%d",
		Killer, Server()->ClientName(Killer),
		m_pPlayer->GetCID(), Server()->ClientName(m_pPlayer->GetCID()), Weapon, ModeSpecial);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	// send the kill message
	CNetMsg_Sv_KillMsg Msg;
	Msg.m_Killer = Killer;
	Msg.m_Victim = m_pPlayer->GetCID();
	Msg.m_Weapon = Weapon;
	Msg.m_ModeSpecial = ModeSpecial;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);

	// a nice sound
	GameServer()->CreateSound(m_Pos, SOUND_PLAYER_DIE);

	// this is for auto respawn after 3 secs
	m_pPlayer->m_DieTick = Server()->Tick();

	m_Alive = false;
	GameServer()->m_World.RemoveEntity(this);
	GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	GameServer()->CreateDeath(m_Pos, m_pPlayer->GetCID());
}

void CCharacter::DieFromMonster(int Weapon)
{
	// send the kill message
	CNetMsg_Sv_KillMsg Msg;
	Msg.m_Killer = m_pPlayer->GetCID();
	Msg.m_Victim = m_pPlayer->GetCID();
	Msg.m_Weapon = Weapon;
	Msg.m_ModeSpecial = 0;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);

	// a nice sound
	GameServer()->CreateSound(m_Pos, SOUND_PLAYER_DIE);

	// this is for auto respawn after 3 secs
	m_pPlayer->m_DieTick = Server()->Tick();

	m_Alive = false;
	GameServer()->m_World.RemoveEntity(this);
	GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	GameServer()->CreateDeath(m_Pos, m_pPlayer->GetCID());
}

bool CCharacter::TakeDamage(vec2 Force, int Dmg, int From, int Weapon, bool FromMonster)
{
	if((Protected() && !m_GameZone) || m_pPlayer->m_Insta)
		return false;

	m_Core.m_Vel += Force;

//	if(!m_pPlayer->m_Zomb && !GameServer()->m_apPlayers[From]->m_Zomb && GameServer()->m_apPlayers[From]->m_onMonster && m_pPlayer->m_onMonster)
//		return false;
	
//	if(m_pPlayer->m_Zomb && GameServer()->m_apPlayers[From]->m_Zomb)
//		return false;
	
	// these will have force impact
	if (m_pPlayer->m_God 
	|| GameServer()->HasDmgDisabled(From, m_pPlayer->GetCID()) 
	|| (m_GameZone && From == m_pPlayer->GetCID()))
		return false;

	if(GameServer()->m_pController->IsFriendlyFire(m_pPlayer->GetCID(), From) && !g_Config.m_SvTeamdamage)
		return false;

	//City
	if(m_pPlayer->m_AccData.m_NoSelfDMG && From == m_pPlayer->GetCID())
		return false;

/*	int LvlDmg = 0;

	if (GameServer()->ValidID(From))
		LvlDmg = floor(GameServer()->m_apPlayers[From]->m_AccData.m_LvlWeapon[Weapon] / 10.0);

	if (LvlDmg > g_Config.m_SvWLvlDmgMax)
		LvlDmg = g_Config.m_SvWLvlDmgMax;
	
	// m_pPlayer only inflicts half damage on self
	if(From == m_pPlayer->GetCID())
		Dmg = max(1, Dmg/2);
	else if (Weapon >= 0 && Weapon <= WEAPON_RIFLE)
		Dmg += LvlDmg; // Add every 10 lvl 1 dmg to others*/

	m_DamageTaken++;
	if (GameServer()->ValidID(From)) {
		m_LastAtkID = From;
		m_LastAtkWeapon = Weapon;
	}
	m_LastAtkIDTimer = 20; // 20 sec save

	// create healthmod indicator
	if(Server()->Tick() < m_DamageTakenTick+25)
	{
		// make sure that the damage indicators doesn't group together
		GameServer()->CreateDamageInd(m_Pos, m_DamageTaken*0.25f, Dmg);
	}
	else
	{
		m_DamageTaken = 0;
		GameServer()->CreateDamageInd(m_Pos, 0, Dmg);
	}

	if(Dmg)
	{
		if(m_Armor)
		{
			if(Dmg > 1)
			{
				m_Health--;
				Dmg--;
			}

			if(Dmg > m_Armor)
			{
				Dmg -= m_Armor;
				m_Armor = 0;
			}
			else
			{
				m_Armor -= Dmg;
				Dmg = 0;
			}
		}

		m_Health -= Dmg;
	}

	m_DamageTakenTick = Server()->Tick();

	// do damage Hit sound
	if(From >= 0 && From != m_pPlayer->GetCID() && GameServer()->m_apPlayers[From])
	{
		int Mask = CmaskOne(From);
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS && GameServer()->m_apPlayers[i]->m_SpectatorID == From)
				Mask |= CmaskOne(i);
		}
		GameServer()->CreateSound(GameServer()->m_apPlayers[From]->m_ViewPos, SOUND_HIT, Mask);
	}

	// check for death
	if(m_Health <= 0)
	{
		Die(From, Weapon);

		// set attacker's face to happy (taunt!)
		if (From >= 0 && From != m_pPlayer->GetCID() && GameServer()->m_apPlayers[From])
		{
			CCharacter *pChr = GameServer()->m_apPlayers[From]->GetCharacter();
			if (pChr)
			{
				pChr->m_EmoteType = EMOTE_HAPPY;
				pChr->m_EmoteStop = Server()->Tick() + Server()->TickSpeed();
			}
		}

		return false;
	}

	if (Dmg > 2)
		GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_LONG);
	else
		GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_SHORT);

	m_EmoteType = EMOTE_PAIN;
	m_EmoteStop = Server()->Tick() + 500 * Server()->TickSpeed() / 1000;

	return true;
}

void CCharacter::SendBroadcast(const char *pText, int ClientID) {
	if (m_LastBroadcast <= (Server()->Tick() - Server()->TickSpeed())) {
		GameServer()->SendBroadcast(pText, ClientID);
		m_LastBroadcast = Server()->Tick();
	}
}

void CCharacter::AddExp(int Weapon, long long unsigned Amount) {
	char aBuf[256];
	char WeaponBuf[128];

	GameServer()->GetStrByWID(Weapon, WeaponBuf, sizeof WeaponBuf);

	m_pPlayer->m_AccData.m_ExpWeapon[Weapon] += Amount;

	if (Weapon == WEAPON_NINJA)
		return;

	if (m_pPlayer->m_AccData.m_ExpWeapon[Weapon] >= (unsigned)g_Config.m_SvWLvlMax)
		return;
	
	if (m_pPlayer->m_AccData.m_ExpWeapon[Weapon] >= m_pPlayer->m_AccData.m_LvlWeapon[Weapon]) {

		while (m_pPlayer->m_AccData.m_ExpWeapon[Weapon] >= m_pPlayer->m_AccData.m_LvlWeapon[Weapon]) {
			m_pPlayer->m_AccData.m_ExpWeapon[Weapon] -= m_pPlayer->m_AccData.m_LvlWeapon[Weapon];
			m_pPlayer->m_AccData.m_LvlWeapon[Weapon]++;

			if (m_pPlayer->m_AccData.m_ExpWeapon[Weapon] >= (unsigned)g_Config.m_SvWLvlMax)
				break;
		}

		str_format(aBuf, sizeof aBuf, "%s level up!", WeaponBuf);
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		str_format(aBuf, sizeof aBuf, "Your %s is level %d (%d|%d)ep", WeaponBuf, m_pPlayer->m_AccData.m_LvlWeapon[Weapon], m_pPlayer->m_AccData.m_ExpWeapon[Weapon], m_pPlayer->m_AccData.m_LvlWeapon[Weapon]);
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		
		return;
	}
	str_format(aBuf, sizeof aBuf, "%s: +%dep (%d|%d)ep", WeaponBuf, Amount, m_pPlayer->m_AccData.m_ExpWeapon[Weapon], m_pPlayer->m_AccData.m_LvlWeapon[Weapon]);
	GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
}

void CCharacter::Snap(int SnappingClient)
{
	int FakeID = m_pPlayer->GetCID();
	if (!Server()->Translate(FakeID, SnappingClient))
		return;

	if(NetworkClipped(SnappingClient))
		return;

	if(m_Invisible && !Server()->IsAuthed(SnappingClient) && SnappingClient != m_pPlayer->GetCID())
		return;

	CNetObj_Character *pCharacter = static_cast<CNetObj_Character *>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, FakeID, sizeof(CNetObj_Character)));
	if(!pCharacter)
		return;

	// write down the m_Core
	if(!m_ReckoningTick || GameServer()->m_World.m_Paused)
	{
		// no dead reckoning when paused because the client doesn't know
		// how far to perform the reckoning
		pCharacter->m_Tick = 0;
		m_Core.Write(pCharacter);
	}
	else
	{
		pCharacter->m_Tick = m_ReckoningTick;
		m_SendCore.Write(pCharacter);
	}

	// set emote
	if (m_EmoteStop < Server()->Tick())
	{
		m_EmoteType = m_Emote;
		m_EmoteStop = -1;
	}

	if (pCharacter->m_HookedPlayer != -1)
	{
		if (!Server()->Translate(pCharacter->m_HookedPlayer, SnappingClient))
			pCharacter->m_HookedPlayer = -1;
	}

	pCharacter->m_Emote = m_EmoteType;

	pCharacter->m_AmmoCount = 0;
	pCharacter->m_Health = 0;
	pCharacter->m_Armor = 0;

	pCharacter->m_Weapon = m_ActiveWeapon;
	pCharacter->m_AttackTick = m_AttackTick;

	pCharacter->m_Direction = m_Input.m_Direction;

	if(m_pPlayer->GetCID() == SnappingClient || SnappingClient == -1 ||
		(!g_Config.m_SvStrictSpectateMode && m_pPlayer->GetCID() == GameServer()->m_apPlayers[SnappingClient]->m_SpectatorID))
	{
		pCharacter->m_Health = m_Health;
		pCharacter->m_Armor = m_Armor;
		if(m_aWeapons[m_ActiveWeapon].m_Ammo > 0)
			pCharacter->m_AmmoCount = m_aWeapons[m_ActiveWeapon].m_Ammo;
	}

	if(pCharacter->m_Emote == EMOTE_NORMAL)
	{
		if(250 - ((Server()->Tick() - m_LastAction)%(250)) < 5)
			pCharacter->m_Emote = EMOTE_BLINK;
	}

	pCharacter->m_PlayerFlags = GetPlayer()->m_PlayerFlags;
}

void CCharacter::CheckLevelUp(int ClientID)
{
	if(m_pPlayer->m_AccData.m_ExpPoints >= calcExp(m_pPlayer->m_AccData.m_Level))
	{
		m_pPlayer->m_AccData.m_ExpPoints = 0;
		m_pPlayer->m_AccData.m_Level++;
		m_pPlayer->m_Score = m_pPlayer->m_AccData.m_Level;
		char aBuf[256];
		GameServer()->SendChatTarget_Localization(m_pPlayer->GetCID(), CHATCATEGORY_INFO, _("You leveled up!"));
	}

	if(GameServer()->m_apPlayers[ClientID]->GetCharacter()->m_Health <= 0)
		Die(-1, WEAPON_GAME);
}

void CCharacter::DoZombieMovement()
{
	if(!m_pPlayer->GetZomb())
		return;

	if(m_Move.m_LastX == m_Pos.x)//direction swap caused by a wall
		m_Move.m_LastXTimer++;
	else
		m_Move.m_LastXTimer = 0;
	m_Move.m_LastX = m_Pos.x;

	if(IsGrounded())//reset jump
		m_Move.m_JumpTimer = 0;

	if(m_Move.m_LastXTimer > 50)//Direction change for non chaning X
	{
		m_Input.m_Direction = m_Input.m_Direction * -1;
		m_Move.m_FJump = true;
	}

	if(GameServer()->Collision()->CheckPoint(vec2(m_Pos.x + m_Input.m_Direction * 45, m_Pos.y)) || GameServer()->Collision()->CheckPoint(vec2(m_Pos.x + m_Input.m_Direction * 77, m_Pos.y)) || m_Move.m_FJump || m_Core.m_Vel == vec2(0.f, 0.f))//set jump
		m_Move.m_JumpTimer++;

	if(m_Move.m_FJump)//Reset Collision jump
		m_Move.m_FJump = false;

	if(m_Move.m_CliffTimer)//Reset Cliff Timer
		m_Move.m_CliffTimer--;

	if(m_Move.m_JumpTimer == 1 || m_Move.m_JumpTimer == 31)//Jump + DoubleJump
		m_Input.m_Jump = 1;
	else
		m_Input.m_Jump = 0;
	
	if(!m_Input.m_Direction)//rare but possible
	{
		int Rand = rand()%2;
		if(Rand < 1)
			m_Input.m_Direction = -1;
		else
			m_Input.m_Direction = 1;
	}

	if(GameServer()->Tuning()->m_PlayerCollision)//Zombies blocking other zombies
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			CCharacter *pChar = GameServer()->GetPlayerChar(i);
			
			if(!pChar || pChar == this)
				continue;
				
			if(distance(m_Pos, pChar->m_Pos) <= 65.0f && m_Pos.y == pChar->m_Pos.y)
			{
				if(m_Pos.x > pChar->m_Pos.x && m_Input.m_Direction != pChar->m_Input.m_Direction)
					m_Move.m_FJump = true;
			}
		}
	}

	//Can jump over cliff?!
	if(!GameServer()->Collision()->CheckTiles(vec2(m_Pos.x + m_Input.m_Direction * 46, m_Pos.y), 15) && !m_Move.m_CliffTimer)
	{
		
		m_Move.m_FJump = true;
		m_Move.m_CliffTimer = 30;
		if(!GameServer()->Collision()->CheckParable(vec2(m_Pos.x, m_Pos.y), 50, m_Input.m_Direction))
		{
			m_Input.m_Direction = m_Input.m_Direction * -1;
			m_Move.m_CliffTimer = 0;
		}
	}

	CCharacter *pClosest = this;
	CCharacter *pCloseZomb = this;
	for(int i = 0; i < MAX_CLIENTS; i++)//see a player? :D Do Aim - Don't jump over the cliff if you see a player falling down
	{
		CCharacter *pChar = GameServer()->GetPlayerChar(i);
		
		if(!pChar || pChar == this || !pChar->IsAlive() || !pChar->GetPlayer() || (GameServer()->Collision()->IntersectTile(m_Pos, pChar->m_Pos) && m_Core.m_HookState != HOOK_GRABBED))
			continue;
		
		if(!GameServer()->m_apPlayers[i]->GetZomb() && (pClosest == this || distance(m_Pos, pClosest->m_Pos) > distance(m_Pos, pChar->m_Pos)))
			pClosest = pChar;
		if(GameServer()->m_apPlayers[i]->GetZomb() == TEAM_BLUE && (pCloseZomb == this || distance(m_Pos, pCloseZomb->m_Pos) > distance(m_Pos, pChar->m_Pos)))
			pCloseZomb = pChar;
	}
	if(pClosest != this)
	{
		if(m_pPlayer->GetZomb(9))//Flombie fly movement //first movement, then Aim because Aim can cause death
		{
			if(pClosest->m_Pos.y < m_Pos.y)
				m_Core.m_Vel.y -= 0.25f + GameServer()->Tuning()->m_Gravity;//must work
			else if(pClosest->m_Pos.y > m_Pos.y)
				m_Core.m_Vel.y += 0.25f;
		}

		//Do Aim
		if(pCloseZomb != this)
			DoZombieAim(pClosest->m_Pos, pClosest->GetPlayer()->GetCID(), pCloseZomb->m_Pos, pCloseZomb->GetPlayer()->GetCID());//Only do aiming if it sees a player, (but i wanted it always...)
		else
			DoZombieAim(pClosest->m_Pos, pClosest->GetPlayer()->GetCID(), vec2(0, 0), -1);//Only do aiming if it sees a player, (but i wanted it always...)
		if(IsAlive() && pClosest && pClosest->IsAlive() && (pClosest->m_Pos.y > m_Pos.y || GameServer()->Collision()->CheckTiles(pClosest->m_Pos, 20)))
		{
			if(m_Pos.x > pClosest->m_Pos.x)
				m_Input.m_Direction = -1;
			else if(m_Pos.x == pClosest->m_Pos.x)
				m_Input.m_Direction = 0;//rare but could
			else
				m_Input.m_Direction = 1;
		}
	}
	else if(pCloseZomb != this)
		DoZombieAim(vec2(0, 0), -1, pCloseZomb->m_Pos, pCloseZomb->GetPlayer()->GetCID());
	else
		ResetAiming();
}

void CCharacter::DoZombieAim(vec2 VictimPos, int VicCID, vec2 NearZombPos, int NearZombCID)
{
	if(!m_pPlayer->GetZomb())
		return;

	//if(m_Aim.m_FireCounter)
		//m_Aim.m_FireCounter--;
	if(m_pPlayer->GetZomb(8) && distance(m_Pos, VictimPos) > 100.0f)
		VictimPos = GetGrenadeAngle(m_Pos, VictimPos, true) + m_Pos;

	if(m_pPlayer->GetZomb(5) && distance(m_Pos, VictimPos) > 100.0f)
		VictimPos = GetGrenadeAngle(m_Pos, VictimPos, false) + m_Pos;

	//Direction is exactly to the player
	m_Input.m_TargetY = 160 * (VictimPos.y - m_Pos.y) / sqrt((VictimPos.x - m_Pos.x)*(VictimPos.x - m_Pos.x) + (VictimPos.y - m_Pos.y)*(VictimPos.y - m_Pos.y));
	m_Input.m_TargetX = 160 * (VictimPos.x - m_Pos.x) / sqrt((VictimPos.x - m_Pos.x)*(VictimPos.x - m_Pos.x) + (VictimPos.y - m_Pos.y)*(VictimPos.y - m_Pos.y));
	m_LatestInput.m_TargetX = m_Input.m_TargetX;
	m_LatestInput.m_TargetY = m_Input.m_TargetY;
	// THIS IS THE FUCKING WORKING KEY, cost me 2 days
	m_LastAction = Server()->Tick();
	
	// || 
	if(m_pPlayer->GetZomb(2))
		m_Aim.m_FireCounter = 21;

	//Zele
	if(m_pPlayer->GetZomb(11))
	{
		if(distance(m_Pos, VictimPos) <= 500.0f && distance(m_Pos, VictimPos) > 200.0f)
		{
			if(!GameServer()->Collision()->CheckPoint(VictimPos + vec2(0, 32)))
				m_Core.m_Pos = VictimPos + vec2(0, 32);
			else
				m_Core.m_Pos = VictimPos - vec2(0, 32);
			m_Core.m_Vel.y =- 0.1;
			m_Pos = m_Core.m_Pos;
		}
	}

	//Zeater
	if(m_pPlayer->GetZomb(13) && NearZombPos != m_Pos && distance(m_Pos, NearZombPos) <= 65.0f && !GameServer()->m_apPlayers[NearZombCID]->GetZomb(13))
	{
		for(int i = 0; i < 3; i++)
		{
			if(!m_pPlayer->m_SubZomb[i])
			{
				int VictimType = GameServer()->m_apPlayers[NearZombCID]->GetZomb();
				if(m_pPlayer->GetZomb(VictimType))
					return;
				if(VictimType == 6)
					IncreaseHealth(100);
				else
					IncreaseHealth(10);
				m_pPlayer->m_Score++;
				m_pPlayer->m_SubZomb[i] = VictimType;
				GameServer()->GetPlayerChar(NearZombCID)->Die(m_pPlayer->GetCID(), WEAPON_GAME);

				if(m_pPlayer->GetZomb(5))//gun
					m_ActiveWeapon = WEAPON_GUN;
				if(m_pPlayer->GetZomb(7))//Shotgun
					m_ActiveWeapon = WEAPON_SHOTGUN;
				if(m_pPlayer->GetZomb(8))//Grenade
					m_ActiveWeapon = WEAPON_GRENADE;
				if(m_pPlayer->GetZomb(2))//Rifle
					m_ActiveWeapon = WEAPON_RIFLE;
				m_aWeapons[m_ActiveWeapon].m_Ammo = -1;
				break;
			}
		}
	}


	//Can do sth.
	if(distance(m_Pos, VictimPos) <= GetTriggerDistance(m_pPlayer->GetZomb()) || (!m_pPlayer->GetZomb(4) && NearZombPos != m_Pos && distance(m_Pos, NearZombPos) <= 65.0f && GetTriggerDistance(m_pPlayer->GetZomb()) == 65.0f))//Zamer shouldnt attak other zombies
	{
		//Zinvi
		if(m_pPlayer->GetZomb(12))
			m_IsVisible = true;

		//Zinja
		if(m_pPlayer->GetZomb(10))
		{
			if(m_ActiveWeapon == WEAPON_HAMMER)
				GameServer()->CreateSound(m_Pos, SOUND_PICKUP_NINJA);
			GiveNinja();
		}

		//Zooker
		if(m_pPlayer->GetZomb(3))
		{
			if(VicCID != -1 && distance(m_Pos, VictimPos) < 380.0f && GameServer()->GetPlayerChar(VicCID))//Look hooklenght in tuning.h
			{
				m_Input.m_Hook = 1;
				m_LatestInput.m_Hook = 1;
				m_Core.m_Zooker = true;
				m_Core.m_HookedPlayer = VicCID;
				m_Core.m_HookState = HOOK_GRABBED;
			}
			if(m_Core.m_HookState != HOOK_IDLE && (!GameServer()->GetPlayerChar(m_Core.m_HookedPlayer) || !GameServer()->GetPlayerChar(m_Core.m_HookedPlayer)->IsAlive()))//return hook by death
			{
				m_Input.m_Hook = 0;
				m_LatestInput.m_Hook = 0;
				m_Core.m_HookState = HOOK_IDLE;
				m_Core.m_HookTick = 0;
				m_Core.m_Zooker = false;
				m_Core.m_HookedPlayer = -1;
			}
		}

		//Zamer
		if(m_pPlayer->GetZomb(4))
		{	
			m_Aim.m_Explode = true;
			GameServer()->CreateExplosion(vec2(m_Pos.x + 5, m_Pos.y + 5), m_pPlayer->GetCID(), WEAPON_GAME, false);
			GameServer()->CreateExplosion(vec2(m_Pos.x - 5, m_Pos.y + 5), m_pPlayer->GetCID(), WEAPON_GAME, false);
			GameServer()->CreateExplosion(vec2(m_Pos.x - 5, m_Pos.y - 5), m_pPlayer->GetCID(), WEAPON_GAME, false);
			GameServer()->CreateExplosion(vec2(m_Pos.x + 5, m_Pos.y - 5), m_pPlayer->GetCID(), WEAPON_GAME, false);
			for(int i = 0; i < 100; i++)
				GameServer()->CreateSound(vec2(m_Pos.x + 5, m_Pos.y + 5), SOUND_GRENADE_EXPLODE, -1);
			Die(m_pPlayer->GetCID(), WEAPON_SELF);
			return;
		}

		//Fire!
		m_Input.m_Fire = 1;
		m_LatestPrevInput.m_Fire = 1;
	}
	else if(!m_pPlayer->GetZomb(8) && !m_pPlayer->GetZomb(5))
	{
		if(m_pPlayer->GetZomb(12))
			m_IsVisible = false;
		ResetAiming();
	}
	
	if(VictimPos == vec2(0, 0) || VictimPos == m_Pos)//Reset all
		ResetAiming();
}

vec2 CCharacter::GetGrenadeAngle(vec2 m_StartPos, vec2 m_ToShoot, bool GrenadeBot)
{
	/*
	inline vec2 CalcPos(vec2 Pos, vec2 Velocity, float Curvature, float Speed, float Time)
	{
		vec2 n;
		Time *= Speed;
		n.x = Pos.x + Velocity.x*Time;
		n.y = Pos.y + Velocity.y*Time + Curvature/10000*(Time*Time);
		return n;
	*/
	if(m_ToShoot == vec2(0, 0))
	{
		return vec2(0, 0);
	}
	char aBuf[128];
	vec2 m_Direction;
	float Curvature = GameServer()->Tuning()->m_GunCurvature;
	if(GrenadeBot)
		Curvature = GameServer()->Tuning()->m_GrenadeCurvature;
	m_Direction.x = (m_ToShoot.x - m_StartPos.x);
	m_Direction.y = (m_ToShoot.y - m_StartPos.y - 32*Curvature);
	str_format(aBuf, sizeof(aBuf), "AimPos %d %d", m_Direction.x, m_Direction.y);
	return m_Direction;
}

void CCharacter::ResetAiming()
{
	m_Input.m_Fire = 0;
	m_LatestPrevInput.m_Fire = 0;
	if(m_pPlayer->GetZomb(10))
	{
		m_aWeapons[WEAPON_NINJA].m_Got = false;
		m_ActiveWeapon = WEAPON_HAMMER;
	}
}

float CCharacter::GetTriggerDistance(int Type)
{
	if(Type == 5 || Type == 9)//Zunner
		return 4000.0f;
	else if(Type == 8)
		return 800.0f;//Grenade
	else if(Type == 2)
		return GameServer()->Tuning()->m_LaserReach;//Zoomer
	else if(Type == 7 || Type == 10)//Zotter, Zinja
		return 500.0f;
	else if(Type == 3)//Zooker
		return 380.0f;
	return 65.0f;//Rest
}