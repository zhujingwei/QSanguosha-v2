#pragma once
#include "package.h"
#include "card.h"

class YJCM2016Package : public Package
{
    Q_OBJECT

public:
    YJCM2016Package();
};

class JisheCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE JisheCard();
    bool targetFixed() const;
    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    bool targetsFeasible(const QList<const Player *> &targets, const Player *Self) const;
    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class JiaozhaoCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE JiaozhaoCard();
    bool targetFixed() const;
    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
    bool isAvailableChoice(const Card *card, ServerPlayer *target) const;
};