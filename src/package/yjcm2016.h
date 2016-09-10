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
    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};
