#pragma once

#include "strategy/AiObject.h"
#include <boost/functional/hash.hpp>
#include "GuidPosition.h"

namespace ai
{   
    class mapTransfer
    {
    public:
        mapTransfer(WorldPosition pointFrom1, WorldPosition pointTo1, float portalLength1 = 0.1f)
            : pointFrom(pointFrom1), pointTo(pointTo1), portalLength(portalLength1) {}

        bool isFrom(WorldPosition point) { return point.getMapId() == pointFrom.getMapId(); }
        bool isTo(WorldPosition point) { return point.getMapId() == pointTo.getMapId(); }

        WorldPosition* getPointFrom() { return &pointFrom; }
        WorldPosition* getPointTo() { return &pointTo; }

        bool isUseful(WorldPosition point) { return isFrom(point) || isTo(point); }
        float distance(WorldPosition point) { return isUseful(point) ? (isFrom(point) ? point.distance(pointFrom) : point.distance(pointTo)) : 200000; }

        bool isUseful(WorldPosition start, WorldPosition end) { return isFrom(start) && isTo(end); }
        float distance(WorldPosition start, WorldPosition end) { return (isUseful(start, end) ? (start.distance(pointFrom) + portalLength + pointTo.distance(end)) : 200000); }
        float fDist(WorldPosition start, WorldPosition end) { return start.fDist(pointFrom) + portalLength + pointTo.fDist(end); }
    private:
        WorldPosition pointFrom, pointTo;
        float portalLength = 0.1f;
    };

    //A destination for a bot to travel to and do something.
    class TravelDestination
    {
    public:
        TravelDestination() {}
        TravelDestination(float radiusMin1, float radiusMax1) { radiusMin = radiusMin1; radiusMax = radiusMax1; }
        TravelDestination(std::vector<WorldPosition*> points1, float radiusMin1, float radiusMax1) { points = points1;  radiusMin = radiusMin1; radiusMax = radiusMax1; }

        void addPoint(WorldPosition* pos) { points.push_back(pos); }
        bool hasPoint(WorldPosition* pos) { return std::find(points.begin(), points.end(), pos) != points.end(); }
        void setExpireDelay(uint32 delay) { expireDelay = delay; }
        void setCooldownDelay(uint32 delay) { cooldownDelay = delay; }

        std::vector<WorldPosition*> getPoints(bool ignoreFull = false);
        uint32 getExpireDelay() { return expireDelay; }
        uint32 getCooldownDelay() { return cooldownDelay; }

        virtual Quest const* GetQuestTemplate() { return nullptr; }

        virtual bool isActive(Player* bot) { return false; }

        virtual std::string getName() { return "TravelDestination"; }
        virtual int32 getEntry() { return 0; }
        virtual std::string getTitle() { return "generic travel destination"; }

        bool hasPointsWithinDistance(WorldPosition* fromPosition, uint32 distance)
        {
            for (auto point : points)
            {
                if (point->distance(*fromPosition) <= distance)
                    return true;
            }
            return false;
        };

        WorldPosition* nearestPoint(WorldPosition* pos) {return nearestPoint(*pos);};
        WorldPosition* nearestPoint(WorldPosition pos);
        float distanceTo(WorldPosition* pos) { return nearestPoint(pos)->distance(*pos); }
        float distanceTo(WorldPosition pos) { return nearestPoint(pos)->distance(pos); }
        bool onMap(WorldPosition pos) {return nearestPoint(pos)->getMapId() == pos.getMapId();}
        virtual bool isIn(WorldPosition pos, float radius = 0) { return onMap(pos) && distanceTo(pos) <= (radius? radius : radiusMin); }
        virtual bool isOut(WorldPosition pos, float radius = 0) { return !onMap(pos) || distanceTo(pos) > (radius? radius : radiusMax); }
        float getRadiusMin() { return radiusMin; }

        std::vector<WorldPosition*> touchingPoints(WorldPosition* pos);
        std::vector<WorldPosition*> sortedPoints(WorldPosition* pos);
        std::vector<WorldPosition*> nextPoint(WorldPosition* pos, bool ignoreFull = true);
    protected:
        std::vector<WorldPosition*> points;
        float radiusMin = 0;
        float radiusMax = 0;

        uint32 expireDelay = 5 * 1000;
        uint32 cooldownDelay = 60 * 1000;
    };

    //A travel target that is always inactive and jumps to cooldown.
    class NullTravelDestination : public TravelDestination
    {
    public:
        NullTravelDestination(uint32 cooldownDelay1 = 5 * 60 * 1000) : TravelDestination() { cooldownDelay = cooldownDelay1;};

        virtual Quest const* GetQuestTemplate() { return nullptr; }

        virtual bool isActive(Player* bot) { return false; }

        virtual std::string getName() { return "NullTravelDestination"; }
        virtual std::string getTitle() { return "no destination"; }

        virtual bool isIn(WorldPosition* pos) { return true; }
        virtual bool isOut(WorldPosition* pos) { return false; }

    protected:
    };


    //A travel target specifically related to a quest.
    class QuestTravelDestination : public TravelDestination
    {
    public:
        QuestTravelDestination(uint32 questId1, float radiusMin1, float radiusMax1) : TravelDestination(radiusMin1, radiusMax1) { questId = questId1; questTemplate = sObjectMgr.GetQuestTemplate(questId); }

        virtual Quest const* GetQuestTemplate() { return questTemplate;  }

        virtual bool isActive(Player* bot) { return bot->IsActiveQuest(questId); }

        virtual std::string getName() { return "QuestTravelDestination"; }
        virtual int32 getEntry() { return 0; }
        virtual std::string getTitle();
    protected:
        uint32 questId;
        Quest const* questTemplate;
    };

    //A quest giver
    class QuestGiverTravelDestination : public QuestTravelDestination
    {
    public:
        QuestGiverTravelDestination(uint32 quest_id1, uint32 entry1, float radiusMin1, float radiusMax1) :QuestTravelDestination(quest_id1, radiusMin1, radiusMax1) { entry = entry1; }

        virtual bool isActive(Player* bot);

        virtual CreatureInfo const* getCreatureInfo() { return entry > 0 ? ObjectMgr::GetCreatureTemplate(entry) : nullptr; }

        virtual std::string getName() { return "QuestGiverTravelDestination"; }
        virtual int32 getEntry() { return entry; }
        virtual std::string getTitle();
    private:
        int32 entry;
    };

    ////A quest taker
    class QuestTakerTravelDestination : public QuestTravelDestination
    {
    public:
        QuestTakerTravelDestination(uint32 quest_id1, uint32 entry1, float radiusMin1, float radiusMax1) :QuestTravelDestination(quest_id1, radiusMin1, radiusMax1) { entry = entry1; }

        virtual bool isActive(Player* bot);

        virtual CreatureInfo const* getCreatureInfo() { return entry > 0 ? ObjectMgr::GetCreatureTemplate(entry) : nullptr; }

        virtual std::string getName() { return "QuestTakerTravelDestination"; }
        virtual int32 getEntry() { return entry; }
        virtual std::string getTitle();
    private:
        int32 entry;
    };

    //A quest objective (creature/gameobject to grind/loot)
    class QuestObjectiveTravelDestination : public QuestTravelDestination
    {
    public:
        QuestObjectiveTravelDestination(uint32 quest_id1, uint32 entry1, uint32 objective1, float radiusMin1, float radiusMax1) : QuestTravelDestination(quest_id1, radiusMin1, radiusMax1) {
            objective = objective1; entry = entry1;
        }

        bool isCreature() { return GetQuestTemplate()->ReqCreatureOrGOId[objective] > 0; }

        int32 ReqCreature() {
            return isCreature() ? GetQuestTemplate()->ReqCreatureOrGOId[objective] : 0;
        }
        int32 ReqGOId() {
            return !isCreature() ? abs(GetQuestTemplate()->ReqCreatureOrGOId[objective]) : 0;
        }
        uint32 ReqCount() { return GetQuestTemplate()->ReqCreatureOrGOCount[objective]; }

        virtual bool isActive(Player* bot);

        virtual std::string getName() { return "QuestObjectiveTravelDestination"; }

        virtual int32 getEntry() { return entry; }

        virtual std::string getTitle();

        virtual uint32 getObjective() { return objective; }
    private:
        uint32 objective;
        int32 entry;
    };

    //A location with rpg target(s) based on race and level
    class RpgTravelDestination : public TravelDestination
    {
    public:
        RpgTravelDestination(uint32 entry1, float radiusMin1, float radiusMax1) : TravelDestination(radiusMin1, radiusMax1) {
            entry = entry1;
        }

        virtual bool isActive(Player* bot);

        virtual CreatureInfo const* getCreatureInfo() { return ObjectMgr::GetCreatureTemplate(entry); }
        virtual GameObjectInfo const* getGoInfo() { return ObjectMgr::GetGameObjectInfo(-1*entry); }
        virtual std::string getName() { return "RpgTravelDestination"; }
        virtual int32 getEntry() { return entry; }
        virtual std::string getTitle();
    protected:
        int32 entry;
    };

    //A location with zone exploration target(s)
    class ExploreTravelDestination : public TravelDestination
    {
    public:
        ExploreTravelDestination(uint32 areaId1, float radiusMin1, float radiusMax1) : TravelDestination(radiusMin1, radiusMax1) {
            areaId = areaId1;
        }

        virtual bool isActive(Player* bot);

        virtual std::string getName() { return "ExploreTravelDestination"; }
        virtual int32 getEntry() { return 0; }
        virtual std::string getTitle() { return title; }
        virtual void setTitle(std::string newTitle) { title = newTitle; }
        virtual uint32 getAreaId() { return areaId; }
    protected:
        uint32 areaId;
        std::string title = "";
    };

    //A location with zone exploration target(s)
    class GrindTravelDestination : public TravelDestination
    {
    public:
        GrindTravelDestination(int32 entry1, float radiusMin1, float radiusMax1) : TravelDestination(radiusMin1, radiusMax1) {
            entry = entry1;
        }

        virtual bool isActive(Player* bot);
        virtual CreatureInfo const* getCreatureInfo() { return ObjectMgr::GetCreatureTemplate(entry); }
        virtual std::string getName() { return "GrindTravelDestination"; }
        virtual int32 getEntry() { return entry; }
        virtual std::string getTitle();
    protected:
        int32 entry;
    };

    //A location with a boss
    class BossTravelDestination : public TravelDestination
    {
    public:
        BossTravelDestination(int32 entry1, float radiusMin1, float radiusMax1) : TravelDestination(radiusMin1, radiusMax1) {
            entry = entry1; cooldownDelay = 1000;
        }

        virtual bool isActive(Player* bot);
        virtual CreatureInfo const* getCreatureInfo() { return ObjectMgr::GetCreatureTemplate(entry); }
        virtual std::string getName() { return "BossTravelDestination"; }
        virtual int32 getEntry() { return entry; }
        virtual std::string getTitle();
    protected:
        int32 entry;
    };

    //A quest destination container for quick lookup of all destinations related to a quest.
    struct QuestContainer
    {
        std::vector<QuestGiverTravelDestination*> questGivers;
        std::vector<QuestTakerTravelDestination*> questTakers;
        std::vector<QuestObjectiveTravelDestination*> questObjectives;
    };

    //
    enum class TravelState : uint8
    {
        TRAVEL_STATE_IDLE = 0,
        TRAVEL_STATE_TRAVEL_PICK_UP_QUEST = 1,
        TRAVEL_STATE_WORK_PICK_UP_QUEST = 2,
        TRAVEL_STATE_TRAVEL_DO_QUEST = 3,
        TRAVEL_STATE_WORK_DO_QUEST = 4,
        TRAVEL_STATE_TRAVEL_HAND_IN_QUEST = 5,
        TRAVEL_STATE_WORK_HAND_IN_QUEST = 6,
        TRAVEL_STATE_TRAVEL_RPG = 7,
        TRAVEL_STATE_TRAVEL_EXPLORE = 8,
        MAX_TRAVEL_STATE
    };

    enum class TravelStatus : uint8
    {
        TRAVEL_STATUS_NONE = 0,
        TRAVEL_STATUS_PREPARE = 1,
        TRAVEL_STATUS_TRAVEL = 2,
        TRAVEL_STATUS_WORK = 3,
        TRAVEL_STATUS_COOLDOWN = 4,
        TRAVEL_STATUS_EXPIRED = 5,
        MAX_TRAVEL_STATUS
    };

    //Current target and location for the bot to travel to.
    //The flow is as follows:
    //PREPARE   (wait until no loot is near)
    //TRAVEL    (move towards target until close enough) (rpg and grind is disabled)
    //WORK      (grind/rpg until the target is no longer active) (rpg and grind is enabled on quest mobs)
    //COOLDOWN  (wait some time free to do what the bot wants)
    //EXPIRE    (if any of the above actions take too long pick a new target)
    class TravelTarget : AiObject
    {
    public:
        TravelTarget(PlayerbotAI* ai) : AiObject(ai) {};
        TravelTarget(PlayerbotAI* ai, TravelDestination* tDestination1, WorldPosition* wPosition1) : AiObject(ai) { setTarget(tDestination1, wPosition1);}
        ~TravelTarget() = default;

        void setTarget(TravelDestination* tDestination1, WorldPosition* wPosition1, bool groupCopy1 = false);
        void setStatus(TravelStatus status);
        void setExpireIn(uint32 expireMs) { statusTime = getExpiredTime() + expireMs; }
        void incRetry(bool isMove) { if (isMove) moveRetryCount+=2; else extendRetryCount+=2; }
        void decRetry(bool isMove) { if (isMove && moveRetryCount > 0) moveRetryCount--; else if (extendRetryCount > 0) extendRetryCount--; }
        void setRetry(bool isMove, uint32 newCount = 0) { if (isMove) moveRetryCount = newCount; else extendRetryCount = newCount; }
        void setGroupCopy(bool isGroupCopy = true) { groupCopy = isGroupCopy; }
        void setForced(bool forced1) { forced = forced1; }
        void setRadius(float radius1) { radius = radius1; }

        void copyTarget(TravelTarget* target);

        float distance(Player* bot) { WorldPosition pos(bot);  return wPosition->distance(pos); };
        WorldPosition* getPosition() { return wPosition; };
        std::string GetPosStr() { return wPosition->to_string(); }
        TravelDestination* getDestination() { return tDestination; };
        int32 getEntry() { if (!tDestination) return 0; return tDestination->getEntry(); }
        PlayerbotAI* getAi() { return ai; }

        uint32 getExpiredTime() { return WorldTimer::getMSTime() - startTime; }
        uint32 getTimeLeft() { return statusTime - getExpiredTime(); }
        uint32 getMaxTravelTime() { return (1000.0 * distance(bot)) / bot->GetSpeed(MOVE_RUN); }
        uint32 getRetryCount(bool isMove) { return isMove ? moveRetryCount: extendRetryCount; }

        bool isNullDestination();

        bool isTraveling();
        bool isActive();
        bool isWorking();
        bool isPreparing();
        bool isMaxRetry(bool isMove) { return isMove ? (moveRetryCount > 10) : (extendRetryCount > 5); }
        TravelStatus getStatus() { return m_status; }

        TravelState getTravelState();

        bool isGroupCopy() { return groupCopy; }
        bool isForced() { return forced; }
    protected:
        TravelStatus m_status = TravelStatus::TRAVEL_STATUS_NONE;

        uint32 startTime = WorldTimer::getMSTime();
        uint32 statusTime = 0;

        bool forced = false;
        float radius = 0;
        bool groupCopy = false;
        bool visitor = true;

        uint32 extendRetryCount = 0;
        uint32 moveRetryCount = 0;

        TravelDestination* tDestination = nullptr;
        WorldPosition* wPosition = nullptr;
    };

    //General container for all travel destinations.
    class TravelMgr
    {
    public:
        TravelMgr() {};
        void Clear();

        void SetMobAvoidArea();
        void SetMobAvoidAreaMap(uint32 mapId);

        void LoadQuestTravelTable();

        template <class D, class W, class URBG>
        void weighted_shuffle
        (D first, D last
            , W first_weight, W last_weight
            , URBG&& g)
        {
            while (first != last && first_weight != last_weight)
            {
                std::discrete_distribution<int> dd(first_weight, last_weight);
                auto i = dd(g);

                if (i)
                {
                    std::swap(*first, *std::next(first, i));
                    std::swap(*first_weight, *std::next(first_weight, i));
                }
                ++first;
                ++first_weight;
            }
        }

        std::vector <WorldPosition*> getNextPoint(WorldPosition* center, std::vector<WorldPosition*> points, uint32 amount = 1);
        std::vector <WorldPosition> getNextPoint(WorldPosition center, std::vector<WorldPosition> points, uint32 amount = 1);
        QuestStatusData* getQuestStatus(Player* bot, uint32 questId);
        bool getObjectiveStatus(Player* bot, Quest const* pQuest, uint32 objective);
        uint32 getDialogStatus(Player* pPlayer, int32 questgiver, Quest const* pQuest);

        std::vector<QuestTravelDestination*> GetAllAvailableQuestTravelDestinations(
            Player* bot,
            bool classQuestsOnly,
            bool ignoreLowLevelQuests,
            uint32 maxDistance
        );

        std::vector<TravelDestination*> getQuestTravelDestinations(Player* bot, int32 questId = -1, bool ignoreFull = false, bool ignoreInactive = false, float maxDistance = 5000, bool ignoreObjectives = false);
        std::vector<RpgTravelDestination*> getRpgTravelDestinations(Player* bot, bool ignoreFull = false, bool ignoreInactive = false, float maxDistance = 5000);
        std::vector<TravelDestination*> getExploreTravelDestinations(Player* bot, bool ignoreFull = false, bool ignoreInactive = false);
        std::vector<TravelDestination*> getGrindTravelDestinations(Player* bot, bool ignoreFull = false, bool ignoreInactive = false, float maxDistance = 5000, uint32 maxCheck = 50);
        std::vector<TravelDestination*> getBossTravelDestinations(Player* bot, bool ignoreFull = false, bool ignoreInactive = false, float maxDistance = 25000);


        void setNullTravelTarget(Player* player);

        void addMapTransfer(WorldPosition start, WorldPosition end, float portalDistance = 0.1f, bool makeShortcuts = true);
        void loadMapTransfers();
        float mapTransDistance(WorldPosition start, WorldPosition end, bool toMap = false);
        float fastMapTransDistance(WorldPosition start, WorldPosition end, bool toMap = false);

        NullTravelDestination* nullTravelDestination = new NullTravelDestination();
        WorldPosition* nullWorldPosition = new WorldPosition();

        void addBadVmap(uint32 mapId, int x, int y) { badVmap.push_back(std::make_tuple(mapId, x, y)); }
        void addBadMmap(uint32 mapId, int x, int y) { badMmap.push_back(std::make_tuple(mapId, x, y)); }
        bool isBadVmap(uint32 mapId, int x, int y) { return std::find(badVmap.begin(), badVmap.end(), std::make_tuple(mapId, x, y)) != badVmap.end(); }
        bool isBadMmap(uint32 mapId, int x, int y) { return std::find(badMmap.begin(), badMmap.end(), std::make_tuple(mapId, x, y)) != badMmap.end(); }


        void printGrid(uint32 mapId, int x, int y, std::string type);
        void printObj(WorldObject* obj, std::string type);

        int32 getAreaLevel(uint32 area_id);
        void loadAreaLevels();
        std::unordered_map<uint32, QuestContainer*> getQuests() { return quests; }
        std::unordered_map<uint32, ExploreTravelDestination*> getExploreLocs() { return exploreLocs; }
    protected:
        void logQuestError(uint32 errorNr, Quest* quest, uint32 objective = 0, uint32 unitId = 0, uint32 itemId = 0);

        std::vector<uint32> avoidLoaded;

        std::vector<QuestGiverTravelDestination*> questGivers;
        std::vector<RpgTravelDestination*> rpgNpcs;
        std::vector<GrindTravelDestination*> grindMobs;
        std::vector<BossTravelDestination*> bossMobs;

        std::unordered_map<uint32, ExploreTravelDestination*> exploreLocs;
        std::unordered_map<uint32, QuestContainer*> quests;
        std::unordered_map<uint64, GuidPosition> pointsMap;
        std::unordered_map<uint32, int32> areaLevels;

        std::vector<std::tuple<uint32, int, int>> badVmap, badMmap;

        std::unordered_map<std::pair<uint32, uint32>, std::vector<mapTransfer>, boost::hash<std::pair<uint32, uint32>>> mapTransfersMap;

    public:
        std::unordered_set<uint32> m_allWorldEventQuestIds = {
            7935,
            7932,
            7981,
            7940,
            7933,
            7930,
            7931,
            7934,
            7936,
            9029,
            8744,
            8803,
            8768,
            8767,
            8788,
            9319,
            9386,
            6984,
            7045,
            9339,
            9365,
            8769,
            171,
            171,
            5502,
            5502,
            9024,
            7885,
            8647,
            7892,
            8715,
            8719,
            8718,
            8673,
            8726,
            8866,
            925,
            7881,
            7882,
            8353,
            8354,
            172,
            1468,
            3861,
            8882,
            8880,
            7889,
            7894,
            1658,
            7884,
            8357,
            8360,
            8903,
            8904,
            8648,
            8677,
            7907,
            7929,
            7927,
            7928,
            8683,
            8897,
            8898,
            8899,
            8900,
            8901,
            8902,
            910,
            8684,
            8868,
            8862,
            7903,
            8727,
            8979,
            8863,
            8864,
            8865,
            8878,
            8877,
            8356,
            8359,
            911,
            8981,
            8993,
            8222,
            8653,
            8652,
            6961,
            7021,
            7024,
            7022,
            7023,
            7896,
            7891,
            8679,
            8311,
            8312,
            8646,
            7890,
            8686,
            8643,
            8149,
            8150,
            8355,
            8358,
            8651,
            558,
            8881,
            8879,
            1800,
            8867,
            8722,
            7897,
            8746,
            8762,
            8685,
            8714,
            8717,
            7941,
            7943,
            7939,
            8223,
            7942,
            9025,
            8619,
            8724,
            8860,
            8861,
            8723,
            8645,
            8654,
            8678,
            8671,
            7893,
            8725,
            8322,
            8409,
            8636,
            8670,
            8642,
            8675,
            8720,
            8682,
            7899,
            8876,
            8650,
            7901,
            7946,
            8635,
            1687,
            8716,
            8713,
            8721,
            9332,
            9331,
            9324,
            9330,
            9326,
            9325,
            1657,
            6963,
            7042,
            8644,
            8672,
            8649,
            1479,
            7061,
            7063,
            9368,
            8763,
            8870,
            8871,
            8872,
            8873,
            8875,
            8874,
            8373,
            6964,
            7062,
            8984,
            9028,
            1558,
            7883,
            7898,
            8681,
            7900,
            8982,
            8983,
            9027,
            9026,
            6962,
            7025,
            8883,
            7902,
            7895,
            9322,
            9323,
            8676,
            8688,
            8680,
            8827,
            8828,
            8674,
            915,
            4822,
            6983,
            7043,
            7937,
            7938,
            7944,
            7945,
            8980,

            //festival???
            9367,
            9388,
            9389,

            //donyal???
            //579

            //commendation signets
            8818,
            8817,
            8816,
            8815,
            8814,
            8813,
            8811,
            8812,
            8830,
            8832,
            8834,
            8836,
            8838,
            8840,
            8842,
            8844,
            8811,
            8812,
            8813,
            8814,
            8815,
            8816,
            8817,
            8818,
            8826,
            8825,
            8824,
            8823,
            8822,
            8821,
            8820,
            8819,
            8831,
            8833,
            8835,
            8837,
            8839,
            8841,
            8843,
            8845,
            8819,
            8820,
            8821,
            8822,
            8823,
            8824,
            8825,
            8826,
        };

        std::unordered_set<uint32> m_allScourgeInvasionQuestIds = {
            9094,
            9318,
            9317,
            9292,
            9304,
            9301,
            9310,
            9295,
            9154,
            9321,
            9320,
            9302,
            9299,
            9300,
            9085,
            9341,
            9247,
            9153,

            //other bs???
            9260, //Lieutenant Dagel
            9261,
            9262,
            9263,
            9264,
            9265,

        };

        std::unordered_set<uint32> m_allWarEffortQuestIds = {
            8848,
            8853,
            8846,
            8851,
            8847,
            8852,
            8509,
            8492,
            8494,
            8511,
            8517,
            8513,
            8510,
            8493,
            8495,
            8512,
            8518,
            8514,
            8506,
            8525,
            8527,
            8523,
            8521,
            8529,
            8504,
            8516,
            8500,
            8505,
            8524,
            8526,
            8522,
            8520,
            8528,
            8503,
            8515,
            8499,
            8795,
            8796,
            8797,
            8615,
            8532,
            8580,
            8588,
            8611,
            8607,
            8545,
            8616,
            8533,
            8581,
            8589,
            8612,
            8608,
            8546,
            8550,
            8583,
            8601,
            8610,
            8614,
            8591,
            8543,
            8605,
            8549,
            8582,
            8600,
            8609,
            8613,
            8590,
            8542,
            8604,
            8792,
            8793,
            8794,
            8850,
            8855,
            8849,
            8854,
        };

        std::unordered_set<uint32> m_allClassQuestQuestIds = {
            //druid
            9053,
            27,
            26,
            5061,
            31,
            5931,
            5932,
            9052,
            6001,
            6002,
            6124,
            6129,
            6123,
            6128,
            5929,
            5930,
            5923,
            5924,
            5925,
            5926,
            5927,
            5928,
            6121,
            6126,
            5921,
            5922,
            6125,
            6130,
            6122,
            6127,
            9063,
            9051,
            28,
            29,
            30,
            272,
            3094,
            3120,

            //hunter
            7635,
            7633,
            7634,
            8153,
            3092,
            3087,
            3108,
            3117,
            3082,
            7636,
            6061,
            6062,
            6063,
            6064,
            6082,
            6083,
            6084,
            6085,
            6087,
            6088,
            6101,
            6102,
            7632,
            8232,
            8151,
            6065,
            6066,
            6067,
            6068,
            6069,
            6070,
            6071,
            6072,
            6073,
            6074,
            6075,
            6076,
            6081,
            6086,
            6089,
            6103,
            8231,

            //mage
            1942,
            1958,
            8253,
            9364,
            1921,
            1961,
            1950,
            3104,
            3114,
            3111,
            3098,
            3086,
            1949,
            1939,
            1960,
            1920,
            1948,
            1947,
            1884,
            1945,
            1952,
            1880,
            8250,
            8251,
            1957,
            1941,
            1861,
            1946,
            1956,
            1940,
            1959,
            1919,
            1953,
            1951,
            1881,
            1879,
            1943,
            1860,
            1883,
            1962,
            1882,
            1955,
            1954,
            8252,
            1938,
            9362,
            1944,

            //paladin
            7666,
            7643,
            1655,
            7644,
            8415,
            7642,
            3101,
            3107,
            8414,
            7637,
            7640,
            8418,
            7648,
            8416,
            7647,
            7638,
            7645,
            1442,
            7646,
            1789,
            1790,
            1653,
            1654,
            1806,
            1641,
            1642,
            1643,
            1644,
            1645,
            1646,
            1647,
            1648,
            1778,
            1779,
            1780,
            1781,
            1783,
            1784,
            1785,
            1786,
            1787,
            1788,
            1661,
            4485,
            4486,
            1649,
            1650,
            1651,
            1652,
            1793,
            1794,
            7641,
            7639,
            2997,
            2998,
            2999,
            3000,
            3681,

            //priest
            5641,
            5645,
            7621,
            5676,
            5677,
            8257,
            8254,
            5634,
            5635,
            5636,
            5637,
            5638,
            5639,
            5644,
            5646,
            5679,
            5672,
            5673,
            5674,
            5675,
            5650,
            5648,
            5624,
            5625,
            5621,
            3103,
            3110,
            3097,
            3119,
            3085,
            5652,
            5654,
            5655,
            5656,
            5657,
            5651,
            5622,
            5649,
            5623,
            5626,
            8255,
            5628,
            5629,
            5630,
            5631,
            5632,
            5633,
            5642,
            5643,
            5680,
            5627,
            7622,
            8256,
            5658,
            5660,
            5661,
            5662,
            5663,
            5659,

            //rogue
            8233,
            2282,
            2458,
            2242,
            8235,
            3102,
            3113,
            3088,
            3109,
            3096,
            3118,
            3083,
            2259,
            2260,
            1998,
            2378,
            2479,
            2480,
            2358,
            8249,
            2298,
            2359,
            2360,
            1885,
            2478,
            2239,
            2381,
            2281,
            2218,
            8234,
            2205,
            2300,
            2238,
            2206,
            6701,
            2241,
            8236,
            1886,
            1898,
            1899,
            1978,
            6681,
            1858,
            1963,
            2460,
            2607,
            2608,
            2609,
            1859,
            2299,
            2380,
            1999,
            2382,
            2379,

            //shaman
            1531,
            1532,
            1516,
            1517,
            1518,
            1519,
            1520,
            1521,
            1523,
            1524,
            1525,
            1526,
            1527,
            2983,
            2984,
            1522,
            1103,
            1528,
            1529,
            1530,
            2985,
            2986,
            1535,
            1536,
            1534,
            220,
            63,
            100,
            96,
            8413,
            1463,
            1462,
            8410,
            1464,
            8411,
            7667,
            7667,
            3093,
            3089,
            3084,
            8412,
            7668,
            8258,
            7668,
            972,

            //warlock
            8419,
            7630,
            1599,
            7626,
            1508,
            4961,
            1796,
            4781,
            4782,
            4783,
            4784,
            1473,
            1501,
            1472,
            1507,
            1716,
            1515,
            7628,
            7631,
            4785,
            7602,
            1799,
            1685,
            1717,
            1506,
            1478,
            1476,
            1738,
            8420,
            7629,
            4736,
            4737,
            4738,
            4739,
            1511,
            4965,
            4967,
            4968,
            4969,
            7603,
            7623,
            1512,
            7562,
            1509,
            1510,
            1470,
            7563,
            4976,
            1798,
            2996,
            3001,
            4962,
            4963,
            3631,
            4487,
            4488,
            4489,
            4490,
            7583,
            1688,
            6000,
            3105,
            3115,
            3090,
            3099,
            1471,
            1474,
            1504,
            1513,
            1689,
            1795,
            1739,
            1474,
            1513,
            1739,
            1795,
            4964,
            4975,
            4786,
            1740,
            7581,
            7582,
            1715,
            1598,
            8421,
            1758,
            1801,
            1802,
            1803,
            1804,
            1805,
            8422,
            7624,
            1485,
            1499,
            7601,
            7627,
            7564,
            7625,

            //warrior
            8417,
            1638,
            1821,
            1639,
            1665,
            1640,
            1838,
            1843,
            1848,
            1845,
            1847,
            1705,
            1844,
            1712,
            1667,
            1846,
            1684,
            1714,
            1701,
            1503,
            1782,
            1682,
            1700,
            1706,
            1822,
            1708,
            1681,
            1704,
            1709,
            1666,
            1703,
            1711,
            1679,
            1840,
            1498,
            1842,
            3100,
            3112,
            3091,
            2383,
            3106,
            3095,
            3116,
            3065,
            1692,
            1820,
            1818,
            1823,
            1825,
            1710,
            1719,
            1718,
            1699,
            1686,
            1702,
            1713,
            1791,
            1502,
            1680,
            1824,
            1839,
            1819,
            1678,
            1841,
            1505,
            8425,
            1683,
            8424,
            8423,
            1693,
            1792,
            1698,
        };
    };
}

#define sTravelMgr MaNGOS::Singleton<TravelMgr>::Instance()

