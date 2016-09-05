-- translation for YJCM Package

return {
	["YJCM"] = "一将成名",

	["#caozhi"] = "八斗之才",
	["caozhi"] = "曹植",
	["designer:caozhi"] = "Foxear",
	["illustrator:caozhi"] = "木美人",
	["luoying"] = "落英",
	[":luoying"] = "其他角色的梅花牌因弃置或判定而置入弃牌堆后，你可以获得其中的任意张。",
	["jiushi"] = "酒诗",
	[":jiushi"] = "当你需要使用【酒】时，若你的武将牌正面向上，你可以翻面，视为使用一张【酒】。当你受到伤害后，若你的武将牌背面向上，你可以翻面。",

	["#yujin"] = "弗克其终",
	["yujin"] = "于禁",
	["jieyue"] = "节钺",
	[":jieyue"] = "结束阶段，你可以弃置一张手牌并选择一名其他角色。若如此做，除非该角色将一张牌置于你的武将牌上，否则你弃置其一张牌。若你有“节钺”牌，则你可以将红色手牌当【闪】、黑色手牌当【无懈可击】使用或打出。准备阶段，你获得“节钺”牌。",
	["@jieyue"] = "你可以发动“<font color=\"yellow\"><b>节钺</b></font>”",
	["~jieyue"] = "选择一张手牌并选择一名目标角色→点击确定",
	["@jieyue_put"] = "%src 对你发动了“<font color=\"yellow\"><b>节钺</b></font>”，请将一张牌置于其武将牌上，或点“取消”令其弃置你的一张牌",
	["jieyue_pile"] = "节钺",


	["#fazheng"] = "恩怨分明",
	["fazheng"] = "法正",
	["designer:fazheng"] = "Michael_Lee",
	["illustrator:fazheng"] = "雷没才",
	["enyuan"] = "恩怨",
	[":enyuan"] = "当你获得一名其他角色两张或更多的牌后，你可以令其摸一张牌；当你受到1点伤害后，你可以令伤害来源选择一项：1.将一张手牌交给你；2.失去1点体力。",
	["EnyuanGive"] = "请交给 %dest %arg 张手牌",
	["xuanhuo"] = "眩惑",
	[":xuanhuo"] = "摸牌阶段，你可以改为令一名其他角色摸两张牌，然后除非该角色对其攻击范围内你选择的另一名角色使用一张【杀】，否则你获得其两张牌。",
	["xuanhuo-invoke"] = "你可以发动“眩惑”<br/> <b>操作提示</b>: 选择一名其他角色→点击确定<br/>",
	["xuanhuo_slash"] = "眩惑",
	["xuanhuo-slash"] = "请对 %dest 使用一张【杀】",

	["#masu"] = "街亭之殇",
	["masu"] = "马谡",
	["sanyao"] = "散谣",
	[":sanyao"] = "出牌阶段限一次，你可以弃置一张牌并选择一名体力值最大的角色，然后你对其造成1点伤害。",
	["zhiman"] = "制蛮",
	[":zhiman"] = "当你对其他角色造成伤害时，你可以防止此伤害。若如此做，你获得其装备区或判定区里的一张牌。",

	["#xushu"] = "身曹心汉",
	["xushu"] = "徐庶",
	["wuyan"] = "无言",
	[":wuyan"] = "锁定技，当你使用锦囊牌造成伤害时，防止此伤害；当你受到锦囊牌造成的伤害时，防止此伤害。",
	["jujian"] = "举荐",
	[":jujian"] = "结束阶段，你可以弃置一张非基本牌并令一名其他角色选择一项：1.摸两张牌；2.回复1点体力；3.复原武将牌。",
	["@jujian-card"] = "你可以发动“举荐”",
	["~jujian"] = "选择一张非基本牌→选择一名其他角→点击确定",
	["#WuyanBad"] = "%from 的“%arg2”被触发，本次伤害被防止",
	["#WuyanGood"] = "%from 的“%arg2”被触发，防止了本次伤害",
	["@jujian-discard"] = "请弃置一张非基本牌",
	["jujian:draw"] = "摸两张牌",
	["jujian:recover"] = "回复1点体力",
	["jujian:reset"] = "复原武将牌",

	["#lingtong"] = "豪情烈胆",
	["lingtong"] = "凌统",
	["xuanfeng"] = "旋风",
	[":xuanfeng"] = "当你于弃牌阶段弃置过至少两张牌，或当你失去装备区里的牌后，你可以弃置至多两名其他角色的共计两张牌。",
	["@@xuanfeng-discard"] = "请选择一个目标，并弃置其一张牌",
	["~xuanfeng"] = "选择一个目标→点击确定",

	["#wuguotai"] = "武烈皇后",
	["wuguotai"] = "吴国太",
	["ganlu"] = "甘露",
	[":ganlu"] = "出牌阶段限一次，你可以选择两名装备区里的牌数之差不大于你已损失体力值数的角色，交换他们装备区里的牌。",
	["buyi"] = "补益",
	[":buyi"] = "当一名角色进入濒死状态时，你可以展示其一张手牌，然后若此牌不为基本牌，则该角色弃置此牌，然后回复1点体力。",
	["#GanluSwap"] = "%from 交换了 %to 的装备",

	["#xusheng"] = "江东的铁壁",
	["xusheng"] = "徐盛",
	["pojun"] = "破军",
	[":pojun"] = "当你于出牌阶段内使用【杀】指定一个目标后，你可以将其至多X张牌扣置于该角色的武将牌旁（X为其体力值）。若如此做，当前回合结束后，该角色获得这些牌。",

	["#gaoshun"] = "攻无不克",
	["gaoshun"] = "高顺",
	["xianzhen"] = "陷阵",
	[":xianzhen"] = "出牌阶段限一次，你可以与一名角色拼点。若你赢，直到回合结束，你无视该角色的防具，且对该角色使用牌没有距离和次数限制；若你没赢，本回合你不能使用【杀】。",
	["jinjiu"] = "禁酒",
	[":jinjiu"] = "锁定技，你的【酒】视为【杀】。",

	["#chengong"] = "刚直壮烈",
	["chengong"] = "陈宫",
	["mingce"] = "明策",
	[":mingce"] = "出牌阶段限一次，你可以将一张装备牌或【杀】交给一名其他角色，然后其选择一项：1.视为对其攻击范围内你选择的另一名角色使用【杀】；2.摸一张牌。",
	["zhichi"] = "智迟",
	[":zhichi"] = "锁定技，当你于回合外受到伤害后，直到当前回合结束，【杀】和普通锦囊牌对你无效。",
	["mingce:use"] = "对攻击范围内的一名角色使用一张【杀】",
	["mingce:draw"] = "摸一张牌",
	["#ZhichiDamaged"] = "%from 受到了伤害，本回合内【<font color=\"yellow\"><b>杀</b></font>】和非延时锦囊都将对其无效",
	["#ZhichiAvoid"] = "%from 的“%arg”被触发，【<font color=\"yellow\"><b>杀</b></font>】和非延时锦囊对其无效",

	["#zhangchunhua"] = "冷血皇后",
	["zhangchunhua"] = "张春华",
	["jueqing"] = "绝情",
	[":jueqing"] = "锁定技，你即将造成的伤害视为失去体力。",
	["shangshi"] = "伤逝",
	[":shangshi"] = "当你的手牌数小于X时，你可以将手牌摸至X张。（X为你已损失的体力值数）",
}
