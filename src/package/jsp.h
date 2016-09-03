#ifndef JSP_PACKAGE_H
#define JSP_PACKAGE_H

#include "package.h"
#include "card.h"

class JSPPackage : public Package
{
    Q_OBJECT

public:
    JSPPackage();
};

class JiqiaoCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE JiqiaoCard();
    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class JSPJianshuCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE JSPJianshuCard();

    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

#endif

