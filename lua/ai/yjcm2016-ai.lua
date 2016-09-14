jishe_skill = {}
jishe_skill.name = "jishe"
table.insert(sgs.ai_skills, jishe_skill)
jishe_skill.getTurnUseCard = function(self)
    if self.player:getMaxCards() == 0 then return nil end

    if self:needBear() then return nil end
    
    return sgs.Card_Parse("@JisheCard=.")
end

sgs.ai_skill_use_func.JisheCard=function(card,use,self)
	if self.player:getHandcardNum() < self.player:getMaxCards()then use.card = card end
	if self.player:getHandcardNum() == 1 and self.player:getMaxCards() == 1 then
		self:updatePlayers()
		self:sort(self.enemies,"defense")
		local num = 0
		for _, enemy in ipairs(self.enemies) do
			if self:damageIsEffective(enemy, sgs.DamageStruct_Thunder) and not enemy:isChained() then
				num = num + 1
			end
		end
		if num > 1 and self.player:getHp() > 1 then use.card=card end
	end
end

sgs.ai_skill_use["@@jishe"] = function(self, prompt)
    self:updatePlayers()
	self:sort(self.enemies,"defense")
	local targets = {}
	if self.player:hasSkill("lianhuo") then
		if self.player:getHp() > 1 and not self.player:isChained() then table.insert(targets, self.player:objectName()) end
	end
	for _, enemy in ipairs(self.enemies) do
		if #targets < self.player:getHp() then
			if self:damageIsEffective(enemy, sgs.DamageStruct_Thunder) and not enemy:isChained() then
				table.insert(targets, enemy:objectName())
			end
		else break end	
	end
    
    if #targets > 0 then
        return "@JisheCard=.->" .. table.concat(targets, "+")
    end
    return "."
end

sgs.ai_use_priority.JisheCard = 3
sgs.ai_use_value.JisheCard = 3


--矫诏
local jiaozhao_skill = {
	name = "jiaozhao", 
	getTurnUseCard = function(self, inclusive)
		local acard = nil
        for _, card in sgs.qlist(self.player:getCards("h")) do
            if card:hasFlag("jiaozhao") then
                acard = card
                break
            end
        end
		if acard == nil and not self.player:hasUsed("JiaozhaoCard") and not self.player:isKongcheng() then
			return sgs.Card_Parse("@JiaozhaoCard=.")
		elseif acard then
			local suit = acard:getSuitString()
			local number = acard:getNumberString()
			local card_id = acard:getEffectiveId()
            local class_name = self.player:property("jiaozhao"):toString()
			local use_card = sgs.Sanguosha:cloneCard(class_name, acard:getSuit(), acard:getNumber())
			local card_str = ("%s:jiaozhao[%s:%s]=%d"):format(class_name, suit, number, card_id)
			local skillcard = sgs.Card_Parse(card_str)
			assert(skillcard)
			if use_card:isKindOf("Analeptic") or use_card:isKindOf("ExNihilo") or use_card:isKindOf("Peach") then--禁止桃酒无中
				local dummyuse = { isDummy = true }
				if use_card:getTypeId() == sgs.Card_TypeBasic then
					self:useBasicCard(use_card, dummyuse)
				else
					self:useTrickCard(use_card, dummyuse)
				end
				if dummyuse.skillcard then return skillcard end
			else
				return skillcard
			end
		end
	end,
}
table.insert(sgs.ai_skills, jiaozhao_skill) --加入AI可用技能表
sgs.ai_skill_use_func.JiaozhaoCard = function(card, use, self)
	local cards = self.player:getCards("h")
	cards = sgs.QList2Table(cards)
	self:sortByUseValue(cards, true)
	if #cards == 0 then return end
	local target = nil
	if self.player:getMark("jiaozhao-self") > 0 then target = self.player end
	if not target then
		local players = self.room:getOtherPlayers(self.player)
		local distance_list = sgs.IntList()
		local nearest = 1000
		for _,p in sgs.qlist(players) do
			local distance = self.player:distanceTo(p)
			distance_list:append(distance)
			nearest = math.min(nearest, distance)
		end
		local danxin_targets = sgs.SPlayerList()
		for i = 0, distance_list:length() - 1, 1 do
			if distance_list:at(i) == nearest then
				danxin_targets:append(players:at(i))
			end
		end
		for _, p in sgs.qlist(danxin_targets) do
			if self:isFriend(p) then target = p break end
		end
		if not target and self.role == "renegade" then
			for _, p in sgs.qlist(danxin_targets) do
				if self:isEnemy(p) then target = p break end
			end
		end
	end
	if not target then return end
	local card_str = string.format("@JiaozhaoCard=%s", cards[1]:getEffectiveId())
	local acard = sgs.Card_Parse(card_str) --根据卡牌构成字符串产生实际将使用的卡牌
	assert(acard)
	use.card = acard --填充卡牌使用结构体（card部分）
	if use.to then
		use.to:append(target)
	end
end

sgs.ai_view_as.jiaozhao = function(card, player, card_place)--该死的禁止桃
    local class_name = player:property("jiaozhao"):toString()
	if class_name ~= "peach" then return end
	local ask = player:getRoom():getCurrentDyingPlayer()
	if not ask then return end
	local suit = card:getSuitString()
	local number = card:getNumberString()
	local card_id = card:getEffectiveId()
	if card_place == sgs.Player_PlaceHand and card:hasFlag("jiaozhao")
	and ask:objectName()~=player:objectName() and player:getMark("Global_PreventPeach") == 0 then
		return ("peach:jiaozhao[%s:%s]=%d"):format(suit, number, card_id)
	end
end

sgs.ai_skill_choice.jiaozhao = function(self, choices)
	local current = self.room:getCurrent()
	if self:isEnemy(current) then
		return "analeptic"--酒
	end
	if current:getMark("jiaozhao-trick") == 0 then
		if sgs.Slash_IsAvailable(current) and getCardsNum("Slash", current, self.player) < 3 then
			for _, enemy in ipairs(self.enemies) do
				if current:canSlash(enemy) and sgs.isGoodTarget(enemy, self.enemies, current, true) then
					local thunder_slash = sgs.Sanguosha:cloneCard("thunder_slash")
					local fire_slash = sgs.Sanguosha:cloneCard("fire_slash")
					if not self:slashProhibit(fire_slash, enemy, current)and self:slashIsEffective(fire_slash, enemy, current)then
						return "fire_slash"--火杀
					end
					if not self:slashProhibit(thunder_slash, enemy, self.player)and self:slashIsEffective(thunder_slash, enemy, self.player)then
						return "thunder_slash"--雷杀
					end
					if not self:slashProhibit(slash, enemy, self.player)and self:slashIsEffective(slash, enemy, self.player)then
						return "slash"--杀
					end
				end
			end
		end
		return "peach"--桃
	end
	local aoename = "savage_assault|archery_attack"
	local aoenames = aoename:split("|")
	local aoe
	local i
	local good, bad = 0, 0
	local qicetrick = "savage_assault|archery_attack|god_salvation"
	local qicetricks = qicetrick:split("|")
	local aoe_available, ge_available = true, true
	for i = 1, #qicetricks do
		local forbiden = qicetricks[i]
		forbid = sgs.Sanguosha:cloneCard(forbiden)
		if current:isCardLimited(forbid, sgs.Card_MethodUse, true) or not forbid:isAvailable(current) then
			if forbid:isKindOf("AOE") then aoe_available = false end
			if forbid:isKindOf("GlobalEffect") then ge_available = false end
		end
	end
	for _,p in sgs.qlist(self.room:getOtherPlayers(current)) do
		if self:isFriend(p) then
			if p:isWounded() then
				good = good + 10 / p:getHp()
				if p:isLord() then good = good + 10 / p:getHp() end
			end
		else
			if p:isWounded() then
				bad = bad + 10 / p:getHp()
				if p:isLord() then
					bad = bad + 10 / p:getHp()
				end
			end
		end
	end
	local godsalvation = sgs.Sanguosha:cloneCard("god_salvation")
	if aoe_available then
		for i = 1, #aoenames do
			local newqice = aoenames[i]
			aoe = sgs.Sanguosha:cloneCard(newqice)
			local earnings = 0
			local need
			if aoe:isKindOf("SavageAssault") then need = "Slash"
			elseif aoe:isKindOf("ArcheryAttack") then need = "Jink" end
			for _,p in sgs.qlist(self.room:getOtherPlayers(current)) do
				if self:isFriend(p) then
					if not p:hasArmorEffect("Vine") and self:damageIsEffective(p, nil, self.player) and getCardsNum(need, p, self.player) == 0 then
						earnings = earnings - 1
						if self:isWeak(p) then
							earnings = earnings - 1
						end
						if self:hasEightDiagramEffect(p) and need == "Jink" then
							earnings = earnings + 1
						end
					else
						earnings = earnings + 1
					end
				else
					if not p:hasArmorEffect("Vine") and self:damageIsEffective(p, nil, self.player) and getCardsNum(need, p, self.player) == 0 then
						earnings = earnings + 1
						if self:isWeak(p) then
							earnings = earnings + 1
						end
						if self:hasEightDiagramEffect(p) and need == "Jink" then
							earnings = earnings - 1
						end
					end
				end
				if earnings > 0 then--'getAoeValue' (a nil value)
					if newqice == "savage_assault" then
						return "savage_assault"--南蛮
					elseif newqice == "archery_attack" then
						return "archery_attack"--万剑
					end
				end
			end
		end
	end
	if ge_available and good > bad then-- 'willUseGodSalvation' (a nil value)
		return "god_salvation"--桃园
	end
	for _,p in sgs.qlist(self.room:getOtherPlayers(current)) do
		local card = sgs.Sanguosha:cloneCard("snatch")
		if current:isCardLimited(card, sgs.Card_MethodUse, true)then break end
		if self.room:isProhibited(current, p, card) or current:distanceTo(p)>1 then continue end
		if self:isFriend(p) and (p:containsTrick("indulgence") or p:containsTrick("supply_shortage")) and not p:containsTrick("YanxiaoCard")then
		elseif self:isEnemy(p) and not p:isNude()then
		else continue end
		return "snatch"--顺
	end
	for _, enemy in ipairs(self.enemies) do
		local card = sgs.Sanguosha:cloneCard("duel")
		if current:isCardLimited(card, sgs.Card_MethodUse, true)then break end
		if self.room:isProhibited(current, enemy, card) then continue end
		if getCardsNum("Slash", current, self.player) >= getCardsNum("Slash", enemy, self.player) then
			return "duel"--决斗
		end
	end
	local a,b
	for _,p in sgs.qlist(self.room:getOtherPlayers(current)) do
		local card = sgs.Sanguosha:cloneCard("iron_chain")
		if current:isCardLimited(card, sgs.Card_MethodUse, true)then break end
		if self.room:isProhibited(current, p, card) then continue end
		if p:isChained() and self:isFriend(p) then
		elseif not p:isChained() and self:isEnemy(p) then
		else continue
		end
		if not a then
			a=p
		else
			if not b then
				b=p
			else break
			end
		end
	end
	if a and b then
		return "iron_chain"--铁索
	end
	for _,p in sgs.qlist(self.room:getOtherPlayers(current)) do
		local card = sgs.Sanguosha:cloneCard("collateral")
		if current:isCardLimited(card, sgs.Card_MethodUse, true) then break end
		if not p:getWeapon() then continue end
		if self:isFriend(p) and getCardsNum("Slash", p, self.player) > 2 then
		elseif self:isEnemy(p) and getCardsNum("Slash", p, self.player)==0 then
		else continue
		end
		return "collateral"--借刀
	end
	if sgs.Slash_IsAvailable(current) and getCardsNum("Slash", current, self.player) == 0 then
		for _, enemy in ipairs(self.enemies) do
			if current:canSlash(enemy) and sgs.isGoodTarget(enemy, self.enemies, current, true) then
				local thunder_slash = sgs.Sanguosha:cloneCard("thunder_slash")
				local fire_slash = sgs.Sanguosha:cloneCard("fire_slash")
				if not self:slashProhibit(fire_slash, enemy, current)and self:slashIsEffective(fire_slash, enemy, current)then
					return "fire_slash"--火杀
				end
				if not self:slashProhibit(thunder_slash, enemy, self.player)and self:slashIsEffective(thunder_slash, enemy, self.player)then
					return "thunder_slash"--雷杀
				end
				if not self:slashProhibit(slash, enemy, self.player)and self:slashIsEffective(slash, enemy, self.player)then
					return "thunder_slash"--雷杀
				end
			end
		end
	end
	if getCardsNum("TrickCard", current, self.player) > 0 and getCardsNum("Nullification", current, self.player) == 0 then
		return "nullification"--无懈
	else
		return "dismantlement"--拆
	end
end
sgs.ai_use_priority.JiaozhaoCard = 10 --卡牌使用优先级

--殚心
sgs.ai_skill_invoke.danxin = true

sgs.ai_skill_choice.danxin = function(self, choices)
	local items = choices:split("+")
	if #items == 1 then
        return items[1]
	else
		if self:isWeak() and self.player:getHp() < 2 then return "draw" end
		if table.contains(items, "jiaozhao-self") then return "jiaozhao-self" end
		if table.contains(items, "jiaozhao-trick") then return "jiaozhao-trick" end
	end
    return items[1]
end