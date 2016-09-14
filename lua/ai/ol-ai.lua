sgs.ai_skill_use["@@bushi"] = function(self, prompt, method)
    local zhanglu = self.room:findPlayerBySkillName("bushi")
    if not zhanglu or zhanglu:getPile("rice"):length() < 1 then return "." end
    if self:isEnemy(zhanglu) and zhanglu:getPile("rice"):length() == 1 and zhanglu:isWounded() then return "." end
    if self:isFriend(zhanglu) and (not (zhanglu:getPile("rice"):length() == 1 and zhanglu:isWounded())) and self:getOverflow() > 1 then return "." end
    local cards = {}
    for _,id in sgs.qlist(zhanglu:getPile("rice")) do
        table.insert(cards,sgs.Sanguosha:getCard(id))
    end
    self:sortByUseValue(cards, true)
    return "@BushiCard="..cards[1]:getEffectiveId()    
end    

sgs.ai_skill_use["@@midao"] = function(self, prompt, method)
    local judge = self.player:getTag("judgeData"):toJudge()
    local ids = self.player:getPile("rice")
    if self.room:getMode():find("_mini_46") and not judge:isGood() then return "@MidaoCard=" .. ids:first() end
    if self:needRetrial(judge) then
        local cards = {}
        for _,id in sgs.qlist(ids) do
            table.insert(cards,sgs.Sanguosha:getCard(id))
        end
        local card_id = self:getRetrialCardId(cards, judge)
        if card_id ~= -1 then
            return "@MidaoCard=" .. card_id
        end
    end
    return "."    
end

--[[
    技能：安恤（阶段技）
    描述：你可以选择两名手牌数不同的其他角色，令其中手牌多的角色将一张手牌交给手牌少的角色，然后若这两名角色手牌数相等，你选择一项：1．摸一张牌；2．回复1点体力。
]]--
--OlAnxuCard:Play
anxu_skill = {
    name = "olanxu",
    getTurnUseCard = function(self, inclusive)
        if self.player:hasUsed("OlAnxuCard") then
            return nil
        elseif self.room:alivePlayerCount() > 2 then
            return sgs.Card_Parse("@OlAnxuCard=.")
        end
    end,
}
table.insert(sgs.ai_skills, anxu_skill)
sgs.ai_skill_use_func["OlAnxuCard"] = function(card, use, self)
    --
end
--room->askForExchange(playerA, "olanxu", 1, 1, false, QString("@olanxu:%1:%2").arg(source->objectName()).arg(playerB->objectName()))
sgs.ai_skill_discard["olanxu"] = function(self, discard_num, min_num, optional, include_equip)
    local others = self.room:getOtherPlayers(self.player)
    local target = nil
    for _,p in sgs.qlist(others) do
        if p:hasFlag("olanxu_target") then
            target = nil
            break
        end
    end
    assert(target)
    local handcards = self.player:getHandcards()
    handcards = sgs.QList2Table(handcards)
    if self:isFriend(target) and not hasManjuanEffect(target) then
        self:sortByUseValue(handcards)
        return { handcards[1]:getEffectiveId() }
    end
    return self:askForDiscard("dummy", discard_num, min_num, optional, include_equip)
end
--room->askForChoice(source, "olanxu", choices)
sgs.ai_skill_choice["olanxu"] = function(self, choices, data)
    local items = choices:split("+")
    if #items == 1 then
        return items[1]
    end
    return "recover"
end
--[[
    技能：追忆
    描述：你死亡时，你可以令一名其他角色（除杀死你的角色）摸三张牌并回复1点体力。
]]--

--[[
    技能：庸肆（锁定技）
    描述：摸牌阶段开始时，你改为摸X张牌。锁定技，弃牌阶段开始时，你选择一项：1．弃置一张牌；2．失去1点体力。（X为场上势力数） 
]]--
--room->askForDiscard(player, "olyongsi", 1, 1, true, true, "@olyongsi")
sgs.ai_skill_discard["olyongsi"] = function(self, discard_num, min_num, optional, include_equip)
    if self:needToLoseHp() or getBestHp(self.player) > self.player:getHp() then
        return "."
    end
    return self:askForDiscard("dummy", discard_num, min_num, optional, include_equip)
end
--[[
    技能：觊玺（觉醒技）
    描述：你的回合结束时，若你连续三回合没有失去过体力，则你加1点体力上限并回复1点体力，然后选择一项：1．获得技能“妄尊”；2．摸两张牌并获得当前主公的主公技，
]]--
--room->askForChoice(player, "oljixi", choices.join("+"))
sgs.ai_skill_choice["oljixi"] = function(self, choices, data)
    local items = choices:split("+")
    if #items == 1 then
        return items[1]
    end
    return "wangzun"
end