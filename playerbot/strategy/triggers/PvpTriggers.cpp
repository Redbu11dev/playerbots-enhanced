#include "botpch.h"
#include "../../playerbot.h"
#include "PvpTriggers.h"
#include "ServerFacade.h"
#include "BattleGroundWS.h"
#ifndef MANGOSBOT_ZERO
#include "BattleGroundEY.h"
#endif

using namespace ai;

bool EnemyPlayerNear::IsActive()
{
    Unit* current = AI_VALUE(Unit*, "current target");
    if (current)
        return current != AI_VALUE(Unit*, "enemy player target");

    return AI_VALUE(Unit*, "enemy player target");
}

bool PlayerHasNoFlag::IsActive()
{
#ifdef MANGOS
    if (ai->GetBot()->InBattleGround())
    {
        if (ai->GetBot()->GetBattleGroundTypeId() == BattleGroundTypeId::BATTLEGROUND_WS)
        {
            BattleGroundWS *bg = (BattleGroundWS*)ai->GetBot()->GetBattleGround();
            if (!(bg->GetFlagState(bg->GetOtherTeam(bot->GetTeam())) == BG_WS_FLAG_STATE_ON_PLAYER))
                return true;
            if (bot->GetObjectGuid() == bg->GetAllianceFlagCarrierGuid() || bot->GetObjectGuid() == bg->GetHordeFlagCarrierGuid())
            {
                return false;
            }
            return true;
        }
        return false;
    }
#endif
    return false;
}

bool PlayerIsInBattleground::IsActive()
{
    return ai->GetBot()->InBattleGround();
}

bool BgWaitingTrigger::IsActive()
{
    if (bot->InBattleGround())
    {
        if (bot->GetBattleGround() && bot->GetBattleGround()->GetStatus() == STATUS_WAIT_JOIN)
            return true;
    }
    return false;
}

bool BgActiveTrigger::IsActive()
{
    if (bot->InBattleGround())
    {
        if (bot->GetBattleGround() && bot->GetBattleGround()->GetStatus() == STATUS_IN_PROGRESS)
            return true;
    }
    return false;
}

bool BgInviteActiveTrigger::IsActive()
{
    if (bot->InBattleGround() || !bot->InBattleGroundQueue())
    {
        return false;
    }

    for (int i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
    {
        BattleGroundQueueTypeId queueTypeId = bot->GetBattleGroundQueueTypeId(i);
        if (queueTypeId == BATTLEGROUND_QUEUE_NONE)
            continue;

        BattleGroundQueue& bgQueue = sServerFacade.bgQueue(queueTypeId);
        GroupQueueInfo ginfo;
        if (bgQueue.GetPlayerGroupInfoData(bot->GetObjectGuid(), &ginfo))
        {
            if (ginfo.isInvitedToBgInstanceGuid && ginfo.removeInviteTime)
            {
                sLog.outDetail("Bot #%d <%s> (%u %s) : Invited to BG but not in BG", bot->GetGUIDLow(), bot->GetName(), bot->GetLevel(), bot->GetTeam() == ALLIANCE ? "A" : "H");
                return true;
            }
        }
    }
    return false;
}

bool PlayerIsInBattlegroundWithoutFlag::IsActive()
{
#ifdef MANGOS
    if (ai->GetBot()->InBattleGround())
    {
        if (ai->GetBot()->GetBattleGroundTypeId() == BattleGroundTypeId::BATTLEGROUND_WS)
        {
            BattleGroundWS *bg = (BattleGroundWS*)ai->GetBot()->GetBattleGround();
            if (!(bg->GetFlagState(bg->GetOtherTeam(bot->GetTeam())) == BG_WS_FLAG_STATE_ON_PLAYER))
                return true;
            if (bot->GetGUIDLow() == bg->GetAllianceFlagCarrierGuid() || bot->GetGUIDLow() == bg->GetHordeFlagCarrierGuid())
            {
                return false;
            }
        }
        return true;
    }
#endif
    return false;
}

bool PlayerHasFlag::IsActive()
{
    if (bot->InBattleGround())
    {
        if (bot->GetBattleGroundTypeId() == BattleGroundTypeId::BATTLEGROUND_WS)
        {
            BattleGroundWS *bg = (BattleGroundWS*)ai->GetBot()->GetBattleGround();
            if (bot->GetObjectGuid() == bg->GetFlagCarrierGuid(TEAM_INDEX_ALLIANCE) || bot->GetObjectGuid() == bg->GetFlagCarrierGuid(TEAM_INDEX_HORDE))
            {
                return true;
            }
        }
#ifndef MANGOSBOT_ZERO
        if (bot->GetBattleGroundTypeId() == BattleGroundTypeId::BATTLEGROUND_EY)
        {
            BattleGroundEY* bg = (BattleGroundEY*)ai->GetBot()->GetBattleGround();
            return bot->GetObjectGuid() == bg->GetFlagCarrierGuid();
        }
#endif
        return false;
    }
    return false;
}

bool TeamHasFlag::IsActive()
{
#ifdef MANGOS
    if (ai->GetBot()->InBattleGround())
    {
        if (ai->GetBot()->GetBattleGroundTypeId() == BattleGroundTypeId::BATTLEGROUND_WS)
        {
            BattleGroundWS *bg = (BattleGroundWS*)ai->GetBot()->GetBattleGround();

            if (bot->GetObjectGuid() == bg->GetAllianceFlagCarrierGuid() || bot->GetObjectGuid() == bg->GetHordeFlagCarrierGuid())
            {
                return false;
            }

            if (bg->GetFlagState(bg->GetOtherTeam(bot->GetTeam())) == BG_WS_FLAG_STATE_ON_PLAYER)
                return true;
        }
        return false;
    }
#endif
    return false;
}

bool EnemyTeamHasFlag::IsActive()
{
    if (ai->GetBot()->InBattleGround())
    {
        if (ai->GetBot()->GetBattleGroundTypeId() == BattleGroundTypeId::BATTLEGROUND_WS)
        {
            BattleGroundWS *bg = (BattleGroundWS*)ai->GetBot()->GetBattleGround();

            if (bot->GetTeam() == HORDE)
            {
                if (!bg->GetFlagCarrierGuid(TEAM_INDEX_HORDE).IsEmpty())
                    return true;
            }
            else
            {
                if (!bg->GetFlagCarrierGuid(TEAM_INDEX_ALLIANCE).IsEmpty())
                    return true;
            }
        }
        return false;
    }
    return false;
}

bool EnemyFlagCarrierNear::IsActive()
{
    Unit* carrier = AI_VALUE(Unit*, "enemy flag carrier");
    return carrier && sServerFacade.IsDistanceLessOrEqualThan(sServerFacade.GetDistance2d(bot, carrier), VISIBILITY_DISTANCE_SMALL);
}

bool TeamFlagCarrierNear::IsActive()
{
    Unit* carrier = AI_VALUE(Unit*, "team flag carrier");
    return carrier && sServerFacade.IsDistanceLessOrEqualThan(sServerFacade.GetDistance2d(bot, carrier), VISIBILITY_DISTANCE_SMALL);
}

bool PlayerWantsInBattlegroundTrigger::IsActive()
{
    if (bot->InBattleGround())
        return false;

    if (bot->GetBattleGround() && bot->GetBattleGround()->GetStatus() == STATUS_WAIT_JOIN)
        return false;

    if (bot->GetBattleGround() && bot->GetBattleGround()->GetStatus() == STATUS_IN_PROGRESS)
        return false;

    if (!bot->CanJoinToBattleground())
        return false;

    return true;
};

bool VehicleNearTrigger::IsActive()
{
    list<ObjectGuid> npcs = AI_VALUE(list<ObjectGuid>, "nearest vehicles");
    return npcs.size();
}

bool InVehicleTrigger::IsActive()
{
    return ai->IsInVehicle();
}
