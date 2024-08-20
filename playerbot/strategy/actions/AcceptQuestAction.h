#pragma once

#include "playerbot/strategy/Action.h"
#include "QuestAction.h"

namespace ai
{
    class AcceptAllQuestsAction : public QuestAction 
    {
    public:
        AcceptAllQuestsAction(PlayerbotAI* ai, std::string name = "accept all quests") : QuestAction(ai, name) {}

    protected:
        virtual bool ProcessQuest(Player* requester, Quest const* quest, WorldObject* questGiver) override;
    };

    class AcceptQuestAction : public AcceptAllQuestsAction 
    {
    public:
        AcceptQuestAction(PlayerbotAI* ai) : AcceptAllQuestsAction(ai, "accept quest") {}
        virtual bool Execute(Event& event);
    };

    class AcceptQuestShareAction : public Action 
    {
    public:
        AcceptQuestShareAction(PlayerbotAI* ai) : Action(ai, "accept quest share") {}
        virtual bool Execute(Event& event);
    };

    class ConfirmQuestAction : public Action {
    public:
        ConfirmQuestAction(PlayerbotAI* ai) : Action(ai, "confirm quest") {}
        virtual bool Execute(Event& event);
        static bool ConfirmQuestForBot(Player* requester, PlayerbotAI* ai, Quest const* qInfo, uint32 quest);
    };

    class QuestDetailsAction : public Action {
    public:
        QuestDetailsAction(PlayerbotAI* ai) : Action(ai, "quest details") {}
        virtual bool Execute(Event& event);
    };
}
