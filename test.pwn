#include "health.inc"

#include <test-boilerplate>
#include <zcmd>
#include <action-text>
#include <strlib>

main() {
	new playerid, targetid;
	new toggle;
	ToggleHealthProcessingForPlayer(playerid, bool:toggle);
	IsHealthProcessingActive(playerid);
	new Float:blood;
	SetPlayerBlood(playerid, Float:blood);
	GivePlayerBlood(playerid, Float:blood);
	GetPlayerBlood(playerid, Float:blood);
	new Float:rate;
	SetPlayerBleedRate(playerid, Float:rate);
	GetPlayerBleedRate(playerid, Float:rate);
	PlayerInflictWound(playerid, targetid, E_WOUND_TYPE:0, Float:0.0, Float:0.0, 0, "");
	new Float:knockmult, Float:chance;
	GetPlayerKnockoutChance(playerid, Float:knockmult, Float:chance);
	new name[MAX_PLAYER_NAME];
	GetLastDeltDamageTo(playerid, name);
	GetLastTookDamageFrom(playerid, name);
	new tick;
	GetPlayerDeltDamageTick(playerid, tick);
	GetPlayerTookDamageTick(playerid, tick);
	RemovePlayerWounds(playerid, 0);
	new wounds;
	GetPlayerWounds(playerid, wounds);
	new output[7];
	GetPlayerWoundsPerBodypart(playerid, output);
	SerialiseWoundData(playerid, output);
	new input[7];
	DeSerialiseWoundData(playerid, input);
}

hook OnPlayerSpawn(playerid) {
	SetPlayerBlood(playerid, 100.0);
	return Y_HOOKS_CONTINUE_RETURN_1;
}

CMD:blood(playerid, params[]) {
	new Float:value = floatstr(params);
	new ret = SetPlayerBlood(playerid, value);
	dbg("health", "SetPlayerBlood", _i("ret", ret));
	return 1;
}

CMD:bleedrate(playerid, params[]) {
	new Float:value = floatstr(params);
	new ret = SetPlayerBleedRate(playerid, value);
	dbg("health", "SetPlayerBleedRate", _i("ret", ret));
	return 1;
}

CMD:woundgun(playerid, params[]) {
	PlayerInflictWound(playerid, playerid, E_WOUND_FIREARM, 0.1, 1.0, BODY_PART_TORSO, "test");
	return 1;
}

CMD:knockout(playerid, params[]) {
	new duration = strval(params);
	new ret = KnockOutPlayer(playerid, duration);
	dbg("health", "KnockOutPlayer", _i("ret", ret));
	return 1;
}

public OnPlayerWounded(playerid, targetid) {
	dbg("health", "OnPlayerWounded",
		_i("playerid", playerid),
		_i("targetid", targetid));
	
	return Y_HOOKS_CONTINUE_RETURN_0;
}

ptask testui[100](playerid) {
	new
		Float:blood,
		Float:bleedrate,
		wounds,
		Float:slowrate;

	GetPlayerBlood(playerid, blood);
	GetPlayerBleedRate(playerid, bleedrate);
	GetPlayerWounds(playerid, wounds);
	slowrate = GetBleedSlowRate(blood, bleedrate, wounds);

	ShowActionText(playerid,
		sprintf(
			"Blood: %f Bleed-rate: %f~n~Wounds %d Bleed slow-rate: %f",
			blood, bleedrate, wounds, slowrate
		)
	);
	return Y_HOOKS_CONTINUE_RETURN_1;
}
