// built-in include guard removal
// just in case the user has a local dependency with the same file name
#if defined _inc_knockout
	#undef _inc_knockout
#endif
// custom include-guard to ensure we don't duplicate
#if defined _knockout_included
	#endinput
#endif
#define _knockout_included


#include <a_samp>
#include <progress2>
#include <tick-difference>
#include <YSI\y_timers>
#include <YSI\y_hooks>

#if !defined IsValidVehicle
	native IsValidVehicle(vehicleid);
#endif


static
	knockout_MaxDuration = 120000,
	PlayerBar:knockout_ProgressBar = INVALID_PLAYER_BAR_ID,
	knockout_KnockedOut[MAX_PLAYERS],
	knockout_InVehicleID[MAX_PLAYERS],
	knockout_InVehicleSeat[MAX_PLAYERS],
	knockout_Tick[MAX_PLAYERS],
	knockout_Duration[MAX_PLAYERS],
	Timer:knockout_Timer[MAX_PLAYERS];


// OnPlayerKnockOut is called before a player is knocked out
// returning 1 here cancels the knock out.
forward OnPlayerKnockOut(playerid);

// OnPlayerKnockedOut is called after a player is knocked out.
forward OnPlayerKnockedOut(playerid);

// OnPlayerWakeUp is called when a player wakes up from being knocked out.
forward OnPlayerWakeUp(playerid);


// -
// API
// -


stock KnockOutPlayer(playerid, duration) {
	if(!IsHealthProcessingActive(playerid)) {
		return 1;
	}

	if(CallLocalFunction("OnPlayerKnockOut", "dd", playerid, duration)) {
		return 2;
	}

	ShowPlayerProgressBar(playerid, knockout_ProgressBar);

	if(IsPlayerInAnyVehicle(playerid)) {
		knockout_InVehicleID[playerid] = GetPlayerVehicleID(playerid);
		knockout_InVehicleSeat[playerid] = GetPlayerVehicleSeat(playerid);
	}

	if(knockout_KnockedOut[playerid]) {
		knockout_Duration[playerid] += duration;
	} else {
		knockout_Tick[playerid] = GetTickCount();
		knockout_Duration[playerid] = duration;
		knockout_KnockedOut[playerid] = true;

		_knockout_playAnimation(playerid);

		stop knockout_Timer[playerid];
		knockout_Timer[playerid] = repeat _knockout_update(playerid);
	}

	if(knockout_Duration[playerid] > knockout_MaxDuration) {
		knockout_Duration[playerid] = knockout_MaxDuration;
	}

	CallLocalFunction("OnPlayerKnockedOut", "dd", playerid, duration);

	return 0;
}

stock WakeUpPlayer(playerid) {
	stop knockout_Timer[playerid];

	HidePlayerProgressBar(playerid, knockout_ProgressBar);
	ApplyAnimation(playerid, "PED", "GETUP_FRONT", 4.0, 0, 1, 1, 0, 0);

	knockout_Tick[playerid] = GetTickCount();
	knockout_KnockedOut[playerid] = false;
	knockout_InVehicleID[playerid] = INVALID_VEHICLE_ID;
	knockout_InVehicleSeat[playerid] = -1;
}

stock GetPlayerKnockOutTick(playerid, &tick) {
	if(!IsPlayerConnected(playerid)) {
		return 1;
	}

	tick = knockout_Tick[playerid];
	return 0;
}

stock GetPlayerKnockoutDuration(playerid, &duration) {
	if(!IsPlayerConnected(playerid)) {
		return 1;
	}

	duration = knockout_Duration[playerid];
	return 0;
}

stock GetPlayerKnockOutRemainder(playerid)
{
	if(!IsPlayerConnected(playerid))
		return 0;

	if(!knockout_KnockedOut[playerid])
		return 0;

	return GetTickCountDifference(GetTickCount(), (knockout_Tick[playerid] + knockout_Duration[playerid]));
}

stock IsPlayerKnockedOut(playerid)
{
	if(!IsPlayerConnected(playerid))
		return 0;

	return knockout_KnockedOut[playerid];
}


// -
// Internal
// -


hook OnPlayerConnect(playerid) {
	knockout_ProgressBar = CreatePlayerProgressBar(playerid, 291.0, 315.0, 57.50, 5.19, 0xE85454FF, 100.0);
	knockout_KnockedOut[playerid] = false;
	knockout_InVehicleID[playerid] = INVALID_VEHICLE_ID;
	knockout_InVehicleSeat[playerid] = -1;
	knockout_Tick[playerid] = 0;
	knockout_Duration[playerid] = 0;
}

hook OnPlayerDisconnect(playerid) {
	DestroyPlayerProgressBar(playerid, knockout_ProgressBar);

	if(knockout_KnockedOut[playerid]) {
		WakeUpPlayer(playerid);
	}

	return 1;
}

hook OnPlayerDeath(playerid, killerid, reason) {
	WakeUpPlayer(playerid);
}

timer _knockout_update[100](playerid) {
	if(!knockout_KnockedOut[playerid]) {
		WakeUpPlayer(playerid);
	}

	// todo: IsPlayerDead(playerid)
	// todo: GetTickCountDifference(GetTickCount(), GetPlayerSpawnTick(playerid)) < 1000
	// todo: IsPlayerSpawned(playerid)
	// if( ^see above ) {
	// 	knockout_KnockedOut[playerid] = false;
	// 	HidePlayerProgressBar(playerid, knockout_ProgressBar);
	// 	return;
	// }

	if(IsValidVehicle(knockout_InVehicleID[playerid])) {
		if(!IsPlayerInVehicle(playerid, knockout_InVehicleID[playerid])) {
			PutPlayerInVehicle(playerid, knockout_InVehicleID[playerid], knockout_InVehicleSeat[playerid]);

			new animidx = GetPlayerAnimationIndex(playerid);
			if(animidx != 1207 && animidx != 1018 && animidx != 1001) {
				_knockout_playAnimation(playerid);
			}
		}
	} else {
		if(IsPlayerInAnyVehicle(playerid)) {
			RemovePlayerFromVehicle(playerid);
		}

		new animidx = GetPlayerAnimationIndex(playerid);
		if(animidx != 1207 && animidx != 1018 && animidx != 1001) {
			_knockout_playAnimation(playerid);
		}
	}

	SetPlayerProgressBarValue(playerid, knockout_ProgressBar, GetTickCountDifference(GetTickCount(), knockout_Tick[playerid]));
	SetPlayerProgressBarMaxValue(playerid, knockout_ProgressBar, knockout_Duration[playerid]);

	if(GetTickCountDifference(GetTickCount(), knockout_Tick[playerid]) >= knockout_Duration[playerid]) {
		WakeUpPlayer(playerid);
	}

	// if debug mode:
	// ShowActionText(playerid, sprintf("%s/%s", MsToString(GetTickCountDifference(GetTickCount(), knockout_Tick[playerid]), "%1m:%1s.%1d"), MsToString(knockout_Duration[playerid], "%1m:%1s.%1d")));

	return;
}

_knockout_playAnimation(playerid) {
	if(!IsPlayerInAnyVehicle(playerid)) {
		ApplyAnimation(playerid, "PED", "KO_SHOT_STOM", 4.0, 0, 1, 1, 1, 0, 1);
	} else {
		new vehicleid = GetPlayerVehicleID(playerid);
		new modelid = GetVehicleModel(vehicleid);

		switch(modelid) {
			case 448, 461, 462, 463, 468, 471, 481, 509, 510, 521, 522, 523, 581, 586: {
				new
					Float:x,
					Float:y,
					Float:z;

				GetVehiclePos(vehicleid, x, y, z);
				RemovePlayerFromVehicle(playerid);
				SetPlayerPos(playerid, x, y, z);
				ApplyAnimation(playerid, "PED", "BIKE_fall_off", 4.0, 0, 1, 1, 0, 0, 1);
			}

			default: {
				ApplyAnimation(playerid, "PED", "CAR_DEAD_LHS", 4.0, 0, 1, 1, 1, 0, 1);
			}
		}
	}
}

hook OnPlayerEnterVehicle(playerid, vehicleid, ispassenger) {
	if(knockout_KnockedOut[playerid]) {
		_knockout_vehicleCheck(playerid);
	}
}

hook OnPlayerExitVehicle(playerid, vehicleid) {
	if(knockout_KnockedOut[playerid]) {
		_knockout_vehicleCheck(playerid);
	}
}

hook OnPlayerKeyStateChange(playerid, newkeys, oldkeys) {
	if(knockout_KnockedOut[playerid]) {
		_knockout_vehicleCheck(playerid);
	}
}

_knockout_vehicleCheck(playerid) {
	if(IsValidVehicle(knockout_InVehicleID[playerid])) {
		PutPlayerInVehicle(playerid, knockout_InVehicleID[playerid], knockout_InVehicleSeat[playerid]);
	} else {
		RemovePlayerFromVehicle(playerid);
	}

	new animidx = GetPlayerAnimationIndex(playerid);
	if(animidx != 1207 && animidx != 1018 && animidx != 1001) {
		_knockout_playAnimation(playerid);
	}
}
