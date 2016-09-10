#include "YJCM2016.h"
#include "general.h"
#include "serverplayer.h"


YJCM2016Package::YJCM2016Package() : Package("YJCM2016")
{
    General *cenhun = new General(this, "cenhun", "wu", 3);
}


JisheCard::JisheCard()
{
    target_fixed = true;
}

void JisheCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const
{
    source->drawCards(1);
}

ADD_PACKAGE(YJCM2016)