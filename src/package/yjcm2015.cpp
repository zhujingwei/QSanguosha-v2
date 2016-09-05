#include "yjcm2015.h"
#include "general.h"
#include "player.h"
#include "structs.h"
#include "room.h"
#include "skill.h"
#include "standard.h"
#include "engine.h"
#include "clientplayer.h"
#include "clientstruct.h"
#include "settings.h"
#include "wrapped-card.h"
#include "roomthread.h"
#include "standard-equips.h"
#include "standard-skillcards.h"
#include "json.h"

class Huituo : public MasochismSkill
{
public:
    Huituo() : MasochismSkill("huituo")
    {

    }

    void onDamaged(ServerPlayer *target, const DamageStruct &damage) const
    {
        Room *room = target->getRoom();
        JudgeStruct j;

        j.who = room->askForPlayerChosen(target, room->getAlivePlayers(), objectName(), "@huituo-select", true, true);
        if (j.who == NULL)
            return;

        room->broadcastSkillInvoke(objectName());
        j.pattern = ".";
        j.play_animation = false;
        j.reason = "huituo";
        room->judge(j);

        if (j.pattern == "red")
            room->recover(j.who, RecoverStruct(target));
        else if (j.pattern == "black")
            room->drawCards(j.who, damage.damage, objectName());
    }
};

class HuituoJudge : public TriggerSkill
{
public:
    HuituoJudge() : TriggerSkill("#huituo")
    {
        events << FinishJudge;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent, Room *, ServerPlayer *, QVariant &data) const
    {
        JudgeStruct *j = data.value<JudgeStruct *>();
        if (j->reason == "huituo")
            j->pattern = j->card->isRed() ? "red" : (j->card->isBlack() ? "black" : "no_suit");

        return false;
    }
};

MingjianCard::MingjianCard()
{
    will_throw = false;
}

bool MingjianCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select != Self;
}

void MingjianCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    ServerPlayer *target = targets.first();
    DummyCard *handcards = source->wholeHandCards();
    CardMoveReason r(CardMoveReason::S_REASON_GIVE, source->objectName());
    room->obtainCard(target, handcards, r, false);
    room->addPlayerMark(target, "@mingjian");
    delete handcards;
}

class MingjianVS : public ZeroCardViewAsSkill
{
public:
    MingjianVS() : ZeroCardViewAsSkill("mingjian")
    {
        
    }

    const Card *viewAs() const
    {
        return new MingjianCard;
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        if (player->isKongcheng())
            return false;
        else
            return !player->hasUsed("MingjianCard");
    }
};

class Mingjian : public TriggerSkill
{
public:
    Mingjian() : TriggerSkill("mingjian")
    {
        events << EventPhaseChanging;
        view_as_skill = new MingjianVS;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->getMark("@mingjian") > 0;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        PhaseChangeStruct change = data.value<PhaseChangeStruct>();
        if (change.to != Player::NotActive)
            return false;

        room->removePlayerMark(player, "@mingjian");
        return false;
    }
};

class MingjianTMD : public TargetModSkill
{
public:
    MingjianTMD() : TargetModSkill("#mingjian-tmd")
    {
    }

    int getResidueNum(const Player *from, const Card *) const
    {
        if (from->getMark("@mingjian") > 0)
            return 1;
        else
            return 0;
    }
};

class MingjianMax : public MaxCardsSkill
{
public:
    MingjianMax() : MaxCardsSkill("#mingjian-max")
    {
    }

    int getExtra(const Player *target) const
    {
        if (target->getMark("@mingjian") > 0)
            return 1;

        return 0;
    }
};

class Xingshuai : public TriggerSkill
{
public:
    Xingshuai() : TriggerSkill("xingshuai$")
    {
        events << Dying << QuitDying;
        limit_mark = "@xingshuai";
        frequency = Limited;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == Dying)
        {
            DyingStruct dying = data.value<DyingStruct>();
            if (dying.who != player)
                return false;


            if (player->hasLordSkill(this) && player->getMark(limit_mark) > 0 && hasWeiGens(player) && player->askForSkillInvoke(this, data)) {
                if (!player->isLord() && player->hasSkill("weidi")) {
                    room->broadcastSkillInvoke("weidi");
                    room->doSuperLightbox("yuanshu", "xingshuai");
                }
                else {
                    room->broadcastSkillInvoke(objectName());
                    room->doSuperLightbox("caorui", "xingshuai");
                }

                room->removePlayerMark(player, limit_mark);

                QList<ServerPlayer *> weis = room->getLieges("wei", player);
                QList<ServerPlayer *> invokes;

                room->sortByActionOrder(weis);
                foreach(ServerPlayer *wei, weis) {
                    if (wei->askForSkillInvoke("_xingshuai", "xing")) {
                        invokes << wei;
                        room->recover(player, RecoverStruct(wei));
                    }
                }
                room->setTag("XingshuaiInvokes", QVariant::fromValue(invokes));
            }
        }
        else
        {
            QList<ServerPlayer *> invokes = room->getTag("XingshuaiInvokes").value<QList<ServerPlayer *>>();
            room->removeTag("XingshuaiInvokes");
            room->sortByActionOrder(invokes);
            foreach(ServerPlayer *wei, invokes)
                room->damage(DamageStruct(objectName(), NULL, wei));
        }

        return false;
    }

private:
    static bool hasWeiGens(const Player *lord)
    {
        QList<const Player *> sib = lord->getAliveSiblings();
        foreach (const Player *p, sib) {
            if (p->getKingdom() == "wei")
                return true;
        }

        return false;
    }
};

class Qianjv : public DistanceSkill
{
public:
    Qianjv() : DistanceSkill("qianjv")
    {
        frequency = Compulsory;
    }

    int getCorrect(const Player *from, const Player *) const
    {
        int correct = 0;
        if (from->hasSkill(this))
            correct = -from->getLostHp();

        return correct;
    }
};

class Qingxi : public TriggerSkill
{
public:
    Qingxi() : TriggerSkill("qingxi")
    {
        events << DamageCaused;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        ServerPlayer *target = damage.to;
        if (player->hasSkill(this) && damage.card && damage.card->isKindOf("Slash") && player->getWeapon() != NULL
            && !damage.chain && !damage.transfer
            && room->askForSkillInvoke(player, objectName(), QVariant::fromValue(target))) {
            room->broadcastSkillInvoke(objectName());

            const Card *weaponCard = player->getWeapon()->getRealCard();
            const Weapon *weapon = qobject_cast<const Weapon *>(weaponCard);
            QString promt = QString("@@qingxi-discard:%1:%2:%3").arg(player->objectName()).arg(target->objectName()).arg(weapon->getRange());
            if (room->askForDiscard(target, objectName(), weapon->getRange(), weapon->getRange(), true, false, promt))
            {
                room->throwCard(weaponCard, player, target);
            }
            else
            {
                damage.damage++;
                data = QVariant::fromValue(damage);
            }
        }

        return false;
    }
};

HuaiyiCard::HuaiyiCard()
{
    target_fixed = true;
}

void HuaiyiCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const
{
    room->showAllCards(source);

    QList<int> blacks;
    QList<int> reds;
    foreach (const Card *c, source->getHandcards()) {
        if (c->isRed())
            reds << c->getId();
        else
            blacks << c->getId();
    }

    if (reds.isEmpty() || blacks.isEmpty())
        return;

    QString to_discard = room->askForChoice(source, "huaiyi", "black+red");
    QList<int> *pile = NULL;
    if (to_discard == "black")
        pile = &blacks;
    else
        pile = &reds;

    int n = pile->length();

    room->setPlayerMark(source, "huaiyi_num", n);

    DummyCard dm(*pile);
    room->throwCard(&dm, source);

    room->askForUseCard(source, "@@huaiyi", "@huaiyi:::" + QString::number(n), -1, Card::MethodNone);
}

HuaiyiSnatchCard::HuaiyiSnatchCard()
{
    handling_method = Card::MethodNone;
    m_skillName = "_huaiyi";
}

bool HuaiyiSnatchCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    int n = Self->getMark("huaiyi_num");
    if (targets.length() >= n)
        return false;

    if (to_select == Self)
        return false;

    if (to_select->isNude())
        return false;

    return true;
}

void HuaiyiSnatchCard::onUse(Room *room, const CardUseStruct &card_use) const
{
    ServerPlayer *player = card_use.from;

    QList<ServerPlayer *> to = card_use.to;

    room->sortByActionOrder(to);

    foreach (ServerPlayer *p, to) {
        int id = room->askForCardChosen(player, p, "he", "huaiyi");
        player->obtainCard(Sanguosha->getCard(id), false);
    }

    if (to.length() >= 2)
        room->loseHp(player);
}

class Huaiyi : public ZeroCardViewAsSkill
{
public:
    Huaiyi() : ZeroCardViewAsSkill("huaiyi")
    {

    }

    const Card *viewAs() const
    {
        if (Sanguosha->currentRoomState()->getCurrentCardUsePattern() == "@@huaiyi")
            return new HuaiyiSnatchCard;
        else
            return new HuaiyiCard;
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("HuaiyiCard");
    }

    bool isEnabledAtResponse(const Player *, const QString &pattern) const
    {
        return pattern == "@@huaiyi";
    }
};

class Jigong : public PhaseChangeSkill
{
public:
    Jigong() : PhaseChangeSkill("jigong")
    {

    }

    bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Play;
    }

    bool onPhaseChange(ServerPlayer *target) const
    {
        if (target->askForSkillInvoke(this)) {
            target->getRoom()->broadcastSkillInvoke(objectName());
            target->drawCards(2, "jigong");
            target->getRoom()->setPlayerFlag(target, "jigong");
        }

        return false;
    }
};

class JigongMax : public MaxCardsSkill
{
public:
    JigongMax() : MaxCardsSkill("#jigong")
    {

    }

    int getFixed(const Player *target) const
    {
        if (target->hasFlag("jigong"))
            return target->getMark("damage_point_play_phase");
        
        return -1;
    }
};

class Shifei : public TriggerSkill
{
public:
    Shifei() : TriggerSkill("shifei")
    {
        events << CardAsked;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        QStringList ask = data.toStringList();
        if (ask.first() != "jink")
            return false;

        ServerPlayer *current = room->getCurrent();
        if (current == NULL || current->isDead() || current->getPhase() == Player::NotActive)
            return false;

        if (player->askForSkillInvoke(this)) {
            room->broadcastSkillInvoke(objectName());
            current->drawCards(1, objectName());

            QList<ServerPlayer *> mosts;
            int most = -1;
            foreach (ServerPlayer *p, room->getAlivePlayers()) {
                int h = p->getHandcardNum();
                if (h > most) {
                    mosts.clear();
                    most = h;
                    mosts << p;
                } else if (most == h)
                    mosts << p;
            }
            if (most < 0 || mosts.contains(current))
                return false;

            QList<ServerPlayer *> mosts_copy = mosts;
            foreach (ServerPlayer *p, mosts_copy) {
                if (!player->canDiscard(p, "he"))
                    mosts.removeOne(p);
            }

            if (mosts.isEmpty())
                return false;

            ServerPlayer *vic = room->askForPlayerChosen(player, mosts, objectName(), "@shifei-dis");
            // it is impossible that vic == NULL
            if (vic == player)
                room->askForDiscard(player, objectName(), 1, 1, false, true);
            else {
                int id = room->askForCardChosen(player, vic, "he", objectName(), false, Card::MethodDiscard);
                room->throwCard(id, vic, player);
            }
                
            Jink *jink = new Jink(Card::NoSuit, 0);
            jink->setSkillName("_shifei");
            room->provide(jink);
            return true;
        }

        return false;
    }
};

class ZhanjueVS : public ZeroCardViewAsSkill
{
public:
    ZhanjueVS() : ZeroCardViewAsSkill("zhanjue")
    {

    }

    const Card *viewAs() const
    {
        Duel *duel = new Duel(Card::SuitToBeDecided, -1);
        duel->addSubcards(Self->getHandcards());
        duel->setSkillName("zhanjue");
        return duel;
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return player->getMark("zhanjuedraw") < 2 && !player->isKongcheng();
    }
};

class Zhanjue : public TriggerSkill
{
public:
    Zhanjue() : TriggerSkill("zhanjue")
    {
        view_as_skill = new ZhanjueVS;
        events << CardFinished << PreDamageDone << EventPhaseChanging;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == PreDamageDone) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.card != NULL && damage.card->isKindOf("Duel") && damage.card->getSkillName() == "zhanjue" && damage.from != NULL) {
                QVariantMap m = room->getTag("zhanjue").toMap();
                QVariantList l = m.value(damage.card->toString(), QVariantList()).toList();
                l << QVariant::fromValue(damage.to);
                m[damage.card->toString()] = l;
                room->setTag("zhanjue", m);
            }
        } else if (triggerEvent == CardFinished) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card != NULL && use.card->isKindOf("Duel") && use.card->getSkillName() == "zhanjue") {
                QVariantMap m = room->getTag("zhanjue").toMap();
                QVariantList l = m.value(use.card->toString(), QVariantList()).toList();
                if (!l.isEmpty()) {
                    QList<ServerPlayer *> l_copy;
                    foreach (const QVariant &s, l)
                        l_copy << s.value<ServerPlayer *>();
                    l_copy << use.from;
                    int n = l_copy.count(use.from);
                    room->addPlayerMark(use.from, "zhanjuedraw", n);
                    room->sortByActionOrder(l_copy);
                    room->drawCards(l_copy, 1, objectName());
                }
                m.remove(use.card->toString());
                room->setTag("zhanjue", m);
            }
        } else if (triggerEvent == EventPhaseChanging) {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.to == Player::NotActive)
                room->setPlayerMark(player, "zhanjuedraw", 0);
        }
        return false;
    }
};

QinwangCard::QinwangCard()
{

}

bool QinwangCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    Slash *slash = new Slash(NoSuit, 0);
    slash->deleteLater();
    return slash->targetFilter(targets, to_select, Self);
}

const Card *QinwangCard::validate(CardUseStruct &cardUse) const
{
    cardUse.from->getRoom()->throwCard(cardUse.card, cardUse.from);

    JijiangCard jj;
    cardUse.from->setFlags("qinwangjijiang");
    try {
        const Card *vs = jj.validate(cardUse);
        if (cardUse.from->hasFlag("qinwangjijiang"))
            cardUse.from->setFlags("-qinwangjijiang");

        return vs;
    }
    catch (TriggerEvent e) {
        if (e == TurnBroken || e == StageChange)
            cardUse.from->setFlags("-qinwangjijiang");

        throw e;
    }

    return NULL;
}

class QinwangVS : public OneCardViewAsSkill
{
public:
    QinwangVS() : OneCardViewAsSkill("qinwang$")
    {
        filter_pattern = ".!";
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        JijiangViewAsSkill jj;
        return jj.isEnabledAtPlay(player);
    }

    bool isEnabledAtResponse(const Player *player, const QString &pattern) const
    {
        JijiangViewAsSkill jj;
        return jj.isEnabledAtResponse(player, pattern);
    }

    const Card *viewAs(const Card *originalCard) const
    {
        QinwangCard *qw = new QinwangCard;
        qw->addSubcard(originalCard);
        return qw;
    }
};

class Qinwang : public TriggerSkill
{
public:
    Qinwang() : TriggerSkill("qinwang$")
    {
        view_as_skill = new QinwangVS;
        events << CardAsked;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->hasLordSkill("qinwang");
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        const TriggerSkill *jj = Sanguosha->getTriggerSkill("jijiang");
        if (jj == NULL)
            return false;

        QString pattern = data.toStringList().first();
        QString prompt = data.toStringList().at(1);
        if (pattern != "slash" || prompt.startsWith("@jijiang-slash"))
            return false;

        QList<ServerPlayer *> lieges = room->getLieges("shu", player);
        if (lieges.isEmpty())
            return false;

        if (!room->askForCard(player, "..", "@qinwang-discard", data, "qinwang"))
            return false;

        player->setFlags("qinwangjijiang");
        try {
            bool t = jj->trigger(triggerEvent, room, player, data);
            if (player->hasFlag("qinwangjijiang"))
                player->setFlags("-qinwangjijiang");

            return t;
        }
        catch (TriggerEvent e) {
            if (e == TurnBroken || e == StageChange)
                player->setFlags("-qinwangjijiang");

            throw e;
        }

        return false;
    }
};

class QinwangDraw : public TriggerSkill
{
public:
    QinwangDraw() : TriggerSkill("qinwang-draw")
    {
        events << CardResponded;
        global = true;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->isAlive();
    }

    bool trigger(TriggerEvent, Room *, ServerPlayer *player, QVariant &data) const
    {
        CardResponseStruct resp = data.value<CardResponseStruct>();
        if (resp.m_card->isKindOf("Slash") && !resp.m_isUse && resp.m_who && resp.m_who->hasFlag("qinwangjijiang")) {
            resp.m_who->setFlags("-qinwangjijiang");
            player->drawCards(1, "qinwang");
        }

        return false;
    }
};

YaomingCard::YaomingCard()
{

}

bool YaomingCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return (to_select->getHandcardNum() != Self->getHandcardNum()) && targets.isEmpty();
}

void YaomingCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    ServerPlayer *target = targets.first();
    if (target->getHandcardNum() < source->getHandcardNum())
    {
        target->drawCards(1);
    }
    else
    {
        int cardId;
        if (cardId = room->askForCardChosen(source, target, "h", "yaoming"))
        {
            room->throwCard(Sanguosha->getCard(cardId), target, source);
        }
    }
    room->setPlayerMark(source, "yaoming-invoked", 1);
}

class YaomingVS : public ZeroCardViewAsSkill
{
public:
    YaomingVS() : ZeroCardViewAsSkill("yaoming")
    {
    }

    bool isEnabledAtPlay(const Player *) const
    {
        return false;
    }

    bool isEnabledAtResponse(const Player *, const QString &pattern) const
    {
        return pattern == "@@yaoming";
    }

    const Card *viewAs() const
    {
        return new YaomingCard;
    }
};

class Yaoming : public TriggerSkill
{
public:
    Yaoming() : TriggerSkill("yaoming")
    {
        events << Damage << Damaged << EventPhaseChanging;
        view_as_skill = new YaomingVS;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging)
        {
            PhaseChangeStruct phaseChange = data.value<PhaseChangeStruct>();
            if (phaseChange.to == Player::NotActive)
            {
                foreach(ServerPlayer *p, room->findPlayersBySkillName(objectName()))
                {
                    room->setPlayerMark(p, "yaoming-invoked", 0);
                }
            }
        }
        else
        {
            if (player->hasSkill(this) && player->getMark("yaoming-invoked") == 0 && room->askForSkillInvoke(player, objectName(), QVariant(), false))
            {
                room->askForUseCard(player, "@@yaoming", "@@yaoming-choose");
            }
        }

        return false;
    }
};

YanzhuCard::YanzhuCard()
{

}

bool YanzhuCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select != Self && !to_select->isNude();
}

void YanzhuCard::onEffect(const CardEffectStruct &effect) const
{
    ServerPlayer *target = effect.to;
    Room *r = target->getRoom();

    if (!r->askForDiscard(target, "yanzhu", 1, 1, !target->getEquips().isEmpty(), true, "@yanzhu-discard")) {
        if (!target->getEquips().isEmpty()) {
            DummyCard dummy;
            dummy.addSubcards(target->getEquips());
            r->obtainCard(effect.from, &dummy);
        }

        if (effect.from->hasSkill("yanzhu", true)) {
            r->setPlayerMark(effect.from, "yanzhu_lost", 1);
            r->handleAcquireDetachSkills(effect.from, "-yanzhu");
        }
    }
}

class Yanzhu : public ZeroCardViewAsSkill
{
public:
    Yanzhu() : ZeroCardViewAsSkill("yanzhu")
    {

    }

    const Card *viewAs() const
    {
        return new YanzhuCard;
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("YanzhuCard");
    }
};

/*
class YanzhuTrig : public TriggerSkill
{
public:
    YanzhuTrig() : TriggerSkill("yanzhu")
    {
        events << EventLoseSkill;
        view_as_skill = new Yanzhu;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->isAlive();
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (data.toString() == "yanzhu")
            room->setPlayerMark(player, "yanzhu_lost", 1);

        return false;
    }
};
*/

XingxueCard::XingxueCard()
{

}

bool XingxueCard::targetFilter(const QList<const Player *> &targets, const Player *, const Player *Self) const
{
    int n = Self->getMark("yanzhu_lost") == 0 ? Self->getHp() : Self->getMaxHp();

    return targets.length() < n;
}

void XingxueCard::use(Room *room, ServerPlayer *, QList<ServerPlayer *> &targets) const
{
    foreach (ServerPlayer *t, targets) {
        room->drawCards(t, 1, "xingxue");
        if (t->isAlive() && !t->isNude()) {
            const Card *c = room->askForExchange(t, "xingxue", 1, 1, true, "@xingxue-put");
            int id = c->getSubcards().first();
            delete c;

            CardsMoveStruct m(id, NULL, Player::DrawPile, CardMoveReason(CardMoveReason::S_REASON_PUT, t->objectName()));
            room->setPlayerFlag(t, "Global_GongxinOperator");
            room->moveCardsAtomic(m, false);
            room->setPlayerFlag(t, "-Global_GongxinOperator");
        }
    }
}

class XingxueVS : public ZeroCardViewAsSkill
{
public:
    XingxueVS() : ZeroCardViewAsSkill("xingxue")
    {
        response_pattern = "@@xingxue";
    }

    const Card *viewAs() const
    {
        return new XingxueCard;
    }
};

class Xingxue : public PhaseChangeSkill
{
public:
    Xingxue() : PhaseChangeSkill("xingxue")
    {
        view_as_skill = new XingxueVS;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return PhaseChangeSkill::triggerable(target) && target->getPhase() == Player::Finish;
    }

    bool onPhaseChange(ServerPlayer *target) const
    {
        target->getRoom()->askForUseCard(target, "@@xingxue", "@xingxue");
        return false;
    }
};

class Qiaoshi : public PhaseChangeSkill
{
public:
    Qiaoshi() : PhaseChangeSkill("qiaoshi")
    {

    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->isAlive() && target->getPhase() == Player::Finish;
    }

    bool onPhaseChange(ServerPlayer *player) const
    {
        Room *room = player->getRoom();
        foreach (ServerPlayer *const &p, room->getOtherPlayers(player)) {
            if (!TriggerSkill::triggerable(p) || p->getHandcardNum() != player->getHandcardNum())
                continue;

            if (p->askForSkillInvoke(objectName(), QVariant::fromValue(player))) {
                room->broadcastSkillInvoke(objectName());
                QList<ServerPlayer *> l;
                l << p << player;
                room->sortByActionOrder(l);
                room->drawCards(l, 1, objectName());
            }
        }

        return false;
    }
};

YjYanyuCard::YjYanyuCard()
{
    will_throw = false;
    can_recast = true;
    handling_method = Card::MethodRecast;
    target_fixed = true;
    mute = true;
}

void YjYanyuCard::onUse(Room *room, const CardUseStruct &card_use) const
{
    ServerPlayer *xiahou = card_use.from;

    CardMoveReason reason(CardMoveReason::S_REASON_RECAST, xiahou->objectName());
    reason.m_skillName = this->getSkillName();
    room->moveCardTo(this, xiahou, NULL, Player::DiscardPile, reason);
    xiahou->broadcastSkillInvoke("@recast");

    int id = card_use.card->getSubcards().first();

    LogMessage log;
    log.type = "#UseCard_Recast";
    log.from = xiahou;
    log.card_str = QString::number(id);
    room->sendLog(log);

    xiahou->drawCards(1, "recast");

    xiahou->addMark("yjyanyu");
}

class YjYanyuVS : public OneCardViewAsSkill
{
public:
    YjYanyuVS() : OneCardViewAsSkill("yjyanyu")
    {
        filter_pattern = "Slash";
    }

    const Card *viewAs(const Card *originalCard) const
    {
        if (Self->isCardLimited(originalCard, Card::MethodRecast))
            return NULL;

        YjYanyuCard *recast = new YjYanyuCard;
        recast->addSubcard(originalCard);
        return recast;
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        Slash *s = new Slash(Card::NoSuit, 0);
        s->deleteLater();
        return !player->isCardLimited(s, Card::MethodRecast);
    }
};

class YjYanyu : public TriggerSkill
{
public:
    YjYanyu() : TriggerSkill("yjyanyu")
    {
        view_as_skill = new YjYanyuVS;
        events << EventPhaseEnd;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->isAlive() && target->getPhase() == Player::Play;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        int recastNum = player->getMark("yjyanyu");
        player->setMark("yjyanyu", 0);

        if (recastNum < 2)
            return false;

        QList<ServerPlayer *> malelist;
        foreach (ServerPlayer *p, room->getAlivePlayers()) {
            if (p->isMale())
                malelist << p;
        }

        if (malelist.isEmpty())
            return false;

        ServerPlayer *male = room->askForPlayerChosen(player, malelist, objectName(), "@yjyanyu-give", true);

        if (male != NULL) {
            room->broadcastSkillInvoke(objectName());
            male->drawCards(2, objectName());
        }

        return false;
    }
};

FurongCard::FurongCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
}

bool FurongCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (targets.length() > 0 || to_select == Self)
        return false;
    return !to_select->isKongcheng();
}

void FurongCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();

    const Card *c = room->askForExchange(effect.to, "furong", 1, 1, false, "@furong-show");

    room->showCard(effect.from, subcards.first());
    room->showCard(effect.to, c->getSubcards().first());

    const Card *card1 = Sanguosha->getCard(subcards.first());
    const Card *card2 = Sanguosha->getCard(c->getSubcards().first());

    if (card1->isKindOf("Slash") && !card2->isKindOf("Jink")) {
        room->throwCard(this, effect.from);
        room->damage(DamageStruct(objectName(), effect.from, effect.to));
    } else if (!card1->isKindOf("Slash") && card2->isKindOf("Jink")) {
        room->throwCard(this, effect.from);
        if (!effect.to->isNude()) {
            int id = room->askForCardChosen(effect.from, effect.to, "he", objectName());
            room->obtainCard(effect.from, id, false);
        }
    }

    delete c;
}

class Furong : public OneCardViewAsSkill
{
public:
    Furong() : OneCardViewAsSkill("furong")
    {
        filter_pattern = ".|.|.|hand";
    }

    const Card *viewAs(const Card *originalCard) const
    {
        FurongCard *fr = new FurongCard;
        fr->addSubcard(originalCard);
        return fr;
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("FurongCard");
    }
};

class Shizhi : public TriggerSkill
{
public:
    Shizhi() : TriggerSkill("#shizhi")
    {
        events << HpChanged << MaxHpChanged << EventAcquireSkill << EventLoseSkill << GameStart;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->isAlive();
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventAcquireSkill || triggerEvent == EventLoseSkill) {
            if (data.toString() != "shizhi")
                return false;
        }

        if (triggerEvent == HpChanged || triggerEvent == MaxHpChanged || triggerEvent == GameStart) {
            if (!player->hasSkill(this))
                return false;
        }

        bool skillStateBefore = false;
        if (triggerEvent != EventAcquireSkill && triggerEvent != GameStart)
            skillStateBefore = player->getMark("shizhi") > 0;

        bool skillStateAfter = false;
        if (triggerEvent == EventLoseSkill)
            skillStateAfter = true;
        else
            skillStateAfter = player->getHp() == 1;

        if (skillStateAfter != skillStateBefore) 
            room->filterCards(player, player->getCards("he"), true);

        player->setMark("shizhi", skillStateAfter ? 1 : 0);

        return false;
    }
};

class ShizhiFilter : public FilterSkill
{
public:
    ShizhiFilter() : FilterSkill("shizhi")
    {

    }

    bool viewFilter(const Card *to_select) const
    {
        Room *room = Sanguosha->currentRoom();
        ServerPlayer *player = room->getCardOwner(to_select->getId());
        return player != NULL && player->getHp() == 1 && to_select->isKindOf("Jink");
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

HuomoDialog::HuomoDialog() : GuhuoDialog("huomo", true, false)
{

}

HuomoDialog *HuomoDialog::getInstance()
{
    static HuomoDialog *instance;
    if (instance == NULL || instance->objectName() != "huomo")
        instance = new HuomoDialog;

    return instance;
}

bool HuomoDialog::isButtonEnabled(const QString &button_name) const
{
    const Card *c = map[button_name];
    QString classname = c->getClassName();
    if (c->isKindOf("Slash"))
        classname = "Slash";

    bool r = Self->getMark("Huomo_" + classname) == 0;
    if (!r)
        return false;

    return GuhuoDialog::isButtonEnabled(button_name);
}

HuomoCard::HuomoCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
}

bool HuomoCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE_USE) {
        const Card *card = NULL;
        if (!user_string.isEmpty())
            card = Sanguosha->cloneCard(user_string.split("+").first());
        return card && card->targetFilter(targets, to_select, Self) && !Self->isProhibited(to_select, card, targets);
    }

    const Card *_card = Self->tag.value("huomo").value<const Card *>();
    if (_card == NULL)
        return false;

    Card *card = Sanguosha->cloneCard(_card->objectName(), Card::NoSuit, 0);
    card->setCanRecast(false);
    card->deleteLater();
    return card && card->targetFilter(targets, to_select, Self) && !Self->isProhibited(to_select, card, targets);
}

bool HuomoCard::targetFixed() const
{
    if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE_USE) {
        const Card *card = NULL;
        if (!user_string.isEmpty())
            card = Sanguosha->cloneCard(user_string.split("+").first());
        return card && card->targetFixed();
    }

    const Card *_card = Self->tag.value("huomo").value<const Card *>();
    if (_card == NULL)
        return false;

    Card *card = Sanguosha->cloneCard(_card->objectName(), Card::NoSuit, 0);
    card->setCanRecast(false);
    card->deleteLater();
    return card && card->targetFixed();
}

bool HuomoCard::targetsFeasible(const QList<const Player *> &targets, const Player *Self) const
{
    if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE_USE) {
        const Card *card = NULL;
        if (!user_string.isEmpty())
            card = Sanguosha->cloneCard(user_string.split("+").first());
        return card && card->targetsFeasible(targets, Self);
    }

    const Card *_card = Self->tag.value("huomo").value<const Card *>();
    if (_card == NULL)
        return false;

    Card *card = Sanguosha->cloneCard(_card->objectName(), Card::NoSuit, 0);
    card->setCanRecast(false);
    card->deleteLater();
    return card && card->targetsFeasible(targets, Self);
}

const Card *HuomoCard::validate(CardUseStruct &card_use) const
{
    ServerPlayer *zhongyao = card_use.from;
    Room *room = zhongyao->getRoom();

    QString to_guhuo = user_string;
    if (user_string == "slash" && Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE_USE) {
        QStringList guhuo_list;
        guhuo_list << "slash";
        if (!Config.BanPackages.contains("maneuvering"))
            guhuo_list = QStringList() << "normal_slash" << "thunder_slash" << "fire_slash";
        to_guhuo = room->askForChoice(zhongyao, "huomo_slash", guhuo_list.join("+"));
    }

    room->moveCardTo(this, NULL, Player::DrawPile, true);

    QString user_str;
    if (to_guhuo == "normal_slash")
        user_str = "slash";
    else
        user_str = to_guhuo;

    Card *c = Sanguosha->cloneCard(user_str, Card::NoSuit, 0);

    QString classname;
    if (c->isKindOf("Slash"))
        classname = "Slash";
    else
        classname = c->getClassName();

    room->setPlayerMark(zhongyao, "Huomo_" + classname, 1);

    QStringList huomoList = zhongyao->tag.value("huomoClassName").toStringList();
    huomoList << classname;
    zhongyao->tag["huomoClassName"] = huomoList;

    c->setSkillName("huomo");
    c->deleteLater();
    return c;
}

const Card *HuomoCard::validateInResponse(ServerPlayer *zhongyao) const
{
    Room *room = zhongyao->getRoom();

    QString to_guhuo = user_string;
    if (user_string == "peach+analeptic") {
        bool can_use_peach = zhongyao->getMark("Huomo_Peach") == 0;
        bool can_use_analeptic = zhongyao->getMark("Huomo_Analeptic") == 0;
        QStringList guhuo_list;
        if (can_use_peach)
            guhuo_list << "peach";
        if (can_use_analeptic && !Config.BanPackages.contains("maneuvering"))
            guhuo_list << "analeptic";
        to_guhuo = room->askForChoice(zhongyao, "huomo_saveself", guhuo_list.join("+"));
    } else if (user_string == "slash") {
        QStringList guhuo_list;
        guhuo_list << "slash";
        if (!Config.BanPackages.contains("maneuvering"))
            guhuo_list = QStringList() << "normal_slash" << "thunder_slash" << "fire_slash";
        to_guhuo = room->askForChoice(zhongyao, "huomo_slash", guhuo_list.join("+"));
    } else
        to_guhuo = user_string;

    room->moveCardTo(this, NULL, Player::DrawPile, true);

    QString user_str;
    if (to_guhuo == "normal_slash")
        user_str = "slash";
    else
        user_str = to_guhuo;

    Card *c = Sanguosha->cloneCard(user_str, Card::NoSuit, 0);

    QString classname;
    if (c->isKindOf("Slash"))
        classname = "Slash";
    else
        classname = c->getClassName();

    room->setPlayerMark(zhongyao, "Huomo_" + classname, 1);

    QStringList huomoList = zhongyao->tag.value("huomoClassName").toStringList();
    huomoList << classname;
    zhongyao->tag["huomoClassName"] = huomoList;

    c->setSkillName("huomo");
    c->deleteLater();
    return c;

}

class HuomoVS : public OneCardViewAsSkill
{
public:
    HuomoVS() : OneCardViewAsSkill("huomo")
    {
        filter_pattern = "^BasicCard|black";
    }

    const Card *viewAs(const Card *originalCard) const
    {
        QString pattern;
        if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_PLAY) {
            const Card *c = Self->tag["huomo"].value<const Card *>();
            if (c == NULL || Self->getMark("Huomo_" + (c->isKindOf("Slash") ? "Slash" : c->getClassName())) > 0)
                return NULL;

            pattern = c->objectName();
        } else {
            pattern = Sanguosha->currentRoomState()->getCurrentCardUsePattern();
            if (pattern == "peach+analeptic" && Self->getMark("Global_PreventPeach") > 0)
                pattern = "analeptic";

            // check if it can use
            bool can_use = false;
            QStringList p = pattern.split("+");
            foreach (const QString &x, p) {
                const Card *c = Sanguosha->cloneCard(x);
                QString us = c->getClassName();
                if (c->isKindOf("Slash"))
                    us = "Slash";

                if (Self->getMark("Huomo_" + us) == 0)
                    can_use = true;

                delete c;
                if (can_use)
                    break;
            }

            if (!can_use)
                return NULL;
        }

        HuomoCard *hm = new HuomoCard;
        hm->setUserString(pattern);
        hm->addSubcard(originalCard);

        return hm;
        
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        QList<const Player *> sib = player->getAliveSiblings();
        if (player->isAlive())
            sib << player;

        bool noround = true;

        foreach (const Player *p, sib) {
            if (p->getPhase() != Player::NotActive) {
                noround = false;
                break;
            }
        }

        return true; // for DIY!!!!!!!
    }

    bool isEnabledAtResponse(const Player *player, const QString &pattern) const
    {
        QList<const Player *> sib = player->getAliveSiblings();
        if (player->isAlive())
            sib << player;

        bool noround = true;

        foreach (const Player *p, sib) {
            if (p->getPhase() != Player::NotActive) {
                noround = false;
                break;
            }
        }

        if (noround)
            return false;

        if (Sanguosha->currentRoomState()->getCurrentCardUseReason() != CardUseStruct::CARD_USE_REASON_RESPONSE_USE)
            return false;

#define HUOMO_CAN_USE(x) (player->getMark("Huomo_" #x) == 0)

        if (pattern == "slash")
            return HUOMO_CAN_USE(Slash);
        else if (pattern == "peach")
            return HUOMO_CAN_USE(Peach) && player->getMark("Global_PreventPeach") == 0;
        else if (pattern.contains("analeptic"))
            return HUOMO_CAN_USE(Peach) || HUOMO_CAN_USE(Analeptic);
        else if (pattern == "jink")
            return HUOMO_CAN_USE(Jink);

#undef HUOMO_CAN_USE

        return false;
    }
};

class Huomo : public TriggerSkill
{
public:
    Huomo() : TriggerSkill("huomo")
    {
        view_as_skill = new HuomoVS;
        events << EventPhaseChanging;
    }

    QDialog *getDialog() const
    {
        return HuomoDialog::getInstance();
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
        PhaseChangeStruct change = data.value<PhaseChangeStruct>();
        if (change.to != Player::NotActive)
            return false;

        foreach (ServerPlayer *p, room->getAlivePlayers()) {
            QStringList sl = p->tag.value("huomoClassName").toStringList();
            foreach (const QString &t, sl)
                room->setPlayerMark(p, "Huomo_" + t, 0);
            
            p->tag["huomoClassName"] = QStringList();
        }

        return false;
    }
};

class Zuoding : public TriggerSkill
{
public:
    Zuoding() : TriggerSkill("zuoding")
    {
        events << TargetSpecified;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->isAlive() && target->getPhase() == Player::Play;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (room->getTag("zuoding").toBool())
            return false;

        CardUseStruct use = data.value<CardUseStruct>();
        if (!(use.card != NULL && !use.card->isKindOf("SkillCard") && use.card->getSuit() == Card::Spade && !use.to.isEmpty()))
            return false;

        foreach (ServerPlayer *zhongyao, room->getAllPlayers()) {
            if (TriggerSkill::triggerable(zhongyao) && player != zhongyao) {
                ServerPlayer *p = room->askForPlayerChosen(zhongyao, use.to, "zuoding", "@zuoding", true, true);
                if (p != NULL) {
                    room->broadcastSkillInvoke(objectName());
                    p->drawCards(1, "zuoding");
                }
            }
        }
        
        return false;
    }
};

class ZuodingRecord : public TriggerSkill
{
public:
    ZuodingRecord() : TriggerSkill("#zuoding")
    {
        events << DamageDone << EventPhaseChanging;
        global = true;
    }

    int getPriority(TriggerEvent) const
    {
        return 0;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging) {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.from == Player::Play)
                room->setTag("zuoding", false);
        } else {
            bool playphase = false;
            foreach (ServerPlayer *p, room->getAlivePlayers()) {
                if (p->getPhase() == Player::Play) {
                    playphase = true;
                    break;
                }
            }
            if (!playphase)
                return false;

            room->setTag("zuoding", true);
        }

        return false;
    }
};

AnguoCard::AnguoCard()
{

}

bool AnguoCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (!targets.isEmpty() || to_select == Self)
        return false;

    return !to_select->getEquips().isEmpty();
}

void AnguoCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.to->getRoom();
    int beforen = 0;
    foreach (ServerPlayer *p, room->getAlivePlayers()) {
        if (effect.to->inMyAttackRange(p))
            beforen++;
    }

    int id = room->askForCardChosen(effect.from, effect.to, "e", "anguo");
    effect.to->obtainCard(Sanguosha->getCard(id));

    int aftern = 0;
    foreach (ServerPlayer *p, room->getAlivePlayers()) {
        if (effect.to->inMyAttackRange(p))
            aftern++;
    }

    if (aftern < beforen)
        effect.from->drawCards(1, "anguo");
}

class Anguo : public ZeroCardViewAsSkill
{
public:
    Anguo() : ZeroCardViewAsSkill("anguo")
    {

    }

    const Card *viewAs() const
    {
        return new AnguoCard;
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("AnguoCard");
    }
};

YJCM2015Package::YJCM2015Package()
    : Package("YJCM2015")
{

    General *caorui = new General(this, "caorui$", "wei", 3);
    caorui->addSkill(new Huituo);
    caorui->addSkill(new HuituoJudge);
    related_skills.insertMulti("huituo", "#huituo");
    caorui->addSkill(new Mingjian);
    caorui->addSkill(new MingjianTMD);
    caorui->addSkill(new MingjianMax);
    related_skills.insertMulti("mingjian", "#mingjian-tmd");
    related_skills.insertMulti("mingjian", "#mingjian-max");
    caorui->addSkill(new Xingshuai);

    General *caoxiu = new General(this, "caoxiu", "wei");
    caoxiu->addSkill(new Qianjv);
    caoxiu->addSkill(new Qingxi);

    General *gongsun = new General(this, "gongsunyuan", "qun");
    gongsun->addSkill(new Huaiyi);

    General *guofeng = new General(this, "guotufengji", "qun", 3);
    guofeng->addSkill(new Jigong);
    guofeng->addSkill(new JigongMax);
    related_skills.insertMulti("jigong", "#jigong");
    guofeng->addSkill(new Shifei);

    General *liuchen = new General(this, "liuchen$", "shu");
    liuchen->addSkill(new Zhanjue);
    liuchen->addSkill(new Qinwang);

    General *quancong = new General(this, "quancong", "wu");
    quancong->addSkill(new Yaoming);

    General *sunxiu = new General(this, "sunxiu$", "wu", 3);
    sunxiu->addSkill(new Yanzhu);
    sunxiu->addSkill(new Xingxue);
    sunxiu->addSkill(new Skill("zhaofu$", Skill::Compulsory));

    General *xiahou = new General(this, "yj_xiahoushi", "shu", 3, false);
    xiahou->addSkill(new Qiaoshi);
    xiahou->addSkill(new YjYanyu);

    General *zhangyi = new General(this, "zhangyi", "shu", 4);
    zhangyi->addSkill(new Furong);
    zhangyi->addSkill(new Shizhi);
    zhangyi->addSkill(new ShizhiFilter);
    related_skills.insertMulti("shizhi", "#shizhi");

    General *zhongyao = new General(this, "zhongyao", "wei", 3);
    zhongyao->addSkill(new Huomo);
    zhongyao->addSkill(new Zuoding);
    zhongyao->addSkill(new ZuodingRecord);
    related_skills.insertMulti("zuoding", "#zuoding");

    General *zhuzhi = new General(this, "zhuzhi", "wu");
    zhuzhi->addSkill(new Anguo);

    addMetaObject<HuaiyiCard>();
    addMetaObject<HuaiyiSnatchCard>();
    addMetaObject<QinwangCard>();
    addMetaObject<YanzhuCard>();
    addMetaObject<XingxueCard>();
    addMetaObject<YjYanyuCard>();
    addMetaObject<FurongCard>();
    addMetaObject<HuomoCard>();
    addMetaObject<AnguoCard>();
    addMetaObject<MingjianCard>();
    addMetaObject<YaomingCard>();

    skills << new QinwangDraw;
}
ADD_PACKAGE(YJCM2015)
