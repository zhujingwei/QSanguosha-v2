#include "yjcm2016.h"
#include "general.h"
#include "serverplayer.h"
#include "room.h"
#include "skill.h"
#include "roomthread.h"
#include "clientplayer.h"
#include "engine.h"
#include "clientstruct.h"
#include "json.h"


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


JiaozhaoCard::JiaozhaoCard()
{
    will_throw = false;
}

bool JiaozhaoCard::targetFixed() const
{
    QStringList options = user_string.split("+");
    if (options.contains("self"))
        return true;
    return false;
}

bool JiaozhaoCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    int min_distance = 999;
    foreach (const Player *p, Self->getSiblings())
    {
        int distance = Self->distanceTo(p);
        min_distance = qMin(min_distance, distance);
    }
    return targets.isEmpty() && Self->distanceTo(to_select) == min_distance;
}

void JiaozhaoCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    ServerPlayer *target = targets.isEmpty() ? source : targets.first();
    QStringList choices;
    QList<const Card *> cards = Sanguosha->findChildren<const Card *>();
    foreach(const Card *card, cards)
    {
        if (isAvailableChoice(card, source) && !choices.contains(card->objectName()))
            choices << card->objectName();
    }
    room->showCard(source, getEffectiveId());
    QString choice = room->askForChoice(target, "jiaozhao", choices.join("+"), QVariant::fromValue(source));
    Card *card = Sanguosha->getCard(subcards.first());
    room->setCardFlag(card, "jiaozhao");
    room->setPlayerProperty(source, "jiaozhao", choice);

    LogMessage message;
    message.type = "#jiaozhao_choice";
    message.from = target;
    message.arg = choice;
    room->sendLog(message);
}

bool JiaozhaoCard::isAvailableChoice(const Card *card, ServerPlayer *) const
{
    QStringList options = user_string.split("+");
    if ((card->getTypeId() == Card::TypeBasic || (options.contains("trick") && card->isNDTrick())) && !ServerInfo.Extensions.contains("!" + card->getPackage()))
        return true;
    return false;
}

class JiaozhaoVS : public OneCardViewAsSkill
{
public:
    JiaozhaoVS() : OneCardViewAsSkill("jiaozhao")
    {
        
    }

    bool viewFilter(const Card *to_select) const
    {
        if (Self->hasUsed("JiaozhaoCard"))
            return to_select->hasFlag("jiaozhao") && !to_select->isEquipped();
        else
            return !to_select->isEquipped();
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        if (player->hasUsed("JiaozhaoCard")) {
            QString card_name = player->property("jiaozhao").toString();
            return card_name != QString();
        }
        return !player->isKongcheng();
    }

    const Card *viewAs(const Card *originalCard) const
    {
        if (Self->hasUsed("JiaozhaoCard")) {
            QString card_name = Self->property("jiaozhao").toString();
            Card *new_card = Sanguosha->cloneCard(card_name);
            new_card->addSubcard(originalCard);
            new_card->setSkillName(objectName());
            return new_card;
        } else {
            JiaozhaoCard *card = new JiaozhaoCard;
            card->addSubcard(originalCard);
            QStringList options;
            if (Self->getMark("jiaozhao-self") > 0)
                options << "self";
            if (Self->getMark("jiaozhao-trick") > 0)
                options << "trick";
            card->setUserString(options.join("+"));
            return card;
        }
    }
};

class Jiaozhao : public TriggerSkill
{
public:
    Jiaozhao() : TriggerSkill("jiaozhao")
    {
        events << CardUsed << EventPhaseChanging;
        view_as_skill = new JiaozhaoVS;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == CardUsed) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (!use.card->getTypeId() == Card::TypeSkill && use.card->getSkillName() == objectName()) {
                room->setPlayerProperty(player, "jiaozhao", QVariant());
                use.card->setFlags("-jiaozhao");
            }
        } else {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.to == Player::NotActive) {
                foreach (const Card *card, player->getHandcards()) { 
                    if (card->hasFlag("jiaozhao"))
                        room->setCardFlag(card, "-jiaozhao");
                }
            }
        }
        return false;
    }

    QString getDescriptionSource(const Player *player) const
    {
        QString add_str;
        if (player != NULL) {
            if (player->getMark("jiaozhao-self") > 0)
                add_str += "-self";
            if (player->getMark("jiaozhao-trick") > 0)
                add_str += "-trick";
        }
        return objectName() + add_str;
    }
};

class JiaozhaoProhibit : public ProhibitSkill
{
public:
    JiaozhaoProhibit() : ProhibitSkill("#jiaozhao-prohibit")
    {

    }

    bool isProhibited(const Player *from, const Player *to, const Card *card, const QList<const Player *> &) const
    {
        if (card->getTypeId() != Card::TypeSkill) {
            if (card->getSkillName() == "jiaozhao")
                return from == to;
        }
        return false;
    }
};

class Danxin : public MasochismSkill
{
public:
    Danxin() : MasochismSkill("danxin")
    {

    }

    void onDamaged(ServerPlayer *target, const DamageStruct &) const
    {
        if (target->askForSkillInvoke(this)) {
            target->broadcastSkillInvoke(objectName());
            Room *room = target->getRoom();

            QStringList choices;
            choices << "draw";
            if (target->hasSkill("jiaozhao")) {
                if (target->getMark("jiaozhao-self") == 0)
                    choices << "jiaozhao-self";
                if (target->getMark("jiaozhao-trick") == 0)
                    choices << "jiaozhao-trick";
            }
            QString choice = target->getRoom()->askForChoice(target, objectName(), choices.join("+"));
            if (choice == "draw")
                target->drawCards(1);
            else {
                room->setPlayerMark(target, choice, 1);
                LogMessage log;
                log.from = target;
                log.type = "#danxin-choice";
                log.arg = choice;
                room->sendLog(log);
                room->updateSkill(target, "jiaozhao");
            }
        }
    }
};


YJCM2016Package::YJCM2016Package() : Package("YJCM2016")
{
    General *cenhun = new General(this, "cenhun", "wu", 3);
    cenhun->addSkill(new Jishe);
    cenhun->addSkill(new JisheMaxCards);
    cenhun->addSkill(new Lianhuo);
    related_skills.insertMulti("jishe", "#jishe-max");

    General *guohuanghou = new General(this, "guohuanghou", "wei", 3, false);
    guohuanghou->addSkill(new Jiaozhao);
    guohuanghou->addSkill(new JiaozhaoProhibit);
    guohuanghou->addSkill(new Danxin);
    related_skills.insertMulti("jiaozhao", "#jiaozhao-prohibit");

    addMetaObject<JisheCard>();
    addMetaObject<JiaozhaoCard>();
}

ADD_PACKAGE(YJCM2016)