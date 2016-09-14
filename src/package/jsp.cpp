#include "jsp.h"
#include "sp.h"
#include "client.h"
#include "general.h"
#include "skill.h"
#include "standard-skillcards.h"
#include "engine.h"
#include "maneuvering.h"
#include "json.h"
#include "settings.h"
#include "clientplayer.h"
#include "util.h"
#include "wrapped-card.h"
#include "room.h"
#include "roomthread.h"


class Liangzhu : public TriggerSkill
{
public:
    Liangzhu() : TriggerSkill("liangzhu")
    {
        events << HpRecover;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->isAlive() && target->getPhase() == Player::Play;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        foreach(ServerPlayer *sun, room->getAllPlayers()) {
            if (TriggerSkill::triggerable(sun)) {
                QString choice = room->askForChoice(sun, objectName(), "draw+letdraw+dismiss", QVariant::fromValue(player));
                if (choice == "dismiss")
                    continue;

                room->broadcastSkillInvoke(objectName());
                room->notifySkillInvoked(sun, objectName());
                if (choice == "draw") {
                    sun->drawCards(1);
                    room->setPlayerMark(sun, "@liangzhu_draw", 1);
                }
                else if (choice == "letdraw") {
                    player->drawCards(2);
                    room->setPlayerMark(player, "@liangzhu_draw", 1);
                }
            }
        }
        return false;
    }
};

class Fanxiang : public TriggerSkill
{
public:
    Fanxiang() : TriggerSkill("fanxiang")
    {
        events << EventPhaseStart;
        frequency = Skill::Wake;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return (target != NULL && target->hasSkill(this) && target->getPhase() == Player::Start && target->getMark("@fanxiang") == 0);
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        bool flag = false;
        foreach(ServerPlayer *p, room->getAlivePlayers()) {
            if (p->getMark("@liangzhu_draw") > 0 && p->isWounded()) {
                flag = true;
                break;
            }
        }
        if (flag) {
            room->broadcastSkillInvoke(objectName());

//             room->doSuperLightbox("jsp_sunshangxiang", "fanxiang");

            //room->doLightbox("$fanxiangAnimate", 5000);
            room->notifySkillInvoked(player, objectName());
            room->setPlayerMark(player, "fanxiang", 1);
            if (room->changeMaxHpForAwakenSkill(player, 1) && player->getMark("fanxiang") > 0) {

                foreach(ServerPlayer *p, room->getAllPlayers()) {
                    if (p->getMark("@liangzhu_draw") > 0)
                        room->setPlayerMark(p, "@liangzhu_draw", 0);
                }

                room->recover(player, RecoverStruct());
                room->handleAcquireDetachSkills(player, "-liangzhu|xiaoji", objectName());
            }
        }
        return false;
    }
};

class Nuzhan : public TriggerSkill
{
public:
    Nuzhan() : TriggerSkill("nuzhan")
    {
        events << PreCardUsed << CardUsed << ConfirmDamage;
        frequency = Compulsory;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->isAlive();
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
        if (triggerEvent == PreCardUsed) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (TriggerSkill::triggerable(use.from)) {
                if (use.card != NULL && use.card->isKindOf("Slash") && use.card->isVirtualCard() && use.card->subcardsLength() == 1 && Sanguosha->getCard(use.card->getSubcards().first())->isKindOf("TrickCard")) {
                    room->broadcastSkillInvoke(objectName(), 1);
                    room->addPlayerHistory(use.from, use.card->getClassName(), -1);
                    use.m_addHistory = false;
                    data = QVariant::fromValue(use);
                }
            }
        }
        else if (triggerEvent == CardUsed) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (TriggerSkill::triggerable(use.from)) {
                if (use.card != NULL && use.card->isKindOf("Slash") && use.card->isVirtualCard() && use.card->subcardsLength() == 1 && Sanguosha->getCard(use.card->getSubcards().first())->isKindOf("EquipCard"))
                    use.card->setFlags("nuzhan_slash");
            }
        }
        else if (triggerEvent == ConfirmDamage) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.card != NULL && damage.card->hasFlag("nuzhan_slash")) {
                if (damage.from != NULL)
                    room->sendCompulsoryTriggerLog(damage.from, objectName(), true);

                room->broadcastSkillInvoke(objectName(), 2);

                ++damage.damage;
                data = QVariant::fromValue(damage);
            }
        }
        return false;
    }
};

/*
class Nuzhan_tms : public TargetModSkill {
public:
Nuzhan_tms() : TargetModSkill("#nuzhan") {

}

int getResidueNum(const Player *from, const Card *card) const {
if (from->hasSkill("nuzhan")) {
if ((card->isVirtualCard() && card->subcardsLength() == 1 && Sanguosha->getCard(card->getSubcards().first())->isKindOf("TrickCard")) || card->hasFlag("Global_SlashAvailabilityChecker"))
return 1000;
}

return 0;
}
};
*/

class JspDanqi : public PhaseChangeSkill
{
public:
    JspDanqi() : PhaseChangeSkill("jspdanqi")
    {
        frequency = Wake;
    }

    bool triggerable(const ServerPlayer *guanyu, Room *room) const
    {
        return PhaseChangeSkill::triggerable(guanyu) && guanyu->getMark(objectName()) == 0 && guanyu->getPhase() == Player::Start && guanyu->getHandcardNum() > guanyu->getHp() && !lordIsLiubei(room);
    }

    bool onPhaseChange(ServerPlayer *target) const
    {
        Room *room = target->getRoom();
        room->broadcastSkillInvoke(objectName());
        //room->doLightbox("$JspdanqiAnimate");
//         room->doSuperLightbox("jsp_guanyu", "jspdanqi");
        room->setPlayerMark(target, objectName(), 1);
        if (room->changeMaxHpForAwakenSkill(target) && target->getMark(objectName()) > 0)
            room->handleAcquireDetachSkills(target, "mashu|nuzhan", objectName());

        return false;
    }

private:
    static bool lordIsLiubei(const Room *room)
    {
        if (room->getLord() != NULL) {
            const ServerPlayer *const lord = room->getLord();
            if (lord->getGeneral() && lord->getGeneralName().contains("liubei"))
                return true;

            if (lord->getGeneral2() && lord->getGeneral2Name().contains("liubei"))
                return true;
        }

        return false;
    }
};

class Kunfen : public PhaseChangeSkill
{
public:
    Kunfen() : PhaseChangeSkill("kunfen")
    {

    }

    Frequency getFrequency(const Player *target) const
    {
        if (target != NULL) {
            return target->getMark("fengliang") > 0 ? NotFrequent : Compulsory;
        }

        return Compulsory;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Finish;
    }

    bool onPhaseChange(ServerPlayer *target) const
    {
        if (invoke(target))
            effect(target);

        return false;
    }

private:
    bool invoke(ServerPlayer *target) const
    {
        return getFrequency(target) == Compulsory ? true : target->askForSkillInvoke(this);
    }

    void effect(ServerPlayer *target) const
    {
        Room *room = target->getRoom();

        if (getFrequency(target) == Compulsory)
            room->broadcastSkillInvoke(objectName(), 1);
        else
            room->broadcastSkillInvoke(objectName(), 2);

        room->loseHp(target);
        if (target->isAlive())
            target->drawCards(2, objectName());
    }
};

class Fengliang : public TriggerSkill
{
public:
    Fengliang() : TriggerSkill("fengliang")
    {
        frequency = Wake;
        events << Dying;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return TriggerSkill::triggerable(target) && target->getMark(objectName()) == 0;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        DyingStruct dying = data.value<DyingStruct>();
        if (dying.who != player)
            return false;

        room->broadcastSkillInvoke(objectName());
//         room->doSuperLightbox("jsp_jiangwei", objectName());

        room->addPlayerMark(player, objectName(), 1);
        if (room->changeMaxHpForAwakenSkill(player) && player->getMark(objectName()) > 0) {
            int recover = 2 - player->getHp();
            room->recover(player, RecoverStruct(NULL, NULL, recover));
            room->handleAcquireDetachSkills(player, "tiaoxin", objectName());

            if (player->hasSkill("kunfen", true))
                room->doNotify(player, QSanProtocol::S_COMMAND_UPDATE_SKILL, QVariant("kunfen"));
        }

        return false;
    }
};

class Chixin : public OneCardViewAsSkill
{  // Slash::isSpecificAssignee
public:
    Chixin() : OneCardViewAsSkill("chixin")
    {
        filter_pattern = ".|diamond";
        response_or_use = true;
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return Slash::IsAvailable(player);
    }

    bool isEnabledAtResponse(const Player *, const QString &pattern) const
    {
        return pattern == "jink" || pattern == "slash";
    }

    const Card *viewAs(const Card *originalCard) const
    {
        //CardUseStruct::CardUseReason r = Sanguosha->currentRoomState()->getCurrentCardUseReason();
        QString p = Sanguosha->currentRoomState()->getCurrentCardUsePattern();
        Card *c = NULL;
        if (p == "jink")
            c = new Jink(Card::SuitToBeDecided, -1);
        else
            c = new Slash(Card::SuitToBeDecided, -1);

        if (c == NULL)
            return NULL;

        c->setSkillName(objectName());
        c->addSubcard(originalCard);
        return c;
    }
};

class ChixinTrigger : public TriggerSkill
{
public:
    ChixinTrigger() : TriggerSkill("chixin")
    {
        events << PreCardUsed << EventPhaseEnd;
        view_as_skill = new Chixin;
        global = true;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    int getPriority(TriggerEvent) const
    {
        return 8;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == PreCardUsed) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card != NULL && use.card->isKindOf("Slash") && player->getPhase() == Player::Play) {
                QSet<QString> s = player->property("chixin").toString().split("+").toSet();
                foreach(ServerPlayer *p, use.to)
                    s.insert(p->objectName());

                QStringList l = s.toList();
                room->setPlayerProperty(player, "chixin", l.join("+"));
            }
        }
        else if (player->getPhase() == Player::Play)
            room->setPlayerProperty(player, "chixin", QString());

        return false;
    }
};

class Suiren : public PhaseChangeSkill
{
public:
    Suiren() : PhaseChangeSkill("suiren")
    {
        frequency = Limited;
        limit_mark = "@suiren";
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return TriggerSkill::triggerable(target) && target->getPhase() == Player::Start && target->getMark("@suiren") > 0;
    }

    bool onPhaseChange(ServerPlayer *target) const
    {
        Room *room = target->getRoom();
        ServerPlayer *p = room->askForPlayerChosen(target, room->getAlivePlayers(), objectName(), "@suiren-draw", true);
        if (p == NULL)
            return false;

        room->broadcastSkillInvoke(objectName());
//         room->doSuperLightbox("jsp_zhaoyun", "suiren");
        room->setPlayerMark(target, "@suiren", 0);

        room->handleAcquireDetachSkills(target, "-yicong", objectName());
        int maxhp = target->getMaxHp() + 1;
        room->setPlayerProperty(target, "maxhp", maxhp);
        room->recover(target, RecoverStruct());

        room->doAnimate(QSanProtocol::S_ANIMATE_INDICATE, target->objectName(), p->objectName());
        p->drawCards(3, objectName());

        return false;
    }
};


JiqiaoCard::JiqiaoCard()
{
    target_fixed = true;
}

void JiqiaoCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const
{
    int n = subcardsLength() * 2;
    QList<int> card_ids = room->getNCards(n, false);
    CardMoveReason reason1(CardMoveReason::S_REASON_TURNOVER, source->objectName(), "jiqiao", QString());
    CardsMoveStruct move(card_ids, NULL, Player::PlaceTable, reason1);
    room->moveCardsAtomic(move, true);
    room->getThread()->delay();
    room->getThread()->delay();

    DummyCard get;
    DummyCard thro;

    foreach(int id, card_ids) {
        const Card *c = Sanguosha->getCard(id);
        if (c == NULL)
            continue;

        if (c->isKindOf("TrickCard"))
            get.addSubcard(c);
        else
            thro.addSubcard(c);
    }

    if (get.subcardsLength() > 0)
        source->obtainCard(&get);

    if (thro.subcardsLength() > 0) {
        CardMoveReason reason2(CardMoveReason::S_REASON_NATURAL_ENTER, QString(), "jiqiao", QString());
        room->throwCard(&thro, reason2, NULL);
    }
}

class JiqiaoVS : public ViewAsSkill
{
public:
    JiqiaoVS() : ViewAsSkill("jiqiao")
    {
        response_pattern = "@@jiqiao";
    }

    bool viewFilter(const QList<const Card *> &, const Card *to_select) const
    {
        return to_select->isKindOf("EquipCard") && !Self->isJilei(to_select);
    }

    const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.length() == 0)
            return NULL;

        JiqiaoCard *jq = new JiqiaoCard;
        jq->addSubcards(cards);
        return jq;
    }
};

class Jiqiao : public PhaseChangeSkill
{
public:
    Jiqiao() : PhaseChangeSkill("jiqiao")
    {
        view_as_skill = new JiqiaoVS;
    }

    bool onPhaseChange(ServerPlayer *player) const
    {
        if (player->getPhase() != Player::Play)
            return false;

        if (!player->canDiscard(player, "he"))
            return false;

        player->getRoom()->askForUseCard(player, "@@jiqiao", "@jiqiao", -1, Card::MethodDiscard);

        return false;
    }
};

class Linglong : public TriggerSkill
{
public:
    Linglong() : TriggerSkill("linglong")
    {
        frequency = Compulsory;
        events << CardAsked;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return TriggerSkill::triggerable(target) && target->getArmor() == NULL && target->hasArmorEffect("eight_diagram");
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *wolong, QVariant &data) const
    {
        QString pattern = data.toStringList().first();

        if (pattern != "jink")
            return false;

        if (wolong->askForSkillInvoke("eight_diagram")) {
            room->broadcastSkillInvoke(objectName());
            JudgeStruct judge;
            judge.pattern = ".|red";
            judge.good = true;
            judge.reason = "eight_diagram";
            judge.who = wolong;

            room->judge(judge);

            if (judge.isGood()) {
                room->setEmotion(wolong, "armor/eight_diagram");
                Jink *jink = new Jink(Card::NoSuit, 0);
                jink->setSkillName("eight_diagram");
                room->provide(jink);
                return true;
            }
        }

        return false;
    }
};

class LinglongMax : public MaxCardsSkill
{
public:
    LinglongMax() : MaxCardsSkill("#linglong-horse")
    {

    }

    int getExtra(const Player *target) const
    {
        if (target->hasSkill("linglong") && target->getDefensiveHorse() == NULL && target->getOffensiveHorse() == NULL)
            return 1;

        return 0;
    }
};

class LinglongTreasure : public TriggerSkill
{
public:
    LinglongTreasure() : TriggerSkill("#linglong-treasure")
    {
        events << GameStart << EventAcquireSkill << EventLoseSkill << CardsMoveOneTime;
        frequency = Compulsory;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventLoseSkill && data.toString() == "linglong") {
            room->handleAcquireDetachSkills(player, "-qicai", objectName(), true);
            player->setMark("linglong_qicai", 0);
        }
        else if ((triggerEvent == EventAcquireSkill && data.toString() == "linglong") || (triggerEvent == GameStart && TriggerSkill::triggerable(player))) {
            if (player->getTreasure() == NULL) {
                room->notifySkillInvoked(player, objectName());
                room->handleAcquireDetachSkills(player, "qicai", objectName());
                player->setMark("linglong_qicai", 1);
            }
        }
        else if (triggerEvent == CardsMoveOneTime && player->isAlive() && player->hasSkill("linglong", true)) {
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (move.from == player && move.from_places.contains(Player::PlaceEquip)) {
                if (player->getTreasure() == NULL && player->getMark("linglong_qicai") == 0) {
                    room->notifySkillInvoked(player, objectName());
                    room->handleAcquireDetachSkills(player, "qicai", objectName());
                    player->setMark("linglong_qicai", 1);
                }
            }
            else if (move.to == player && move.to_place == Player::PlaceEquip) {
                if (player->getTreasure() != NULL && player->getMark("linglong_qicai") == 1) {
                    room->handleAcquireDetachSkills(player, "-qicai", objectName(), true);
                    player->setMark("linglong_qicai", 0);
                }
            }
        }
        return false;
    }
};

class JSPZhuiji : public DistanceSkill
{
public:
    JSPZhuiji() : DistanceSkill("jspzhuiji")
    {

    }

    int getCorrect(const Player *from, const Player *to) const
    {
        int correct = 0;
        if (from->hasSkill(this) && from->getHp() >= to->getHp())
            correct = 1 - from->distanceTo(to, 0, this);

        return correct;
    }
};

JSPShichouCard::JSPShichouCard()
{

}

bool JSPShichouCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return to_select->hasFlag("JSPShichouOptionalTarget") && targets.length() < Self->getLostHp();
}

void JSPShichouCard::use(Room *room, ServerPlayer *, QList<ServerPlayer *> &targets) const
{
    foreach(ServerPlayer *p, targets)
        room->setPlayerFlag(p, "JSPShichouTarget");
}

class JSPShichouVS : public ZeroCardViewAsSkill
{
public:
    JSPShichouVS() : ZeroCardViewAsSkill("jspshichou")
    {
    }

    bool isEnabledAtPlay(const Player *) const
    {
        return false;
    }

    bool isEnabledAtResponse(const Player *, const QString &pattern) const
    {
        return pattern == "@@jspshichou";
    }

    const Card *viewAs() const
    {
        return new JSPShichouCard;
    }
};

class JSPShichou : public TriggerSkill
{
public:
    JSPShichou() : TriggerSkill("jspshichou")
    {
        events << TargetSpecifying;
        view_as_skill = new JSPShichouVS;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (!player->isWounded())
            return false;
        CardUseStruct useStruct = data.value<CardUseStruct>();
        if (useStruct.card->isKindOf("Slash") && useStruct.from->hasSkill(this))
        {
            QList<ServerPlayer *> targets;
            foreach(ServerPlayer *p, room->getOtherPlayers(player))
            {
                if (!useStruct.to.contains(p) && player->canSlash(p, useStruct.card))
                {
                    targets.append(p);
                }
            }

            if (!targets.isEmpty())
            {
                if (player->askForSkillInvoke(this, data, false))
                {
                    foreach(ServerPlayer* p, targets)
                        room->setPlayerFlag(p, "JSPShichouOptionalTarget");
                    QString promptStr = QString("@@jspshichou-invoke:%1:%2:%3").arg(player->objectName()).arg(QString()).arg(player->getLostHp());
                    if (room->askForUseCard(player, "@@jspshichou", promptStr))
                    {
                        foreach(ServerPlayer *p, targets)
                        {
                            if (p->hasFlag("JSPShichouTarget"))
                            {
                                room->setPlayerFlag(p, "-JSPShichouTarget");
                                useStruct.to.append(p);
                            }
                        }
                        room->sortByActionOrder(useStruct.to);
                        data = QVariant::fromValue(useStruct);
                    }
                    foreach(ServerPlayer* p, targets)
                        room->setPlayerFlag(p, "-JSPShichouOptionalTarget");
                }
            }
        }
        return false;
    }
};

class JSPZhenlve : public TriggerSkill
{
public:
    JSPZhenlve() : TriggerSkill("jspzhenlve")
    {
        events << TrickCardCanceling;
        frequency = Compulsory;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent, Room *, ServerPlayer *, QVariant &data) const
    {
        CardEffectStruct effect = data.value<CardEffectStruct>();
        if (effect.from != NULL && effect.from->isAlive() && effect.from->hasSkill(this))
            return true;
        return false;
    }
};

class JSPZhenlveProhibit : public ProhibitSkill
{
public:
    JSPZhenlveProhibit() : ProhibitSkill("#jspzhenlve-prohibit")
    {
    }

    bool isProhibited(const Player *, const Player *to, const Card *card, const QList<const Player *> &) const
    {
        return to->hasSkill(this) && card->isKindOf("DelayedTrick");
    }
};

JSPJianshuCard::JSPJianshuCard()
{
    mute = true;
    will_throw = false;
}

bool JSPJianshuCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (targets.length() == 1)
    {
        if (!to_select->inMyAttackRange(targets.first()))
        {
            return false;
        }
    }

    return to_select != Self;
}

bool JSPJianshuCard::targetsFeasible(const QList<const Player *> &targets, const Player *) const
{
    return targets.length() == 2;
}

void JSPJianshuCard::onUse(Room *room, const CardUseStruct &card_use) const
{
    CardUseStruct use = card_use;
    ServerPlayer *player = card_use.from;
    ServerPlayer *target = card_use.to.first();
    QVariant data = QVariant::fromValue(use);
    RoomThread *thread = room->getThread();

    thread->trigger(PreCardUsed, room, player, data);
    use = data.value<CardUseStruct>();

    LogMessage log;
    log.from = card_use.from;
    log.to << card_use.to;
    log.type = "#UseCard";
    log.card_str = toString();
    room->sendLog(log);

    room->removePlayerMark(player, "@jspjianshu");
    player->broadcastSkillInvoke("jspjianshu");
//     room->doSuperLightbox("jsp_jiaxu", "jspjianshu");

    CardMoveReason reason(CardMoveReason::S_REASON_GIVE, player->objectName(), target->objectName(), "jspjianshu", QString());
    room->obtainCard(target, this, reason, true);

    thread->trigger(CardUsed, room, player, data);
    use = data.value<CardUseStruct>();
    thread->trigger(CardFinished, room, player, data);
}

void JSPJianshuCard::use(Room *, ServerPlayer *, QList<ServerPlayer *> &targets) const
{
    ServerPlayer *startor = targets.at(0);
    ServerPlayer *victim = targets.at(1);
    if (!startor->isKongcheng() && !victim->isKongcheng())
    {
        startor->pindian(victim, "jspjianshu", NULL);
    }
}

class JSPJianshuViewAsSkill : public OneCardViewAsSkill
{
public:
    JSPJianshuViewAsSkill() : OneCardViewAsSkill("jspjianshu")
    {
        filter_pattern = ".|black";
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return player->getMark("@jspjianshu") > 0;
    }

    const Card *viewAs(const Card *originalCard) const
    {
        JSPJianshuCard *card = new JSPJianshuCard;
        card->addSubcard(originalCard);
        return card;
    }
};

class JSPJianshu : public TriggerSkill
{
public:
    JSPJianshu() : TriggerSkill("jspjianshu")
    {
        events << Pindian;
        frequency = Limited;
        limit_mark = "@jspjianshu";
        view_as_skill = new JSPJianshuViewAsSkill;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
        PindianStruct *pindian = data.value<PindianStruct *>();
        if (pindian->reason != objectName())
            return false;

        ServerPlayer *winner = pindian->getWinner();
        QList<ServerPlayer *> losers = pindian->getLosers();
        if (winner != NULL)
            room->askForDiscard(winner, objectName(), 2, 2, false, true);
        for (int i = 0; i < losers.length(); i++)
            room->loseHp(losers.at(i));
        return false;
    }
};

class JSPYongdi : public MasochismSkill
{
public:
    JSPYongdi() : MasochismSkill("jspyongdi")
    {
        frequency = Limited;
        limit_mark = "@jspyongdi";
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->hasSkill(this) && target->getMark("@jspyongdi") > 0;
    }

    void onDamaged(ServerPlayer *player, const DamageStruct &) const
    {
        if (!player->askForSkillInvoke(this, QVariant(), false))
            return;

        Room *room = player->getRoom();
        QList<ServerPlayer *> targets;
        foreach(ServerPlayer *p, room->getOtherPlayers(player)) {
            if (p->isMale())
                targets << p;
        }

        if (player->getMark("@jspyongdi") > 0 && targets.length() > 0)
        {
            ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), "@@jspyongdi-invoke", true);
            if (target != NULL)
            {
                room->removePlayerMark(player, "@jspyongdi");
                player->broadcastSkillInvoke(objectName());
//                 room->doSuperLightbox("jsp_jiaxu", objectName());
                LogMessage log;
                log.type = "#GainMaxHp";
                log.from = target;
                log.arg = "1";
                log.arg2 = objectName();
                room->sendLog(log);

                room->setPlayerProperty(target, "maxhp", QVariant(target->getMaxHp() + 1));

                if (!target->isLord())
                {
                    foreach(const Skill *sk, target->getGeneral()->getVisibleSkillList()) {
                        if (sk->isLordSkill() && !target->hasSkill(sk))
                        {
                            room->acquireSkill(target, sk->objectName(), objectName());
                        }
                    }
                    if (target->getGeneral2() != NULL)
                    {
                        foreach(const Skill *sk, target->getGeneral2()->getVisibleSkillList()) {
                            if (sk->isLordSkill() && !target->hasSkill(sk))
                            {
                                room->acquireSkill(target, sk->objectName(), objectName());
                            }
                        }
                    }
                }
            }
        }
    }
};

class Chenqing : public TriggerSkill
{
public:
    Chenqing() : TriggerSkill("chenqing")
    {
        events << Dying << EventPhaseStart;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == Dying) {
            DyingStruct dying = data.value<DyingStruct>();
            if (dying.who != player)
                return false;

            QList<ServerPlayer *> alives = room->getAlivePlayers();
            foreach(ServerPlayer *source, alives)
            {
                if (source->hasSkill(this) && source->getMark("@advise") == 0) {
                    if (doChenqing(room, source, player)) {
                        if (player->getHp() > 0)
                            return false;
                    }
                }
            }
        } else if (triggerEvent == EventPhaseStart) {
            if (player->getPhase() == Player::RoundStart) {
                if (player->getMark("@advise") > 0 && player->hasSkill(this, true) && player->getMark("chenqing") != room->getTurnCount())
                    player->loseAllMarks("@advise");
            }
        }
        return false;
    }

    bool triggerable(const Player *target) const
    {
        if (target)
            return target->isAlive();

        return false;
    }

private:
    bool doChenqing(Room *room, ServerPlayer *source, ServerPlayer *victim) const
    {
        QList<ServerPlayer *> targets = room->getOtherPlayers(source);
        targets.removeOne(victim);
        if (targets.isEmpty())
            return false;

        ServerPlayer *target = room->askForPlayerChosen(source, targets, "chenqing", QString("@chenqing:%1").arg(victim->objectName()), false, true);
        if (target) {
            source->gainMark("@advise");
            room->setPlayerMark(source, "chenqing", room->getTurnCount());
            source->broadcastSkillInvoke("chenqing");

            room->drawCards(target, 4, "chenqing");

            const Card *to_discard = NULL;
            if (target->getCardCount() > 4) {
                to_discard = room->askForExchange(target, "chenqing", 4, 4, true, QString("@chenqing-exchange:%1:%2").arg(source->objectName()).arg(victim->objectName()), false);
            } else {
                DummyCard *dummy = new DummyCard;
                dummy->addSubcards(target->getCards("he"));
                to_discard = dummy;
            }
            QSet<Card::Suit> suit;
            foreach(int id, to_discard->getSubcards())
            {
                const Card *c = Sanguosha->getCard(id);
                if (c == NULL) continue;
                suit.insert(c->getSuit());
            }
            room->throwCard(to_discard, target);
            delete to_discard;

            if (suit.count() == 4 && room->getCurrentDyingPlayer() == victim && target->getMark("Global_PreventPeach") == 0) {
                Card *peach = Sanguosha->cloneCard("peach");
                peach->setSkillName("_chenqing");
                room->useCard(CardUseStruct(peach, target, victim, false), true);
            }

            return true;
        }
        return false;
    }
};

class MoshiViewAsSkill : public OneCardViewAsSkill
{
public:
    MoshiViewAsSkill() : OneCardViewAsSkill("moshi")
    {
        response_or_use = true;
        response_pattern = "@@moshi";
    }

    bool viewFilter(const Card *to_select) const
    {
        if (to_select->isEquipped()) return false;
        QString ori = Self->property("moshi").toString();
        if (ori.isEmpty()) return NULL;
        Card *a = Sanguosha->cloneCard(ori);
        a->addSubcard(to_select);
        return a->isAvailable(Self);
    }

    const Card *viewAs(const Card *originalCard) const
    {
        QString ori = Self->property("moshi").toString();
        if (ori.isEmpty()) return NULL;
        Card *a = Sanguosha->cloneCard(ori);
        a->addSubcard(originalCard);
        a->setSkillName(objectName());
        return a;
    }

    bool isEnabledAtPlay(const Player *) const
    {
        return false;
    }
};

class Moshi : public TriggerSkill
{
public:
    Moshi() : TriggerSkill("moshi")
    {
        view_as_skill = new MoshiViewAsSkill;
        events << EventPhaseStart << CardUsed;
    }
    bool trigger(TriggerEvent e, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (e == CardUsed && player->getPhase() == Player::Play) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card->isKindOf("SkillCard") || use.card->isKindOf("EquipCard")) return false;
            QStringList list = player->tag[objectName()].toStringList();
            if (list.length() == 2) return false;
            list.append(use.card->objectName());
            player->tag[objectName()] = list;
        } else if (e == EventPhaseStart && player->getPhase() == Player::Finish) {
            QStringList list = player->tag[objectName()].toStringList();
            player->tag.remove(objectName());
            if (list.isEmpty()) return false;
            room->setPlayerProperty(player, "moshi", list.first());
            try {
                if (!cardIsAvailable(player, list.first())) {
                    room->setPlayerProperty(player, "moshi", QString());
                    return false;
                }
                const Card *first = room->askForUseCard(player, "@@moshi", QString("@moshi_ask:::%1").arg(list.takeFirst()));
                if (first != NULL && !list.isEmpty() && !(player->isKongcheng() && player->getHandPile().isEmpty())) {
                    room->setPlayerProperty(player, "moshi", list.first());
                    Q_ASSERT(list.length() == 1);
                    if (!cardIsAvailable(player, list.first())) {
                        room->setPlayerProperty(player, "moshi", QString());
                        return false;
                    }
                    room->askForUseCard(player, "@@moshi", QString("@moshi_ask:::%1").arg(list.takeFirst()));
                }
            } catch (TriggerEvent e) {
                if (e == TurnBroken || e == StageChange) {
                    room->setPlayerProperty(player, "moshi", QString());
                }
                throw e;
            }
        }
        return false;
    }

    bool cardIsAvailable(ServerPlayer *player, QString &card_name) const
    {
        QList<const Card *> cards = player->getHandcards();
        for (int i = 0; i < cards.length(); i++) {
            const Card *card = cards.at(i);
            Card *dest_card = Sanguosha->cloneCard(card_name, card->getSuit(), card->getNumber());
            if (dest_card->isAvailable(player))
                return true;
        }
        return false;
    }
};

JSPPackage::JSPPackage()
    : Package("jiexian_sp")
{
    General *jsp_sunshangxiang = new General(this, "jsp_sunshangxiang", "shu", 3, false); // JSP 001
    jsp_sunshangxiang->addSkill(new Liangzhu);
    jsp_sunshangxiang->addSkill(new Fanxiang);
    jsp_sunshangxiang->addRelateSkill("xiaoji");

    General *jsp_machao = new General(this, "jsp_machao", "qun");
    jsp_machao->addSkill(new JSPZhuiji);
    jsp_machao->addSkill(new JSPShichou);

    General *jsp_guanyu = new General(this, "jsp_guanyu", "wei"); // JSP 003
    jsp_guanyu->addSkill("wusheng");
    jsp_guanyu->addSkill(new JspDanqi);
    jsp_guanyu->addRelateSkill("nuzhan");

    General *jsp_jiangwei = new General(this, "jsp_jiangwei", "wei");
    jsp_jiangwei->addSkill(new Kunfen);
    jsp_jiangwei->addSkill(new Fengliang);
    jsp_jiangwei->addRelateSkill("tiaoxin");

    General *jsp_zhaoyun = new General(this, "jsp_zhaoyun", "qun", 3);
    jsp_zhaoyun->addSkill(new ChixinTrigger);
    jsp_zhaoyun->addSkill(new Suiren);
    jsp_zhaoyun->addSkill("yicong");

    General *jsp_huangyy = new General(this, "jsp_huangyueying", "qun", 3, false);
    jsp_huangyy->addSkill(new Jiqiao);
    jsp_huangyy->addSkill(new Linglong);
    jsp_huangyy->addSkill(new LinglongTreasure);
    jsp_huangyy->addSkill(new LinglongMax);
    related_skills.insertMulti("linglong", "#linglong-horse");
    related_skills.insertMulti("linglong", "#linglong-treasure");

    General *jsp_jiaxu = new General(this, "jsp_jiaxu", "wei", 3);
    jsp_jiaxu->addSkill(new JSPZhenlve);
    jsp_jiaxu->addSkill(new JSPZhenlveProhibit);
    related_skills.insertMulti("jspzhenlve", "#jspzhenlve-prohibit");
    jsp_jiaxu->addSkill(new JSPJianshu);
    jsp_jiaxu->addSkill(new JSPYongdi);

    General *jsp_caiwenji = new General(this, "jsp_caiwenji", "wei", 3, false);
    jsp_caiwenji->addSkill(new Chenqing);
    jsp_caiwenji->addSkill(new Moshi);

    skills << new Nuzhan;

    addMetaObject<JiqiaoCard>();
    addMetaObject<JSPShichouCard>();
    addMetaObject<JSPJianshuCard>();
}

ADD_PACKAGE(JSP)
