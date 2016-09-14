local jianshu_skill = {
	name = "jspjianshu", 
	getTurnUseCard = function(self, inclusive)
		if self.player:getMark("@jspjianshu") == 0 then return end
		local can_use = false
		self:sort(self.enemies, "chaofeng")
		for _, enemy_a in ipairs(self.enemies) do
			for _, enemy_b in ipairs(self.enemies) do
				if enemy_b:isKongcheng() then continue end
				if enemy_a:objectName() == enemy_b:objectName() then continue end
				if not enemy_b:inMyAttackRange(enemy_a) then continue end
				can_use = true
				break
			end
			if can_use == true then break end
		end
		if can_use then
			return sgs.Card_Parse("@JSPJianshuCard=.")
		end
	end,
}
table.insert(sgs.ai_skills, jianshu_skill) 
sgs.ai_skill_use_func.JSPJianshuCard = function(card, use, self)
	local handcards = self.player:getHandcards()
	local blacks = {}
	for _,c in sgs.qlist(handcards) do
		if c:isBlack() then
			table.insert(blacks, c)
		end
	end
	if #blacks == 0 then return end
	self:sortByUseValue(blacks, true) 
	local togive = nil
	local max_card, max_point = nil, 0
	local min_card, min_point = nil, 14
	for _, c in ipairs(blacks) do
		local point = c:getNumber()
		if point > max_point then
			max_point = point
			max_card = c
		elseif point < min_point then
			min_point = point
			min_card = c
		end
	end
	self:sort(self.enemies, "chaofeng")
	local weak_enemy = {}
	local kong_enemy = {}
	local kong_weak = {}
	for _, enemy in ipairs(self.enemies) do
		if self:isWeak(enemy) and not enemy:isKongcheng() then
			table.insert(weak_enemy, enemy)
		elseif not self:isWeak(enemy) and enemy:isKongcheng() then
			if enemy:hasSkill("manjuan") or enemy:hasSkill("qingjian") then continue end
			table.insert(kong_enemy, enemy)
		elseif self:isWeak(enemy) and enemy:isKongcheng() then
			table.insert(weak_enemy, enemy)
			if enemy:hasSkill("manjuan") or enemy:hasSkill("qingjian") then continue end
			table.insert(kong_enemy, enemy)
			table.insert(kong_weak, enemy)
		end
	end
	if #weak_enemy == 0 then return end
	local lost_enemy = nil
	local dis_enemy = nil
	if #kong_weak > 0 then
		for _, enemy_a in ipairs(kong_weak) do
			for _, enemy_b in ipairs(self.enemies) do
				if enemy_b:isKongcheng() then continue end
				if enemy_a:objectName() == enemy_b:objectName() then continue end
				if not enemy_b:inMyAttackRange(enemy_a) then continue end
				local min_card = self:getMinCard(enemy_b)
				local min_num = 10
				if min_card then
					min_num = min_card:getNumber()
				end
				if min_num >= min_point then
					dis_enemy = enemy_b
					break
				end
			end
			if dis_enemy then
				lost_enemy = enemy_a
				break
			end
		end
	end
	if not lost_enemy then
		if #weak_enemy > 1 then
			for _, enemy_a in ipairs(weak_enemy) do
				for _, enemy_b in ipairs(self.enemies) do
					if enemy_b:isKongcheng() then continue end
					if enemy_a:objectName() == enemy_b:objectName() then continue end
					if not enemy_b:inMyAttackRange(enemy_a) then continue end
					if not self:isWeak(enemy_b) then continue end
					togive = blacks[1]
					if enemy_a:getCardCount(true) > enemy_b:getCardCount(true) then
						if enemy_b:getCardCount(true) > 1 then
							lost_enemy = enemy_a
							dis_enemy = enemy_b
						else
							lost_enemy = enemy_b
							dis_enemy = enemy_a
						end
					else
						if enemy_a:getCardCount(true) > 1 then
							lost_enemy = enemy_b
							dis_enemy = enemy_a
						else
							lost_enemy = enemy_a
							dis_enemy = enemy_b
						end
					end
					break
				end
				if togive then break end
			end
		end
		if not lost_enemy then
			for _, enemy_a in ipairs(weak_enemy) do
				for _, enemy_b in ipairs(self.enemies) do
					if enemy_b:isKongcheng() then continue end
					if enemy_a:objectName() == enemy_b:objectName() then continue end
					if not enemy_b:inMyAttackRange(enemy_a) then continue end
					local min_card = self:getMinCard(enemy_b)
					local min_num = 10
					if min_card then
						min_num = min_card:getNumber()
					end
					local max_card = self:getMaxCard(enemy_a)
					if max_card then
						max_num = math.max(min_point,max_card:getNumber())
					else
						max_num = math.max(min_point,4)
					end
					if min_num >= max_num then
						dis_enemy = enemy_b
						break
					end
				end
				if not dis_enemy and #kong_enemy > 0 then
					for _, enemy_b in ipairs(kong_enemy) do
						if enemy_a:objectName() == enemy_b:objectName() then continue end
						if not enemy_b:inMyAttackRange(enemy_a) then continue end
						local max_card = self:getMaxCard(enemy_a)
						local max_num = 4
						if max_card then 
							max_num = max_card:getNumber()
						end
						if max_point > max_num then
							dis_enemy = enemy_b
							break
						end
					end
				end
				if dis_enemy then
					lost_enemy = enemy_a
					break
				end
			end
		end
	end
	if lost_enemy and dis_enemy then
		if dis_enemy:isKongcheng() then
			togive = max_card
		elseif not togive then
			togive = min_card
		end 
		local card_str = string.format("@JSPJianshuCard=%s", togive:getEffectiveId())
		local acard = sgs.Card_Parse(card_str)
		use.card = acard
		if use.to then 
			if dis_enemy:isKongcheng() then
				use.to:append(dis_enemy)
				use.to:append(lost_enemy)
			else
				use.to:append(lost_enemy)
				use.to:append(dis_enemy)
			end
		end
	end
end


sgs.ai_skill_invoke.jspyongdi = true

sgs.ai_skill_playerchosen.jspyongdi = function(self, targets)
	self:updatePlayers()
	local lords = {}
	local targets = {}
	local good_targets = {}
	local best_target = nil
	self:sort(self.friends_noself, "chaofeng")
	for _, friend in ipairs(self.friends_noself) do
		if not friend:isMale() then continue end
		if self:hasSkills(sgs.need_maxhp_skill, friend) then
			table.insert(good_targets, friend)
		end
		for _,skill in sgs.qlist(friend:getGeneral():getVisibleSkillList()) do
			if skill:isLordSkill() and not friend:hasSkill(skill:objectName()) and not friend:isLord() then
				table.insert(lords, friend)
				break
			end
		end
		if self:isWeak(friend) then continue end
		table.insert(targets, friend)
	end
	for _, lord in ipairs(lords) do
		if self:hasSkills(sgs.need_maxhp_skill, lord) then
			best_target = lord
			break
		end
	end
	if best_target then return best_target end
	if #good_targets > 0 then return good_targets[1] end
	if #lords > 0 then return lords[1] end
	if self:isWeak() then return targets[1] end
	return nil
end


sgs.ai_skill_playerchosen.chenqing = function(self, targets)
    local victim = self.room:getCurrentDyingPlayer()
    local help = false
    local careLord = false
    if victim then
        if self:isFriend(victim) then
            help = true
        elseif self.role == "renegade" and victim:isLord() and self.room:alivePlayerCount() > 2 then
            help = true
            careLord = true
        end
    end
    local friends, enemies = {}, {}
    for _,p in sgs.qlist(targets) do
        if self:isFriend(p) then
            table.insert(friends, p)
        else
            table.insert(enemies, p)
        end
    end
    local compare_func = function(a, b)
        local nA = a:getCardCount(true)
        local nB = b:getCardCount(true)
        if nA == nB then
            return a:getHandcardNum() > b:getHandcardNum()
        else
            return nA > nB
        end
    end
    if help and #friends > 0 then
        table.sort(friends, compare_func)
        for _,friend in ipairs(friends) do
            if not hasManjuanEffect(friend) then
                return friend
            end
        end
    end
    if careLord and #enemies > 0 then
        table.sort(enemies, compare_func)
        for _,enemy in ipairs(enemies) do
            if sgs.evaluatePlayerRole(enemy) == "loyalist" then
                return enemy
            end
        end
    end
    if #enemies > 0 then
        self:sort(enemies, "threat")
        for _,enemy in ipairs(enemies) do
            if hasManjuanEffect(enemy) then
                return enemy
            end
        end
    end
    if #friends > 0 then
        self:sort(friends, "defense")
        for _,friend in ipairs(friends) do
            if not hasManjuanEffect(friend) then
                return friend
            end
        end
    end
end


sgs.ai_skill_discard.chenqing = function(self, discard_num, min_num, optional, include_equip)
    local victim = self.room:getCurrentDyingPlayer()
    local help = false
    if victim then
        if self:isFriend(victim) then
            help = true
        elseif self.role == "renegade" and victim:isLord() and self.room:alivePlayerCount() > 2 then
            help = true
        end
    end
    local cards = self.player:getCards("he")
    cards = sgs.QList2Table(cards)
    self:sortByKeepValue(cards)
    if help then
        local peach_num = 0
        local spade, heart, club, diamond = nil, nil, nil, nil
        for _,c in ipairs(cards) do
            if isCard("Peach", c, self.player) then
                peach_num = peach_num + 1
            else
                local suit = c:getSuit()
                if not spade and suit == sgs.Card_Spade then
                    spade = c:getEffectiveId()
                elseif not heart and suit == sgs.Card_Heart then
                    heart = c:getEffectiveId()
                elseif not club and suit == sgs.Card_Club then
                    club = c:getEffectiveId()
                elseif not diamond and suit == sgs.Card_Diamond then
                    diamond = c:getEffectiveId()
                end
            end
        end
        if peach_num + victim:getHp() <= 0 then
            if spade and heart and club and diamond then
                return {spade, heart, club, diamond}
            end
        end
    end
    return self:askForDiscard("dummy", discard_num, min_num, optional, include_equip)
end

sgs.ai_skill_choice.liangzhu = function(self, choices, data)
    local current = self.room:getCurrent()
    if self:isFriend(current) and (self:isWeak(current) or current:hasSkills(sgs.cardneed_skill))then
        return "letdraw"
    end
    return "draw"
end

sgs.ai_skill_invoke.jspshichou = true

sgs.ai_skill_use["@@jspshichou"] = function(self, prompt)
    local targets = {}
    local targetNum = self.player:getLostHp()
    
    for _, friend in ipairs(self.friends_noself) do
        if targetNum > 0 then
            if friend:hasFlag("JSPShichouOptionalTarget") and self:needToLoseHp(friend) then
                table.insert(targets, friend:objectName())
                targetNum = targetNum - 1
            end
        end
    end
    
    self:sort(self.enemies)
    for _, enemy in ipairs(self.enemies) do
        if targetNum > 0 then
            if enemy:hasFlag("JSPShichouOptionalTarget") and not self:needToLoseHp(enemy) then
                table.insert(targets, enemy:objectName())
                targetNum = targetNum - 1
            end
        end
    end
    
    
    if #targets ~= 0 then
        return "@JSPShichouCard=.->" .. table.concat(targets, "+")
    end
    
    return "."
end

sgs.ai_skill_invoke.kunfen = function(self, data)
    if not self:isWeak() and (self.player:getHp() > 2 or (self:getCardsNum("Peach") > 0 and self.player:getHp() > 1)) then
        return true
    end
return false
end

local chixin_skill={}
chixin_skill.name="chixin"
table.insert(sgs.ai_skills,chixin_skill)
chixin_skill.getTurnUseCard = function(self, inclusive)
    local cards = self.player:getCards("he")
    cards=sgs.QList2Table(cards)

    local diamond_card

    self:sortByUseValue(cards,true)

    local useAll = false
    self:sort(self.enemies, "defense")
    for _, enemy in ipairs(self.enemies) do
        if enemy:getHp() == 1 and not enemy:hasArmorEffect("EightDiagram") and self.player:distanceTo(enemy) <= self.player:getAttackRange() and self:isWeak(enemy)
            and getCardsNum("Jink", enemy, self.player) + getCardsNum("Peach", enemy, self.player) + getCardsNum("Analeptic", enemy, self.player) == 0 then
            useAll = true
            break
        end
    end

    local disCrossbow = false
    if self:getCardsNum("Slash") < 2 or self.player:hasSkill("paoxiao") then
        disCrossbow = true
    end


    for _,card in ipairs(cards)  do
        if card:getSuit() == sgs.Card_Diamond
        and (not isCard("Peach", card, self.player) and not isCard("ExNihilo", card, self.player) and not useAll)
        and (not isCard("Crossbow", card, self.player) and not disCrossbow)
        and (self:getUseValue(card) < sgs.ai_use_value.Slash or inclusive or sgs.Sanguosha:correctCardTarget(sgs.TargetModSkill_Residue, self.player, sgs.Sanguosha:cloneCard("slash")) > 0) then
            diamond_card = card
            break
        end
    end

    if not diamond_card then return nil end
    local suit = diamond_card:getSuitString()
    local number = diamond_card:getNumberString()
    local card_id = diamond_card:getEffectiveId()
    local card_str = ("slash:chixin[%s:%s]=%d"):format(suit, number, card_id)
    local slash = sgs.Card_Parse(card_str)
    assert(slash)

    return slash

end

sgs.ai_view_as.chixin = function(card, player, card_place, class_name)
    local suit = card:getSuitString()
    local number = card:getNumberString()
    local card_id = card:getEffectiveId()
    if card_place ~= sgs.Player_PlaceSpecial and card:getSuit() == sgs.Card_Diamond and not card:isKindOf("Peach") and not card:hasFlag("using") then
        if class_name == "Slash" then
            return ("slash:chixin[%s:%s]=%d"):format(suit, number, card_id)
        elseif class_name == "Jink" then
            return ("jink:chixin[%s:%s]=%d"):format(suit, number, card_id)
        end
    end
end

sgs.ai_cardneed.chixin = function(to, card)
    return card:getSuit() == sgs.Card_Diamond
end

sgs.ai_skill_playerchosen.suiren = function(self, targets)
    if self.player:getMark("@suiren") == 0 then return "." end
    if self:isWeak() and (self:getOverflow() < -2 or not self:willSkipPlayPhase()) then return self.player end
    self:sort(self.friends_noself, "defense")
    for _, friend in ipairs(self.friends) do
        if self:isWeak(friend) and not self:needKongcheng(friend) then
            return friend
        end
    end
    self:sort(self.enemies, "defense")
    for _, enemy in ipairs(self.enemies) do
        if (self:isWeak(enemy) and enemy:getHp() == 1)
            and self.player:getHandcardNum() < 2 and not self:willSkipPlayPhase() and self.player:inMyAttackRange(enemy) then
            return self.player
        end
    end
end

sgs.ai_playerchosen_intention.suiren = -60
