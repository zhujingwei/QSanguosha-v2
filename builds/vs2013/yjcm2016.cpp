#include "yjcm2016.h"
#include "general.h"
#include "serverplayer.h"
#include "room.h"
#include "skill.h"
#include "roomthread.h"
#include "clientplayer.h"
#include "engine.h"


JisheCard::JisheCard()
{

}

bool JisheCard::targetFixed() const
{
    if (user_string == "IronChain")
        return false;
    return true;
}

bool JisheCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.length() < Self->getHp() && !to_select->isChained();
}

bool JisheCard::targetsFeasible(const QList<const Player *> &targets, const Player *) const
{
    return !targets.isEmpty();
}

void JisheCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    if (!targets.isEmpty()) {
        for (int i = 0; i < targets.length(); i++) {
            ServerPlayer *target = targets[i];
            target->setChained(true);
            room->broadcastProperty(target, "chained");
            room->setEmotion(target, "chain");
            room->getThread()->trigger(ChainStateChanged, room, target);
        }
    } else {
        source->drawCards(1);
        room->addPlayerMark(source, "@jishe");
    }
}

class JisheVS : public ZeroCardViewAsSkill
{
public:
    JisheVS() : ZeroCardViewAsSkill("jishe")
    {

    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return player->getMaxCards() > 0;
    }

    bool isEnabledAtResponse(const Player *, const QString &pattern) const
    {
        return pattern == "@@jishe";
    }

    const Card *viewAs() const
    {
        JisheCard *card = new JisheCard;
        if (Sanguosha->getCurrentCardUsePattern() == "@@jishe")
            card->setUserString("IronChain");
        return card;
    }
};

class Jishe : public PhaseChangeSkill
{
public:
    Jishe() : PhaseChangeSkill("jishe")
    {
        view_as_skill = new JisheVS;
    }

    bool onPhaseChange(ServerPlayer *target) const
    {
        if (target->getPhase() != Player::Finish)
            return false;

        Room *room = target->getRoom();
        room->setPlayerMark(target, "@jishe", 0);
        if (target->isKongcheng())
            room->askForUseCard(target, "@@jishe", "@jishe-chain");

        return false;
    }
};

class JisheMaxCards : public MaxCardsSkill
{
public:
    JisheMaxCards() : MaxCardsSkill("#jishe-max")
    {
    }

    int getExtra(const Player *target) const
    {
        return -target->getMark("@jishe");
    }
};

class Lianhuo : public TriggerSkill
{
public:
    Lianhuo() : TriggerSkill("lianhuo")
    {
        frequency = Compulsory;
        events << DamageInflicted;
    }

    bool trigger(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.nature == DamageStruct::Fire && !damage.chain && player->isChained()) {
            room->sendCompulsoryTriggerLog(player, objectName());
            player->broadcastSkillInvoke(objectName());
            damage.damage++;
            data.setValue(damage);
        }

        return false;
    }
};


YJCM2016Package::YJCM2016Package() : Package("YJCM2016")
{
    General *cenhun = new General(this, "cenhun", "wu", 3);
    cenhun->addSkill(new Jishe);
    cenhun->addSkill(new JisheMaxCards);
    cenhun->addSkill(new Lianhuo);
    related_skills.insertMulti("jishe", "#jishe-max");

    addMetaObject<JisheCard>();
}

ADD_PACKAGE(YJCM2016)