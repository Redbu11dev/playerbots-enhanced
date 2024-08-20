
#include "playerbot/playerbot.h"
#include "playerbot/LootObjectStack.h"
#include "ChooseTravelTargetAction.h"
#include "playerbot/PlayerbotAIConfig.h"
#include "playerbot/TravelMgr.h"
#include <iomanip>
#include <boost/algorithm/string.hpp>

using namespace ai;

bool ChooseTravelTargetAction::Execute(Event& event)
{
    Player* requester = event.getOwner() ? event.getOwner() : GetMaster();

    auto worldPositionRef = ai->GetCurrentWorldPositionRef();

    if (!worldPositionRef || !bot->IsInWorld()
        || (worldPositionRef.getX() == 0 && worldPositionRef.getY() == 0 && worldPositionRef.getZ() == 0))
    {
        return false;
    }

    //Get the current travel target. This target is no longer active.
    TravelTarget* oldTarget = ai->GetTravelTarget();

    if (oldTarget == nullptr)
    {
        SetNullTarget(oldTarget);
        return true;
    }

    //Select a new target to travel to.
    TravelTarget newTarget = TravelTarget(ai);

    bool isOldTargetAtZeros = oldTarget->isNullDestination()
        || !oldTarget->getPosition()
        || (oldTarget->getPosition()->getX() == 0 && oldTarget->getPosition()->getY() == 0 && oldTarget->getPosition()->getZ() == 0);

    if (!oldTarget->isForced()
        || oldTarget->getStatus() == TravelStatus::TRAVEL_STATUS_EXPIRED
        || isOldTargetAtZeros)
    {
        getNewTarget(requester, &newTarget, oldTarget);
    }
    else
    {
        newTarget.copyTarget(oldTarget);
    }

    //If the new target is not active we failed.
    if (!newTarget.isActive() && !newTarget.isForced())
       return false;

    setNewTarget(requester, &newTarget, oldTarget);

    return true;
}

//Select a new travel target.
//Currently this selects mostly based on priority (current quest > new quest).
//This works fine because destinations can be full (max 15 bots per quest giver, max 1 bot per quest mob).
//
//Eventually we want to rewrite this to be more intelligent.
void ChooseTravelTargetAction::getNewTarget(Player* requester, TravelTarget* newTarget, TravelTarget* oldTarget)
{

    std::ostringstream out;
    out << "---------bot: " << bot->GetName() << " Attempting to find travel target in ChooseTravelTargetAction";
    sLog.outBasic(out.str().c_str());

    bool foundTarget = SetGroupTarget(requester, newTarget); //Join groups members

    bool isOldTargetExpired = oldTarget->getStatus() == TravelStatus::TRAVEL_STATUS_EXPIRED;

    bool isOldTargetAtZeros = oldTarget->isNullDestination()
        || !oldTarget->getPosition()
        || (oldTarget->getPosition()->getX() == 0 && oldTarget->getPosition()->getY() == 0 && oldTarget->getPosition()->getZ() == 0);



    WorldPosition botPos(bot);

    uint32 botMaxLevel = sPlayerbotAIConfig.randomBotMaxLevel;

    uint8 maxRetries = 3;

    bool alreadyTriedToFindQuest = false;

    //loop few times to try to find target
    for (uint8 retry = 1; retry < maxRetries + 1; ++retry)
    {
        //Continue current target.
        //90%
        if (!foundTarget && isOldTargetExpired && !isOldTargetAtZeros && (!botPos.isOverworld() || urand(1, 100) <= 90))        //90% chance or currently in dungeon.
        {
            ai->TellDebug(requester, "Continue previous target", "debug travel");
            PerformanceMonitorOperation* pmo = sPerformanceMonitor.start(PERF_MON_VALUE, "SetCurrentTarget", &context->performanceStack);
            foundTarget = SetCurrentTarget(requester, newTarget, oldTarget);             //Extend current target.
            if (pmo) pmo->finish();
        }

        //do some bag shit
        //50%
        if (urand(1, 100) <= 50)
        {
            //Empty bags/repair
            bool shouldRpg = false;
            if (AI_VALUE2(bool, "group or", "should sell,can sell,following party,near leader")) //One of party members wants to sell items (full bags).
            {
                ai->TellDebug(requester, "Group needs to sell items (full bags)", "debug travel");
                shouldRpg = true;
            }
            else if (AI_VALUE2(bool, "group or", "should repair,can repair,following party,near leader")) //One of party members wants to repair.
            {
                ai->TellDebug(requester, "Group needs to repair", "debug travel");
                shouldRpg = true;
            }
            else if (AI_VALUE2(bool, "group or", "should sell,can ah sell,following party,near leader") && bot->GetLevel() > 5) //One of party members wants to sell items to AH (full bags).
            {
                ai->TellDebug(requester, "Group needs to ah items (full bags)", "debug travel");
                shouldRpg = true;
            }
            else if (!shouldRpg && ai->HasStrategy("free", BotState::BOT_STATE_NON_COMBAT))
            {
                if (AI_VALUE(bool, "should sell") && (AI_VALUE(bool, "can sell") || AI_VALUE(bool, "can ah sell"))) //Bot wants to sell (full bags).
                {
                    ai->TellDebug(requester, "Bot needs to sell/ah items (full bags)", "debug travel");
                    shouldRpg = true;
                }
                else if (AI_VALUE(bool, "should repair") && AI_VALUE(bool, "can repair")) //Bot wants to repair.
                {
                    ai->TellDebug(requester, "Bot needs to repair", "debug travel");
                    shouldRpg = true;
                }
            }

            if (false && shouldRpg)
            {
                PerformanceMonitorOperation* pmo = sPerformanceMonitor.start(PERF_MON_VALUE, "SetRpgTarget1", &context->performanceStack);
                foundTarget = SetRpgTarget(requester, newTarget, true);                           //Go to town to sell items or repair
                if (pmo) pmo->finish();

                if (foundTarget)
                {
                    std::ostringstream out;
                    out << "-------shouldRpg selecting travel point: " << newTarget->getDestination()->getTitle() << "bot: " << bot->GetName();
                    sLog.outBasic(out.str().c_str());
                }
            }
        }

        //Do class quests (start, do, end)
        //100% chance
        uint8 doClassQuestsChance = 100;
        if (false && bot->GetLevel() >= 10 && !foundTarget && ((retry == maxRetries && bot->GetLevel() < botMaxLevel) || urand(1, 100) <= 100))
        {
            ai->TellDebug(requester, "Do questing", "debug travel");
            PerformanceMonitorOperation* pmo = sPerformanceMonitor.start(PERF_MON_VALUE, "SetQuestTarget", &context->performanceStack);
            foundTarget = SetQuestTarget(requester, newTarget, true, true);
            if (pmo) pmo->finish();

            if (foundTarget)
            {
                std::ostringstream out;
                out << "---------bot: " << bot->GetName() << " DoClassQuest (found) selecting travel point: " << newTarget->getDestination()->getTitle();
                sLog.outBasic(out.str().c_str());
            }
            else
            {
                std::ostringstream out;
                out << "---------bot: " << bot->GetName() << " DoClassQuest (not found) target not found";
                sLog.outBasic(out.str().c_str());
            }
        }

        //Do quests (start, do, end)
        //95% chance or 100% chance on last retry if bot is not max level
        uint8 doQuestsChance = bot->GetLevel() < botMaxLevel ? 95 : 30; //can finally relax a bit from questing at max level
        if (!foundTarget && !alreadyTriedToFindQuest && ((retry == maxRetries && bot->GetLevel() < botMaxLevel) || urand(1, 100) <= doQuestsChance))
        {
            ai->TellDebug(requester, "Do questing", "debug travel");
            PerformanceMonitorOperation* pmo = sPerformanceMonitor.start(PERF_MON_VALUE, "SetQuestTarget", &context->performanceStack);
            foundTarget = SetQuestTarget(requester, newTarget, true, false);
            if (pmo) pmo->finish();

            alreadyTriedToFindQuest = true;

            if (foundTarget)
            {
                std::ostringstream out;
                out << "---------bot: " << bot->GetName() << " DoQuest (found) selecting travel point: " << newTarget->getDestination()->getTitle();
                sLog.outBasic(out.str().c_str());
            }
            else
            {
                std::ostringstream out;
                out << "---------bot: " << bot->GetName() << " DoQuest (not found) target not found";
                sLog.outBasic(out.str().c_str());
            }
        }

        bool shouldGetMoney = AI_VALUE(bool, "should get money");

        //Grind for money (if not in dungeon)
        if (false && !foundTarget && shouldGetMoney && botPos.isOverworld())
        {
            //Empty mail for money
            if (AI_VALUE(bool, "can get mail"))
            {
                ai->TellDebug(requester, "Get mail for money", "debug travel");
                PerformanceMonitorOperation* pmo = sPerformanceMonitor.start(PERF_MON_VALUE, "SetGoTarget1", &context->performanceStack);
                foundTarget = SetGOTypeTarget(requester, newTarget, GAMEOBJECT_TYPE_MAILBOX, "", false);  //Find a mailbox
                if (pmo) pmo->finish();
            }
        }

        //Get mail
        if (false && !foundTarget && urand(1, 100) > 70)                                  //30% chance
        {
            if (AI_VALUE(bool, "can get mail"))
            {
                ai->TellDebug(requester, "Get mail", "debug travel");
                PerformanceMonitorOperation* pmo = sPerformanceMonitor.start(PERF_MON_VALUE, "SetGoTarget2", &context->performanceStack);
                foundTarget = SetGOTypeTarget(requester, newTarget, GAMEOBJECT_TYPE_MAILBOX, "", false);  //Find a mailbox
                if (pmo) pmo->finish();
            }
        }

        //Rpg in city
        if (false && !foundTarget && urand(1, 100) > 90 && bot->GetLevel() > 5 && botPos.isOverworld())           //10% chance if not currenlty in dungeon.
        {
            ai->TellDebug(requester, "Random rpg in city", "debug travel");
            PerformanceMonitorOperation* pmo = sPerformanceMonitor.start(PERF_MON_VALUE, "SetNpcFlagTarget2", &context->performanceStack);
            foundTarget = SetNpcFlagTarget(requester, newTarget, { UNIT_NPC_FLAG_BANKER,UNIT_NPC_FLAG_BATTLEMASTER,UNIT_NPC_FLAG_AUCTIONEER });
            if (pmo) pmo->finish();
        }

        //Just hang with an npc
        if (false && !foundTarget && urand(1, 100) > 30)                                 //30% chance
        {
            {
                ai->TellDebug(requester, "Rpg with random npcs", "debug travel");
                PerformanceMonitorOperation* pmo = sPerformanceMonitor.start(PERF_MON_VALUE, "SetRpgTarget2", &context->performanceStack);
                foundTarget = SetRpgTarget(requester, newTarget, false);
                if (foundTarget)
                    newTarget->setForced(true);
                if (pmo) pmo->finish();
            }
        }

        //Dungeon in group.
        if (false && !foundTarget && (!botPos.isOverworld() || urand(1, 100) > 50))      //50% chance or currently in instance.
        {
            if (AI_VALUE(bool, "can fight boss"))
            {
                ai->TellDebug(requester, "Fight boss for loot", "debug travel");
                PerformanceMonitorOperation* pmo = sPerformanceMonitor.start(PERF_MON_VALUE, "SetBossTarget", &context->performanceStack);
                foundTarget = SetBossTarget(requester, newTarget);                         //Go fight a (dungeon boss)
                if (pmo) pmo->finish();
            }
        }

        //Explore a nearby unexplored area.
        if (false && !foundTarget && ai->HasStrategy("explore", BotState::BOT_STATE_NON_COMBAT) && urand(1, 100) > 90)  //10% chance Explore a unexplored sub-zone.
        {
            ai->TellDebug(requester, "Explore unexplored areas", "debug travel");
            PerformanceMonitorOperation* pmo = sPerformanceMonitor.start(PERF_MON_VALUE, "SetExploreTarget", &context->performanceStack);
            foundTarget = SetExploreTarget(requester, newTarget);
            if (pmo) pmo->finish();
        }

        //grind random mobs
        if (false && !foundTarget && bot->GetLevel() >= botMaxLevel)
        {
            ai->TellDebug(requester, "Grind random mobs", "debug travel");
            PerformanceMonitorOperation* pmo = sPerformanceMonitor.start(PERF_MON_VALUE, "SetGrindTarget2", &context->performanceStack);
            foundTarget = SetGrindTarget(requester, newTarget);
            if (pmo) pmo->finish();
        }

        if (foundTarget)
            break;
    }

    // idle
    if (!foundTarget)
    {
        ai->TellDebug(requester, "Stop traveling", "debug travel");
        SetNullTarget(newTarget);
        //Idle a bit.
    }
}

void ChooseTravelTargetAction::setNewTarget(Player* requester, TravelTarget* newTarget, TravelTarget* oldTarget)
{
    if (oldTarget->isForced()
        && (oldTarget->getStatus() == TravelStatus::TRAVEL_STATUS_COOLDOWN || oldTarget->getStatus() == TravelStatus::TRAVEL_STATUS_EXPIRED)
        && ai->HasStrategy("travel once", BotState::BOT_STATE_NON_COMBAT)
        )
    {
        ai->ChangeStrategy("-travel once", BotState::BOT_STATE_NON_COMBAT);
        ai->TellPlayerNoFacing(requester, "Arrived at " + oldTarget->getDestination()->getTitle());
        SetNullTarget(newTarget);
    }

    if(AI_VALUE2(bool, "can free move to", newTarget->GetPosStr()))
        ReportTravelTarget(requester, newTarget, oldTarget);

    //If we are heading to a creature/npc clear it from the ignore list.
    if (oldTarget && oldTarget == newTarget && newTarget->getEntry())
    {
        std::set<ObjectGuid>& ignoreList = context->GetValue<std::set<ObjectGuid>&>("ignore rpg target")->Get();

        for (auto& i : ignoreList)
        {
            if (i.GetEntry() == newTarget->getEntry())
            {
                ignoreList.erase(i);
            }
        }

        context->GetValue<std::set<ObjectGuid>&>("ignore rpg target")->Set(ignoreList);
    }

    //Actually apply the new target to the travel target used by the bot.
    oldTarget->copyTarget(newTarget);

    //If we are idling but have a master. Idle only 10 seconds.
    if (ai->GetMaster() && oldTarget->isActive() && oldTarget->getDestination()->getName() == "NullTravelDestination")
        oldTarget->setExpireIn(10 * IN_MILLISECONDS);
    else if (oldTarget->isForced()) //Make sure travel goes into cooldown after getting to the destination.
        oldTarget->setExpireIn(HOUR * IN_MILLISECONDS);

    //Clear rpg and attack/grind target. We want to travel, not hang around some more.
    RESET_AI_VALUE(GuidPosition,"rpg target");
    RESET_AI_VALUE(ObjectGuid,"attack target");
};

//Tell the master what travel target we are moving towards.
//This should at some point be rewritten to be denser or perhaps logic moved to ->getTitle()
void ChooseTravelTargetAction::ReportTravelTarget(Player* requester, TravelTarget* newTarget, TravelTarget* oldTarget)
{
    TravelDestination* destination = newTarget->getDestination();

    TravelDestination* oldDestination = oldTarget->getDestination();

    std::ostringstream out;

    if (newTarget->isForced())
        out << "(Forced) ";

    if (destination->getName() == "QuestGiverTravelDestination"
        || destination->getName() == "QuestTakerTravelDestination"
        || destination->getName() == "QuestObjectiveTravelDestination")
    {
        QuestTravelDestination* QuestDestination = (QuestTravelDestination*)destination;
        Quest const* quest = QuestDestination->GetQuestTemplate();
        WorldPosition botLocation(bot);
        CreatureInfo const* cInfo = NULL;
        GameObjectInfo const* gInfo = NULL;

        if (destination->getEntry() > 0)
            cInfo = ObjectMgr::GetCreatureTemplate(destination->getEntry());
        else
            gInfo = ObjectMgr::GetGameObjectInfo(destination->getEntry() * -1);

        std::string Sub;

        if (newTarget->isGroupCopy())
            out << "Following group ";
        else if(oldDestination && oldDestination == destination)
            out << "Continuing ";
        else
            out << "Traveling ";

        out << round(newTarget->getDestination()->distanceTo(botLocation)) << "y";

        out << " for " << chat->formatQuest(quest);

        out << " to " << QuestDestination->getTitle();
    }
    else if (destination->getName() == "RpgTravelDestination")
    {
        RpgTravelDestination* RpgDestination = (RpgTravelDestination*)destination;

        WorldPosition botLocation(bot);

        if (newTarget->isGroupCopy())
            out << "Following group ";
        else if (oldDestination && oldDestination == destination)
            out << "Continuing ";
        else
            out << "Traveling ";

        out << round(newTarget->getDestination()->distanceTo(botLocation)) << "y";

        out << " for ";

        if (RpgDestination->getEntry() > 0)
        {
            CreatureInfo const* cInfo = RpgDestination->getCreatureInfo();

            if (cInfo)
            {
                if ((cInfo->NpcFlags & UNIT_NPC_FLAG_VENDOR ) && AI_VALUE2(bool, "group or", "should sell,can sell"))
                    out << "selling items";
                else if ((cInfo->NpcFlags & UNIT_NPC_FLAG_REPAIR) && AI_VALUE2(bool, "group or", "should repair,can repair"))
                    out << "repairing";
                else if ((cInfo->NpcFlags & UNIT_NPC_FLAG_AUCTIONEER) && AI_VALUE2(bool, "group or", "should sell,can ah sell"))
                    out << "posting items on the auctionhouse";
                else
                    out << "rpg";
            }
            else
                out << "rpg";
        }
        else
        {
            GameObjectInfo const* gInfo = RpgDestination->getGoInfo();

            if (gInfo)
            {
                if (gInfo->type == GAMEOBJECT_TYPE_MAILBOX && AI_VALUE(bool, "can get mail"))
                    out << "getting mail";
                else
                    out << "rpg";
            }
            else
                out << "rpg";
        }

        out << " to " << RpgDestination->getTitle();
    }
    else if (destination->getName() == "ExploreTravelDestination")
    {
        ExploreTravelDestination* ExploreDestination = (ExploreTravelDestination*)destination;

        WorldPosition botLocation(bot);

        if (newTarget->isGroupCopy())
            out << "Following group ";
        else if (oldDestination && oldDestination == destination)
            out << "Continuing ";
        else
            out << "Traveling ";

        out << round(newTarget->getDestination()->distanceTo(botLocation)) << "y";

        out << " for exploration";

        out << " to " << ExploreDestination->getTitle();
    }
    else if (destination->getName() == "GrindTravelDestination")
    {
        GrindTravelDestination* GrindDestination = (GrindTravelDestination*)destination;

        WorldPosition botLocation(bot);

        if (newTarget->isGroupCopy())
            out << "Following group ";
        else if (oldDestination && oldDestination == destination)
            out << "Continuing ";
        else
            out << "Traveling ";

        out << round(newTarget->getDestination()->distanceTo(botLocation)) << "y";

        out << " for grinding money";

        out << " to " << GrindDestination->getTitle();
    }
    else if (destination->getName() == "BossTravelDestination")
    {
        BossTravelDestination* BossDestination = (BossTravelDestination*)destination;

        WorldPosition botLocation(bot);

        if (newTarget->isGroupCopy())
            out << "Following group ";
        else if (oldDestination && oldDestination == destination)
            out << "Continuing ";
        else
            out << "Traveling ";

        out << round(newTarget->getDestination()->distanceTo(botLocation)) << "y";

        out << " for good loot";

        out << " to " << BossDestination->getTitle();
    }
    else if (destination->getName() == "NullTravelDestination")
    {
        if (!oldTarget->getDestination() || oldTarget->getDestination()->getName() != "NullTravelDestination")
        {
            out.clear();
            out << "No where to travel. Idling a bit.";
        }
    }

    if (out.str().empty())
        return;

    ai->TellPlayerNoFacing(requester, out,PlayerbotSecurityLevel::PLAYERBOT_SECURITY_ALLOW_ALL, false);

    std::string message = out.str().c_str();

    if (sPlayerbotAIConfig.hasLog("travel_map.csv"))
    {
        WorldPosition botPos(bot);
        WorldPosition destPos = *newTarget->getPosition();

        std::ostringstream out;
        out << sPlayerbotAIConfig.GetTimestampStr() << "+00,";
        out << bot->GetName() << ",";
        out << std::fixed << std::setprecision(2);

        out << std::to_string(bot->getRace()) << ",";
        out << std::to_string(bot->getClass()) << ",";
        float subLevel = ai->GetLevelFloat();

        out << subLevel << ",";

        if (!destPos)
            destPos = botPos;

        botPos.printWKT({ botPos,destPos }, out, 1);

        if (destination->getName() == "NullTravelDestination")
            out << "0,";
        else
            out << round(newTarget->getDestination()->distanceTo(botPos)) << ",";

        out << "1," << "\"" << destination->getTitle() << "\",\"" << message << "\"";

        if (destination->getName() == "NullTravelDestination")
            out << ",none";
        else if (destination->getName() == "QuestTravelDestination")
            out << ",quest";
        else if (destination->getName() == "QuestGiverTravelDestination")
            out << ",questgiver";
        else if (destination->getName() == "QuestTakerTravelDestination")
            out << ",questtaker";
        else if (destination->getName() == "QuestObjectiveTravelDestination")
            out << ",objective";
        else  if (destination->getName() == "RpgTravelDestination")
        {
            RpgTravelDestination* RpgDestination = (RpgTravelDestination*)destination;
            if (RpgDestination->getEntry() > 0)
            {
                CreatureInfo const* cInfo = RpgDestination->getCreatureInfo();

                if (cInfo)
                {
                    if ((cInfo->NpcFlags & UNIT_NPC_FLAG_VENDOR) && AI_VALUE2(bool, "group or", "should sell,can sell"))
                        out << ",sell";
                    else if ((cInfo->NpcFlags & UNIT_NPC_FLAG_REPAIR) && AI_VALUE2(bool, "group or", "should repair,can repair"))
                        out << ",repair";
                    else if ((cInfo->NpcFlags & UNIT_NPC_FLAG_AUCTIONEER) && AI_VALUE2(bool, "group or", "should sell,can ah sell"))
                        out << ",ah";
                    else
                        out << ",rpg";
                }
                else
                    out << ",rpg";
            }
            else
            {
                GameObjectInfo const* gInfo = RpgDestination->getGoInfo();

                if (gInfo)
                {
                    if (gInfo->type == GAMEOBJECT_TYPE_MAILBOX && AI_VALUE(bool, "can get mail"))
                        out << ",mail";
                    else
                        out << ",rpg";
                }
                else
                    out << ",rpg";
            }
        }
        else if (destination->getName() == "ExploreTravelDestination")
            out << ",explore";
        else if (destination->getName() == "GrindTravelDestination")
            out << ",grind";
        else if (destination->getName() == "BossTravelDestination")
            out << ",boss";
        else
            out << ",unknown";

        sPlayerbotAIConfig.log("travel_map.csv", out.str().c_str());

        WorldPosition lastPos = AI_VALUE2(WorldPosition, "custom position", "last choose travel");

        if (lastPos)
        {
            std::ostringstream out;
            out << sPlayerbotAIConfig.GetTimestampStr() << "+00,";
            out << bot->GetName() << ",";
            out << std::fixed << std::setprecision(2);

            out << std::to_string(bot->getRace()) << ",";
            out << std::to_string(bot->getClass()) << ",";
            float subLevel = ai->GetLevelFloat();

            out << subLevel << ",";

            WorldPosition lastPos = AI_VALUE2(WorldPosition, "custom position", "last choose travel");

            botPos.printWKT({ lastPos, botPos }, out, 1);

            if (destination->getName() == "NullTravelDestination")
                out << "0,";
            else
                out << round(newTarget->getDestination()->distanceTo(botPos)) << ",";

            out << "0," << "\"" << destination->getTitle() << "\",\""<< message << "\"";

            sPlayerbotAIConfig.log("travel_map.csv", out.str().c_str());
        }

        SET_AI_VALUE2(WorldPosition, "custom position", "last choose travel", botPos);
    }
}

//Select only those points that are in sight distance or failing that a multiplication of the sight distance.
//std::vector<WorldPosition*> ChooseTravelTargetAction::getLogicalPoints(Player* requester, std::vector<WorldPosition*>& travelPoints, float pMaxSearchDistance)
//{
//    PerformanceMonitorOperation* pmo = sPerformanceMonitor.start(PERF_MON_VALUE, "getLogicalPoints", &context->performanceStack);
//    std::vector<WorldPosition*> retvec;
//
//    float initialSearchDistance = 60.0f;
//
//    //allow lesser values
//    if (pMaxSearchDistance < initialSearchDistance && pMaxSearchDistance > 0)
//    {
//        initialSearchDistance = pMaxSearchDistance;
//    }
//
//    float absoluteMaxSearchDistance = 600000.0f;
//
//    float maxSearchDisatance = pMaxSearchDistance > 0 ? pMaxSearchDistance : absoluteMaxSearchDistance;
//
//    const std::vector<float> possibleDistanceLimits = {
//        100.0f,
//        150.0f,
//        200.0f,
//        300.0f,
//        600.0f,
//        1200.0f,
//        3000.0f,
//        6000.0f,
//        absoluteMaxSearchDistance
//    };
//
//    std::vector<float> distanceLimits = {
//        initialSearchDistance
//    };
//
//    //insert distance limits
//    for (auto possibleDistanceLimit : possibleDistanceLimits)
//    {
//        if (possibleDistanceLimit <= maxSearchDisatance) {
//            distanceLimits.push_back(possibleDistanceLimit);
//        }
//    }
//
//    //insert max search distance
//    if (distanceLimits[distanceLimits.size() - 1] < maxSearchDisatance)
//    {
//        distanceLimits.push_back(maxSearchDisatance);
//    }
//
//    std::vector<std::vector<WorldPosition*>> partitions;
//
//    for (uint8 l = 0; l < distanceLimits.size(); l++)
//        partitions.push_back({});
//
//    WorldPosition centerLocation;
//
//    int32 botLevel = (int)bot->GetLevel();
//
//    bool canFightElite = AI_VALUE(bool, "can fight elite");
//
//    if (AI_VALUE(bool, "can fight boss"))
//        botLevel += 5;
//    else if (canFightElite)
//        botLevel += 2;
//    else if (!AI_VALUE(bool, "can fight equal"))
//        botLevel -= 2;
//
//    if (botLevel < 6)
//        botLevel = 6;
//
//    if (requester)
//        centerLocation = WorldPosition(requester);
//    else
//        centerLocation = WorldPosition(bot);
//
//    auto lastPoint = travelPoints[travelPoints.size() - 1];
//
//    PerformanceMonitorOperation* pmo1 = sPerformanceMonitor.start(PERF_MON_VALUE, "Shuffle", &context->performanceStack);
//    if (travelPoints.size() > 50)
//        std::shuffle(travelPoints.begin(), travelPoints.end(), *GetRandomGenerator());
//    if (pmo1) pmo1->finish();
//
//    uint8 checked = 0;
//
//    //Loop over all points
//    for (uint8 i = 0; i < travelPoints.size(); ++i)
//    {
//        auto pos = travelPoints[i];
//
//        PerformanceMonitorOperation* pmo1 = sPerformanceMonitor.start(PERF_MON_VALUE, "AreaLevel", &context->performanceStack);
//
//        if (pos->getMapId() == bot->GetMapId())
//        {
//            int32 areaLevel = pos->getAreaLevel();
//            if (pmo1) pmo1->finish();
//
//            if (!pos->isOverworld() && !canFightElite)
//                areaLevel += 10;
//
//            if (!areaLevel || botLevel < areaLevel) //Skip points that are in a area that is too high level.
//            {
//                continue;
//            }
//        }
//
//        GuidPosition* guidP = dynamic_cast<GuidPosition*>(pos);
//
//        PerformanceMonitorOperation* pmo2 = sPerformanceMonitor.start(PERF_MON_VALUE, "IsEventUnspawned", &context->performanceStack);
//        if (guidP && guidP->IsEventUnspawned()) //Skip points that are not spawned due to events.
//        {
//            if(pmo2) pmo2->finish();
//            continue;
//        }
//        if (pmo2) pmo2->finish();
//
//        PerformanceMonitorOperation* pmo3 = sPerformanceMonitor.start(PERF_MON_VALUE, "distancePartition", &context->performanceStack);
//        centerLocation.distancePartition(distanceLimits, pos, partitions); //Partition point in correct distance bracket.
//        if (pmo3) pmo3->finish();
//
//        if (checked++ > 50)
//            break;
//    }
//
//    if(pmo) pmo->finish();
//
//    for (uint8 l = 0; l < distanceLimits.size(); l++)
//    {
//        if (partitions[l].empty() || !urand(0, 10)) //Return the first non-empty bracket with 10% chance to skip a higher bracket.
//            continue;
//
//        return partitions[l];
//    }
//
//    if (requester && centerLocation.fDist(bot) > 500.0f) //Try again with bot as center.
//        return getLogicalPoints(nullptr, travelPoints, maxSearchDisatance);
//
//    return partitions.back();
//}

//std::vector<WorldPosition*> ChooseTravelTargetAction::getLogicalPoints(Player* requester, std::vector<WorldPosition*>& travelPoints, float pMaxSearchDistance)
//{
//    auto
//}

//bool ChooseTravelTargetAction::ApplyBestQuestTargetFromDestinations(Player* requester, TravelTarget* target, std::vector<TravelDestination*>& TravelDestinations)
//{
//    std::ostringstream out;
//    out << "---------bot: " << bot->GetName() << " Attempting to ApplyBestTargetFromDestinations";
//    sLog.outBasic(out.str().c_str());
//
//    if (TravelDestinations.empty())
//        return false;
//
//    WorldPosition botLocation(bot);
//
//    if (TravelDestinations.size() == 1)
//    {
//        auto destination = TravelDestinations.front();
//
//        if (destination->getPoints().size() == 1)
//        {
//            auto point = destination->getPoints().front();
//
//            //ignore complicated points for now
//            if (point->isDungeon()
//                || point->getMapEntry()->IsRaid()
//                || (point->getMapEntry()->IsBattleGround() && !bot->InBattleGround()))
//            {
//                return false;
//            }
//
//            target->setTarget(destination, point);
//            return target->isActive();
//        }
//    }
//
//    std::vector<std::tuple<TravelDestination*, WorldPosition*, uint32>> destinationsPointsWeightsFlat;
//
//    //std::shuffle(travelPoints.begin(), travelPoints.end(), *GetRandomGenerator());
//
//    for (auto destination : TravelDestinations)
//    {
//        for (auto point : destination->getPoints())
//        {
//
//            //bool canFightElite = AI_VALUE(bool, "can fight elite");
//            //AI_VALUE(bool, "can fight boss");
//            //AI_VALUE(bool, "can fight equal");
//
//            uint32 weight = 0;
//
//            if (point->getMapId() != bot->GetMapId())
//            {
//                weight += 1;
//            }
//
//            if (!point->isOverworld())
//            {
//                weight += 5;
//            }
//
//            if (point->getAreaLevel() > (int32)bot->GetLevel())
//            {
//                weight += 10;
//            }
//
//            if (point->isDungeon())
//            {
//                weight += 1000;
//            }
//
//            //ignore complicated points for now
//            if (point->isDungeon()
//                || point->getMapEntry()->IsRaid()
//                || (point->getMapEntry()->IsBattleGround() && !bot->InBattleGround()))
//            {
//                continue;
//            }
//
//            destinationsPointsWeightsFlat.push_back(std::tuple(destination, point, weight));
//        }
//    }
//
//    //shuffle points
//    std::shuffle(destinationsPointsWeightsFlat.begin(), destinationsPointsWeightsFlat.end(), *GetRandomGenerator());
//
//    //sort by least weight
//    std::sort(
//        destinationsPointsWeightsFlat.begin(),
//        destinationsPointsWeightsFlat.end(),
//        [](std::tuple<TravelDestination*, WorldPosition*, uint32> a, std::tuple<TravelDestination*, WorldPosition*, uint32> b) {
//            return std::get<2>(a) < std::get<2>(b);
//        }
//    );
//
//    if (destinationsPointsWeightsFlat.size() > 0)
//    {
//        //TODO if point requires group - assemble group
//        //
//        //if point requires raid - assemble raid?
//
//
//        target->setTarget(std::get<0>(destinationsPointsWeightsFlat.front()), std::get<1>(destinationsPointsWeightsFlat.front()));
//        ai->TellDebug(requester, "Point at " + std::to_string(uint32(target->distance(bot))) + "y selected.", "debug travel");
//        return target->isActive();
//    }
//
//    return false;
//}

//Sets the target to the best destination.
bool ChooseTravelTargetAction::ApplyTravelTargetFromDestinations(
    Player* requester,
    TravelTarget* target,
    std::vector<std::tuple<TravelDestination*, WorldPosition*, int>>& destinationsPointsWeightsFlat
)
{
    std::ostringstream out;
    out << "---------bot: " << bot->GetName() << " Attempting to ApplyTravelTargetFromDestinations";
    sLog.outBasic(out.str().c_str());

    if (destinationsPointsWeightsFlat.empty())
        return false;

    /*for (auto dpw : destinationsPointsWeightsFlat)
    {
        if (std::get<1>(dpw)->getX() == 0
            && std::get<1>(dpw)->getY() == 0
            && std::get<1>(dpw)->getZ() == 0)
        {

        }
    }*/

    //filter out 0,0,0?
    destinationsPointsWeightsFlat.erase(std::remove_if(
        destinationsPointsWeightsFlat.begin(), destinationsPointsWeightsFlat.end(),
        [](std::tuple<TravelDestination*, WorldPosition*, int> element) {
            auto point = std::get<1>(element);
            if (point->getX() == 0
                && point->getY() == 0
                && point->getZ() == 0)
            {
                return true;
            }
            return false;
        }), destinationsPointsWeightsFlat.end());

    //shuffle points
    std::shuffle(destinationsPointsWeightsFlat.begin(), destinationsPointsWeightsFlat.end(), *GetRandomGenerator());

    //sort by least weight
    std::sort(
        destinationsPointsWeightsFlat.begin(),
        destinationsPointsWeightsFlat.end(),
        [](std::tuple<TravelDestination*, WorldPosition*, int> a, std::tuple<TravelDestination*, WorldPosition*, int> b) {
            return std::get<2>(a) < std::get<2>(b);
        }
    );

    if (boost::algorithm::contains(std::get<0>(destinationsPointsWeightsFlat.front())->getTitle(), "allina"))
    {
        out << "\n+++++++++++++++bot: " << bot->GetName() << " ApplyTravelTargetFromDestinations:";
        out << "\n+++++++++++++++bot: " << bot->GetName() << " front:" << std::get<0>(destinationsPointsWeightsFlat.front())->getTitle() << " weight: " << std::get<2>(destinationsPointsWeightsFlat.front());
        for (uint32 i = 0; i < destinationsPointsWeightsFlat.size(); ++i)
        {
            auto iDest = destinationsPointsWeightsFlat[i];
            out << "\n+++++++++++++++bot: " << bot->GetName() << " i dest " << i << ": " << std::get<0>(iDest)->getTitle() << " weight: " << std::get<2>(iDest) << " dist: " << std::get<0>(iDest)->distanceTo(bot)
                << " questid: " << std::get<0>(iDest)->GetQuestTemplate()->GetQuestId()
                << " botx: " << ai->GetCurrentWorldPositionRef().getX()
                << " boty: " << ai->GetCurrentWorldPositionRef().getY()
                << " botz: " << ai->GetCurrentWorldPositionRef().getZ()
                ;

            if (i >= 15)
                break;
        }
        sLog.outBasic(out.str().c_str());
    }

    if (bot->getRace() == RACE_HUMAN)
    {
        out << "\n---------bot: " << bot->GetName() << " ApplyTravelTargetFromDestinations:";
        out << "\n---------bot: " << bot->GetName() << " front:" << std::get<0>(destinationsPointsWeightsFlat.front())->getTitle() << " weight: " << std::get<2>(destinationsPointsWeightsFlat.front());
        for (uint32 i = 0; i < destinationsPointsWeightsFlat.size(); ++i)
        {
            auto iDest = destinationsPointsWeightsFlat[i];
            out << "\n---------bot: " << bot->GetName() << " i dest " << i << ": " << std::get<0>(iDest)->getTitle() << " weight: " << std::get<2>(iDest);

            if (i >= 5)
                break;
        }
        sLog.outBasic(out.str().c_str());
    }


    //TODO if point requires group - assemble group
    //
    //if point requires raid - assemble raid?

    target->setTarget(std::get<0>(destinationsPointsWeightsFlat.front()), std::get<1>(destinationsPointsWeightsFlat.front()));
    ai->TellDebug(requester, "Point at " + std::to_string(uint32(target->distance(bot))) + "y selected.", "debug travel");

    return target->isActive();
}

bool ChooseTravelTargetAction::SetGroupTarget(Player* requester, TravelTarget* target)
{
    std::vector<TravelDestination*> activeDestinations;
    std::vector<WorldPosition*> activePoints;

    std::list<ObjectGuid> groupPlayers;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        if (ref->getSource() != bot)
        {
            if (ref->getSubGroup() != bot->GetSubGroup())
            {
                groupPlayers.push_back(ref->getSource()->GetObjectGuid());
            }
            else
            {
                groupPlayers.push_front(ref->getSource()->GetObjectGuid());
            }
        }
    }

    //Find targets of the group.
    for (auto& member : groupPlayers)
    {
        Player* player = sObjectMgr.GetPlayer(member);

        if (!player)
            continue;

        if (!player->GetPlayerbotAI())
            continue;

        if (!player->GetPlayerbotAI()->GetAiObjectContext())
            continue;

        TravelTarget* groupTarget = player->GetPlayerbotAI()->GetTravelTarget();

        if (groupTarget->isGroupCopy())
            continue;

        if (!groupTarget->isActive())
            continue;

        if (!groupTarget->getDestination()->isActive(bot) || groupTarget->getDestination()->getName() == "RpgTravelDestination")
            continue;

        activeDestinations.push_back(groupTarget->getDestination());
        activePoints.push_back(groupTarget->getPosition());
    }

    if (ai->HasStrategy("debug travel", BotState::BOT_STATE_NON_COMBAT))
        ai->TellPlayerNoFacing(requester, std::to_string(activeDestinations.size()) + " group targets found.");

    std::vector<std::tuple<TravelDestination*, WorldPosition*, int>> destinationsPointsWeightsFlat;
    for (auto destination : activeDestinations)
    {
        for (auto point : destination->getPoints())
        {
            destinationsPointsWeightsFlat.push_back(std::tuple(destination, point, 0));
        }
    }

    bool hasTarget = ApplyTravelTargetFromDestinations(requester, target, destinationsPointsWeightsFlat);

    if (hasTarget)
        target->setGroupCopy();

    return hasTarget;
}

bool ChooseTravelTargetAction::SetCurrentTarget(Player* requester, TravelTarget* target, TravelTarget* oldTarget)
{
    TravelDestination* oldDestination = oldTarget->getDestination();

    if (oldTarget->isMaxRetry(false))
        return false;

    if (!oldDestination) //Does this target have a destination?
        return false;

    if (!oldDestination->isActive(bot)) //Is the destination still valid?
        return false;

    //std::vector<TravelDestination*> TravelDestinations = { oldDestination };

    std::vector<std::tuple<TravelDestination*, WorldPosition*, int>> destinationsPointsWeightsFlat;

    for (auto point : oldDestination->getPoints())
    {
        destinationsPointsWeightsFlat.push_back(std::tuple(oldDestination, point, 0));
    }

    if (!ApplyTravelTargetFromDestinations(requester, target, destinationsPointsWeightsFlat))
        return false;

    target->setStatus(TravelStatus::TRAVEL_STATUS_TRAVEL);
    target->setRetry(false, oldTarget->getRetryCount(false) + 1);

    return target->isActive();
}

bool ChooseTravelTargetAction::SetQuestTarget(Player* requester, TravelTarget* target, bool ignoreNewLowLevelQuests, bool classQuestsOnly)
{
    uint8 maxCurrentQuestsForTaking = MAX_QUEST_LOG_SIZE - 2;
    auto amountOfTakenQuests = ai->GetAllCurrentQuestIds().size();
    auto amountOfCompleteQuests = ai->GetCurrentCompleteQuestIds().size();
    bool isQuestLogAlmostFull = amountOfTakenQuests >= maxCurrentQuestsForTaking;
    bool isQuestLogEmpty = amountOfTakenQuests <= 0;

    //turn in any completed class quests anywhere
    //do current class quest objectives anywhere
    //find any available class quests anywhere (if quest log is not full)

    //search nearby only at first
    /*uint32 questSearchDistance = 400 + (bot->GetLevel() * 10);

    std::vector<uint32> questSearchDistances = {
        400 + (bot->GetLevel() * 10),
        400 + (bot->GetLevel() * 10) * 2,
        400 + (bot->GetLevel() * 10) * 4,
        400 + (bot->GetLevel() * 10) * 8,
        0,
    };*/

    //turn in any completed class quests anywhere
    std::vector<QuestTravelDestination*> travelDestinations;

    travelDestinations = sTravelMgr.GetAllAvailableQuestTravelDestinations(
        bot,
        classQuestsOnly,
        false,
        0
    );

    std::vector<std::tuple<TravelDestination*, WorldPosition*, int>> destinationsPointsWeightsFlat;

    auto spawnPosition = ai->GetSpawnPositionRef();
    auto spawnPositionAreaId = sTerrainMgr.GetAreaId(spawnPosition.getMapId(), spawnPosition.getX(), spawnPosition.getY(), spawnPosition.getZ());
    auto spawnPositionZoneId = sTerrainMgr.GetZoneId(spawnPosition.getMapId(), spawnPosition.getX(), spawnPosition.getY(), spawnPosition.getZ());

    auto botPosition = ai->GetCurrentWorldPositionRef();
    auto botLevel = bot->GetLevel();
    auto completedQuestsCount = ai->GetCurrentCompleteQuestIds().size();

    bool isBotPositionValid = bot->IsInWorld() && !bot->IsBeingTeleported() && botPosition;

    bool shouldSkipQuestGivers = false;
    bool shouldSkipQuestObjectives = false;

    if (isQuestLogAlmostFull
        //the more quests the bot has in the log, the less chances to go for new ones
        || urand(amountOfTakenQuests, MAX_QUEST_LOG_SIZE) >= MAX_QUEST_LOG_SIZE - 3)
    {
        shouldSkipQuestGivers = true;
    }

    if (isQuestLogEmpty)
    {
        shouldSkipQuestObjectives = true;
    }

    //chance to ignore quest givers and objectives in favor of completing quests
    if (amountOfCompleteQuests > 0 && !urand(0, 3))
    {
        shouldSkipQuestGivers = true;
        shouldSkipQuestObjectives = true;
    }

    //if no questgivers within 500 dist, then focus on objectives and turn ins?

    for (auto destination : travelDestinations)
    {
        if (!isBotPositionValid)
            break;

        auto destinationQuestId = destination->GetQuestTemplate()->GetQuestId();

        if (!destinationQuestId)
            continue;

        auto destinationQuest = sObjectMgr.GetQuestTemplate(destinationQuestId);

        auto questLevel = destinationQuest->GetQuestLevel();

        bool isClassQuest = sTravelMgr.m_allClassQuestQuestIds.count(destinationQuestId) > 0;

        bool isQuestGiver = destination->getName() == "QuestGiverTravelDestination";
        bool isQuestTaker = destination->getName() == "QuestTakerTravelDestination";
        bool isQuestObjective = destination->getName() == "QuestObjectiveTravelDestination";

        for (auto point : destination->getPoints())
        {
            auto pointAreaId = sTerrainMgr.GetAreaId(point->mapid, point->getX(), point->getY(), point->getZ());
            auto pointZoneId = sTerrainMgr.GetZoneId(point->mapid, point->getX(), point->getY(), point->getZ());

            //bool canFightElite = AI_VALUE(bool, "can fight elite");
            //AI_VALUE(bool, "can fight boss");
            //AI_VALUE(bool, "can fight equal");

            //ignore complicated points for now
            if (point->isDungeon()
                || point->getMapEntry()->IsRaid()
                || (point->getMapEntry()->IsBattleGround() && !bot->InBattleGround()))
            {
                continue;
            }

            int weight = 10000000;

            auto distanceToPoint = (int)(point->distance(botPosition));

            /*if (distanceToPoint <= 0)
                continue;*/

            //ignore distance weights within 300
            weight += distanceToPoint <= 300 ? 0 : distanceToPoint;

            if (point->getMapId() != bot->GetMapId())
            {
                weight += 50000;

                //how to even???
                //continue;
            }

            //prioritize class quests above all
            if (botLevel >= 10 && isClassQuest)
            {
                weight -= 1000000;
            }

            //clear out spawn area first
            if (botLevel <= 5 && pointAreaId == spawnPositionAreaId)
            {
                weight -= 100000;
            }

            //prefer doing and giving quests in spawn zone
            if ((isQuestObjective || isQuestTaker) && botLevel <= 10 && pointZoneId == spawnPositionZoneId)
            {
                weight -= 100000;
            }

            /*if (point->getAreaLevel() > (int32)botLevel)
            {
                weight += 1000 * (point->getAreaLevel() - (int32)botLevel);
            }*/

            if (isQuestTaker)
            {
                if (isQuestLogEmpty)
                {
                    continue;
                }

                if (amountOfCompleteQuests >= 5)
                {
                    weight -= 100000;
                }

                if (distanceToPoint <= 300)
                {
                    weight -= 110000;
                }
            }

            if (isQuestObjective)
            {
                if (isQuestLogEmpty)
                {
                    continue;
                }

                if (shouldSkipQuestObjectives)
                {
                    continue;
                }

                //if current quest is higher level
                if (questLevel > botLevel + 2)
                {
                    weight += (questLevel - botLevel) * 10;
                }
            }

            //take whatever quests within 300 radius
            if (isQuestGiver)
            {
                //skip far away quest givers if has quests
                if (amountOfTakenQuests > 0 && distanceToPoint > 150)
                    continue;

                //special behavior
                if (botLevel < 5
                    //falkhaan human
                    && destination->getEntry() == 6774
                    )
                {
                    continue;
                }

                if (shouldSkipQuestGivers)
                {
                    continue;
                }

                //repeatable quests should be super low priority
                if ((destinationQuest->IsRepeatable() && !destinationQuest->IsWeekly()) && botLevel < sPlayerbotAIConfig.randomBotMaxLevel)
                {
                    weight += 1000000;
                }

                if (distanceToPoint <= 300)
                {
                    weight -= 100000;
                }

                //low priority on low level quests
                if (botLevel > 10 && !isClassQuest && !bot->CanSeeStartQuest(destinationQuest))
                {
                    weight += 1000;
                }

            }

            /*if (point->getMapId() != bot->GetMapId())
            {
                weight += 1;
            }

            if (!point->isOverworld())
            {
                weight += 5;
            }*/

            destinationsPointsWeightsFlat.push_back(std::tuple(destination, point, weight));
        }
    }

    return ApplyTravelTargetFromDestinations(requester, target, destinationsPointsWeightsFlat);
}

//(RpgTravelDestination*)
bool ChooseTravelTargetAction::SetRpgTarget(Player* requester, TravelTarget* target, bool requireLongDistanceSearch)
{
    //Find rpg npcs
    std::vector<TravelDestination*> travelDestinations;

    uint32 distance = 5000;
    if (!requireLongDistanceSearch)
    {
        distance = ai->IsInCapitalCity() ? 1000.0f : 75.0f;
    }

    for (auto rpgDest : sTravelMgr.getRpgTravelDestinations(bot, true, false, distance))
    {
        auto rpgDestCreatureInfo = rpgDest->getCreatureInfo();

        if (!rpgDestCreatureInfo)
            continue;

        auto rpgDestCreatureReaction = ai->getReaction(sFactionTemplateStore.LookupEntry(rpgDestCreatureInfo->Faction));

        if (rpgDestCreatureReaction < ReputationRank::REP_NEUTRAL)
        {
            continue;
        }

        travelDestinations.push_back(rpgDest);
    }

    if (ai->HasStrategy("debug travel", BotState::BOT_STATE_NON_COMBAT))
        ai->TellPlayerNoFacing(requester, std::to_string(travelDestinations.size()) + " rpg destinations found.");

    //return ApplyBestTargetFromDestinations(requester, target, travelDestinations);

    return false;
}

bool ChooseTravelTargetAction::SetGOTypeTarget(Player* requester, TravelTarget* target, GameobjectTypes type, std::string name, bool force)
{
    WorldPosition pos = WorldPosition(bot);
    WorldPosition* botPos = &pos;

    std::vector<TravelDestination*> TravelDestinations;

    //Loop over all npcs.
    for (auto& d : sTravelMgr.getRpgTravelDestinations(bot, true, true, 1000))
    {
        if (d->getEntry() >= 0)
            continue;

        GameObjectInfo const* gInfo = ObjectMgr::GetGameObjectInfo(-1 * d->getEntry());

        if (!gInfo)
            continue;

        //Check if the object has any of the required type.
        if (gInfo->type != type)
            continue;

        //Check if the npc has (part of) the required name.
        //if (!name.empty() && !strstri(gInfo->name, name.c_str()))
        //    continue;

        if (!name.empty() && !boost::algorithm::contains(gInfo->name, name.c_str()))
            continue;


        TravelDestinations.push_back(d);
    }

    if (ai->HasStrategy("debug travel", BotState::BOT_STATE_NON_COMBAT))
        ai->TellPlayerNoFacing(requester, std::to_string(TravelDestinations.size()) + " go type targets found.");

    // ai->IsInCapitalCity() ? 1000.0f : 150.0f
    //bool isActive = ApplyBestTargetFromDestinations(requester, target, TravelDestinations);

    bool isActive = false;

    if (!target->getDestination())
        return false;

    if (force)
    {
        target->setForced(true);
        return true;
    }

    return isActive;
}

bool ChooseTravelTargetAction::SetGrindTarget(Player* requester, TravelTarget* target)
{
    //Find grind mobs.
    std::vector<TravelDestination*> TravelDestinations = sTravelMgr.getGrindTravelDestinations(bot, true, false, 600+bot->GetLevel()*400);

    if (ai->HasStrategy("debug travel", BotState::BOT_STATE_NON_COMBAT))
        ai->TellPlayerNoFacing(requester, std::to_string(TravelDestinations.size()) + " grind destinations found.");

    //return ApplyBestTargetFromDestinations(requester, target, TravelDestinations);

    return false;
}

bool ChooseTravelTargetAction::SetBossTarget(Player* requester, TravelTarget* target)
{
    //Find boss mobs.
    std::vector<TravelDestination*> TravelDestinations = sTravelMgr.getBossTravelDestinations(bot, true);

    if (ai->HasStrategy("debug travel", BotState::BOT_STATE_NON_COMBAT))
        ai->TellPlayerNoFacing(requester, std::to_string(TravelDestinations.size()) + " boss destinations found.");

    //return ApplyBestTargetFromDestinations(requester, target, TravelDestinations);

    return false;
}

bool ChooseTravelTargetAction::SetExploreTarget(Player* requester, TravelTarget* target)
{
    //Find exploration locations (middle of a sub-zone).
    std::vector<TravelDestination*> TravelDestinations = sTravelMgr.getExploreTravelDestinations(bot, true, false);

    if (ai->HasStrategy("debug travel", BotState::BOT_STATE_NON_COMBAT))
        ai->TellPlayerNoFacing(requester, std::to_string(TravelDestinations.size()) + " explore destinations found.");

    //bot->GetLevel() >= sPlayerbotAIConfig.randomBotMaxLevel ? 0 : 6000.0f)

    //return ApplyBestTargetFromDestinations(requester, target, TravelDestinations);

    return false;
}

char* strstri(const char* haystack, const char* needle);

bool ChooseTravelTargetAction::SetNpcFlagTarget(Player* requester, TravelTarget* target, std::vector<NPCFlags> flags, std::string name, std::vector<uint32> items, bool force)
{
    WorldPosition pos = WorldPosition(bot);
    WorldPosition* botPos = &pos;

    std::vector<TravelDestination*> TravelDestinations;

    //Loop over all npcs.
    for (auto& d : sTravelMgr.getRpgTravelDestinations(bot, true, true))
    {
        if (d->getEntry() <= 0)
            continue;

        CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(d->getEntry());

        if (!cInfo)
            continue;

        //Check if the npc has any of the required flags.
        bool foundFlag = false;
        for(auto flag : flags)
            if (cInfo->NpcFlags & flag)
            {
                foundFlag = true;
                break;
            }

        if (!foundFlag)
            continue;

        //Check if the npc has (part of) the required name.
        if (!name.empty() && !strstri(cInfo->Name, name.c_str()) && !strstri(cInfo->SubName, name.c_str()))
            continue;

        //Check if the npc sells any of the wanted items.
        if (!items.empty())
        {
            bool foundItem = false;
            VendorItemData const* vItems = nullptr;
            VendorItemData const* tItems = nullptr;

            vItems = sObjectMgr.GetNpcVendorItemList(d->getEntry());

//#ifndef MANGOSBOT_ZERO
            uint32 vendorId = cInfo->VendorTemplateId;
            if (vendorId)
                tItems = sObjectMgr.GetNpcVendorTemplateItemList(vendorId);
//#endif

            for (auto item : items)
            {
                if (vItems && !vItems->Empty())
                for(auto vitem : vItems->m_items)
                   if (vitem->item == item)
                    {
                        foundItem = true;
                        break;
                    }
                if(tItems && !tItems->Empty())
                for (auto titem : tItems->m_items)
                    if (titem->item == item)
                    {
                        foundItem = true;
                        break;
                    }
            }

            if (!foundItem)
                continue;
        }

        //Check if the npc is friendly.
        FactionTemplateEntry const* factionEntry = sFactionTemplateStore.LookupEntry(cInfo->Faction);
        ReputationRank reaction = ai->getReaction(factionEntry);

        if (reaction  < REP_NEUTRAL)
            continue;

        TravelDestinations.push_back(d);
    }

    if (ai->HasStrategy("debug travel", BotState::BOT_STATE_NON_COMBAT))
        ai->TellPlayerNoFacing(requester, std::to_string(TravelDestinations.size()) + " npc flag targets found.");

    //bool isActive = ApplyBestTargetFromDestinations(requester, target, TravelDestinations);

    bool isActive = false;

    if (!target->getDestination())
        return false;

    if (force)
    {
        target->setForced(true);
        return true;
    }

    return isActive;
}

bool ChooseTravelTargetAction::SetNullTarget(TravelTarget* target)
{
    ai->SetTravelTarget(
        sTravelMgr.nullTravelDestination,
        sTravelMgr.nullWorldPosition,
        true,
        false,
        TravelStatus::TRAVEL_STATUS_COOLDOWN,
        60000
    );

    return true;
}

std::vector<std::string> split(const std::string& s, char delim);
char* strstri(const char* haystack, const char* needle);

//Find a destination based on (part of) it's name. Includes zones, ncps and mobs. Picks the closest one that matches.
TravelDestination* ChooseTravelTargetAction::FindDestination(Player* bot, std::string name, bool zones, bool npcs, bool quests, bool mobs, bool bosses)
{
    PlayerbotAI* ai = bot->GetPlayerbotAI();

    AiObjectContext* context = ai->GetAiObjectContext();

    std::vector<TravelDestination*> dests;

    //Quests
    if (quests)
    {
        for (auto& d : sTravelMgr.getQuestTravelDestinations(bot, 0, true, true))
        {
            if (strstri(d->getTitle().c_str(), name.c_str()))
                dests.push_back(d);
        }
    }

    //Zones
    if (zones)
    {
        for (auto& d : sTravelMgr.getExploreTravelDestinations(bot, true, true))
        {
            if (strstri(d->getTitle().c_str(), name.c_str()))
                dests.push_back(d);
        }
    }

    //Npcs
    if (npcs)
    {
        for (auto& d : sTravelMgr.getRpgTravelDestinations(bot, true, true))
        {
            if (strstri(d->getTitle().c_str(), name.c_str()))
                dests.push_back(d);
        }
    }

    //Mobs
    if (mobs)
    {
        for (auto& d : sTravelMgr.getGrindTravelDestinations(bot, true, true, 5000.0f, 0))
        {
            if (strstri(d->getTitle().c_str(), name.c_str()))
                dests.push_back(d);
        }
    }

    //Bosses
    if (bosses)
    {
        for (auto& d : sTravelMgr.getBossTravelDestinations(bot, true, true))
        {
            if (strstri(d->getTitle().c_str(), name.c_str()))
                dests.push_back(d);
        }
    }

    WorldPosition botPos(bot);

    if (dests.empty())
        return nullptr;

    TravelDestination* dest = *std::min_element(dests.begin(), dests.end(), [botPos](TravelDestination* i, TravelDestination* j) {return i->distanceTo(botPos) < j->distanceTo(botPos); });

    return dest;
};

bool ChooseTravelTargetAction::isUseful()
{
    if (!ai->AllowActivity(TRAVEL_ACTIVITY))
        return false;

    if (bot->GetGroup() && !bot->GetGroup()->IsLeader(bot->GetObjectGuid()))
        if (ai->HasStrategy("follow", BotState::BOT_STATE_NON_COMBAT) || ai->HasStrategy("stay", BotState::BOT_STATE_NON_COMBAT) || ai->HasStrategy("guard", BotState::BOT_STATE_NON_COMBAT))
            return false;

    if (AI_VALUE(bool, "has available loot"))
    {
        LootObject lootObject = AI_VALUE(LootObjectStack*, "available loot")->GetLoot(sPlayerbotAIConfig.lootDistance);
        if (lootObject.IsLootPossible(bot))
            return false;
    }

    return !ai->GetTravelTarget()->isActive();
}


bool ChooseTravelTargetAction::needForQuest(Unit* target)
{
    bool justCheck = (bot->GetObjectGuid() == target->GetObjectGuid());

    QuestStatusMap& questMap = bot->getQuestStatusMap();
    for (auto& quest : questMap)
    {
        const Quest* questTemplate = sObjectMgr.GetQuestTemplate(quest.first);
        if (!questTemplate)
            continue;

        uint32 questId = questTemplate->GetQuestId();

        if (!questId)
            continue;

        QuestStatus status = bot->GetQuestStatus(questId);

        if ((status == QUEST_STATUS_COMPLETE && !bot->GetQuestRewardStatus(questId)))
        {
            if (!justCheck && !target->HasInvolvedQuest(questId))
                continue;

            return true;
        }
        else if (status == QUEST_STATUS_INCOMPLETE)
        {
            QuestStatusData questStatus = quest.second;

            if (questTemplate->GetQuestLevel() > (int)bot->GetLevel())
                continue;

            for (int j = 0; j < QUEST_OBJECTIVES_COUNT; j++)
            {
                int32 entry = questTemplate->ReqCreatureOrGOId[j];

                if (entry && entry > 0)
                {
                    int required = questTemplate->ReqCreatureOrGOCount[j];
                    int available = questStatus.m_creatureOrGOcount[j];

                    if(required && available < required && (target->GetEntry() == entry || justCheck))
                        return true;
                }         

                if (justCheck)
                {
                    int32 itemId = questTemplate->ReqItemId[j];

                    if (itemId && itemId > 0)
                    {
                        int required = questTemplate->ReqItemCount[j];
                        int available = questStatus.m_itemcount[j];

                        if (required && available < required)
                            return true;
                    }
                }
            }

            if (!justCheck)
            {
                CreatureInfo const* data = sObjectMgr.GetCreatureTemplate(target->GetEntry());

                if (data)
                {
                    uint32 lootId = data->LootId;

                    if (lootId)
                    {
                        if (LootTemplates_Creature.HaveQuestLootForPlayer(lootId, bot))
                            return true;
                    }
                }
            }
        }

    }
    return false;
}

bool ChooseTravelTargetAction::needItemForQuest(uint32 itemId, const Quest* questTemplate, const QuestStatusData* questStatus)
{
    for (int i = 0; i < QUEST_OBJECTIVES_COUNT; i++)
    {
        if (questTemplate->ReqItemId[i] != itemId)
            continue;

        int required = questTemplate->ReqItemCount[i];
        int available = questStatus->m_itemcount[i];

        if (!required)
            continue;

        return available < required;
    }

    return false;
}
