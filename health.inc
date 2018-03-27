// built-in include guard removal
// just in case the user has a local dependency with the same file name
#if defined _inc_health
	#undef _inc_health
#endif
// custom include-guard to ensure we don't duplicate
#if defined _health_included
	#endinput
#endif
#define _health_included

#include "health_version"


#include <a_samp>
#include <logger>
#include <mathutil>
#include <energy>
#include <YSI\y_iterate>

#include "knockout.inc"

#include <YSI\y_hooks>


// MAX_WOUNDS controls the maximum amount of wounds that can be applied to a player. If a player has
// more wounds inflicted than this value, they will die instantly.
#if !defined MAX_WOUNDS
	#define MAX_WOUNDS (32)
#endif

// MAX_WOUND_SOURCE_LEN is the maximum string length for a wound source.
#if !defined MAX_WOUND_SOURCE_LEN
	#define MAX_WOUND_SOURCE_LEN (10)
#endif

// BLOOD_ATTACH_INDEX controls which attachment index is used for the blood particle effect.
#if !defined BLOOD_ATTACH_INDEX
	#define BLOOD_ATTACH_INDEX (9)
#endif


enum {
	BODY_PART_TORSO = 3,
	BODY_PART_GROIN,
	BODY_PART_LEFT_ARM,
	BODY_PART_RIGHT_ARM,
	BODY_PART_LEFT_LEG,
	BODY_PART_RIGHT_LEG,
	BODY_PART_HEAD,
}

enum E_WOUND_TYPE {
	E_WOUND_FIREARM,    // All guns
	E_WOUND_LACERATION, // Bladed melee weapons
	E_WOUND_BRUISE,     // Blunt melee weapons
	E_WOUND_BURN        // Fire/acid/etc
}

enum E_WOUND_DATA {
	E_WOUND_TYPE:wound_type,           // Type of wound from the E_WOUND_TYPE enumerator
	Float:wound_bleedrate,             // Bleed rate of the wound
	wound_timestamp,                   // Time of wound infliction
	wound_bodypart,                    // Body part the wound is on
	wound_source[MAX_WOUND_SOURCE_LEN] // Name of the source of the wound infliction
}

static
	bool:health_Active[MAX_PLAYERS],                         // health processing is active
	Float:health_Blood[MAX_PLAYERS],                         // total blood ("health")
	Float:health_BleedRate[MAX_PLAYERS],                     // total bleed rate
	health_WoundData[MAX_PLAYERS][MAX_WOUNDS][E_WOUND_DATA], // all wound data for player
	Iterator:health_WoundIndex[MAX_PLAYERS]<MAX_WOUNDS>,     // index for wound data
	health_DeltDamageTo[MAX_PLAYERS][MAX_PLAYER_NAME],       // last player to deal damage to
	health_TookDamageFrom[MAX_PLAYERS][MAX_PLAYER_NAME],     // last player took damage from
	health_DeltDamageTick[MAX_PLAYERS],                      // last recorded damage delt
	health_TookDamageTick[MAX_PLAYERS];                      // last recorded damage taken


forward OnPlayerWounded(playerid, targetid);


// -
// Init
// -


hook OnPlayerConnect(playerid) {
	health_Active[playerid] = true;
	health_DeltDamageTo[playerid][0] = EOS;
	health_TookDamageFrom[playerid][0] = EOS;
	health_DeltDamageTick[playerid] = 0;
	health_TookDamageTick[playerid] = 0;

	Iter_Clear(health_WoundIndex[playerid]);

	return 1;
}

hook OnScriptInit() {
	Iter_Init(health_WoundIndex);
}


// -
// API
// -


// ToggleHealthProcessingForPlayer when set to false disables all health effects and processing for
// the given player. This is useful for when players go on admin duty or change minigame.
stock ToggleHealthProcessingForPlayer(playerid, bool:toggle) {
	if(!IsPlayerConnected(playerid)) {
		return 1;
	}

	health_Active[playerid] = toggle;
	return 0;
}

stock IsHealthProcessingActive(playerid) {
	if(!IsPlayerConnected(playerid)) {
		return 1;
	}

	return health_Active[playerid];
}

// SetPlayerBlood updates the player's blood
stock SetPlayerBlood(playerid, Float:blood) {
	if(!IsPlayerConnected(playerid)) {
		return 1;
	}

	if(blood > 100.0) {
		blood = 100.0;
	} else if(blood < 0.0) {
		blood = 0.0;
	}

	health_Blood[playerid] = blood;
	return 0;
}

// GivePlayerBlood adds `blood` to the player's blood level
stock GivePlayerBlood(playerid, Float:blood) {
	if(!IsPlayerConnected(playerid)) {
		return 1;
	}

	return SetPlayerBlood(playerid, health_Blood[playerid] + blood);
}

// GetPlayerBlood returns the players blood into `blood`
stock GetPlayerBlood(playerid, &Float:blood) {
	if(!IsPlayerConnected(playerid)) {
		return 1;
	}

	blood = health_Blood[playerid];
	return 0;
}

// SetPlayerBleedRate updates the player's bleed rate
stock SetPlayerBleedRate(playerid, Float:rate) {
	if(!IsPlayerConnected(playerid)) {
		return 1;
	}

	health_BleedRate[playerid] = rate;
	return 0;
}

// GetPlayerBleedRate returns the players bleed rate into `rate`
stock GetPlayerBleedRate(playerid, &Float:rate) {
	if(!IsPlayerConnected(playerid)) {
		return 1;
	}

	rate = health_BleedRate[playerid];
	return 0;
}

stock PlayerInflictWound(playerid, targetid, E_WOUND_TYPE:type, Float:bleedrate, Float:knockmult, bodypart, source[]) {
	if(!IsPlayerConnected(playerid)) {
		return 1;
	}

	if(!IsPlayerConnected(targetid)) {
		return 2;
	}

	if(!health_Active[targetid]) {
		return 3;
	}

	new
		woundid = Iter_Free(health_WoundIndex[targetid]),
		woundcount,
		Float:totalbleedrate = health_BleedRate[targetid];

	// target has maximum amount of wounds, kill instantly
	if(woundid == ITER_NONE) {
		SetPlayerBlood(targetid, 0.0);
		return 4;
	}

	Iter_Add(health_WoundIndex[targetid], woundid);

	woundcount = Iter_Count(health_WoundIndex[targetid]);

	health_WoundData[targetid][woundid][wound_type] = type;
	health_WoundData[targetid][woundid][wound_bleedrate] = bleedrate;
	health_WoundData[targetid][woundid][wound_timestamp] = gettime();
	health_WoundData[targetid][woundid][wound_bodypart] = bodypart;
	strcpy(health_WoundData[targetid][woundid][wound_source], source, MAX_WOUND_SOURCE_LEN);

	totalbleedrate += bleedrate;

	// Truncate result to 1.0
	totalbleedrate = totalbleedrate > 1.0 ? 1.0 : totalbleedrate;
	health_BleedRate[targetid] = totalbleedrate;

	// Remove a chunk of blood
	GivePlayerBlood(targetid, -(bleedrate * 100.0));

	switch(bodypart) {
		case BODY_PART_TORSO:		knockmult *= 1.0;
		case BODY_PART_GROIN:		knockmult *= 1.2;
		case BODY_PART_LEFT_ARM:	knockmult *= 0.9;
		case BODY_PART_RIGHT_ARM:	knockmult *= 0.9;
		case BODY_PART_LEFT_LEG:	knockmult *= 0.8;
		case BODY_PART_RIGHT_LEG:	knockmult *= 0.8;
		case BODY_PART_HEAD:		knockmult *= 9.9;
	}

	if(frandom(100.0) < knockmult * (((woundcount + 1) * 0.2) * ((totalbleedrate * 50) + 1))) {
		new
			Float:hp = health_Blood[targetid],
			knockouttime;

		knockouttime = floatround((knockmult * 0.2) * ((woundcount + 1) * ((totalbleedrate * 10) + 1) * (110.0 - hp) + (200 * (110.0 - hp))));

		if(knockouttime > 1500) {
			dbg("health", "knocked out player",
				_i("targetid", targetid),
				_i("knockouttime", knockouttime),
				_i("woundcount", woundcount),
				_f("hp", hp),
				_f("totalbleedrate", totalbleedrate));
			KnockOutPlayer(targetid, knockouttime);
		}
	}

	health_TookDamageTick[targetid] = GetTickCount();

	if(IsPlayerConnected(playerid)) {
		health_DeltDamageTick[playerid] = GetTickCount();

		GetPlayerName(targetid, health_DeltDamageTo[playerid], MAX_PLAYER_NAME);
		GetPlayerName(playerid, health_TookDamageFrom[targetid], MAX_PLAYER_NAME);

		dbg("health", "player wounded another player",
			_i("playerid", playerid),
			_i("targetid", targetid),
			_f("bleedrate", bleedrate),
			_f("knockmult", knockmult),
			_i("bodypart", bodypart),
			_s("source", source));
	} else {
		dbg("health", "player wounded",
			_i("targetid", targetid),
			_f("bleedrate", bleedrate),
			_f("knockmult", knockmult),
			_i("bodypart", bodypart),
			_s("source", source));
	}

	CallLocalFunction("OnPlayerWounded", "dd", playerid, targetid);

	// ShowActionText(targetid,
	// 	sprintf(ls(targetid, "WOUNDEDMSSG", true),
	// 		source,
	// 		(knockmult * (((woundcount + 1) * 0.2) * ((totalbleedrate * 50) + 1)) < 50.0 ? ("Minor") : ("Severe"))
	// 	),
	// 	5000);

	return 1;
}

stock GetPlayerKnockoutChance(playerid, Float:knockmult, &Float:chance) {
	if(!IsPlayerConnected(playerid)) {
		return 1;
	}

	chance = knockmult * (((Iter_Count(health_WoundIndex[playerid]) + 1) * 0.2) * ((health_BleedRate[playerid] * 50) + 1));
	return 0;
}

stock GetLastDeltDamageTo(playerid, name[MAX_PLAYER_NAME])
{
	if(!IsPlayerConnected(playerid))
		return 0;

	name[0] = EOS;
	strcat(name, health_DeltDamageTo[playerid]);

	return 1;
}

stock GetLastTookDamageFrom(playerid, name[MAX_PLAYER_NAME])
{
	if(!IsPlayerConnected(playerid))
		return 0;

	name[0] = EOS;
	strcat(name, health_TookDamageFrom[playerid]);

	return 1;
}

stock GetPlayerDeltDamageTick(playerid, &tick)
{
	if(!IsPlayerConnected(playerid))
		return 0;

	tick = health_DeltDamageTick[playerid];
	return 0;
}

stock GetPlayerTookDamageTick(playerid, &tick) {
	if(!IsPlayerConnected(playerid)) {
		return 1;
	}

	tick = health_TookDamageTick[playerid];
	return 0;
}

stock RemovePlayerWounds(playerid, amount = 1) {
	if(!IsPlayerConnected(playerid)) {
		return 1;
	}

	new idx;

	foreach(new i : health_WoundIndex[playerid]) {
		if(idx == amount) {
			break;
		}

		new next;
		Iter_SafeRemove(health_WoundIndex[playerid], i, next);
		i = next;
		idx++;
	}

	return 0;
}

stock GetPlayerWounds(playerid, &wounds) {
	if(!IsPlayerConnected(playerid)) {
		return 1;
	}

	wounds = Iter_Count(health_WoundIndex[playerid]);
	return 0;
}

stock GetPlayerWoundsPerBodypart(playerid, output[7]) {
	if(!IsPlayerConnected(playerid)) {
		return 0;
	}

	foreach(new i : health_WoundIndex[playerid]) {
		switch(health_WoundData[playerid][i][wound_bodypart]) {
			case BODY_PART_TORSO:		output[0]++;
			case BODY_PART_GROIN:		output[1]++;
			case BODY_PART_LEFT_ARM:	output[2]++;
			case BODY_PART_RIGHT_ARM:	output[3]++;
			case BODY_PART_LEFT_LEG:	output[4]++;
			case BODY_PART_RIGHT_LEG:	output[5]++;
			case BODY_PART_HEAD:		output[6]++;
			default:					output[0]++;
		}
	}

	return 1;
}

// SerialiseWoundData turns player's wound data into an array of cells for storage.
stock SerialiseWoundData(playerid, output[], len = sizeof(output)) {
	if(!IsPlayerConnected(playerid)) {
		return 1;
	}

	new
		idx = 1,
		sourcelen;

	output[0] = Iter_Count(health_WoundIndex[playerid]);

	if(output[0] == 0) {
		return 2;
	}

	foreach(new i : health_WoundIndex[playerid]) {
		if(idx > len) {
			err("array index out of bounds");
			return 3;
		}
		output[idx++] = _:health_WoundData[playerid][i][wound_type];
		output[idx++] = _:health_WoundData[playerid][i][wound_bleedrate];
		output[idx++] = health_WoundData[playerid][i][wound_timestamp];
		output[idx++] = health_WoundData[playerid][i][wound_bodypart];
		sourcelen = strlen(health_WoundData[playerid][i][wound_source]) + 1; // + \0
		output[idx++] = sourcelen;

		//memcpy(output[idx++], health_WoundData[playerid][i][wound_source], 0, 32 * 4, 32);
		// alternative version, memcpy seems to be causing stack issues:
		for(new j; j < sourcelen; j++) {
			output[idx++] = health_WoundData[playerid][i][wound_source][j];
		}
	}

	return 0;
}

// DeSerialiseWoundData unpacks an array of cells from SerialiseWoundData and applies it.
stock DeSerialiseWoundData(playerid, input[]) {
	if(!IsPlayerConnected(playerid)) {
		return 1;
	}

	if(input[0] == 0) {
		// no wounds in stream
		return 2;
	}

	if(!(0 < input[0] < MAX_WOUNDS)) {
		err("wound count invalid",
			_i("count", input[0]));
		return 3;
	}

	new
		idx = 1,
		woundid,
		sourcelen;

	for(new i; i < input[0]; i++) {
		woundid = Iter_Free(health_WoundIndex[playerid]);

		if(woundid == ITER_NONE) {
			err("out of wound slots",
				_i("cell", (idx - 1) / _:E_WOUND_DATA),
				_i("idx", idx));
			break;
		}

		health_WoundData[playerid][woundid][wound_type] = E_WOUND_TYPE:input[idx++];
		health_WoundData[playerid][woundid][wound_bleedrate] = Float:input[idx++];
		health_WoundData[playerid][woundid][wound_timestamp] = input[idx++];
		health_WoundData[playerid][woundid][wound_bodypart] = input[idx++];
		sourcelen = input[idx++]; // source string length

		if(sourcelen > MAX_WOUND_SOURCE_LEN) {
			err("sourcelen out of bounds", _i("sourcelen", sourcelen));
			sourcelen = MAX_WOUND_SOURCE_LEN;
		} else if(sourcelen == 0) {
			err("sourcelen below zero", _i("sourcelen", sourcelen));
			sourcelen = 0;
		}

		// memcpy(health_WoundData[playerid][woundid][wound_source], input[idx], 0, 32 * 4); // no idx++
		// idx += sourcelen; // jump over the string
		for(new k; k < sourcelen; k++) {
			health_WoundData[playerid][woundid][wound_source][k] = input[idx++];
		}

		Iter_Add(health_WoundIndex[playerid], woundid);
	}

	return 0;
}


// -
// Internal
// -


ptask _health_update[100](playerid) {
	if(!health_Active[playerid]) {
		RemovePlayerAttachedObject(playerid, BLOOD_ATTACH_INDEX);
		return;
	}

	if(IsNaN(health_BleedRate[playerid]) || health_BleedRate[playerid] < 0.0) {
		health_BleedRate[playerid] = 0.0;
	}

	if(health_BleedRate[playerid] > 0.0) {
		new Float:hp = health_Blood[playerid];

		// bleed slow-down rate calculation
		new Float:slowrate = (((((100.0 - hp) / 360.0) * health_BleedRate[playerid]) / Iter_Count(health_WoundIndex[playerid])) / 100.0);

		if(frandom(1.0) < 0.7) {
			SetPlayerBlood(playerid, hp - health_BleedRate[playerid]);

			if(hp < 0.1) {
				SetPlayerBlood(playerid, 0.0);
			}
		}

		/*
			Slow bleeding based on health and wound count. Less wounds means
			faster degradation of bleed rate. As blood rate drops, the bleed
			rate will slow down faster (pseudo blood pressure). Results in a
			bleed-out that slows down faster over time (only subtly). No wounds
			will automatically stop the bleed rate due to the nature of the
			formula (however this is still intentional).
		*/
		if(random(100) < 50) {
			health_BleedRate[playerid] -= slowrate;
		}

		// ShowActionText(playerid, sprintf("HP: %f Bleed-rate: %f~n~Wounds %d Bleed slow-rate: %f", hp, health_BleedRate[playerid], GetPlayerWounds(playerid)));

		if(!IsPlayerInAnyVehicle(playerid)) {
			if(IsPlayerAttachedObjectSlotUsed(playerid, BLOOD_ATTACH_INDEX)) {
				if(frandom(0.1) < 0.1 - health_BleedRate[playerid]) {
					RemovePlayerAttachedObject(playerid, BLOOD_ATTACH_INDEX);
				}
			} else {
				if(frandom(0.1) < health_BleedRate[playerid]) {
					SetPlayerAttachedObject(playerid, BLOOD_ATTACH_INDEX, 18706, 1,  0.088999, 0.020000, 0.044999,  0.088999, 0.020000, 0.044999,  1.179000, 1.510999, 0.005000);
				}
			}
		} else {
			RemovePlayerAttachedObject(playerid, BLOOD_ATTACH_INDEX);
		}
	} else {
		if(IsPlayerAttachedObjectSlotUsed(playerid, BLOOD_ATTACH_INDEX)) {
			RemovePlayerAttachedObject(playerid, BLOOD_ATTACH_INDEX);
		}

		new Float:energy;
		GetPlayerEnergy(playerid, energy);

		GivePlayerBlood(playerid, 0.001925925 * energy);

		if(health_BleedRate[playerid] < 0.0) {
			health_BleedRate[playerid] = 0.0;
		}
	}

	return;
}

hook OnDeath(playerid, killerid, reason) {
	Iter_Clear(health_WoundIndex[playerid]);

	return Y_HOOKS_CONTINUE_RETURN_0;
}
