#include "health.inc"

#include <test-boilerplate>

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
