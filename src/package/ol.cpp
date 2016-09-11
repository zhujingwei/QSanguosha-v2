#include "ol.h"
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
#include "clientstruct.h"


class AocaiViewAsSkill : public ZeroCardViewAsSkill
{
public:
    AocaiViewAsSkill() : ZeroCardViewAsSkill("aocai") {
    }

    bool isEnabledAtPlay(const Player *) const {
        return false;
    }

    bool isEnabledAtResponse(const Player *player, const QString &pattern) const {
        if (player->getPhase() != Player::NotActive || player->hasFlag("Global_AocaiFailed")) return false;
        if (pattern == "slash")
            return Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE_USE;
        else if (pattern == "peach")
            return player->getMark("Global_PreventPeach") == 0;
        else if (pattern.contains("analeptic"))
            return true;
        return false;
    }

    const Card *viewAs() const {
        AocaiCard *aocai_card = new AocaiCard;
        QString pattern = Sanguosha->currentRoomState()->getCurrentCardUsePattern();
        if (pattern == "peach+analeptic" && Self->getMark("Global_PreventPeach") > 0)
            pattern = "analeptic";
        aocai_card->setUserString(pattern);
        return aocai_card;
    }
};

class Aocai : public TriggerSkill
{
public:
    Aocai() : TriggerSkill("aocai") {
        events << CardAsked;
        view_as_skill = new AocaiViewAsSkill;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const {
        QString pattern = data.toStringList().first();
        if (player->getPhase() == Player::NotActive
            && (pattern == "slash" || pattern == "jink")
            && room->askForSkillInvoke(player, objectName(), data)) {
            QList<int> ids = room->getNCards(2, false);
            QList<int> enabled, disabled;
            foreach(int id, ids) {
                if (Sanguosha->getCard(id)->objectName().contains(pattern))
                    enabled << id;
                else
                    disabled << id;
            }
            int id = Aocai::view(room, player, ids, enabled, disabled);
            if (id != -1) {
                const Card *card = Sanguosha->getCard(id);
                room->provide(card);
                return true;
            }
        }
        return false;
    }

    static int view(Room *room, ServerPlayer *player, QList<int> &ids, QList<int> &enabled, QList<int> &disabled) {
        int result = -1, index = -1;
        LogMessage log;
        log.type = "$ViewDrawPile";
        log.from = player;
        log.card_str = IntList2StringList(ids).join("+");
        room->sendLog(log, player);

        room->broadcastSkillInvoke("aocai");
        room->notifySkillInvoked(player, "aocai");
        if (enabled.isEmpty()) {
            JsonArray arg;
            arg << "." << false << JsonUtils::toJsonArray(ids);
            room->doNotify(player, QSanProtocol::S_COMMAND_SHOW_ALL_CARDS, arg);
        } else {
            room->fillAG(ids, player, disabled);
            int id = room->askForAG(player, enabled, true, "aocai");
            if (id != -1) {
                index = ids.indexOf(id);
                ids.removeOne(id);
                result = id;
            }
            room->clearAG(player);
        }

        QList<int> &drawPile = room->getDrawPile();
        for (int i = ids.length() - 1; i >= 0; i--)
            drawPile.prepend(ids.at(i));
        room->doBroadcastNotify(QSanProtocol::S_COMMAND_UPDATE_PILE, QVariant(drawPile.length()));
        if (result == -1)
            room->setPlayerFlag(player, "Global_AocaiFailed");
        else {
            LogMessage log;
            log.type = "#AocaiUse";
            log.from = player;
            log.arg = "aocai";
            log.arg2 = QString("CAPITAL(%1)").arg(index + 1);
            room->sendLog(log);
        }
        return result;
    }
};


AocaiCard::AocaiCard() {
}

bool AocaiCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const {
    const Card *card = NULL;
    if (!user_string.isEmpty())
        card = Sanguosha->cloneCard(user_string.split("+").first());
    return card && card->targetFilter(targets, to_select, Self) && !Self->isProhibited(to_select, card, targets);
}

bool AocaiCard::targetFixed() const {
    if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE)
        return true;

    const Card *card = NULL;
    if (!user_string.isEmpty())
        card = Sanguosha->cloneCard(user_string.split("+").first());
    return card && card->targetFixed();
}

bool AocaiCard::targetsFeasible(const QList<const Player *> &targets, const Player *Self) const {
    const Card *card = NULL;
    if (!user_string.isEmpty())
        card = Sanguosha->cloneCard(user_string.split("+").first());
    return card && card->targetsFeasible(targets, Self);
}

const Card *AocaiCard::validateInResponse(ServerPlayer *user) const {
    Room *room = user->getRoom();
    QList<int> ids = room->getNCards(2, false);
    QStringList names = user_string.split("+");
    if (names.contains("slash")) names << "fire_slash" << "thunder_slash";

    QList<int> enabled, disabled;
    foreach(int id, ids) {
        if (names.contains(Sanguosha->getCard(id)->objectName()))
            enabled << id;
        else
            disabled << id;
    }

    LogMessage log;
    log.type = "#InvokeSkill";
    log.from = user;
    log.arg = "aocai";
    room->sendLog(log);

    int id = Aocai::view(room, user, ids, enabled, disabled);
    return Sanguosha->getCard(id);
}

const Card *AocaiCard::validate(CardUseStruct &cardUse) const {
    cardUse.m_isOwnerUse = false;
    ServerPlayer *user = cardUse.from;
    Room *room = user->getRoom();
    QList<int> ids = room->getNCards(2, false);
    QStringList names = user_string.split("+");
    if (names.contains("slash")) names << "fire_slash" << "thunder_slash";

    QList<int> enabled, disabled;
    foreach(int id, ids) {
        if (names.contains(Sanguosha->getCard(id)->objectName()))
            enabled << id;
        else
            disabled << id;
    }

    LogMessage log;
    log.type = "#InvokeSkill";
    log.from = user;
    log.arg = "aocai";
    room->sendLog(log);

    int id = Aocai::view(room, user, ids, enabled, disabled);
    return Sanguosha->getCard(id);
}

DuwuCard::DuwuCard() {
    mute = true;
}

bool DuwuCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const {
    if (!targets.isEmpty() || qMax(0, to_select->getHp()) != subcardsLength())
        return false;

    if (Self->getWeapon() && subcards.contains(Self->getWeapon()->getId())) {
        const Weapon *weapon = qobject_cast<const Weapon *>(Self->getWeapon()->getRealCard());
        int distance_fix = weapon->getRange() - Self->getAttackRange(false);
        if (Self->getOffensiveHorse() && subcards.contains(Self->getOffensiveHorse()->getId()))
            distance_fix += 1;
        return Self->inMyAttackRange(to_select, distance_fix);
    } else if (Self->getOffensiveHorse() && subcards.contains(Self->getOffensiveHorse()->getId())) {
        return Self->inMyAttackRange(to_select, 1);
    } else
        return Self->inMyAttackRange(to_select);
}

void DuwuCard::onEffect(const CardEffectStruct &effect) const {
    Room *room = effect.to->getRoom();

    if (subcards.length() <= 1)
        room->broadcastSkillInvoke("duwu", 2);
    else
        room->broadcastSkillInvoke("duwu", 1);

    room->damage(DamageStruct("duwu", effect.from, effect.to));
}

class DuwuViewAsSkill : public ViewAsSkill
{
public:
    DuwuViewAsSkill() : ViewAsSkill("duwu") {
    }

    bool isEnabledAtPlay(const Player *player) const {
        return player->canDiscard(player, "he") && !player->hasFlag("DuwuEnterDying");
    }

    bool viewFilter(const QList<const Card *> &, const Card *to_select) const {
        return !Self->isJilei(to_select);
    }

    const Card *viewAs(const QList<const Card *> &cards) const {
        DuwuCard *duwu = new DuwuCard;
        if (!cards.isEmpty())
            duwu->addSubcards(cards);
        return duwu;
    }
};

class Duwu : public TriggerSkill
{
public:
    Duwu() : TriggerSkill("duwu") {
        events << QuitDying;
        view_as_skill = new DuwuViewAsSkill;
    }

    bool triggerable(const ServerPlayer *target) const {
        return target != NULL;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *, QVariant &data) const {
        DyingStruct dying = data.value<DyingStruct>();
        if (dying.damage && dying.damage->getReason() == "duwu" && !dying.damage->chain && !dying.damage->transfer) {
            ServerPlayer *from = dying.damage->from;
            if (from && from->isAlive()) {
                room->setPlayerFlag(from, "DuwuEnterDying");
                room->loseHp(from, 1);
            }
        }
        return false;
    }
};

class Dujin : public DrawCardsSkill
{
public:
    Dujin() : DrawCardsSkill("dujin") {
        frequency = Frequent;
    }

    int getDrawNum(ServerPlayer *player, int n) const {
        if (player->askForSkillInvoke(this)) {
            player->getRoom()->broadcastSkillInvoke(objectName());
            return n + player->getEquips().length() / 2 + 1;
        } else
            return n;
    }
};

QingyiCard::QingyiCard() {
    mute = true;
}

bool QingyiCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const {
    Slash *slash = new Slash(NoSuit, 0);
    slash->setSkillName("qingyi");
    slash->deleteLater();
    return slash->targetFilter(targets, to_select, Self);
}

void QingyiCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const {
    foreach(ServerPlayer *target, targets) {
        if (!source->canSlash(target, NULL, false))
            targets.removeOne(target);
    }

    if (targets.length() > 0) {
        Slash *slash = new Slash(Card::NoSuit, 0);
        slash->setSkillName("_qingyi");
        room->useCard(CardUseStruct(slash, source, targets));
    }
}

class QingyiViewAsSkill : public ZeroCardViewAsSkill
{
public:
    QingyiViewAsSkill() : ZeroCardViewAsSkill("qingyi") {
        response_pattern = "@@qingyi";
    }

    bool isEnabledAtPlay(const Player *) const {
        return false;
    }

    bool isEnabledAtResponse(const Player *, const QString &pattern) const {
        return pattern == "@@qingyi";
    }

    const Card *viewAs() const {
        return new QingyiCard;
    }
};

class Qingyi : public TriggerSkill
{
public:
    Qingyi() : TriggerSkill("qingyi") {
        events << EventPhaseChanging;
        view_as_skill = new QingyiViewAsSkill;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const {
        PhaseChangeStruct change = data.value<PhaseChangeStruct>();
        if (change.to == Player::Judge && !player->isSkipped(Player::Judge)
            && !player->isSkipped(Player::Draw)) {
            if (Slash::IsAvailable(player) && room->askForUseCard(player, "@@qingyi", "@qingyi-slash")) {
                player->skip(Player::Judge, true);
                player->skip(Player::Draw, true);
            }
        }
        return false;
    }
};

class Shixin : public TriggerSkill
{
public:
    Shixin() : TriggerSkill("shixin") {
        events << DamageInflicted;
        frequency = Compulsory;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const {
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.nature == DamageStruct::Fire) {
            room->notifySkillInvoked(player, objectName());
            room->broadcastSkillInvoke(objectName());

            LogMessage log;
            log.type = "#ShixinProtect";
            log.from = player;
            log.arg = QString::number(damage.damage);
            log.arg2 = "fire_nature";
            room->sendLog(log);
            return true;
        }
        return false;
    }
};

class Fenyin : public TriggerSkill
{
public:
    Fenyin() : TriggerSkill("fenyin") {
        events << EventPhaseStart << CardUsed << CardResponded;
        global = true;
        frequency = Frequent;
    }

    bool triggerable(const ServerPlayer *target) const {
        return target != NULL && target->isAlive();
    }

    int getPriority(TriggerEvent triggerEvent) const {
        if (triggerEvent == EventPhaseStart)
            return 6;

        return TriggerSkill::getPriority(triggerEvent);
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const {
        if (triggerEvent == EventPhaseStart) {
            if (player->getPhase() == Player::RoundStart)
                player->setMark(objectName(), 0);

            return false;
        }

        const Card *c = NULL;
        if (triggerEvent == CardUsed) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (player == use.from)
                c = use.card;
        } else {
            CardResponseStruct resp = data.value<CardResponseStruct>();
            if (resp.m_isUse)
                c = resp.m_card;
        }

        if (c == NULL || c->isKindOf("SkillCard") || player->getPhase() == Player::NotActive)
            return false;

        if (player->getMark(objectName()) != 0) {
            Card::Color old_color = static_cast<Card::Color>(player->getMark(objectName()) - 1);
            if (old_color != c->getColor() && player->hasSkill(this) && player->askForSkillInvoke(this, QVariant::fromValue(c))) {
                room->broadcastSkillInvoke(objectName());
                player->drawCards(1);
            }
        }

        player->setMark(objectName(), static_cast<int>(c->getColor()) + 1);

        return false;
    }
};

class TunchuDraw : public DrawCardsSkill
{
public:
    TunchuDraw() : DrawCardsSkill("tunchu") {

    }

    int getDrawNum(ServerPlayer *player, int n) const {
        if (player->askForSkillInvoke("tunchu")) {
            player->setFlags("tunchu");
            player->getRoom()->broadcastSkillInvoke("tunchu");
            return n + 2;
        }

        return n;
    }
};

class TunchuEffect : public TriggerSkill
{
public:
    TunchuEffect() : TriggerSkill("#tunchu-effect") {
        events << AfterDrawNCards;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const {
        if (player->hasFlag("tunchu") && !player->isKongcheng()) {
            const Card *c = room->askForExchange(player, "tunchu", 1, 1, false, "@tunchu-put");
            if (c != NULL)
                player->addToPile("food", c);
            delete c;
        }

        return false;
    }
};

class Tunchu : public TriggerSkill
{
public:
    Tunchu() : TriggerSkill("#tunchu-disable") {
        events << EventLoseSkill << EventAcquireSkill << CardsMoveOneTime;
    }

    bool triggerable(const ServerPlayer *target) const {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const {
        if (triggerEvent == EventLoseSkill && data.toString() == "tunchu") {
            room->removePlayerCardLimitation(player, "use", "Slash,Duel$0");
        } else if (triggerEvent == EventAcquireSkill && data.toString() == "tunchu") {
            if (!player->getPile("food").isEmpty())
                room->setPlayerCardLimitation(player, "use", "Slash,Duel", false);
        } else if (triggerEvent == CardsMoveOneTime && player->isAlive() && player->hasSkill("tunchu", true)) {
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (move.to == player && move.to_place == Player::PlaceSpecial && move.to_pile_name == "food") {
                if (player->getPile("food").length() == 1)
                    room->setPlayerCardLimitation(player, "use", "Slash,Duel", false);
            } else if (move.from == player && move.from_places.contains(Player::PlaceSpecial)
                && move.from_pile_names.contains("food")) {
                if (player->getPile("food").isEmpty())
                    room->removePlayerCardLimitation(player, "use", "Slash,Duel$0");
            }
        }
        return false;
    }
};

ShuliangCard::ShuliangCard() {
    target_fixed = true;
    will_throw = false;
    handling_method = Card::MethodNone;
}

void ShuliangCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const {
    CardMoveReason r(CardMoveReason::S_REASON_REMOVE_FROM_PILE, source->objectName(), objectName(), QString());
    room->moveCardTo(this, NULL, Player::DiscardPile, r, true);
}

class ShuliangVS : public OneCardViewAsSkill
{
public:
    ShuliangVS() : OneCardViewAsSkill("shuliang") {
        response_pattern = "@@shuliang";
        filter_pattern = ".|.|.|food";
        expand_pile = "food";
    }

    const Card *viewAs(const Card *originalCard) const {
        ShuliangCard *c = new ShuliangCard;
        c->addSubcard(originalCard);
        return c;
    }
};

class Shuliang : public TriggerSkill
{
public:
    Shuliang() : TriggerSkill("shuliang") {
        events << EventPhaseStart;
        view_as_skill = new ShuliangVS;
    }

    bool triggerable(const ServerPlayer *target) const {
        return target != NULL && target->isAlive() && target->getPhase() == Player::Finish && target->isKongcheng();
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const {
        foreach(ServerPlayer *const &p, room->getAllPlayers()) {
            if (!TriggerSkill::triggerable(p))
                continue;

            if (!p->getPile("food").isEmpty()) {
                if (room->askForUseCard(p, "@@shuliang", "@shuliang", -1, Card::MethodNone))
                    player->drawCards(2, objectName());
            }
        }

        return false;
    }
};


ZhanyiViewAsBasicCard::ZhanyiViewAsBasicCard() {
    m_skillName = "_zhanyi";
    will_throw = false;
}

bool ZhanyiViewAsBasicCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const {
    if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE_USE) {
        const Card *card = NULL;
        if (!user_string.isEmpty())
            card = Sanguosha->cloneCard(user_string.split("+").first());
        return card && card->targetFilter(targets, to_select, Self) && !Self->isProhibited(to_select, card, targets);
    } else if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE) {
        return false;
    }

    const Card *card = Self->tag.value("zhanyi").value<const Card *>();
    return card && card->targetFilter(targets, to_select, Self) && !Self->isProhibited(to_select, card, targets);
}

bool ZhanyiViewAsBasicCard::targetFixed() const {
    if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE_USE) {
        const Card *card = NULL;
        if (!user_string.isEmpty())
            card = Sanguosha->cloneCard(user_string.split("+").first());
        return card && card->targetFixed();
    } else if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE) {
        return true;
    }

    const Card *card = Self->tag.value("zhanyi").value<const Card *>();
    return card && card->targetFixed();
}

bool ZhanyiViewAsBasicCard::targetsFeasible(const QList<const Player *> &targets, const Player *Self) const {
    if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE_USE) {
        const Card *card = NULL;
        if (!user_string.isEmpty())
            card = Sanguosha->cloneCard(user_string.split("+").first());
        return card && card->targetsFeasible(targets, Self);
    } else if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE) {
        return true;
    }

    const Card *card = Self->tag.value("zhanyi").value<const Card *>();
    return card && card->targetsFeasible(targets, Self);
}

const Card *ZhanyiViewAsBasicCard::validate(CardUseStruct &card_use) const {
    ServerPlayer *zhuling = card_use.from;
    Room *room = zhuling->getRoom();

    QString to_zhanyi = user_string;
    if (user_string == "slash" && Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE_USE) {
        QStringList guhuo_list;
        guhuo_list << "slash";
        if (!Config.BanPackages.contains("maneuvering"))
            guhuo_list << "normal_slash" << "thunder_slash" << "fire_slash";
        to_zhanyi = room->askForChoice(zhuling, "zhanyi_slash", guhuo_list.join("+"));
    }

    const Card *card = Sanguosha->getCard(subcards.first());
    QString user_str;
    if (to_zhanyi == "slash") {
        if (card->isKindOf("Slash"))
            user_str = card->objectName();
        else
            user_str = "slash";
    } else if (to_zhanyi == "normal_slash")
        user_str = "slash";
    else
        user_str = to_zhanyi;
    Card *use_card = Sanguosha->cloneCard(user_str, card->getSuit(), card->getNumber());
    use_card->setSkillName("_zhanyi");
    use_card->addSubcard(subcards.first());
    use_card->deleteLater();
    return use_card;
}

const Card *ZhanyiViewAsBasicCard::validateInResponse(ServerPlayer *zhuling) const {
    Room *room = zhuling->getRoom();

    QString to_zhanyi;
    if (user_string == "peach+analeptic") {
        QStringList guhuo_list;
        guhuo_list << "peach";
        if (!Config.BanPackages.contains("maneuvering"))
            guhuo_list << "analeptic";
        to_zhanyi = room->askForChoice(zhuling, "zhanyi_saveself", guhuo_list.join("+"));
    } else if (user_string == "slash") {
        QStringList guhuo_list;
        guhuo_list << "slash";
        if (!Config.BanPackages.contains("maneuvering"))
            guhuo_list << "normal_slash" << "thunder_slash" << "fire_slash";
        to_zhanyi = room->askForChoice(zhuling, "zhanyi_slash", guhuo_list.join("+"));
    } else
        to_zhanyi = user_string;

    const Card *card = Sanguosha->getCard(subcards.first());
    QString user_str;
    if (to_zhanyi == "slash") {
        if (card->isKindOf("Slash"))
            user_str = card->objectName();
        else
            user_str = "slash";
    } else if (to_zhanyi == "normal_slash")
        user_str = "slash";
    else
        user_str = to_zhanyi;
    Card *use_card = Sanguosha->cloneCard(user_str, card->getSuit(), card->getNumber());
    use_card->setSkillName("_zhanyi");
    use_card->addSubcard(subcards.first());
    use_card->deleteLater();
    return use_card;
}

ZhanyiCard::ZhanyiCard() {
    target_fixed = true;
}

void ZhanyiCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const {
    room->loseHp(source);
    if (source->isAlive()) {
        const Card *c = Sanguosha->getCard(subcards.first());
        if (c->getTypeId() == Card::TypeBasic) {
            room->setPlayerMark(source, "ViewAsSkill_zhanyiEffect", 1);
        } else if (c->getTypeId() == Card::TypeEquip)
            source->setFlags("zhanyiEquip");
        else if (c->getTypeId() == Card::TypeTrick) {
            source->drawCards(2, "zhanyi");
            room->setPlayerFlag(source, "zhanyiTrick");
        }
    }
}

class ZhanyiNoDistanceLimit : public TargetModSkill
{
public:
    ZhanyiNoDistanceLimit() : TargetModSkill("#zhanyi-trick") {
        pattern = "^SkillCard";
    }

    int getDistanceLimit(const Player *from, const Card *) const {
        return from->hasFlag("zhanyiTrick") ? 1000 : 0;
    }
};

class ZhanyiDiscard2 : public TriggerSkill
{
public:
    ZhanyiDiscard2() : TriggerSkill("#zhanyi-equip") {
        events << TargetSpecified;
    }

    bool triggerable(const ServerPlayer *target) const {
        return target != NULL && target->isAlive() && target->hasFlag("zhanyiEquip");
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *, QVariant &data) const {
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card == NULL || !use.card->isKindOf("Slash"))
            return false;


        foreach(ServerPlayer *p, use.to) {
            if (p->isNude())
                continue;

            if (p->getCardCount() <= 2) {
                DummyCard dummy;
                dummy.addSubcards(p->getCards("he"));
                room->throwCard(&dummy, p);
            } else
                room->askForDiscard(p, "zhanyi_equip", 2, 2, false, true, "@zhanyiequip_discard");
        }
        return false;
    }
};

class Zhanyi : public OneCardViewAsSkill
{
public:
    Zhanyi() : OneCardViewAsSkill("zhanyi") {

    }

    bool isResponseOrUse() const {
        return Self->getMark("ViewAsSkill_zhanyiEffect") > 0;
    }

    bool isEnabledAtPlay(const Player *player) const {
        if (!player->hasUsed("ZhanyiCard"))
            return true;

        if (player->getMark("ViewAsSkill_zhanyiEffect") > 0)
            return true;

        return false;
    }

    bool isEnabledAtResponse(const Player *player, const QString &pattern) const {
        if (player->getMark("ViewAsSkill_zhanyiEffect") == 0) return false;
        if (pattern.startsWith(".") || pattern.startsWith("@")) return false;
        if (pattern == "peach" && player->getMark("Global_PreventPeach") > 0) return false;
        for (int i = 0; i < pattern.length(); i++) {
            QChar ch = pattern[i];
            if (ch.isUpper() || ch.isDigit()) return false; // This is an extremely dirty hack!! For we need to prevent patterns like 'BasicCard'
        }
        return !(pattern == "nullification");
    }

    QDialog *getDialog() const {
        return GuhuoDialog::getInstance("zhanyi", true, false);
    }

    bool viewFilter(const Card *to_select) const {
        if (Self->getMark("ViewAsSkill_zhanyiEffect") > 0)
            return to_select->isKindOf("BasicCard");
        else
            return true;
    }

    const Card *viewAs(const Card *originalCard) const {
        if (Self->getMark("ViewAsSkill_zhanyiEffect") == 0) {
            ZhanyiCard *zy = new ZhanyiCard;
            zy->addSubcard(originalCard);
            return zy;
        }

        if (Sanguosha->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE
            || Sanguosha->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE_USE) {
            ZhanyiViewAsBasicCard *card = new ZhanyiViewAsBasicCard;
            card->setUserString(Sanguosha->getCurrentCardUsePattern());
            card->addSubcard(originalCard);
            return card;
        }

        const Card *c = Self->tag.value("zhanyi").value<const Card *>();
        if (c) {
            ZhanyiViewAsBasicCard *card = new ZhanyiViewAsBasicCard;
            card->setUserString(c->objectName());
            card->addSubcard(originalCard);
            return card;
        } else
            return NULL;
    }
};

class ZhanyiRemove : public TriggerSkill
{
public:
    ZhanyiRemove() : TriggerSkill("#zhanyi-basic") {
        events << EventPhaseChanging;
    }

    bool triggerable(const ServerPlayer *target) const {
        return target != NULL && target->isAlive() && target->getMark("ViewAsSkill_zhanyiEffect") > 0;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const {
        PhaseChangeStruct change = data.value<PhaseChangeStruct>();
        if (change.to == Player::NotActive)
            room->setPlayerMark(player, "ViewAsSkill_zhanyiEffect", 0);

        return false;
    }
};

class OlYuhua :public TriggerSkill
{
public:
    OlYuhua() : TriggerSkill("olyuhua") {
        events << EventPhaseStart << EventPhaseEnd;
        frequency = Compulsory;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const {
        if (player->getPhase() != Player::Discard)
            return false;

        if (triggerEvent == EventPhaseStart) {
            room->sendCompulsoryTriggerLog(player, objectName(), true);
            room->notifySkillInvoked(player, objectName());
            player->broadcastSkillInvoke(objectName());
            room->setPlayerCardLimitation(player, "discard", "^BasicCard|.|.|hand", true);

            int num = 0;
            foreach(const Card *card, player->getHandcards()) {
                if (!card->isKindOf("BasicCard"))
                    continue;
                num++;
            }

            if (num != player->getHandcardNum())
                player->setFakeHandCardNum(num);
        } else if (triggerEvent == EventPhaseEnd) {
            room->removePlayerCardLimitation(player, "discard", "^BasicCard|.|.|hand$1");
            player->setFakeHandCardNum(-1);
        }
        return false;
    }
};

class OlQirang : public TriggerSkill
{
public:
    OlQirang() : TriggerSkill("olqirang") {
        events << CardsMoveOneTime;
        frequency = Frequent;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const {
        CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
        if (!move.to)
            return false;
        if (move.to->objectName() != player->objectName())
            return false;
        if (move.to_place != Player::PlaceEquip)
            return false;
        if (!player->askForSkillInvoke("olqirang", data))
            return false;

        room->broadcastSkillInvoke("olqirang");
        room->notifySkillInvoked(player, "olqirang");

        QList<int> drawPile = room->getDrawPile();
        QList<int> trickIDs;
        foreach(int id, drawPile) {
            if (Sanguosha->getCard(id)->isKindOf("TrickCard"))
                trickIDs.append(id);
        }

        if (trickIDs.isEmpty()) {
            LogMessage msg;
            msg.type = "#olqirang-failed";
            room->sendLog(msg);
            return false;
        }

        room->fillAG(trickIDs, player);
        int trick_id = room->askForAG(player, trickIDs, true, "olqirang");
        room->clearAG(player);
        if (trick_id != -1)
            room->obtainCard(player, trick_id, true);

        return false;
    }
};

class OlShenxian : public TriggerSkill
{
public:
    OlShenxian() : TriggerSkill("olshenxian") {
        events << CardsMoveOneTime << EventPhaseStart;
        frequency = Frequent;
    }

    bool triggerable(const ServerPlayer *target) const {
        return target;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const {
        if (triggerEvent == EventPhaseStart) {
            if (player->getPhase() == Player::NotActive) {
                foreach(ServerPlayer *p, room->getAlivePlayers()) {
                    if (p->getMark(objectName()) > 0)
                        p->setMark(objectName(), 0);
                }
            }
        } else {
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (TriggerSkill::triggerable(player) && player->getPhase() == Player::NotActive && player->getMark(objectName()) == 0 && move.from && move.from->isAlive()
                && move.from->objectName() != player->objectName() && (move.from_places.contains(Player::PlaceHand) || move.from_places.contains(Player::PlaceEquip))
                && (move.reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_DISCARD) {
                foreach(int id, move.card_ids) {
                    if (Sanguosha->getCard(id)->getTypeId() == Card::TypeBasic) {
                        if (room->askForSkillInvoke(player, objectName(), data)) {
                            room->broadcastSkillInvoke(objectName());
                            player->drawCards(1, "olshenxian");
                            player->setMark(objectName(), 1);
                        }
                        break;
                    }
                }
            }
        }
        return false;
    }
};

class Biluan : public PhaseChangeSkill
{
public:
    Biluan() : PhaseChangeSkill("biluan") {

    }

    bool triggerable(const ServerPlayer *target) const {
        return target != NULL && target->isAlive();
    }

    bool onPhaseChange(ServerPlayer *target) const {
        Room *room = target->getRoom();
        switch (target->getPhase()) {
        case (Player::Draw) :
            if (PhaseChangeSkill::triggerable(target) && target->askForSkillInvoke(this)) {
                room->setPlayerMark(target, "biluan", 1);
                room->broadcastSkillInvoke(objectName());
                return true;
            }
                            break;
        case (Player::RoundStart) :
            room->setPlayerMark(target, "biluan", 0);
            break;
        default:

            break;
        }

        return false;
    }
};

class BiluanDist : public DistanceSkill
{
public:
    BiluanDist() : DistanceSkill("#biluan-dist") {

    }

    int getCorrect(const Player *, const Player *to) const {
        if (to->getMark("biluan") == 1) {
            QList<const Player *> sib = to->getAliveSiblings();
            sib << to;
            QSet<QString> kingdoms;
            foreach(const Player *p, sib)
                kingdoms.insert(p->getKingdom());

            return kingdoms.count();
        }

        return 0;
    }
};

class Lixia : public PhaseChangeSkill
{
public:
    Lixia() : PhaseChangeSkill("lixia") {

    }

    bool triggerable(const ServerPlayer *target) const {
        return target != NULL && target->isAlive();
    }

    bool onPhaseChange(ServerPlayer *target) const {
        Room *r = target->getRoom();
        switch (target->getPhase()) {
        case (Player::Finish) : {
            QList<ServerPlayer *> misterious1s = r->getOtherPlayers(target);
            foreach(ServerPlayer *misterious1, misterious1s) {
                if (TriggerSkill::triggerable(misterious1) && !target->inMyAttackRange(misterious1) && misterious1->askForSkillInvoke(this, QVariant::fromValue(target))) {
                    r->broadcastSkillInvoke(objectName());
                    misterious1->drawCards(1, objectName());
                    r->addPlayerMark(misterious1, "lixia", 1);
                }
            }
        }
                                break;
        case (Player::RoundStart) :
            target->getRoom()->setPlayerMark(target, "lixia", 0);
            break;
        default:

            break;
        }

        return false;
    }
};

class LixiaDist : public DistanceSkill
{
public:
    LixiaDist() : DistanceSkill("#lixia-dist") {

    }

    int getCorrect(const Player *, const Player *to) const {
        return -to->getMark("lixia");
    }
};

class Yishe : public TriggerSkill
{
public:
    Yishe() : TriggerSkill("yishe") {
        events << EventPhaseStart << CardsMoveOneTime;
        frequency = Frequent;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const {
        if (triggerEvent == EventPhaseStart) {
            if (player->getPile("rice").isEmpty() && player->getPhase() == Player::Finish) {
                if (player->askForSkillInvoke(this)) {
                    room->broadcastSkillInvoke(objectName());
                    player->drawCards(2, objectName());
                    if (!player->isNude()) {
                        const Card *dummy = NULL;
                        if (player->getCardCount(true) <= 2) {
                            DummyCard *dummy2 = new DummyCard;
                            dummy2->addSubcards(player->getCards("he"));
                            dummy2->deleteLater();
                            dummy = dummy2;
                        } else
                            dummy = room->askForExchange(player, objectName(), 2, 2, true, "@yishe");

                        player->addToPile("rice", dummy);
                        delete dummy;
                    }
                }
            }
        } else {
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (move.from == player && move.from_pile_names.contains("rice") && move.from->getPile("rice").isEmpty())
                room->recover(player, RecoverStruct(player));
        }

        return false;
    }
};

BushiCard::BushiCard() {
    target_fixed = true;
    will_throw = false;
    handling_method = Card::MethodNone;
}

void BushiCard::onUse(Room *, const CardUseStruct &card_use) const {
    // log

    card_use.from->obtainCard(this);
}

class BushiVS : public OneCardViewAsSkill
{
public:
    BushiVS() : OneCardViewAsSkill("bushi") {
        response_pattern = "@@bushi";
        filter_pattern = ".|.|.|rice,%rice";
        expand_pile = "rice,%rice";
    }

    const Card *viewAs(const Card *card) const {
        BushiCard *bs = new BushiCard;
        bs->addSubcard(card);
        return bs;
    }
};

class Bushi : public TriggerSkill
{
public:
    Bushi() : TriggerSkill("bushi") {
        events << Damage << Damaged;
        view_as_skill = new BushiVS;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const {
        if (player->getPile("rice").isEmpty())
            return false;

        DamageStruct damage = data.value<DamageStruct>();
        ServerPlayer *p = damage.to;

        if (damage.from->isDead() || damage.to->isDead())
            return false;

        for (int i = 0; i < damage.damage; ++i) {
            if (!room->askForUseCard(p, "@@bushi", "@bushi", -1, Card::MethodNone))
                break;

            if (p->isDead() || player->getPile("rice").isEmpty())
                break;
        }

        return false;
    }
};

MidaoCard::MidaoCard() {
    target_fixed = true;
    will_throw = false;
    handling_method = Card::MethodNone;
}

void MidaoCard::onUse(Room *, const CardUseStruct &card_use) const {
    card_use.from->tag["midao"] = subcards.first();
}

class MidaoVS : public OneCardViewAsSkill
{
public:
    MidaoVS() : OneCardViewAsSkill("midao") {
        response_pattern = "@@midao";
        filter_pattern = ".|.|.|rice";
        expand_pile = "rice";
    }

    const Card *viewAs(const Card *card) const {
        MidaoCard *bs = new MidaoCard;
        bs->addSubcard(card);
        return bs;
    }
};

class Midao : public RetrialSkill
{
public:
    Midao() : RetrialSkill("midao", false) {
        view_as_skill = new MidaoVS;
    }

    const Card *onRetrial(ServerPlayer *player, JudgeStruct *judge) const {
        if (player->getPile("rice").isEmpty())
            return NULL;

        QStringList prompt_list;
        prompt_list << "@midao-card" << judge->who->objectName()
            << objectName() << judge->reason << QString::number(judge->card->getEffectiveId());
        QString prompt = prompt_list.join(":");

        player->tag.remove("midao");
        player->tag["judgeData"] = QVariant::fromValue(judge);
        Room *room = player->getRoom();
        bool invoke = room->askForUseCard(player, "@@midao", prompt, -1, Card::MethodNone);
        player->tag.remove("judgeData");
        if (invoke && player->tag.contains("midao")) {
            int id = player->tag.value("midao", player->getPile("rice").first()).toInt();
            return Sanguosha->getCard(id);
        }

        return NULL;
    }
};

class OldMoshiViewAsSkill : public OneCardViewAsSkill
{
public:
    OldMoshiViewAsSkill() : OneCardViewAsSkill("Omoshi") {
        filter_pattern = ".";
        response_or_use = true;
        response_pattern = "@@Omoshi";
    }

    const Card *viewAs(const Card *originalCard) const {
        const Card *ori = Self->tag[objectName()].value<const Card *>();
        if (ori == NULL) return NULL;
        Card *a = Sanguosha->cloneCard(ori->objectName());
        a->addSubcard(originalCard);
        a->setSkillName(objectName());
        return a;

    }

    bool isEnabledAtPlay(const Player *) const {
        return false;
    }
};

class OldMoshi : public TriggerSkill  //a souvenir for Xusine......
{
public:
    OldMoshi() : TriggerSkill("Omoshi") {
        view_as_skill = new OldMoshiViewAsSkill;
        events << EventPhaseStart << CardUsed;
    }

    QDialog *getDialog() const {
        return GuhuoDialog::getInstance(objectName(), true, true, false, false, true);
    }

    bool trigger(TriggerEvent e, Room *room, ServerPlayer *player, QVariant &data) const {
        if (e == CardUsed && player->getPhase() == Player::Play) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card->isKindOf("SkillCard") || use.card->isKindOf("EquipCard")) return false;
            QStringList list = player->tag[objectName()].toStringList();
            list.append(use.card->objectName());
            player->tag[objectName()] = list;
        } else if (e == EventPhaseStart && player->getPhase() == Player::Finish) {
            QStringList list = player->tag[objectName()].toStringList();
            player->tag.remove(objectName());
            if (list.isEmpty()) return false;
            room->setPlayerProperty(player, "allowed_guhuo_dialog_buttons", list.join("+"));
            try {
                const Card *first = room->askForUseCard(player, "@@Omoshi", "@moshi_ask_first");
                if (first) {
                    list.removeOne(first->objectName());
                    room->setPlayerProperty(player, "allowed_guhuo_dialog_buttons", list.join("+"));
                    room->askForUseCard(player, "@@Omoshi", "@moshi_ask_second");
                }
            } catch (TriggerEvent e) {
                if (e == TurnBroken || e == StageChange) {
                    room->setPlayerProperty(player, "allowed_guhuo_dialog_buttons", QString());
                }
                throw e;
            }
        }
        return false;
    }
};


class MoshiViewAsSkill : public OneCardViewAsSkill
{
public:
    MoshiViewAsSkill() : OneCardViewAsSkill("moshi") {
        response_or_use = true;
        response_pattern = "@@moshi";
    }

    bool viewFilter(const Card *to_select) const {
        if (to_select->isEquipped()) return false;
        QString ori = Self->property("moshi").toString();
        if (ori.isEmpty()) return NULL;
        Card *a = Sanguosha->cloneCard(ori);
        a->addSubcard(to_select);
        return a->isAvailable(Self);
    }

    const Card *viewAs(const Card *originalCard) const {
        QString ori = Self->property("moshi").toString();
        if (ori.isEmpty()) return NULL;
        Card *a = Sanguosha->cloneCard(ori);
        a->addSubcard(originalCard);
        a->setSkillName(objectName());
        return a;
    }

    bool isEnabledAtPlay(const Player *) const {
        return false;
    }
};


class Moshi : public TriggerSkill
{
public:
    Moshi() : TriggerSkill("moshi") {
        view_as_skill = new MoshiViewAsSkill;
        events << EventPhaseStart << CardUsed;
    }
    bool trigger(TriggerEvent e, Room *room, ServerPlayer *player, QVariant &data) const {
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
                const Card *first = room->askForUseCard(player, "@@moshi", QString("@moshi_ask:::%1").arg(list.takeFirst()));
                if (first != NULL && !list.isEmpty() && !(player->isKongcheng() && player->getHandPile().isEmpty())) {
                    room->setPlayerProperty(player, "moshi", list.first());
                    Q_ASSERT(list.length() == 1);
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
};

class FengpoRecord : public TriggerSkill
{
public:
    FengpoRecord() : TriggerSkill("#fengpo-record") {
        events << EventPhaseChanging << PreCardUsed << CardResponded;
        global = true;
    }

    bool triggerable(const ServerPlayer *target) const {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const {
        if (triggerEvent == EventPhaseChanging) {
            if (data.value<PhaseChangeStruct>().to == Player::Play)
                room->setPlayerFlag(player, "-fengporec");
        } else {
            if (player->getPhase() != Player::Play)
                return false;

            const Card *c = NULL;
            if (triggerEvent == PreCardUsed)
                c = data.value<CardUseStruct>().card;
            else {
                CardResponseStruct resp = data.value<CardResponseStruct>();
                if (resp.m_isUse)
                    c = resp.m_card;
            }

            if (c == NULL || c->isKindOf("SkillCard"))
                return false;

            if (triggerEvent == PreCardUsed && !player->hasFlag("fengporec"))
                room->setCardFlag(c, "fengporecc");

            room->setPlayerFlag(player, "fengporec");
        }

        return false;
    }
};

class Fengpo : public TriggerSkill
{
public:
    Fengpo() : TriggerSkill("fengpo") {
        events << TargetSpecified << DamageCaused << CardFinished;
    }

    bool trigger(TriggerEvent e, Room *room, ServerPlayer *player, QVariant &data) const {
        if (e == TargetSpecified) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.to.length() != 1) return false;
            if (use.to.first()->isKongcheng()) return false;
            if (use.card == NULL) return false;
            if (!use.card->isKindOf("Slash") && !use.card->isKindOf("Duel")) return false;
            if (!use.card->hasFlag("fengporecc")) return false;
            QStringList choices;
            int n = 0;
            foreach(const Card *card, use.to.first()->getHandcards())
                if (card->getSuit() == Card::Diamond)
                    ++n;
            if (n > 0) choices << "drawCards";
            choices << "addDamage" << "cancel";
            QString choice = room->askForChoice(player, objectName(), choices.join("+"));
            if (choice == "drawCards") {
                room->broadcastSkillInvoke(objectName(), 1);
                player->drawCards(n);
            } else if (choice == "addDamage") {
                room->broadcastSkillInvoke(objectName(), 2);
                player->tag["fengpoaddDamage" + use.card->toString()] = n;
            }
        } else if (e == DamageCaused) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.card == NULL || damage.from == NULL)
                return false;
            if (damage.from->tag.contains("fengpoaddDamage" + damage.card->toString()) && (damage.card->isKindOf("Slash") || damage.card->isKindOf("Duel"))) {
                damage.damage += damage.from->tag.value("fengpoaddDamage" + damage.card->toString()).toInt();
                data = QVariant::fromValue(damage);
                damage.from->tag.remove("fengpoaddDamage" + damage.card->toString());
            }
        } else if (e == CardFinished) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.to.length() != 1) return false;
            if (use.to.first()->isKongcheng()) return false;
            if (!use.card->isKindOf("Slash") || !use.card->isKindOf("Duel")) return false;
            if (player->tag.contains("fengpoaddDamage" + use.card->toString()))
                player->tag.remove("fengpoaddDamage" + use.card->toString());
        }
        return false;
    }
};

OlAnxuCard::OlAnxuCard() {
    mute = true;
    will_throw = true;
    target_fixed = false;
}

bool OlAnxuCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const {
    if (to_select == Self)
        return false;

    if (targets.isEmpty())
        return true;

    if (targets.length() == 1)
        return to_select->getHandcardNum() != targets.first()->getHandcardNum();

    return false;
}

bool OlAnxuCard::targetsFeasible(const QList<const Player *> &targets, const Player *) const {
    return targets.length() == 2;
}

void OlAnxuCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const {
    ServerPlayer *playerA = targets.first();
    ServerPlayer *playerB = targets.last();
    if (playerA->getHandcardNum() < playerB->getHandcardNum()) {
        playerA = targets.last();
        playerB = targets.first();
    }
    if (playerA->isKongcheng())
        return;

    room->setPlayerFlag(playerB, "olanxu_target"); // For AI
    const Card *card = room->askForExchange(playerA, "olanxu", 1, 1, false, QString("@olanxu:%1:%2").arg(source->objectName()).arg(playerB->objectName()));
    room->setPlayerFlag(playerB, "-olanxu_target"); // For AI
    if (!card)
        card = playerA->getRandomHandCard();

    room->obtainCard(playerB, card);

    if (playerA->getHandcardNum() == playerB->getHandcardNum()) {
        QString choices = source->getLostHp() > 0 ? "draw+recover" : "draw";
        QString choice = room->askForChoice(source, "olanxu", choices);
        if (choice == "draw")
            room->drawCards(source, 1, "olanxu");
        else if (choice == "recover") {
            RecoverStruct recover;
            recover.recover = 1;
            recover.who = source;
            room->recover(source, recover);
        }
    }
}

class OlAnxu : public ZeroCardViewAsSkill
{
public:
    OlAnxu() : ZeroCardViewAsSkill("olanxu") {
    }

    const Card *viewAs() const {
        return new OlAnxuCard;
    }

    bool isEnabledAtPlay(const Player *player) const {
        return !player->hasUsed("OlAnxuCard") && player->getSiblings().length() >= 2;
    }
};

class OlChenqing : public TriggerSkill
{
public:
    OlChenqing() : TriggerSkill("olchenqing") {
        events << Dying << EventPhaseStart;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const {
        if (triggerEvent == Dying) {
            DyingStruct dying = data.value<DyingStruct>();
            if (dying.who != player)
                return false;

            QList<ServerPlayer *> alives = room->getAlivePlayers();
            foreach(ServerPlayer *source, alives) {
                if (source->hasSkill(this) && source->getMark("@advise") == 0) {
                    if (doChenqing(room, source, player)) {
                        if (player->getHp() > 0)
                            return false;
                    }
                }
            }
        } else if (triggerEvent == EventPhaseStart) {
            if (room->getTag("ExtraTurn").toBool())
                return false;
            if (player->getPhase() == Player::RoundStart) {
                if (player->getMark("@advise") > 0 && player->hasSkill(this, true))
                    player->loseAllMarks("@advise");
            }
        }
        return false;
    }

    bool triggerable(const Player *target) const {
        if (target)
            return target->isAlive();

        return false;
    }

private:
    bool doChenqing(Room *room, ServerPlayer *source, ServerPlayer *victim) const {
        QList<ServerPlayer *> targets = room->getOtherPlayers(source);
        targets.removeOne(victim);
        if (targets.isEmpty())
            return false;

        ServerPlayer *target = room->askForPlayerChosen(source, targets, "olchenqing", QString("@olchenqing:%1").arg(victim->objectName()), false, true);
        if (target) {
            source->gainMark("@advise");
            room->broadcastSkillInvoke("olchenqing");

            room->drawCards(target, 4, "olchenqing");

            const Card *to_discard = NULL;
            if (target->getCardCount() > 4) {
                to_discard = room->askForExchange(target, "olchenqing", 4, 4, true, QString("@olchenqing-exchange:%1:%2").arg(source->objectName()).arg(victim->objectName()), false);
            } else {
                DummyCard *dummy = new DummyCard;
                dummy->addSubcards(target->getCards("he"));
                to_discard = dummy;
            }
            QSet<Card::Suit> suit;
            foreach(int id, to_discard->getSubcards()) {
                const Card *c = Sanguosha->getCard(id);
                if (c == NULL) continue;
                suit.insert(c->getSuit());
            }
            room->throwCard(to_discard, target);
            delete to_discard;

            if (suit.count() == 4 && room->getCurrentDyingPlayer() == victim) {
                Card *peach = Sanguosha->cloneCard("peach");
                peach->setSkillName("_olchenqing");
                room->useCard(CardUseStruct(peach, target, victim, false), true);
            }

            return true;
        }
        return false;
    }
};

class OlYongsi : public TriggerSkill
{
public:
    OlYongsi() : TriggerSkill("olyongsi") {
        frequency = Compulsory;
        events << DrawNCards << EventPhaseStart;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const {
        if (triggerEvent == DrawNCards) {
            room->sendCompulsoryTriggerLog(player, "olyongsi", true);
            room->broadcastSkillInvoke("olyongsi");

            QSet<QString> kingdoms;
            QList<ServerPlayer *> alives = room->getAlivePlayers();
            foreach(ServerPlayer *p, alives) {
                QString kingdom = p->getKingdom();
                if (!kingdoms.contains(kingdom))
                    kingdoms.insert(kingdom);
            }
            data.setValue(kingdoms.count());
        } else {
            if (player->getPhase() == Player::Discard) {
                room->sendCompulsoryTriggerLog(player, "olyongsi", true);
                if (!room->askForDiscard(player, "olyongsi", 1, 1, true, true, "@olyongsi"))
                    room->loseHp(player);
            }
        }
        return false;
    }
};

class OlJixi : public TriggerSkill
{
public:
    OlJixi() : TriggerSkill("oljixi") {
        frequency = Compulsory;
        events << EventPhaseStart << HpLost;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const {
        if (triggerEvent == EventPhaseStart) {
            if (player->getPhase() == Player::Finish && player->getMark(objectName()) == 0) {
                room->addPlayerMark(player, "oljixi_turn", 1);
                if (player->getMark("oljixi_turn") == 3) {
                    room->setPlayerMark(player, objectName(), 1);
                    LogMessage msg;
                    msg.type = "#oljixi-wake";
                    msg.from = player;
                    room->sendLog(msg);
                    room->doSuperLightbox("yuanshu", "oljixi");
                    room->broadcastSkillInvoke("oljixi");
                    room->notifySkillInvoked(player, "oljixi");
                    room->changeMaxHpForAwakenSkill(player, 1);
                    room->recover(player, RecoverStruct());

                    QStringList choices;
                    if (Sanguosha->getSkill("wangzun"))
                        choices << "wangzun";
                    QStringList lordskills;
                    ServerPlayer *lord = room->getLord();
                    if (lord) {
                        QList<const Skill *> skills = lord->getVisibleSkillList();
                        foreach(const Skill *skill, skills) {
                            if (skill->isLordSkill() && lord->hasLordSkill(skill, true)) {
                                lordskills.append(skill->objectName());
                            }
                        }
                    }
                    if (!lordskills.isEmpty())
                        choices << "lordskill";
                    if (choices.isEmpty())
                        return false;

                    QString choice = room->askForChoice(player, "oljixi", choices.join("+"));
                    if (choice == "wangzun")
                        room->handleAcquireDetachSkills(player, "wangzun", objectName());
                    else {
                        room->drawCards(player, 2, "oljixi");
                        room->handleAcquireDetachSkills(player, lordskills, objectName());
                    }
                }
            }
        } else if (triggerEvent == HpLost) {
            if (player->getMark(objectName()) == 0)
                room->setPlayerMark(player, "oljixi_turn", 0);
        }
        return false;
    }
};

OLPackage::OLPackage()
    : Package("OL") {
    General *zhugeke = new General(this, "zhugeke", "wu", 3); // OL 002
    zhugeke->addSkill(new Aocai);
    zhugeke->addSkill(new Duwu);

    General *lingcao = new General(this, "lingcao", "wu", 4);
    lingcao->addSkill(new Dujin);

    General *sunru = new General(this, "sunru", "wu", 3, false);
    sunru->addSkill(new Qingyi);
    sunru->addSkill(new SlashNoDistanceLimitSkill("qingyi"));
    sunru->addSkill(new Shixin);
    related_skills.insertMulti("qingyi", "#qingyi-slash-ndl");

    General *liuzan = new General(this, "liuzan", "wu");
    liuzan->addSkill(new Fenyin);

    General *lifeng = new General(this, "lifeng", "shu", 3);
    lifeng->addSkill(new TunchuDraw);
    lifeng->addSkill(new TunchuEffect);
    lifeng->addSkill(new Tunchu);
    lifeng->addSkill(new Shuliang);
    related_skills.insertMulti("tunchu", "#tunchu-effect");
    related_skills.insertMulti("tunchu", "#tunchu-disable");

    General *zhuling = new General(this, "zhuling", "wei");
    zhuling->addSkill(new Zhanyi);
    zhuling->addSkill(new ZhanyiDiscard2);
    zhuling->addSkill(new ZhanyiNoDistanceLimit);
    zhuling->addSkill(new ZhanyiRemove);
    related_skills.insertMulti("zhanyi", "#zhanyi-basic");
    related_skills.insertMulti("zhanyi", "#zhanyi-equip");
    related_skills.insertMulti("zhanyi", "#zhanyi-trick");

    General *zhugeguo = new General(this, "zhugeguo", "shu", 3, false);
    zhugeguo->addSkill(new OlYuhua);
    zhugeguo->addSkill(new OlQirang);

    General *ol_xingcai = new General(this, "ol_xingcai", "shu", 3, false);
    ol_xingcai->addSkill(new OlShenxian);
    ol_xingcai->addSkill("qiangwu");

    General *shixie = new General(this, "shixie", "qun", 3);
    shixie->addSkill(new Biluan);
    shixie->addSkill(new BiluanDist);
    shixie->addSkill(new Lixia);
    shixie->addSkill(new LixiaDist);
    related_skills.insertMulti("biluan", "#biluan-dist");
    related_skills.insertMulti("lixia", "#lixia-dist");

    General *zhanglu = new General(this, "zhanglu", "qun", 3);
    zhanglu->addSkill(new Yishe);
    zhanglu->addSkill(new Bushi);
    zhanglu->addSkill(new Midao);

    General *mayl = new General(this, "mayunlu", "shu", 4, false);
    mayl->addSkill("mashu");
    mayl->addSkill(new Fengpo);
    mayl->addSkill(new FengpoRecord);
    related_skills.insertMulti("fengpo", "#fengpo-record");

    General *bulianshi = new General(this, "ol_bulianshi", "wu", 3, false);
    bulianshi->addSkill(new OlAnxu);
    bulianshi->addSkill("zhuiyi");

    General *caiwenji2 = new General(this, "ol_ii_caiwenji", "wei", 3, false);
    caiwenji2->addSkill(new OlChenqing);
    caiwenji2->addSkill(new Moshi);

    General *yuanshu = new General(this, "ol_yuanshu", "qun");
    yuanshu->addSkill(new OlYongsi);
    yuanshu->addSkill(new OlJixi);

    addMetaObject<AocaiCard>();
    addMetaObject<DuwuCard>();
    addMetaObject<QingyiCard>();
    addMetaObject<ShuliangCard>();
    addMetaObject<ZhanyiCard>();
    addMetaObject<ZhanyiViewAsBasicCard>();
    addMetaObject<MidaoCard>();
    addMetaObject<BushiCard>();
    addMetaObject<OlAnxuCard>();

    skills << new OldMoshi;
}

ADD_PACKAGE(OL)
