/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
/*
    By Puppet
*/
#ifndef GAME_SERVER_PUP_ENTITIES_FLAMETHTOWER_H
#define GAME_SERVER_PUP_ENTITIES_FLAMETHROWER_H

#include <game/server/entity.h>

class CFlameThrower : public CEntity
{
public:

    CFlameThrower(CGameWorld *pGameWorld, vec2 Direction, vec2 ProjStartPos, int Owner, int Damage);

    virtual void Reset();
    virtual void Tick();
    virtual void Snap(int SnappingClient);

    virtual void HitCharacter();

private:
    enum {
        MAX_PROJECTILES = 6
    };

    int m_Owner;
    int m_Damage;
    vec2 m_Pos;
    vec2 m_Dir;
    vec2 m_PosTmp[MAX_PROJECTILES];
    int m_IDs[MAX_PROJECTILES];
};

#endif