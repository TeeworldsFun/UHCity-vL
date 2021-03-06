/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include <math.h>

#include <game/server/city/shopitems/shotgun.h>
#include <game/server/city/shopitems/grenade.h>
#include <game/server/city/shopitems/rifle.h>
#include <game/server/city/shopitems/gun.h>
#include <game/server/city/shopitems/hammer.h>
#include <game/server/city/shopitems/ninja.h>
#include <game/server/city/shopitems/hook.h>

#include <game/server/city/shopitems/healthregen.h>
#include <game/server/city/shopitems/infjumps.h>
#include <game/server/city/shopitems/infammo.h>
#include <game/server/city/shopitems/allweapons.h>
#include <game/server/city/shopitems/fastreload.h>
#include <game/server/city/shopitems/noselfdmg.h>
#include <game/server/city/shopitems/pushaura.h>
#include <game/server/city/shopitems/pullaura.h>

#include "gui.h"

using namespace std;


CGui::CGui(CGameWorld *pGameWorld, int Owner)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE)
{
	m_Owner = Owner;
	m_Visible = false;
	m_Pos = vec2(0, 0);

	m_ItrFlag = true;
	m_Group = 0;
	m_Page = 0;

	// sorted by itemgroups, starting with ITEM_HAMMMER
	std::list<CEntity*> group;

	group.push_back(new CHammer(GameWorld(), m_Owner, m_Pos, 1));
	group.push_back(new CHammer(GameWorld(), m_Owner, m_Pos, 2));
	group.push_back(new CHammer(GameWorld(), m_Owner, m_Pos, 3));
	group.push_back(new CHammer(GameWorld(), m_Owner, m_Pos, 4));
	group.push_back(new CHammer(GameWorld(), m_Owner, m_Pos, 5));

	m_aShop.push_back(group);
	group.clear();

	group.push_back(new CGun(GameWorld(), m_Owner, m_Pos, 1));
	group.push_back(new CGun(GameWorld(), m_Owner, m_Pos, 2));
	group.push_back(new CGun(GameWorld(), m_Owner, m_Pos, 3));

	m_aShop.push_back(group);
	group.clear();

	group.push_back(new CShotgun(GameWorld(), m_Owner, m_Pos, 1));
	group.push_back(new CShotgun(GameWorld(), m_Owner, m_Pos, 2));

	m_aShop.push_back(group);
	group.clear();

	group.push_back(new CGrenade(GameWorld(), m_Owner, m_Pos, 1));
	group.push_back(new CGrenade(GameWorld(), m_Owner, m_Pos, 2));

	m_aShop.push_back(group);
	group.clear();
	
	group.push_back(new CRifle(GameWorld(), m_Owner, m_Pos, 1));
	group.push_back(new CRifle(GameWorld(), m_Owner, m_Pos, 2));
	group.push_back(new CRifle(GameWorld(), m_Owner, m_Pos, 3));

	m_aShop.push_back(group);
	group.clear();

	group.push_back(new CNinja(GameWorld(), m_Owner, m_Pos, 1));
	group.push_back(new CNinja(GameWorld(), m_Owner, m_Pos, 2));
	group.push_back(new CNinja(GameWorld(), m_Owner, m_Pos, 3));
	group.push_back(new CNinja(GameWorld(), m_Owner, m_Pos, 4));
	group.push_back(new CNinja(GameWorld(), m_Owner, m_Pos, 5));

	m_aShop.push_back(group);
	group.clear();

	group.push_back(new CHammer(GameWorld(), m_Owner, m_Pos, 5));
	group.push_back(new CInfAmmo(GameWorld(), m_Owner, m_Pos));
	group.push_back(new CAllWeapons(GameWorld(), m_Owner, m_Pos));
	group.push_back(new CFastReload(GameWorld(), m_Owner, m_Pos));
	group.push_back(new CHealthRegen(GameWorld(), m_Owner, m_Pos));
	group.push_back(new CNoSelfDMG(GameWorld(), m_Owner, m_Pos));

	m_aShop.push_back(group);
	group.clear();

	group.push_back(new CHook(GameWorld(), m_Owner, m_Pos, 1));
	group.push_back(new CHook(GameWorld(), m_Owner, m_Pos, 2));
	group.push_back(new CHook(GameWorld(), m_Owner, m_Pos, 3));

	m_aShop.push_back(group);
	group.clear();

	group.push_back(new CInfJumps(GameWorld(), m_Owner, m_Pos));
	group.push_back(new CPushAura(GameWorld(), m_Owner, m_Pos));
	group.push_back(new CPullAura(GameWorld(), m_Owner, m_Pos));

	m_aShop.push_back(group);
	group.clear();
	
	//m_Menu = 1;
	list<list<CEntity*>>::iterator it;
	for(it = m_aShop.begin(); it != m_aShop.end(); ++it) {
		list<CEntity*>::iterator entIt;
		list<CEntity*>& pEnts = *it;
		for(entIt = pEnts.begin(); entIt != pEnts.end(); ++entIt) {
			CEntity *ent = *entIt;
			ent->m_Visible = true;
		}
	}

	GameWorld()->InsertEntity(this);
}

CGui::~CGui()
{
	m_aShop.clear();
}

void CGui::Switch()
{
	dbg_msg("debug", "switch");
}

void CGui::Reset()
{
	GameServer()->m_World.DestroyEntity(this);
}

void CGui::Tick()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);

	if(!pOwner)
	{
		Reset();
		return;
	}
}

void CGui::CheckClick(vec2 Pos)
{
	dbg_msg("debug", "checkclick");
}

void CGui::Menu()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);

	if(!pOwner)
		return;

	list<list<CEntity*>>::iterator it;
	list<CEntity*>::iterator itE;
	
	if (!GameServer()->Collision()->TileShop(pOwner->m_Pos)) { // we want to hide them only once
		if (m_ItrFlag) {
			for(it = m_aShop.begin(); it != m_aShop.end(); ++it) {
				list<CEntity*>::iterator entIt;
				list<CEntity*>& pEnts = *it;
				for(entIt = pEnts.begin(); entIt != pEnts.end(); ++entIt) {
					CEntity *ent = *entIt;
					ent->m_Visible = false;
				}
			}
			m_ItrFlag = false;
		}
		m_MsgFlag = true;
		return;
	} else
		m_ItrFlag = false;
		
	m_Group = pOwner->m_ShopGroup;

	if (m_MsgFlag) {
		GameServer()->SendChatTarget_Localization(pOwner->GetPlayer()->GetCID(), CHATCATEGORY_JOIN, _("Welcome to the Shop!"));
		GameServer()->SendChatTarget_Localization(pOwner->GetPlayer()->GetCID(), CHATCATEGORY_JOIN, _("Use F3 and F4 to switch the shop menu"));
		GameServer()->SendChatTarget_Localization(pOwner->GetPlayer()->GetCID(), CHATCATEGORY_JOIN, _("Or use /shop to quick Switch Shop menu"));
		m_MsgFlag = false;
	}
	
	if (!m_ItrFlag) { // we only wanna do this, while on shop
		for(it = m_aShop.begin(); it != m_aShop.end(); ++it) {
			list<CEntity*>::iterator entIt;
			list<CEntity*>& pEnts = *it;
			for(entIt = pEnts.begin(); entIt != pEnts.end(); ++entIt) {
				CEntity *ent = *entIt;
				ent->m_Visible = false;
			}
		}

		int Size = next(m_aShop.begin(), m_Group)->size();
		int VisibleItems = Size - pOwner->m_ShopPage < 6 ? Size - pOwner->m_ShopPage : 6;
		int i = 1;


		if (pOwner->m_ShopPage < 0)
			pOwner->m_ShopPage = Size - Size % 6;
		else if (pOwner->m_ShopPage > Size - 1)
			pOwner->m_ShopPage = 0;
			
		list<CEntity*> Ents = *next(m_aShop.begin(), m_Group);

		for(itE = next(Ents.begin(), pOwner->m_ShopPage); (itE != next(Ents.begin(), pOwner->m_ShopPage + VisibleItems) && itE != Ents.end()); ++itE)
		{
			CEntity *Ent = *itE;

			if(!pOwner)
				return;

			Ent->m_Visible = true;
			if (!(VisibleItems % 2))
				Ent->m_Pos = pOwner->m_Pos + normalize(GetDir((2*pi/VisibleItems) * -i))*250;
			else
				Ent->m_Pos = pOwner->m_Pos + normalize(GetDir((2*pi/VisibleItems) * -i - 0.5*pi))*250;

			i++;
		}
		m_ItrFlag = true;
	}
}

void CGui::Snap(int SnappingClient)
{
	if(SnappingClient != m_Owner)
		return;

	Menu();
}
