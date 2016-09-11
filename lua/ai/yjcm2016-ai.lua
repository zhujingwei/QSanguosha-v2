jishe_skill = {}
jishe_skill.name = "jishe"
table.insert(sgs.ai_skills, jishe_skill)
jishe_skill.getTurnUseCard = function(self)
    if self.player:getMaxCards() == 0 then return nil end

    if self:needBear() then return nil end
    
    return sgs.Card_Parse("@JisheCard=.")
end

sgs.ai_skill_use_func.JisheCard=function(card,use,self)
	use.card = card
end

sgs.ai_skill_use["@@jishe"] = function(self, prompt)
    local hp = self.player:getHp()
    local targets = {}
    self:sort(self.enemies)
    for _, enemy in ipairs(self.enemies) do
        if #targets == hp then
            break
        end
        if not enemy:isChained() then
            table.insert(targets, enemy:objectName())
        end
    end
    
    if #targets < hp then -- 自爆
        if #targets >= 2 then
            table.insert(targets, self.player:objectName())
        end
    end
    
    if #targets > 0 then
        return "@JisheCard=.->" .. table.concat(targets, "+")
    end
    return "."
end

sgs.ai_use_priority.JisheCard = 8
sgs.ai_use_value.JisheCard = 8


sgs.ai_skill_choice.jiaozhao = function(self, choices, data)
    local source = data:toPlayer()
    local choices_table = choices:split("+")
    local choice
    if self:isEnemy(source) then
        if table.contains(choices_table, "analeptic") then
            return "analeptic"
        else
            return "jink"
        end
    else
        local forbids = {"peach", "analeptic", "jink", "exnihilo"}
        repeat
            choice = choices_table[math.random(1, #choices_table)]
        until not forbids.contains(choice)
    end
    return choice
end