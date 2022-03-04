#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include "flamethrower.h"

CFlameThrower::CFlameThrower(CGameWorld *pGameWorld, vec2 Direction, vec2 ProjStartPos, int Owner, int Damage)
    : CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE)
{
    m_Owner = Owner;
    m_Pos = ProjStartPos;
    m_Dir = Direction;
    m_Damage = Damage;

    m_PosTmp[0] = m_Pos + Direction * 100.0f;

    for (int i = 0; i < MAX_PROJECTILES; i++)
        m_IDs[i] = Server()->SnapNewID();

    GameWorld()->InsertEntity(this);
}

void CFlameThrower::Reset()
{
    GameServer()->m_World.DestroyEntity(this);
    // for (int i = 0; i < MAX_PROJECTILES; i++)
    //     Server()->SnapFreeID(m_IDs[i]);
}

void CFlameThrower::HitCharacter()
{
    CPlayer *m_pPlayer = GameServer()->m_apPlayers[m_Owner];
    if (!m_pPlayer)
    {
        Reset();
        return;
    }

    CCharacter *m_pOwner = m_pPlayer->GetCharacter();
    if (!m_pOwner || !m_pOwner->IsAlive())
    {
        Reset();
        return;
    }

    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        for (int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
        {
            if (ClientID == m_Owner || !m_PosTmp[i])
                continue;

            if (GameServer()->Collision()->IntersectLine(m_pOwner->m_Pos, m_PosTmp[i], NULL, NULL) || GameServer()->Collision()->CheckPoint(m_PosTmp[i]))
                return;
        
            // deal damage
            CCharacter *apEnts[MAX_CLIENTS];
            float Radius = 135.0f;
            float InnerRadius = 48.0f;
            int Num = GameWorld()->FindEntities(m_PosTmp[i], Radius, (CEntity **)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
            for (int i = 0; i < Num; i++)
            {
                vec2 Diff = apEnts[i]->m_Pos - m_PosTmp[i];
                vec2 ForceDir(0, 1);
                float l = length(Diff);
                if (l)
                    ForceDir = normalize(Diff);
                l = 1 - clamp((l - InnerRadius) / (Radius - InnerRadius), 0.0f, 1.0f);
                float Dmg = m_Damage * l;
                if ((int)Dmg)
                    apEnts[i]->TakeDamage(vec2(0.0f, 0.0f), (int)Dmg, m_Owner, WEAPON_GRENADE);
            }
        }
    }
}

void CFlameThrower::Tick()
{
    CPlayer *m_pPlayer = GameServer()->m_apPlayers[m_Owner];
    if (!m_pPlayer)
    {
        Reset();
        return;
    }

    CCharacter *m_pOwner = m_pPlayer->GetCharacter();
    if (!m_pOwner || !m_pOwner->IsAlive())
    {
        Reset();
        return;
    }

    for (int i = 1; i < MAX_PROJECTILES; i++)
    {
        if (!m_PosTmp[i - 1])
            continue;

        m_PosTmp[i] = m_PosTmp[i - 1] + GetDir(GetAngle(m_Dir)) * 32 * 2;
    }

    HitCharacter();
}

void CFlameThrower::Snap(int SnappingClient)
{
    CPlayer *m_pPlayer = GameServer()->m_apPlayers[m_Owner];
    if (!m_pPlayer)
    {
        Reset();
        return;
    }

    CCharacter *m_pOwner = m_pPlayer->GetCharacter();
    if (!m_pOwner || !m_pOwner->IsAlive())
    {
        Reset();
        return;
    }

    CNetEvent_Explosion *pEplosion[MAX_PROJECTILES];
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        if (!m_PosTmp[i])
        {
            Reset();
            return;
        }

        if (GameServer()->Collision()->IntersectLine(m_pOwner->m_Pos, m_PosTmp[i], NULL, NULL) || GameServer()->Collision()->CheckPoint(m_PosTmp[i]))
        {
            Reset();
            return;
        }

        pEplosion[i] = static_cast<CNetEvent_Explosion *>(Server()->SnapNewItem(NETEVENTTYPE_EXPLOSION, m_IDs[i], sizeof(CNetEvent_Explosion)));
        if (!pEplosion[i])
            return;

        pEplosion[i]->m_X = (int)m_PosTmp[i].x;
        pEplosion[i]->m_Y = (int)m_PosTmp[i].y;
    }

    Reset();
}