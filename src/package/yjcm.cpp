#include "yjcm.h"
#include "skill.h"
#include "standard.h"
#include "maneuvering.h"
#include "clientplayer.h"
#include "engine.h"
#include "settings.h"
#include "ai.h"
#include "general.h"
#include "util.h"
#include "wrapped-card.h"
#include "room.h"
#include "roomthread.h"

class Luoying : public TriggerSkill
{
public:
    Luoying() : TriggerSkill("luoying")
    {
        events << CardsMoveOneTime;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *caozhi, QVariant &data) const
    {
        CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
        if (move.from == caozhi || move.from == NULL)
            return false;
        if (move.to_place == Player::DiscardPile
            && ((move.reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_DISCARD
            || move.reason.m_reason == CardMoveReason::S_REASON_JUDGEDONE)) {
            QList<int> card_ids;
            int i = 0;
            foreach(int card_id, move.card_ids) {
                if (Sanguosha->getCard(card_id)->getSuit() == Card::Club) {
                    if (move.reason.m_reason == CardMoveReason::S_REASON_JUDGEDONE
                        && move.from_places[i] == Player::PlaceJudge)
                        card_ids << card_id;
                    else if (move.reason.m_reason != CardMoveReason::S_REASON_JUDGEDONE
                        && (move.from_places[i] == Player::PlaceHand || move.from_places[i] == Player::PlaceEquip))
                        card_ids << card_id;
                }
                i++;
            }
            if (card_ids.isEmpty())
                return false;
            else if (caozhi->askForSkillInvoke(this, data)) {
                caozhi->broadcastSkillInvoke(objectName());
                while (card_ids.length() > 1) {
                    room->fillAG(card_ids, caozhi);
                    int id = room->askForAG(caozhi, card_ids, true, objectName());
                    if (id == -1) {
                        room->clearAG(caozhi);
                        break;
                    }
                    card_ids.removeOne(id);
                    room->clearAG(caozhi);
                }

                if (!card_ids.isEmpty()) {
                    DummyCard *dummy = new DummyCard(card_ids);
                    caozhi->obtainCard(dummy);
                    delete dummy;
                }
            }
        }
        return false;
    }
};

class Jiushi : public ZeroCardViewAsSkill
{
public:
    Jiushi() : ZeroCardViewAsSkill("jiushi")
    {
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return Analeptic::IsAvailable(player) && player->faceUp();
    }

    bool isEnabledAtResponse(const Player *player, const QString &pattern) const
    {
        return pattern.contains("analeptic") && player->faceUp();
    }

    const Card *viewAs() const
    {
        Analeptic *analeptic = new Analeptic(Card::NoSuit, 0);
        analeptic->setSkillName(objectName());
        return analeptic;
    }
};

class JiushiFlip : public TriggerSkill
{
public:
    JiushiFlip() : TriggerSkill("#jiushi-flip")
    {
        events << PreCardUsed << PreDamageDone << DamageComplete;
    }

    bool trigger(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == PreCardUsed) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card->getSkillName() == "jiushi")
                player->turnOver();
        } else if (triggerEvent == PreDamageDone) {
            player->tag["PredamagedFace"] = !player->faceUp();
        } else if (triggerEvent == DamageComplete) {
            bool facedown = player->tag.value("PredamagedFace").toBool();
            player->tag.remove("PredamagedFace");
            if (facedown && !player->faceUp() && player->askForSkillInvoke("jiushi", data)) {
                player->broadcastSkillInvoke("jiushi");
                player->turnOver();
            }
        }

        return false;
    }
};

class Wuyan : public TriggerSkill
{
public:
    Wuyan() : TriggerSkill("wuyan")
    {
        events << DamageCaused << DamageInflicted;
        frequency = Compulsory;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.card && damage.card->getTypeId() == Card::TypeTrick) {
            if (triggerEvent == DamageInflicted && TriggerSkill::triggerable(player)) {
                LogMessage log;
                log.type = "#WuyanGood";
                log.from = player;
                log.arg = damage.card->objectName();
                log.arg2 = objectName();
                room->sendLog(log);
                room->notifySkillInvoked(player, objectName());
                player->broadcastSkillInvoke(objectName(), 2);

                return true;
            } else if (triggerEvent == DamageCaused && damage.from && TriggerSkill::triggerable(damage.from)) {
                LogMessage log;
                log.type = "#WuyanBad";
                log.from = player;
                log.arg = damage.card->objectName();
                log.arg2 = objectName();
                room->sendLog(log);
                room->notifySkillInvoked(player, objectName());
                player->broadcastSkillInvoke(objectName(), 1);

                return true;
            }
        }

        return false;
    }
};

JujianCard::JujianCard()
{
    
}

bool JujianCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select != Self;
}

void JujianCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();

    QStringList choicelist;
    choicelist << "draw";
    if (effect.to->isWounded())
        choicelist << "recover";
    if (!effect.to->faceUp() || effect.to->isChained())
        choicelist << "reset";
    QString choice = room->askForChoice(effect.to, "jujian", choicelist.join("+"));

    if (choice == "draw")
        effect.to->drawCards(2, "jujian");
    else if (choice == "recover")
        room->recover(effect.to, RecoverStruct(effect.from));
    else if (choice == "reset") {
        if (effect.to->isChained())
            room->setPlayerProperty(effect.to, "chained", false);
        if (!effect.to->faceUp())
            effect.to->turnOver();
    }
}

class JujianViewAsSkill : public OneCardViewAsSkill
{
public:
    JujianViewAsSkill() : OneCardViewAsSkill("jujian")
    {
        filter_pattern = "^BasicCard!";
        response_pattern = "@@jujian";
    }

    const Card *viewAs(const Card *originalCard) const
    {
        JujianCard *jujianCard = new JujianCard;
        jujianCard->addSubcard(originalCard);
        return jujianCard;
    }
};

class Jujian : public PhaseChangeSkill
{
public:
    Jujian() : PhaseChangeSkill("jujian")
    {
        view_as_skill = new JujianViewAsSkill;
    }

    bool onPhaseChange(ServerPlayer *xushu) const
    {
        Room *room = xushu->getRoom();
        if (xushu->getPhase() == Player::Finish && xushu->canDiscard(xushu, "he"))
            room->askForUseCard(xushu, "@@jujian", "@jujian-card", -1, Card::MethodDiscard);
        return false;
    }
};

class Enyuan : public TriggerSkill
{
public:
    Enyuan() : TriggerSkill("enyuan")
    {
        events << CardsMoveOneTime << Damaged;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == CardsMoveOneTime) {
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (move.to == player && move.from && move.from->isAlive() && move.from != move.to && move.card_ids.size() >= 2 && move.reason.m_reason != CardMoveReason::S_REASON_PREVIEWGIVE
                    && (move.to_place == Player::PlaceHand || move.to_place == Player::PlaceEquip)) {
                move.from->setFlags("EnyuanDrawTarget");
                bool invoke = room->askForSkillInvoke(player, objectName(), data);
                move.from->setFlags("-EnyuanDrawTarget");
                if (invoke) {
                    room->drawCards((ServerPlayer *)move.from, 1, objectName());
                    player->broadcastSkillInvoke(objectName(), 1);
                }
            }
        } else if (triggerEvent == Damaged) {
            DamageStruct damage = data.value<DamageStruct>();
            ServerPlayer *source = damage.from;
            if (!source || source == player) return false;
            int x = damage.damage;
            for (int i = 0; i < x; i++) {
                if (source->isAlive() && player->isAlive() && room->askForSkillInvoke(player, objectName(), data)) {
                    player->broadcastSkillInvoke(objectName(), 2);
                    room->doIndicate(player, source);
                    const Card *card = NULL;
                    if (!source->isKongcheng())
                        card = room->askForExchange(source, objectName(), 1, 1, false, "EnyuanGive::" + player->objectName(), true);
                    if (card) {
                        CardMoveReason reason(CardMoveReason::S_REASON_GIVE, source->objectName(),
                            player->objectName(), objectName(), QString());
                        reason.m_playerId = player->objectName();
                        room->moveCardTo(card, source, player, Player::PlaceHand, reason);
                        delete card;
                    } else {
                        room->loseHp(source);
                    }
                } else {
                    break;
                }
            }
        }
        return false;
    }
};

class Xuanhuo : public PhaseChangeSkill
{
public:
    Xuanhuo() : PhaseChangeSkill("xuanhuo")
    {
    }

    bool onPhaseChange(ServerPlayer *fazheng) const
    {
        Room *room = fazheng->getRoom();
        if (fazheng->getPhase() == Player::Draw) {
            ServerPlayer *to = room->askForPlayerChosen(fazheng, room->getOtherPlayers(fazheng), objectName(), "xuanhuo-invoke", true, true);
            if (to) {
                fazheng->broadcastSkillInvoke(objectName());
                room->drawCards(to, 2, objectName());
                if (!fazheng->isAlive() || !to->isAlive())
                    return true;

                QList<ServerPlayer *> targets;
                foreach (ServerPlayer *vic, room->getOtherPlayers(to)) {
                    if (to->canSlash(vic))
                        targets << vic;
                }
                ServerPlayer *victim = NULL;
                if (!targets.isEmpty()) {
                    victim = room->askForPlayerChosen(fazheng, targets, "xuanhuo_slash", "@dummy-slash2:" + to->objectName());
                    room->doIndicate(to, victim);

                    LogMessage log;
                    log.type = "#CollateralSlash";
                    log.from = fazheng;
                    log.to << victim;
                    room->sendLog(log);
                }

                if (victim == NULL || !room->askForUseSlashTo(to, victim, QString("xuanhuo-slash:%1:%2").arg(fazheng->objectName()).arg(victim->objectName()))) {
                    if (to->isNude())
                        return true;
                    room->setPlayerFlag(to, "xuanhuo_InTempMoving");
                    int first_id = room->askForCardChosen(fazheng, to, "he", "xuanhuo");
                    Player::Place original_place = room->getCardPlace(first_id);
                    DummyCard *dummy = new DummyCard;
                    dummy->addSubcard(first_id);
                    to->addToPile("#xuanhuo", dummy, false);
                    if (!to->isNude()) {
                        int second_id = room->askForCardChosen(fazheng, to, "he", "xuanhuo");
                        dummy->addSubcard(second_id);
                    }

                    //move the first card back temporarily
                    room->moveCardTo(Sanguosha->getCard(first_id), to, original_place, false);
                    room->setPlayerFlag(to, "-xuanhuo_InTempMoving");
                    room->moveCardTo(dummy, fazheng, Player::PlaceHand, false);
                    delete dummy;
                }

                return true;
            }
        }

        return false;
    }
};

XuanfengCard::XuanfengCard()
{
    
}

bool XuanfengCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && Self != to_select && Self->canDiscard(to_select, "he");
}

void XuanfengCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    ServerPlayer *target = targets.first();
    int id = room->askForCardChosen(source, target, "he", "xuanfeng", false, Card::MethodDiscard);
    room->throwCard(id, target, source);
}

class XuanfengVS : public ZeroCardViewAsSkill
{
public:
    XuanfengVS() : ZeroCardViewAsSkill("xuanfeng")
    {

    }

    bool isEnabledAtPlay(const Player *) const
    {
        return false;
    }

    bool isEnabledAtResponse(const Player *, const QString &pattern) const
    {
        return pattern == "@@xuanfeng";
    }

    const Card *viewAs() const
    {
        return new XuanfengCard;
    }
};

class Xuanfeng : public TriggerSkill
{
public:
    Xuanfeng() : TriggerSkill("xuanfeng")
    {
        events << CardsMoveOneTime << EventPhaseEnd << EventPhaseChanging;
        view_as_skill = new XuanfengVS;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    void perform(Room *room, ServerPlayer *lingtong) const
    {
        if (lingtong->askForSkillInvoke(this, QVariant(), false)) {
            room->setPlayerFlag(lingtong, "first_xuanfeng");
            if (room->askForUseCard(lingtong, "@@xuanfeng", "@@xuanfeng-discard"))
            {
                room->setPlayerFlag(lingtong, "-first_xuanfeng");
                room->askForUseCard(lingtong, "@@xuanfeng", "@@xuanfeng-discard");
            }
        }
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *lingtong, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging) {
            lingtong->setMark("xuanfeng", 0);
        } else if (triggerEvent == CardsMoveOneTime) {
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (move.from != lingtong)
                return false;

            if (lingtong->getPhase() == Player::Discard
                && (move.reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_DISCARD)
                lingtong->addMark("xuanfeng", move.card_ids.length());

            if (move.from_places.contains(Player::PlaceEquip) && TriggerSkill::triggerable(lingtong))
                perform(room, lingtong);
        } else if (triggerEvent == EventPhaseEnd && TriggerSkill::triggerable(lingtong)
            && lingtong->getPhase() == Player::Discard && lingtong->getMark("xuanfeng") >= 2) {
            perform(room, lingtong);
        }

        return false;
    }

    int getEffectIndex(const ServerPlayer *player, const Card *) const
    {
        if (player->hasFlag("first_xuanfeng"))
            return -1;
        return -2;
    }
};

XianzhenCard::XianzhenCard()
{
}

bool XianzhenCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select != Self && !to_select->isKongcheng();
}

void XianzhenCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();
    if (effect.from->pindian(effect.to, "xianzhen", NULL)) {
        ServerPlayer *target = effect.to;
        effect.from->tag["XianzhenTarget"] = QVariant::fromValue(target);
        room->setPlayerFlag(effect.from, "XianzhenSuccess");

        QStringList assignee_list = effect.from->property("extra_slash_specific_assignee").toString().split("+");
        assignee_list << target->objectName();
        room->setPlayerProperty(effect.from, "extra_slash_specific_assignee", assignee_list.join("+"));

        room->setFixedDistance(effect.from, effect.to, 1);
        room->addPlayerMark(effect.to, "Armor_Nullified");
    } else {
        room->setPlayerCardLimitation(effect.from, "use", "Slash", true);
    }
}

class XianzhenViewAsSkill : public ZeroCardViewAsSkill
{
public:
    XianzhenViewAsSkill() : ZeroCardViewAsSkill("xianzhen")
    {
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("XianzhenCard") && !player->isKongcheng();
    }

    const Card *viewAs() const
    {
        return new XianzhenCard;
    }
};

class Xianzhen : public TriggerSkill
{
public:
    Xianzhen() : TriggerSkill("xianzhen")
    {
        events << EventPhaseChanging << Death;
        view_as_skill = new XianzhenViewAsSkill;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->tag["XianzhenTarget"].value<ServerPlayer *>() != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *gaoshun, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging) {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.to != Player::NotActive)
                return false;
        }
        ServerPlayer *target = gaoshun->tag["XianzhenTarget"].value<ServerPlayer *>();
        if (triggerEvent == Death) {
            DeathStruct death = data.value<DeathStruct>();
            if (death.who != gaoshun) {
                if (death.who == target) {
                    room->removeFixedDistance(gaoshun, target, 1);
                    gaoshun->tag.remove("XianzhenTarget");
                    room->setPlayerFlag(gaoshun, "-XianzhenSuccess");
                }
                return false;
            }
        }
        if (target) {
            QStringList assignee_list = gaoshun->property("extra_slash_specific_assignee").toString().split("+");
            assignee_list.removeOne(target->objectName());
            room->setPlayerProperty(gaoshun, "extra_slash_specific_assignee", assignee_list.join("+"));

            room->removeFixedDistance(gaoshun, target, 1);
            gaoshun->tag.remove("XianzhenTarget");
            room->removePlayerMark(target, "Armor_Nullified");
        }
        return false;
    }
};

class Jinjiu : public FilterSkill
{
public:
    Jinjiu() : FilterSkill("jinjiu")
    {
    }

    bool viewFilter(const Card *to_select) const
    {
        return to_select->objectName() == "analeptic";
    }

    const Card *viewAs(const Card *originalCard) const
    {
        Slash *slash = new Slash(originalCard->getSuit(), originalCard->getNumber());
        slash->setSkillName(objectName());
        WrappedCard *card = Sanguosha->getWrappedCard(originalCard->getId());
        card->takeOver(slash);
        return card;
    }
};

MingceCard::MingceCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
}

void MingceCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.to->getRoom();
    QList<ServerPlayer *> targets;
    if (Slash::IsAvailable(effect.to)) {
        foreach (ServerPlayer *p, room->getOtherPlayers(effect.to)) {
            if (effect.to->canSlash(p))
                targets << p;
        }
    }

    ServerPlayer *target = NULL;
    QStringList choicelist;
    choicelist << "draw";
    if (!targets.isEmpty() && effect.from->isAlive()) {
        target = room->askForPlayerChosen(effect.from, targets, "mingce", "@dummy-slash2:" + effect.to->objectName());
        target->setFlags("MingceTarget"); // For AI
        room->doIndicate(effect.to, target);

        LogMessage log;
        log.type = "#CollateralSlash";
        log.from = effect.from;
        log.to << target;
        room->sendLog(log);

        choicelist << "use";
    }

    try {
        CardMoveReason reason(CardMoveReason::S_REASON_GIVE, effect.from->objectName(), effect.to->objectName(), "mingce", QString());
        room->obtainCard(effect.to, this, reason);
    }
    catch (TriggerEvent triggerEvent) {
        if (triggerEvent == TurnBroken || triggerEvent == StageChange)
            if (target && target->hasFlag("MingceTarget")) target->setFlags("-MingceTarget");
        throw triggerEvent;
    }

    QString choice = room->askForChoice(effect.to, "mingce", choicelist.join("+"));
    if (target && target->hasFlag("MingceTarget")) target->setFlags("-MingceTarget");

    if (choice == "use") {
        if (effect.to->canSlash(target, NULL, false)) {
            Slash *slash = new Slash(Card::NoSuit, 0);
            slash->setSkillName("_mingce");
            room->useCard(CardUseStruct(slash, effect.to, target));
        }
    } else if (choice == "draw") {
        effect.to->drawCards(1, "mingce");
    }
}

class Mingce : public OneCardViewAsSkill
{
public:
    Mingce() : OneCardViewAsSkill("mingce")
    {
        filter_pattern = "EquipCard,Slash";
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("MingceCard");
    }

    const Card *viewAs(const Card *originalCard) const
    {
        MingceCard *mingceCard = new MingceCard;
        mingceCard->addSubcard(originalCard);

        return mingceCard;
    }

    int getEffectIndex(const ServerPlayer *, const Card *card) const
    {
        if (card->isKindOf("Slash"))
            return -2;
        else
            return -1;
    }
};

class Zhichi : public TriggerSkill
{
public:
    Zhichi() : TriggerSkill("zhichi")
    {
        events << Damaged;
        frequency = Compulsory;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (player->getPhase() != Player::NotActive)
            return false;

        ServerPlayer *current = room->getCurrent();
        if (current && current->isAlive() && current->getPhase() != Player::NotActive) {
            player->broadcastSkillInvoke(objectName(), 1);
            room->notifySkillInvoked(player, objectName());
            if (player->getMark("@late") == 0)
                room->addPlayerMark(player, "@late");

            LogMessage log;
            log.type = "#ZhichiDamaged";
            log.from = player;
            room->sendLog(log);
        }

        return false;
    }
};

class ZhichiProtect : public TriggerSkill
{
public:
    ZhichiProtect() : TriggerSkill("#zhichi-protect")
    {
        events << CardEffected;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
        CardEffectStruct effect = data.value<CardEffectStruct>();
        if ((effect.card->isKindOf("Slash") || effect.card->isNDTrick()) && effect.to->getMark("@late") > 0) {
            effect.to->broadcastSkillInvoke("zhichi", 2);
            room->notifySkillInvoked(effect.to, "zhichi");
            LogMessage log;
            log.type = "#ZhichiAvoid";
            log.from = effect.to;
            log.arg = "zhichi";
            room->sendLog(log);

            return true;
        }
        return false;
    }
};

class ZhichiClear : public TriggerSkill
{
public:
    ZhichiClear() : TriggerSkill("#zhichi-clear")
    {
        events << EventPhaseChanging << Death;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging) {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.to != Player::NotActive)
                return false;
        } else {
            DeathStruct death = data.value<DeathStruct>();
            if (death.who != player || player != room->getCurrent())
                return false;
        }

        foreach (ServerPlayer *p, room->getAllPlayers()) {
            if (p->getMark("@late") > 0)
                room->setPlayerMark(p, "@late", 0);
        }

        return false;
    }
};

GanluCard::GanluCard()
{
}

void GanluCard::swapEquip(ServerPlayer *first, ServerPlayer *second) const
{
    Room *room = first->getRoom();

    QList<int> equips1, equips2;
    foreach(const Card *equip, first->getEquips())
        equips1.append(equip->getId());
    foreach(const Card *equip, second->getEquips())
        equips2.append(equip->getId());

    QList<CardsMoveStruct> exchangeMove;
    CardsMoveStruct move1(equips1, second, Player::PlaceEquip,
        CardMoveReason(CardMoveReason::S_REASON_SWAP, first->objectName(), second->objectName(), "ganlu", QString()));
    CardsMoveStruct move2(equips2, first, Player::PlaceEquip,
        CardMoveReason(CardMoveReason::S_REASON_SWAP, second->objectName(), first->objectName(), "ganlu", QString()));
    exchangeMove.push_back(move2);
    exchangeMove.push_back(move1);
    room->moveCardsAtomic(exchangeMove, false);
}

bool GanluCard::targetsFeasible(const QList<const Player *> &targets, const Player *) const
{
    return targets.length() == 2;
}

bool GanluCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    switch (targets.length()) {
    case 0: return true;
    case 1: {
        int n1 = targets.first()->getEquips().length();
        int n2 = to_select->getEquips().length();
        return qAbs(n1 - n2) <= Self->getLostHp();
    }
    default:
        return false;
    }
}

void GanluCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    LogMessage log;
    log.type = "#GanluSwap";
    log.from = source;
    log.to = targets;
    room->sendLog(log);

    swapEquip(targets.first(), targets[1]);
}

class Ganlu : public ZeroCardViewAsSkill
{
public:
    Ganlu() : ZeroCardViewAsSkill("ganlu")
    {
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("GanluCard");
    }

    const Card *viewAs() const
    {
        return new GanluCard;
    }
};

class Buyi : public TriggerSkill
{
public:
    Buyi() : TriggerSkill("buyi")
    {
        events << Dying;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *wuguotai, QVariant &data) const
    {
        DyingStruct dying = data.value<DyingStruct>();
        ServerPlayer *player = dying.who;
        if (player->isKongcheng()) return false;
        if (player->getHp() < 1 && wuguotai->askForSkillInvoke(this, data)) {
            wuguotai->broadcastSkillInvoke(objectName());
            room->doIndicate(wuguotai, player);
            const Card *card = NULL;
            if (player == wuguotai)
                card = room->askForCardShow(player, wuguotai, objectName());
            else {
                int card_id = room->askForCardChosen(wuguotai, player, "h", "buyi");
                card = Sanguosha->getCard(card_id);
            }

            room->showCard(player, card->getEffectiveId());

            if (card->getTypeId() != Card::TypeBasic) {
                if (!player->isJilei(card))
                    room->throwCard(card, player);

                room->recover(player, RecoverStruct(wuguotai));
            }
        }
        return false;
    }
};

class Jueqing : public TriggerSkill
{
public:
    Jueqing() : TriggerSkill("jueqing")
    {
        frequency = Compulsory;
        events << Predamage;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *zhangchunhua, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.from == zhangchunhua) {
            zhangchunhua->broadcastSkillInvoke(objectName());
            room->sendCompulsoryTriggerLog(zhangchunhua, objectName());
            room->loseHp(damage.to, damage.damage);

            return true;
        }
        return false;
    }
};

Shangshi::Shangshi() : TriggerSkill("shangshi")
{
    events << HpChanged << MaxHpChanged << CardsMoveOneTime;
}

int Shangshi::getMaxLostHp(ServerPlayer *zhangchunhua) const
{
    int losthp = zhangchunhua->getLostHp();
    return qMin(losthp, zhangchunhua->getMaxHp());
}

bool Shangshi::trigger(TriggerEvent triggerEvent, Room *, ServerPlayer *zhangchunhua, QVariant &data) const
{
    int losthp = getMaxLostHp(zhangchunhua);
    if (triggerEvent == CardsMoveOneTime) {
        bool can_invoke = false;
        CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
        if (move.from == zhangchunhua && move.from_places.contains(Player::PlaceHand))
            can_invoke = true;
        if (move.to == zhangchunhua && move.to_place == Player::PlaceHand)
            can_invoke = true;
        if (!can_invoke)
            return false;
    } else if (triggerEvent == HpChanged || triggerEvent == MaxHpChanged) {
        if (zhangchunhua->getPhase() == Player::Discard) {
            zhangchunhua->addMark("shangshi");
            return false;
        }
    }

    if (zhangchunhua->getHandcardNum() < losthp && zhangchunhua->askForSkillInvoke(this)) {
        zhangchunhua->broadcastSkillInvoke(objectName());
        zhangchunhua->drawCards(losthp - zhangchunhua->getHandcardNum(), objectName());
    }

    return false;
}

SanyaoCard::SanyaoCard()
{
}

bool SanyaoCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (!targets.isEmpty()) return false;
    QList<const Player *> players = Self->getAliveSiblings();
    players << Self;
    int max = -1000;
    foreach(const Player *p, players) {
        if (max < p->getHp())
            max = p->getHp();
    }
    return to_select->getHp() == max;
}

void SanyaoCard::onEffect(const CardEffectStruct &effect) const
{
    effect.from->getRoom()->damage(DamageStruct("sanyao", effect.from, effect.to));
}

class Sanyao : public OneCardViewAsSkill
{
public:
    Sanyao() : OneCardViewAsSkill("sanyao")
    {
        filter_pattern = ".!";
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return player->canDiscard(player, "he") && !player->hasUsed("SanyaoCard");
    }

    const Card *viewAs(const Card *originalcard) const
    {
        SanyaoCard *first = new SanyaoCard;
        first->addSubcard(originalcard->getId());
        first->setSkillName(objectName());
        return first;
    }
};

class Zhiman : public TriggerSkill
{
public:
    Zhiman() : TriggerSkill("zhiman")
    {
        events << DamageCaused;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();

        if (player->askForSkillInvoke(this, QVariant::fromValue(damage.to))) {
            player->broadcastSkillInvoke(objectName());
            room->doIndicate(player, damage.to);
            LogMessage log;
            log.type = "#Yishi";
            log.from = player;
            log.arg = objectName();
            log.to << damage.to;
            room->sendLog(log);

            if (damage.to->getEquips().isEmpty() && damage.to->getJudgingArea().isEmpty())
                return true;
            int card_id = room->askForCardChosen(player, damage.to, "ej", objectName());
            CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, player->objectName());
            room->obtainCard(player, Sanguosha->getCard(card_id), reason);
            return true;
        }
        return false;
    }
};



JieyueCard::JieyueCard()
{

}

void JieyueCard::onEffect(const CardEffectStruct &effect) const
{
    if (!effect.to->isNude()) {
        Room *room = effect.to->getRoom();
        const Card *card = room->askForExchange(effect.to, "jieyue", 1, 1, true, QString("@jieyue_put:%1").arg(effect.from->objectName()), true);

        if (card != NULL)
            effect.from->addToPile("jieyue_pile", card);
        else if (effect.from->canDiscard(effect.to, "he")) {
            int id = room->askForCardChosen(effect.from, effect.to, "he", objectName(), false, Card::MethodDiscard);
            room->throwCard(id, effect.to, effect.from);
        }
    }
}

class JieyueVS : public OneCardViewAsSkill
{
public:
    JieyueVS() : OneCardViewAsSkill("jieyue")
    {
    }

    bool isResponseOrUse() const
    {
        return !Self->getPile("jieyue_pile").isEmpty();
    }

    bool viewFilter(const QList<const Card *> &, const Card *to_select) const
    {
        if (to_select->isEquipped())
            return false;
        QString pattern = Sanguosha->getCurrentCardUsePattern();
        if (pattern == "@@jieyue") {
            return !Self->isJilei(to_select);
        }

        if (pattern == "jink")
            return to_select->isRed();
        else if (pattern == "nullification")
            return to_select->isBlack();
        return false;
    }

    bool isEnabledAtPlay(const Player *) const
    {
        return false;
    }

    bool isEnabledAtResponse(const Player *player, const QString &pattern) const
    {
        return (!player->getPile("jieyue_pile").isEmpty() && (pattern == "jink" || pattern == "nullification")) || (pattern == "@@jieyue" && !player->isKongcheng());
    }

    bool isEnabledAtNullification(const ServerPlayer *player) const
    {
        if (!player->getPile("jieyue_pile").isEmpty()) {
            foreach(const Card *card, player->getHandcards() + player->getEquips()) {
                if (card->isBlack())
                    return true;
            }

            foreach(int id, player->getHandPile())  {
                if (Sanguosha->getCard(id)->isBlack())
                    return true;
            }
        }

        return false;
    }

    const Card *viewAs(const Card *card) const
    {
        QString pattern = Sanguosha->currentRoomState()->getCurrentCardUsePattern();
        if (pattern == "@@jieyue") {
            JieyueCard *jy = new JieyueCard;
            jy->addSubcard(card);
            return jy;
        }

        if (card->isRed()) {
            Jink *jink = new Jink(Card::SuitToBeDecided, 0);
            jink->addSubcard(card);
            jink->setSkillName(objectName());
            return jink;
        }
        else if (card->isBlack()) {
            Nullification *nulli = new Nullification(Card::SuitToBeDecided, 0);
            nulli->addSubcard(card);
            nulli->setSkillName(objectName());
            return nulli;
        }
        return NULL;
    }
};

class Jieyue : public TriggerSkill
{
public:
    Jieyue() : TriggerSkill("jieyue")
    {
        events << EventPhaseStart;
        view_as_skill = new JieyueVS;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (player->getPhase() == Player::Start && !player->getPile("jieyue_pile").isEmpty()) {
            LogMessage log;
            log.type = "#TriggerSkill";
            log.from = player;
            log.arg = objectName();
            room->sendLog(log);
            DummyCard *dummy = new DummyCard(player->getPile("jieyue_pile"));
            player->obtainCard(dummy);
            delete dummy;
        }
        else if (player->getPhase() == Player::Finish) {
            room->askForUseCard(player, "@@jieyue", "@jieyue", -1, Card::MethodDiscard, false);
        }
        return false;
    }
};

class Pojun : public TriggerSkill
{
public:
    Pojun() : TriggerSkill("pojun")
    {
        events << TargetSpecified << EventPhaseStart << Death;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == TargetSpecified) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card != NULL && use.card->isKindOf("Slash") && TriggerSkill::triggerable(player) && player->getPhase() == Player::Play) {
                foreach(ServerPlayer *t, use.to) {
                    int n = qMin(t->getCards("he").length(), t->getHp());
                    if (n > 0 && player->askForSkillInvoke(this, QVariant::fromValue(t))) {
                        player->broadcastSkillInvoke(objectName());
                        room->doIndicate(player, t);
                        QStringList dis_num;
                        for (int i = 1; i <= n; ++i)
                            dis_num << QString::number(i);

                        int ad = Config.AIDelay;
                        Config.AIDelay = 0;

                        bool ok = false;
                        int discard_n = room->askForChoice(player, objectName() + "_num", dis_num.join("+"), QVariant::fromValue(t)).toInt(&ok);
                        if (!ok || discard_n == 0) {
                            Config.AIDelay = ad;
                            continue;
                        }

                        QList<Player::Place> orig_places;
                        QList<int> cards;
                        // fake move skill needed!!!
                        t->setFlags("pojun_InTempMoving");

                        for (int i = 0; i < discard_n; ++i) {
                            int id = room->askForCardChosen(player, t, "he", objectName() + "_dis", false, Card::MethodNone);
                            Player::Place place = room->getCardPlace(id);
                            orig_places << place;
                            cards << id;
                            t->addToPile("#pojun", id, false);
                        }

                        for (int i = 0; i < discard_n; ++i)
                            room->moveCardTo(Sanguosha->getCard(cards.value(i)), t, orig_places.value(i), false);

                        t->setFlags("-pojun_InTempMoving");
                        Config.AIDelay = ad;

                        DummyCard dummy(cards);
                        t->addToPile("pojun", &dummy, false, QList<ServerPlayer *>() << t);

                        // for record
                        if (!t->tag.contains("pojun") || !t->tag.value("pojun").canConvert(QVariant::Map))
                            t->tag["pojun"] = QVariantMap();

                        QVariantMap vm = t->tag["pojun"].toMap();
                        foreach(int id, cards)
                            vm[QString::number(id)] = player->objectName();

                        t->tag["pojun"] = vm;
                    }
                }
            }
        }
        else if (cardGoBack(triggerEvent, player, data)) {
            foreach(ServerPlayer *p, room->getAllPlayers()) {
                if (p->tag.contains("pojun")) {
                    QVariantMap vm = p->tag.value("pojun", QVariantMap()).toMap();
                    if (vm.values().contains(player->objectName())) {
                        QList<int> to_obtain;
                        foreach(const QString &key, vm.keys()) {
                            if (vm.value(key) == player->objectName())
                                to_obtain << key.toInt();
                        }

                        DummyCard dummy(to_obtain);
                        room->obtainCard(p, &dummy, false);

                        foreach(int id, to_obtain)
                            vm.remove(QString::number(id));

                        p->tag["pojun"] = vm;
                    }
                }
            }
        }

        return false;
    }

private:
    static bool cardGoBack(TriggerEvent triggerEvent, ServerPlayer *player, const QVariant &data)
    {
        if (triggerEvent == EventPhaseStart)
            return player->getPhase() == Player::Finish;
        else if (triggerEvent == Death) {
            DeathStruct death = data.value<DeathStruct>();
            return death.who == player;
        }

        return false;
    }
};

YJCMPackage::YJCMPackage()
    : Package("YJCM")
{
    General *caozhi = new General(this, "caozhi", "wei", 3); // YJ 001
    caozhi->addSkill(new Luoying);
    caozhi->addSkill(new Jiushi);
    caozhi->addSkill(new JiushiFlip);
    related_skills.insertMulti("jiushi", "#jiushi-flip");

    General *chengong = new General(this, "chengong", "qun", 3); // YJ 002
    chengong->addSkill(new Mingce);
    chengong->addSkill(new Zhichi);
    chengong->addSkill(new ZhichiProtect);
    chengong->addSkill(new ZhichiClear);
    related_skills.insertMulti("zhichi", "#zhichi-protect");
    related_skills.insertMulti("zhichi", "#zhichi-clear");

    General *fazheng = new General(this, "fazheng", "shu", 3); // YJ 003
    fazheng->addSkill(new Enyuan);
    fazheng->addSkill(new Xuanhuo);
    fazheng->addSkill(new FakeMoveSkill("xuanhuo"));
    related_skills.insertMulti("xuanhuo", "#xuanhuo-fake-move");

    General *gaoshun = new General(this, "gaoshun", "qun"); // YJ 004
    gaoshun->addSkill(new Xianzhen);
    gaoshun->addSkill(new Jinjiu);

    General *lingtong = new General(this, "lingtong", "wu"); // YJ 005
    lingtong->addSkill(new Xuanfeng);

    General *masu = new General(this, "masu", "shu", 3); // YJ 006
    masu->addSkill(new Sanyao);
    masu->addSkill(new Zhiman);

    General *wuguotai = new General(this, "wuguotai", "wu", 3, false); // YJ 007
    wuguotai->addSkill(new Ganlu);
    wuguotai->addSkill(new Buyi);

    General *xusheng = new General(this, "xusheng", "wu");
    xusheng->addSkill(new Pojun);
    xusheng->addSkill(new FakeMoveSkill("pojun"));
    related_skills.insertMulti("pojun", "#pojun-fake-move");

    General *xushu = new General(this, "xushu", "shu", 3); // YJ 009
    xushu->addSkill(new Wuyan);
    xushu->addSkill(new Jujian);

    General *yujin = new General(this, "yujin", "wei"); // YJ 010
    yujin->addSkill(new Jieyue);

    General *zhangchunhua = new General(this, "zhangchunhua", "wei", 3, false); // YJ 011
    zhangchunhua->addSkill(new Jueqing);
    zhangchunhua->addSkill(new Shangshi);

    addMetaObject<MingceCard>();
    addMetaObject<GanluCard>();
    addMetaObject<XianzhenCard>();
    addMetaObject<SanyaoCard>();
    addMetaObject<JieyueCard>();
    addMetaObject<JujianCard>();
    addMetaObject<XuanfengCard>();
}

ADD_PACKAGE(YJCM)

