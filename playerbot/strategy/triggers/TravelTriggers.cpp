
#include "playerbot/playerbot.h"
#include "TravelTriggers.h"

#include "playerbot/PlayerbotAIConfig.h"
#include "playerbot/TravelMgr.h"
#include "playerbot/ServerFacade.h"
using namespace ai;

bool NoTravelTargetTrigger::IsActive()
{
    return !ai->GetTravelTarget()->isActive();
}

bool FarFromTravelTargetTrigger::IsActive()
{
    return ai->GetTravelTarget()->isTraveling();
}

bool NearDarkPortalTrigger::IsActive()
{
    return sServerFacade.GetAreaId(bot) == 72;
}

bool AtDarkPortalAzerothTrigger::IsActive()
{
    if (sServerFacade.GetAreaId(bot) == 72)
    {
        if (sServerFacade.GetDistance2d(bot, -11906.9f, -3208.53f) < 20.0f)
        {
            return true;
        }
    }
    return false;
}

bool AtDarkPortalOutlandTrigger::IsActive()
{
    if (sServerFacade.GetAreaId(bot) == 3539)
    {
        if (sServerFacade.GetDistance2d(bot, -248.1939f, 921.919f) < 10.0f)
        {
            return true;
        }
    }
    return false;
}
