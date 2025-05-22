/*
===========================================================================

Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Return to Castle Wolfenstein single player GPL Source Code (RTCW SP Source Code).  

RTCW SP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW SP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW SP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW SP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW SP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

/*
 * name:		ai_cast_survival.c
 *
 * desc:		Wolfenstein AI Character Survival
 *
*/

#include <stdlib.h> // For rand()
#include <stdio.h>  // For snprintf()

#include "g_local.h"
#include "../qcommon/q_shared.h"
#include "../botlib/botlib.h"      //bot lib interface
#include "../botlib/be_aas.h"
#include "../botlib/be_ea.h"
#include "../botlib/be_ai_gen.h"
#include "../botlib/be_ai_goal.h"
#include "../botlib/be_ai_move.h"
#include "../botlib/botai.h"          //bot ai interface

#include "ai_cast.h"
#include "g_survival.h"

#include "../steam/steam.h"

/*
============
AICast_InitSurvival
============
*/
void AICast_InitSurvival(void) {
	svParams.killCountRequirement = svParams.initialKillCountRequirement;
	svParams.spawnedThisWave = 0;
	svParams.spawnedThisWaveFriendly = 0;
	svParams.waveCount = 1;
	svParams.waveInProgress = qtrue;

	svParams.maxActiveAI[AICHAR_SOLDIER] = svParams.initialSoldiersCount;
	svParams.maxActiveAI[AICHAR_ZOMBIE_SURV] = svParams.initialZombiesCount;
	svParams.maxActiveAI[AICHAR_ZOMBIE_GHOST] = svParams.initialGhostsCount;
	svParams.maxActiveAI[AICHAR_ZOMBIE_FLAME] = svParams.initialFlamersCount;
	svParams.maxActiveAI[AICHAR_WARZOMBIE] = svParams.initialWarriorsCount;
	svParams.maxActiveAI[AICHAR_PROTOSOLDIER] = svParams.initialProtosCount;
	svParams.maxActiveAI[AICHAR_PARTISAN] = svParams.initialPartisansCount;
	svParams.maxActiveAI[AICHAR_PRIEST] = svParams.initialPriestsCount;
	svParams.maxActiveAI[AICHAR_ELITEGUARD] = svParams.initialEliteGuardsCount;
	svParams.maxActiveAI[AICHAR_BLACKGUARD] = svParams.initialBlackGuardsCount;
	svParams.maxActiveAI[AICHAR_VENOM] = svParams.initialVenomsCount;
}


/*
============
AICast_CreateCharacter_Survival

Applies Survival mode overrides after character creation
============
*/
void AICast_CreateCharacter_Survival(gentity_t *newent, cast_state_t *cs) {
    // If the character is friendly AI (aiTeam == 1), set respawnsleft to 0
    if (newent->aiTeam == 1) {
        cs->respawnsleft = 0;
    } else {
        // Unlimited respawn for other AI
        cs->respawnsleft = -1;
    }
}


/*
============
AIChar_AIScript_AlertEntity_Survival

  triggered spawning, called from AI scripting
============
*/
void AIChar_AIScript_AlertEntity_Survival( gentity_t *ent ) {
	
	vec3_t mins, maxs;
	int numTouch, touch[10], i;
	cast_state_t    *cs;
	vec3_t spawn_origin, spawn_angles;

	gentity_t *player = AICast_FindEntityForName( "player" );

	if ( !ent->aiInactive ) {
		return;
	}

	cs = AICast_GetCastState( ent->s.number );

	// if the current bounding box is invalid, then wait
	VectorAdd( ent->r.currentOrigin, ent->r.mins, mins );
	VectorAdd( ent->r.currentOrigin, ent->r.maxs, maxs );
	trap_UnlinkEntity( ent );

	numTouch = trap_EntitiesInBox( mins, maxs, touch, 10 );

	// check that another client isn't inside us
	if ( numTouch ) {
		for ( i = 0; i < numTouch; i++ ) {
			// RF, note we should only check against clients since zombies need to spawn inside func_explosive (so they dont clip into view after it explodes)
			if ( g_entities[touch[i]].client && g_entities[touch[i]].r.contents == CONTENTS_BODY ) {
				//if (g_entities[touch[i]].r.contents & MASK_PLAYERSOLID)
				break;
			}
		}
		if ( i == numTouch ) {
			numTouch = 0;
		}
	}

	if ( numTouch ) {
		// invalid location
		cs->aiFlags |= AIFL_WAITINGTOSPAWN;
		return;
	}

    
	   if ( svParams.activeAI[ent->aiCharacter] >= svParams.maxActiveAI[ent->aiCharacter] || !svParams.waveInProgress)  { 
		cs->aiFlags |= AIFL_WAITINGTOSPAWN;
		return;
	   }

	    // Prevent friendly AI from spawning if respawnsleft is 0
		if (ent->aiTeam == 1 && cs->respawnsleft == 0) {
			cs->aiFlags |= AIFL_WAITINGTOSPAWN; // Mark as waiting to spawn
			return;
		}

	// Selecting the spawn point for the AI
				SelectSpawnPoint_AI( player, ent, spawn_origin, spawn_angles );
				G_SetOrigin( ent, spawn_origin );
				VectorCopy( spawn_origin, ent->client->ps.origin );
				SetClientViewAngle( ent, spawn_angles );
				// Increment the counter for active AI characters
                svParams.activeAI[ent->aiCharacter]++;

	// RF, has to disable this so I could test some maps which have erroneously placed alertentity calls
	//ent->AIScript_AlertEntity = NULL;
	cs->aiFlags &= ~AIFL_WAITINGTOSPAWN;
	ent->aiInactive = qfalse;
	trap_LinkEntity( ent );

	// trigger a spawn script event
	AICast_ScriptEvent( AICast_GetCastState( ent->s.number ), "respawn", "" );

	// Increment spawned counters based on aiTeam
	if (ent->aiTeam == 1) { 
		svParams.spawnedThisWaveFriendly++;
	} else { 
		svParams.spawnedThisWave++;
	}

	// make it think so we update animations/angles
	AICast_Think( ent->s.number, (float)FRAMETIME / 1000 );
	cs->lastThink = level.time;
	AICast_UpdateInput( cs, FRAMETIME );
	trap_BotUserCommand( cs->bs->client, &( cs->lastucmd ) );
}


/*
===============
AICast_RegisterSurvivalKill

Handles wave and survival kill counters + progression trigger.
Call this from AICast_Die_Survival.
===============
*/
void AICast_RegisterSurvivalKill(gentity_t *self, gentity_t *attacker, int meansOfDeath) {
	if (!self || !attacker || !svParams.waveInProgress)
		return;

	// Only count kills from players or friendly AI
	qboolean killerPlayer   = attacker->client && !attacker->aiCharacter;
	qboolean killerFriendly = attacker->aiCharacter && attacker->aiTeam == 1;

	if (!killerPlayer && !killerFriendly)
		return;

	svParams.survivalKillCount++;
	svParams.waveKillCount++;

	if (killerPlayer) {
		Survival_AddKillScore(attacker, self, meansOfDeath);
	}

	// Always use attacker to trigger progression check
	AICast_CheckSurvivalProgression(attacker);
}


/*
====================
AICast_SetRebirthTimeSurvival

Centralized function for calculating AI rebirth time in Survival mode.
Handles friendly AI, and special cases per AI character if needed.
====================
*/
void AICast_SetRebirthTimeSurvival(gentity_t *ent, cast_state_t *cs) {
	if (!ent || !cs)
		return;

	// Skip characters that should never respawn
	if (ent->aiCharacter == AICHAR_ZOMBIE || 
		ent->aiCharacter == AICHAR_HELGA || 
		ent->aiCharacter == AICHAR_HEINRICH || 
		cs->norespawn)
		return;

	if (ent->aiTeam == AITEAM_ALLIES) { // Friendly AI
		if (cs->respawnsleft > 0) {
			cs->respawnsleft--;
		}
		svParams.spawnedThisWaveFriendly--;
		cs->rebirthTime = level.time + (svParams.friendlySpawnTime * 1000) + rand() % 2000;
		return;
	}

	// Non-friendly AI
	if (cs->respawnsleft != 0) {
		if (cs->respawnsleft > 0) {
			cs->respawnsleft--;
		}

		int baseTime;

		switch (ent->aiCharacter) {
			case AICHAR_ELITEGUARD:
				baseTime = svParams.egSpawnTime * 1000;
				break;
			case AICHAR_BLACKGUARD:
				baseTime = svParams.bgSpawnTime * 1000;
				break;
			case AICHAR_VENOM:
				baseTime = svParams.vSpawnTime * 1000;
				break;
			case AICHAR_PROTOSOLDIER:
				baseTime = svParams.protoSpawnTime * 1000;
				break;
			case AICHAR_WARZOMBIE:
				baseTime = svParams.warzSpawnTime * 1000;
				break;
			case AICHAR_ZOMBIE_GHOST:
				baseTime = svParams.ghostSpawnTime * 1000;
				break;
			case AICHAR_PRIEST:
				baseTime = svParams.priestSpawnTime * 1000;
				break;
			case AICHAR_ZOMBIE_FLAME:
				baseTime = svParams.flamerSpawnTime * 1000;
				break;
			default: // Regular soldiers and zombies
				baseTime = svParams.defaultSpawnTime * 1000;
				break;
		}

		baseTime += rand() % 2000; // slight randomness

		cs->rebirthTime = level.time + baseTime;
	}
}

/*
============
AICast_Die_Survival
============
*/
void AICast_Die_Survival( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath ) {
	int contents;
	int killer = 0;
	cast_state_t    *cs;
	qboolean nogib = qtrue;
	qboolean respawn = qfalse;

	// Achievements related stuff! 
	qboolean modPanzerfaust = (meansOfDeath == MOD_ROCKET || meansOfDeath == MOD_ROCKET_SPLASH);
	qboolean modKicked = (meansOfDeath == MOD_KICKED);
	qboolean modKnife = (meansOfDeath == MOD_KNIFE);
	qboolean modCrush = (meansOfDeath == MOD_CRUSH);
	qboolean modFalling = (meansOfDeath == MOD_FALLING);
	qboolean killerPlayer	 = attacker && attacker->client && !( attacker->aiCharacter );
	qboolean killerEnv	 = attacker && !(attacker->client) && !( attacker->aiCharacter );

    // ETSP Achievements stuff!
	qboolean modGL = (meansOfDeath == MOD_M7 );
	qboolean modBr = (meansOfDeath == MOD_BROWNING );
	qboolean modAir = (meansOfDeath == MOD_AIRSTRIKE );
	qboolean modGas = (meansOfDeath == MOD_POISONGAS );
	
	
	if(self->aiCharacter == AICHAR_LOPER && killerPlayer && modPanzerfaust)
	{
		if ( !g_cheats.integer )
		{
		steamSetAchievement("ACH_LOPER_ROCKET");
		}
	}

	if(self->aiCharacter == AICHAR_PROTOSOLDIER && killerEnv && modFalling)
	{
		if ( !g_cheats.integer ) 
		{
		steamSetAchievement("ACH_PROTO_FALL");
		}
	}

		
	if(self->aiCharacter == AICHAR_ELITEGUARD && killerPlayer && modKicked)
	{
		if ( !g_cheats.integer ) 
		{
		steamSetAchievement("ACH_ELITE_FOOT");
		}
	}

	if(self->aiCharacter == AICHAR_PROTOSOLDIER && killerPlayer && modKnife)
	{
		if ( !g_cheats.integer ) 
		{
		steamSetAchievement("ACH_PROTO_KNIFE");
		}
	}

		if(self->aiCharacter == AICHAR_HEINRICH && killerEnv && modCrush)
	{
		if ( !g_cheats.integer ) 
		{
		steamSetAchievement("ACH_HEIN_NOSHOT");
		}
	}

		if(self->aiCharacter && killerPlayer && modGL)
	{
		if ( !g_cheats.integer )
		{
		steamSetAchievement("ACH_GL");
		}
	}

		if(self->aiCharacter == AICHAR_VENOM && killerPlayer && modBr)
	{
		if ( !g_cheats.integer ) 
		{
		steamSetAchievement("ACH_BROWNING");
		}
	}

		if(self->aiCharacter && killerPlayer && modAir)
	{
		if ( !g_cheats.integer ) 
		{
		steamSetAchievement("ACH_AIR");
		}
	}


		if(self->aiCharacter && killerPlayer && modGas)
	{
		if ( !g_cheats.integer ) 
		{
		steamSetAchievement("ACH_GAS");
		}
	}

	  if (killerPlayer && attacker->client->ps.powerups[PW_VAMPIRE]) {

			trap_SendServerCommand( -1, "mu_play sound/Zombie/firstsight/firstsight3.wav 0\n" );
			G_AddEvent( self, EV_GIB_VAMPIRISM, killer );
		    attacker->health += 25;
		
			if ( attacker->health > 300 ) 
			{
			attacker->health = 300;
		    }

	  }

	// print debugging message
	if ( aicast_debug.integer == 2 && attacker->s.number == 0 ) {
		G_Printf( "killed %s\n", self->aiName );
	}

    AICast_RegisterSurvivalKill(self, attacker, meansOfDeath);

	cs = AICast_GetCastState( self->s.number );

	if ( attacker ) {
		killer = attacker->s.number;
	} else {
		killer = ENTITYNUM_WORLD;
	}

	// record the sighting (FIXME: silent weapons shouldn't do this, but the AI should react in some way)
	if ( attacker && attacker->client ) {
		AICast_UpdateVisibility( self, attacker, qtrue, qtrue );
	}

	if ( self->aiCharacter == AICHAR_HEINRICH || self->aiCharacter == AICHAR_HELGA || self->aiCharacter == AICHAR_SUPERSOLDIER || self->aiCharacter == AICHAR_SUPERSOLDIER_LAB || self->aiCharacter == AICHAR_PROTOSOLDIER ) {
		if ( self->health <= GIB_HEALTH ) {
			self->health = -1;
		}
	}

	// the zombie should show special effect instead of gibbing
	if ( self->aiCharacter == AICHAR_ZOMBIE && cs->secondDeadTime ) {
		if ( cs->secondDeadTime > 1 ) {
			// we are already totally dead
			self->health += damage; // don't drop below gib_health if we weren't already below it
			return;
		}

		// always gib
		self->health = -999;
		damage = 999;
	}

	// Zombies are very fragile against highly explosives
	if ( (self->aiCharacter == AICHAR_ZOMBIE || self->aiCharacter == AICHAR_ZOMBIE_SURV || self->aiCharacter == AICHAR_ZOMBIE_GHOST || self->aiCharacter == AICHAR_ZOMBIE_FLAME  ) && damage > 20 && inflictor != attacker ) {
		self->health = -999;
		damage = 999;
	}

	// process the event
	if ( self->client->ps.pm_type == PM_DEAD ) {
		// already dead
		if ( self->health < GIB_HEALTH ) {
			if ( self->aiCharacter == AICHAR_ZOMBIE || self->aiCharacter == AICHAR_ZOMBIE_SURV || self->aiCharacter == AICHAR_ZOMBIE_GHOST || self->aiCharacter == AICHAR_ZOMBIE_FLAME ) {
				// RF, changed this so Zombies always gib now
				GibEntity( self, killer );
				nogib = qfalse;
				self->takedamage = qfalse;
				self->r.contents = 0;
				cs->secondDeadTime = 2;
				cs->rebirthTime = 0;
				cs->revivingTime = 0;
			} else {
				body_die( self, inflictor, attacker, damage, meansOfDeath );
				return;
			}
		}

	} else {    // this is our first death, so set everything up

		if ( level.intermissiontime ) {
			return;
		}

		self->client->ps.pm_type = PM_DEAD;

		self->enemy = attacker;

		// drop a weapon?
		// if client is in a nodrop area, don't drop anything
		contents = trap_PointContents( self->r.currentOrigin, -1 );
		if ( !( contents & CONTENTS_NODROP ) ) {
			TossClientWeapons( self );
			TossClientItems( self, attacker );
			TossClientPowerups( self, attacker );
		}

		// make sure the client doesn't forget about this entity until it's set to "dead" frame
		// otherwise it might replay it's death animation if it goes out and into client view
		self->r.svFlags |= SVF_BROADCAST;

		self->takedamage = qtrue;   // can still be gibbed

		self->s.weapon = WP_NONE;
		if ( cs->bs ) {
			cs->weaponNum = WP_NONE;
		}
		self->client->ps.weapon = WP_NONE;

		self->s.powerups = 0;
		self->r.contents = CONTENTS_CORPSE;

		self->s.angles[0] = 0;
		self->s.angles[1] = self->client->ps.viewangles[1];
		self->s.angles[2] = 0;

		VectorCopy( self->s.angles, self->client->ps.viewangles );

		self->s.loopSound = 0;

		self->r.maxs[2] = -8;
		self->client->ps.maxs[2] = self->r.maxs[2];

		// remove powerups
		memset( self->client->ps.powerups, 0, sizeof( self->client->ps.powerups ) );

		//cs->rebirthTime = 0;

		// never gib in a nodrop
		if ( self->health <= GIB_HEALTH ) {
			if ( self->aiCharacter == AICHAR_ZOMBIE || self->aiCharacter == AICHAR_ZOMBIE_SURV || self->aiCharacter == AICHAR_ZOMBIE_GHOST || self->aiCharacter == AICHAR_ZOMBIE_FLAME  ) {
				// RF, changed this so Zombies always gib now
				GibEntity( self, killer );
				nogib = qfalse;
			} else if ( !( contents & CONTENTS_NODROP ) ) {
				body_die( self, inflictor, attacker, damage, meansOfDeath );
				//GibEntity( self, killer );
				nogib = qfalse;
			}
		}

		// if we are a zombie, and lying down during our first death, then we should just die
		if ( !( self->aiCharacter == AICHAR_ZOMBIE && cs->secondDeadTime && cs->rebirthTime ) ) {

			// set enemy weapon
			BG_UpdateConditionValue( self->s.number, ANIM_COND_ENEMY_WEAPON, 0, qfalse );
			if ( attacker && attacker->client ) {
				BG_UpdateConditionValue( self->s.number, ANIM_COND_ENEMY_WEAPON, inflictor->s.weapon, qtrue );
			} else {
				BG_UpdateConditionValue( self->s.number, ANIM_COND_ENEMY_WEAPON, 0, qfalse );
			}

			// set enemy location
			BG_UpdateConditionValue( self->s.number, ANIM_COND_ENEMY_POSITION, 0, qfalse );
			if ( infront( self, inflictor ) ) {
				BG_UpdateConditionValue( self->s.number, ANIM_COND_ENEMY_POSITION, POSITION_INFRONT, qtrue );
			} else {
				BG_UpdateConditionValue( self->s.number, ANIM_COND_ENEMY_POSITION, POSITION_BEHIND, qtrue );
			}

			if ( self->takedamage ) { // only play the anim if we haven't gibbed
				// play the animation
				BG_AnimScriptEvent( &self->client->ps, ANIM_ET_DEATH, qfalse, qtrue );
			}

			// set gib delay
			if ( cs->aiCharacter == AICHAR_HEINRICH || cs->aiCharacter == AICHAR_HELGA ) {
				cs->lastLoadTime = level.time + self->client->ps.torsoTimer - 200;
			}

			// set this flag so no other anims override us
			self->client->ps.eFlags |= EF_DEAD;
			self->s.eFlags |= EF_DEAD;

		}

		cs->deadSinkStartTime = 0;
		
	}

	if ( nogib ) {
		// set for rebirth
		if ( self->aiCharacter == AICHAR_ZOMBIE ) {
			if ( !cs->secondDeadTime ) {
				cs->rebirthTime = level.time + 5000 + rand() % 2000;
				// RF, only set for gib at next death, if NoRevive is not set
				if ( !( self->spawnflags & 2 ) ) {
					cs->secondDeadTime = qtrue;
				}
				cs->revivingTime = 0;
			} else if ( cs->secondDeadTime > 1 ) {
				cs->rebirthTime = 0;
				cs->revivingTime = 0;
				cs->deathTime = level.time;
			}
		} else {
			// the body can still be gibbed
			self->die = body_die;
		}
	}

		respawn = qtrue;
		nogib = qtrue;

    AICast_SetRebirthTimeSurvival(self, cs);

	trap_LinkEntity( self );

	// kill, instanly, any streaming sound the character had going
	G_AddEvent( &g_entities[self->s.number], EV_STOPSTREAMINGSOUND, 0 );

	// mark the time of death
	cs->deathTime = level.time;

	// dying ai's can trigger a target
	if ( !cs->rebirthTime ) {
		G_UseTargets( self, self );
		// really dead now, so call the script
		if ( attacker ) {
			AICast_ScriptEvent( cs, "death", attacker->aiName ? attacker->aiName : "" );
		} else {
			AICast_ScriptEvent( cs, "death", "" );
		}
		// call the deathfunc for this cast, so we can play associated sounds, or do any character-specific things
		if ( !( cs->aiFlags & AIFL_DENYACTION ) && cs->deathfunc ) {
			cs->deathfunc( self, attacker, damage, meansOfDeath );   //----(SA)	added mod
		}
	} else {
		// really dead now, so call the script
		if ( respawn && self->aiCharacter != AICHAR_ZOMBIE && self->aiCharacter != AICHAR_HELGA
			 && self->aiCharacter != AICHAR_HEINRICH && nogib && !cs->norespawn ) {

			if ( !cs->died ) {
				G_UseTargets( self, self );                 // testing
				AICast_ScriptEvent( cs, "death", "" );
				cs->died = qtrue;
			}
		} else {
			AICast_ScriptEvent( cs, "fakedeath", "" );
		}
		// call the deathfunc for this cast, so we can play associated sounds, or do any character-specific things
		if ( !( cs->aiFlags & AIFL_DENYACTION ) && cs->deathfunc ) {
			cs->deathfunc( self, attacker, damage, meansOfDeath );   //----(SA)	added mod
		}
	}
}

/*
============
AICast_UpdateMaxActiveAI
  Updates the maximum number of active AI characters based on the current wave count.
  This function is called at the start of each wave to adjust the limits for each character type.
  The limits are defined in svParams and are adjusted according to the wave count.
  The function ensures that the number of active AI characters does not exceed the defined maximums.
============
*/
void AICast_UpdateMaxActiveAI(void)
{
    // Normal soldiers
    svParams.maxActiveAI[AICHAR_SOLDIER] += svParams.soldiersIncrease;
    if (svParams.maxActiveAI[AICHAR_SOLDIER] > svParams.maxSoldiers) {
        svParams.maxActiveAI[AICHAR_SOLDIER] = svParams.maxSoldiers;
    }

    // Elite Guards
    if (svParams.waveCount >= svParams.waveEg) {
        svParams.maxActiveAI[AICHAR_ELITEGUARD] += svParams.eliteGuardsIncrease;
        if (svParams.maxActiveAI[AICHAR_ELITEGUARD] > svParams.maxEliteGuards) {
            svParams.maxActiveAI[AICHAR_ELITEGUARD] = svParams.maxEliteGuards;
        }
    }

    // Black Guards
    if (svParams.waveCount >= svParams.waveBg) {
        svParams.maxActiveAI[AICHAR_BLACKGUARD] += svParams.blackGuardsIncrease;
        if (svParams.maxActiveAI[AICHAR_BLACKGUARD] > svParams.maxBlackGuards) {
            svParams.maxActiveAI[AICHAR_BLACKGUARD] = svParams.maxBlackGuards;
        }
    }

    // Venoms
    if (svParams.waveCount >= svParams.waveV) {
        svParams.maxActiveAI[AICHAR_VENOM] += svParams.venomsIncrease;
        if (svParams.maxActiveAI[AICHAR_VENOM] > svParams.maxVenoms) {
            svParams.maxActiveAI[AICHAR_VENOM] = svParams.maxVenoms;
        }
    }

    // Default Zombies
    svParams.maxActiveAI[AICHAR_ZOMBIE_SURV] += svParams.zombiesIncrease;
    if (svParams.maxActiveAI[AICHAR_ZOMBIE_SURV] > svParams.maxZombies) {
        svParams.maxActiveAI[AICHAR_ZOMBIE_SURV] = svParams.maxZombies;
    }

    // Warriors
    if (svParams.waveCount >= svParams.waveWarz) {
        svParams.maxActiveAI[AICHAR_WARZOMBIE] += svParams.warriorsIncrease;
        if (svParams.maxActiveAI[AICHAR_WARZOMBIE] > svParams.maxWarriors) {
            svParams.maxActiveAI[AICHAR_WARZOMBIE] = svParams.maxWarriors;
        }
    }

    // Protos
    if (svParams.waveCount >= svParams.waveProtos) {
        svParams.maxActiveAI[AICHAR_PROTOSOLDIER] += svParams.protosIncrease;
        if (svParams.maxActiveAI[AICHAR_PROTOSOLDIER] > svParams.maxProtos) {
            svParams.maxActiveAI[AICHAR_PROTOSOLDIER] = svParams.maxProtos;
        }
    }

    // Ghost Zombies
    if (svParams.waveCount >= svParams.waveGhosts) {
        svParams.maxActiveAI[AICHAR_ZOMBIE_GHOST] += svParams.ghostsIncrease;
        if (svParams.maxActiveAI[AICHAR_ZOMBIE_GHOST] > svParams.maxGhosts) {
            svParams.maxActiveAI[AICHAR_ZOMBIE_GHOST] = svParams.maxGhosts;
        }
    }

    // Priests
    if (svParams.waveCount >= svParams.wavePriests) {
        svParams.maxActiveAI[AICHAR_PRIEST] += svParams.priestsIncrease;
        if (svParams.maxActiveAI[AICHAR_PRIEST] > svParams.maxPriests) {
            svParams.maxActiveAI[AICHAR_PRIEST] = svParams.maxPriests;
        }
    }

	// Flamers
    if (svParams.waveCount >= svParams.waveFlamers) {
        svParams.maxActiveAI[AICHAR_ZOMBIE_FLAME] += svParams.flamersIncrease;
        if (svParams.maxActiveAI[AICHAR_ZOMBIE_FLAME] > svParams.maxFlamers) {
            svParams.maxActiveAI[AICHAR_ZOMBIE_FLAME] = svParams.maxFlamers;
        }
    }
}

/*
============
AICast_ApplySurvivalAttributes
  Applies survival mode attributes to the AI character based on the current wave count.
  The function adjusts health, speed scales, and other attributes based on the character type and wave count.
  This function is called when a new wave starts or when an AI character is spawned.
  The adjustments are made to ensure that the AI characters become progressively stronger as the game progresses.
============
*/
void AICast_ApplySurvivalAttributes(gentity_t *ent, cast_state_t *cs) {
	// Default: +1 step every wave after wave 1
	int steps = (svParams.waveCount > 1) ? svParams.waveCount / 1 : 0;

	// Disable scaling if the character hasn't "unlocked" yet
	if ((cs->aiCharacter == AICHAR_ELITEGUARD && svParams.waveCount < svParams.waveEg) ||
		(cs->aiCharacter == AICHAR_BLACKGUARD && svParams.waveCount < svParams.waveBg) ||
		(cs->aiCharacter == AICHAR_VENOM       && svParams.waveCount < svParams.waveV)  ||
		(cs->aiCharacter == AICHAR_ZOMBIE_GHOST && svParams.waveCount < svParams.waveGhosts) ||
		(cs->aiCharacter == AICHAR_WARZOMBIE    && svParams.waveCount < svParams.waveWarz)   ||
		(cs->aiCharacter == AICHAR_PROTOSOLDIER && svParams.waveCount < svParams.waveProtos) ||
		(cs->aiCharacter == AICHAR_PRIEST       && svParams.waveCount < svParams.wavePriests))
	{
		steps = 0;
	}

	int newHealth = 0;
	float runSpeedScale = 1.0f;
	float sprintSpeedScale = 1.0f;
	float crouchSpeedScale = 1.0f;

	switch (cs->aiCharacter) {
		case AICHAR_SOLDIER:
			newHealth = 20 + steps * 5;
			if (newHealth > 50) newHealth = 50;
			break;

		case AICHAR_ELITEGUARD:
			newHealth = 30 + steps * 5;
			if (newHealth > 60) newHealth = 60;
			break;

		case AICHAR_BLACKGUARD:
			newHealth = 40 + steps * 5;
			if (newHealth > 80) newHealth = 80;
			break;

		case AICHAR_VENOM:
			newHealth = 50 + steps * 5;
			if (newHealth > 100) newHealth = 100;
			break;

		case AICHAR_ZOMBIE_SURV:
			newHealth = 20 + steps * 5;
			if (newHealth > 200) newHealth = 200;
			runSpeedScale    = fminf(0.8f + steps * 0.1f, 1.2f);
			sprintSpeedScale = fminf(1.2f + steps * 0.1f, 1.6f);
			crouchSpeedScale = fminf(0.25f + steps * 0.1f, 0.5f);
			break;

		case AICHAR_ZOMBIE_GHOST:
			newHealth = 20 + steps * 5;
			if (newHealth > 200) newHealth = 200;
			runSpeedScale    = fminf(0.8f + steps * 0.1f, 1.6f);
			sprintSpeedScale = fminf(1.2f + steps * 0.1f, 2.0f);
			crouchSpeedScale = fminf(0.25f + steps * 0.1f, 0.75f);
			break;

		case AICHAR_WARZOMBIE:
			newHealth = 40 + steps * 5;
			if (newHealth > 300) newHealth = 300;
			runSpeedScale    = fminf(0.8f + steps * 0.1f, 1.6f);
			sprintSpeedScale = fminf(1.2f + steps * 0.1f, 2.0f);
			crouchSpeedScale = fminf(0.25f + steps * 0.1f, 0.75f);
			break;

		case AICHAR_PROTOSOLDIER:
			newHealth = 250 + steps * 5;
			if (newHealth > 600) newHealth = 600;
			runSpeedScale    = fminf(0.8f + steps * 0.1f, 1.6f);
			sprintSpeedScale = fminf(1.2f + steps * 0.1f, 1.5f);
			crouchSpeedScale = fminf(0.25f + steps * 0.1f, 0.75f);
			break;

		case AICHAR_PARTISAN:
			newHealth = 250 + steps * 5;
			if (newHealth > 500) newHealth = 500;
			break;

		case AICHAR_PRIEST:
			newHealth = 250 + steps * 5;
			if (newHealth > 500) newHealth = 500;
			runSpeedScale    = fminf(0.8f + steps * 0.1f, 1.4f);
			sprintSpeedScale = fminf(1.2f + steps * 0.1f, 2.0f);
			crouchSpeedScale = fminf(0.25f + steps * 0.1f, 0.5f);
			break;

		case AICHAR_ZOMBIE_FLAME:
			newHealth = 50 + steps * 5;
			if (newHealth > 500) newHealth = 500;
			runSpeedScale    = fminf(0.8f + steps * 0.1f, 1.4f);
			sprintSpeedScale = fminf(1.2f + steps * 0.1f, 2.0f);
			crouchSpeedScale = fminf(0.25f + steps * 0.1f, 0.5f);
			break;

		default:
			break;
	}

	// Apply to entity
	ent->health = ent->client->ps.stats[STAT_HEALTH] = ent->client->ps.stats[STAT_MAX_HEALTH] = cs->attributes[STARTING_HEALTH] = newHealth;
	ent->client->ps.runSpeedScale    = runSpeedScale;
	ent->client->ps.sprintSpeedScale = sprintSpeedScale;
	ent->client->ps.crouchSpeedScale = crouchSpeedScale;
}

/*
============
BG_SetBehaviorForSurvival
  Sets the behavior attributes for AI characters in survival mode.
  The function adjusts the aim skill, accuracy, attack skill, aggression, and reaction time
  based on the current wave count. The values are capped to ensure they do not exceed defined limits.
  This function is called at the start of each wave to adjust the AI behavior dynamically.
============
*/
void BG_SetBehaviorForSurvival(AICharacters_t characterNum) {
	// Base scaling: +0.1 per wave after wave 1
	int steps = (svParams.waveCount > 1) ? svParams.waveCount / 1 : 0;
	if (steps > 5) steps = 5;

	float delta = 0.1f * steps;

	// Clamp delta to 0 for special enemies not yet eligible
	if (characterNum == AICHAR_ELITEGUARD && svParams.waveCount < svParams.waveEg)
		delta = 0.0f;
	else if (characterNum == AICHAR_BLACKGUARD && svParams.waveCount < svParams.waveBg)
		delta = 0.0f;
	else if (characterNum == AICHAR_VENOM && svParams.waveCount < svParams.waveV)
		delta = 0.0f;

	float aimSkill     = 0.0f;
	float aimAccuracy  = 0.0f;
	float attackSkill  = 0.0f;
	float aggression   = 0.0f;
	float reactionTime = 1.0f;

	switch (characterNum) {
		case AICHAR_SOLDIER:
			aimSkill     = fminf(0.1f + delta, 0.5f);
			aimAccuracy  = fminf(0.1f + delta, 0.5f);
			attackSkill  = fminf(0.1f + delta, 0.5f);
			aggression   = fminf(0.1f + delta, 1.0f);
			reactionTime = fmaxf(1.0f - delta, 0.5f);
			break;

		case AICHAR_ELITEGUARD:
			aimSkill     = fminf(0.3f + delta, 0.6f);
			aimAccuracy  = fminf(0.3f + delta, 0.6f);
			attackSkill  = fminf(0.3f + delta, 0.6f);
			aggression   = fminf(0.3f + delta, 1.0f);
			reactionTime = fmaxf(1.0f - delta, 0.5f);
			break;

		case AICHAR_BLACKGUARD:
		case AICHAR_VENOM:
		case AICHAR_PROTOSOLDIER:
			aimSkill     = fminf(0.4f + delta, 0.7f);
			aimAccuracy  = fminf(0.4f + delta, 0.7f);
			attackSkill  = fminf(0.4f + delta, 0.7f);
			aggression   = fminf(0.5f + delta, 1.0f);
			reactionTime = fmaxf(1.0f - delta, 0.5f);
			break;

		case AICHAR_PARTISAN:
			aimSkill     = 0.8f;
			aimAccuracy  = 0.8f;
			attackSkill  = 0.8f;
			aggression   = 0.8f;
			reactionTime = 0.5f;
			break;

		case AICHAR_ZOMBIE_SURV:
		case AICHAR_ZOMBIE_FLAME:
		case AICHAR_WARZOMBIE:
		case AICHAR_PRIEST:
		case AICHAR_ZOMBIE_GHOST:
			aimSkill     = 1.0f;
			aimAccuracy  = 1.0f;
			attackSkill  = 1.0f;
			aggression   = 1.0f;
			reactionTime = 0.5f;
			break;

		default:
			// Unhandled characters
			return;
	}

	// Apply the values
	aiDefaults[characterNum].attributes[AIM_SKILL]     = aimSkill;
	aiDefaults[characterNum].attributes[AIM_ACCURACY]  = aimAccuracy;
	aiDefaults[characterNum].attributes[ATTACK_SKILL]  = attackSkill;
	aiDefaults[characterNum].attributes[AGGRESSION]    = aggression;
	aiDefaults[characterNum].attributes[REACTION_TIME] = reactionTime;
}

void AICast_CheckSurvivalProgression(gentity_t *attacker) {
    if (svParams.waveKillCount == svParams.killCountRequirement && !svParams.wavePending) {
        svParams.wavePending = qtrue;
        svParams.waveChangeTime = level.time + svParams.intermissionTime * 1000;

        // Play the wave end sound from the configuration file
        static char command_end[256];
        snprintf(command_end, sizeof(command_end), "mu_play %s 0\n", svParams.waveEndSound);
        trap_SendServerCommand(-1, command_end);
    }
}

void AICast_TickSurvivalWave(void) {
    if (!svParams.wavePending)
        return;
    if (level.time < svParams.waveChangeTime)
        return;

    svParams.wavePending = qfalse;
    svParams.waveInProgress = qtrue;
    svParams.waveCount++;
    svParams.waveKillCount = 0;
    svParams.spawnedThisWave = 0;

	int wave = svParams.waveCount;
	int killReq = 0;

	if (wave == 1) {
		// Explicitly use user-defined value for wave 1
		killReq = svParams.initialKillCountRequirement;
	} else {
		// quadratic formula:
		killReq = (int)(0.15f * wave * wave + 3.0f * wave + 10.0f);
	}

	svParams.killCountRequirement = killReq;

    // Track wave count per player
    for (int i = 0; i < g_maxclients.integer; i++) {
        gentity_t *cl = &g_entities[i];
        if (!cl->inuse || !cl->client) continue;

        cl->client->ps.persistant[PERS_WAVES]++;

        // Achievements
        if (svParams.waveCount == 10 && !g_cheats.integer && !cl->client->hasPurchased) {
            steamSetAchievement("ACH_NO_BUY");
        }

        if (svParams.waveCount == 15 && !g_cheats.integer &&
            cl->client->ps.stats[STAT_PLAYER_CLASS] == PC_NONE) {
            steamSetAchievement("ACH_NO_CLASS");
        }
    }

    // Play the wave start sound from the configuration file
    static char command_start[256];
    snprintf(command_start, sizeof(command_start), "mu_play %s 0\n", svParams.waveStartSound);
    trap_SendServerCommand(-1, command_start);

    AICast_UpdateMaxActiveAI();
}


/*
============
AICast_SurvivalRespawn
============
*/
void AICast_SurvivalRespawn(gentity_t *ent, cast_state_t *cs) {

   vec3_t mins, maxs;
   int touch[10], numTouch;
   float oldmaxZ;
   int i;
   gentity_t *player;
   vec3_t spawn_origin, spawn_angles;

   if (svParams.spawnedThisWave >= svParams.killCountRequirement || !svParams.waveInProgress)
   {
	   return;
   }

    // Prevent friendly AI from respawning if respawnsleft is 0
    if (ent->aiTeam == 1 && cs->respawnsleft == 0) {
        return;
    }


			if ( ent->aiCharacter != AICHAR_ZOMBIE && ent->aiCharacter != AICHAR_HELGA
				 && ent->aiCharacter != AICHAR_HEINRICH ) {

				for ( i = 0 ; i < g_maxclients.integer ; i++ ) {
					player = &g_entities[i];

					if ( !player || !player->inuse ) {
						continue;
					}

					if ( player->r.svFlags & SVF_CASTAI ) {
						continue;
					}
				}
			}

			oldmaxZ = ent->r.maxs[2];

			// make sure the area is clear
			AIChar_SetBBox( ent, cs, qfalse );

			VectorAdd( ent->r.currentOrigin, ent->r.mins, mins );
			VectorAdd( ent->r.currentOrigin, ent->r.maxs, maxs );
			trap_UnlinkEntity( ent );

			numTouch = trap_EntitiesInBox( mins, maxs, touch, 10 );

			if ( numTouch ) {
				for ( i = 0; i < numTouch; i++ ) {
					if ( g_entities[touch[i]].r.contents & MASK_PLAYERSOLID ) {
						break;
					}
				}
				if ( i == numTouch ) {
					numTouch = 0;
				}
			}

			if ( numTouch == 0 ) {    // ok to spawn

				BG_SetBehaviorForSurvival(ent->aiCharacter);
				AICast_ApplySurvivalAttributes(ent, cs);
				ent->r.contents = CONTENTS_BODY;
				ent->clipmask = MASK_PLAYERSOLID | CONTENTS_MONSTERCLIP;
				ent->takedamage = qtrue;
				ent->waterlevel = 0;
				ent->watertype = 0;
				ent->flags = 0;
				ent->die = AICast_Die;
				ent->client->ps.eFlags &= ~EF_DEAD;
				ent->s.eFlags &= ~EF_DEAD;
				player = AICast_FindEntityForName( "player" );

                // Selecting the spawn point for the AI
				SelectSpawnPoint_AI( player, ent, spawn_origin, spawn_angles );
				G_SetOrigin( ent, spawn_origin );
				VectorCopy( spawn_origin, ent->client->ps.origin );
				SetClientViewAngle( ent, spawn_angles );

				// Activate respawn scripts for AI
				AICast_ScriptEvent(cs, "respawn", "");
                
				// Turn off Headshot flag and reattach hat
				ent->client->ps.eFlags &= ~EF_HEADSHOT;
				G_AddEvent( ent, EV_REATTACH_HAT, 0 );

				cs->rebirthTime = 0;
				cs->deathTime = 0;

				ent->client->ps.eFlags &= ~EF_DEATH_FRAME;
				ent->client->ps.eFlags &= ~EF_FORCE_END_FRAME;
				ent->client->ps.eFlags |= EF_NO_TURN_ANIM;

				// play the revive animation
				cs->revivingTime = level.time + BG_AnimScriptEvent( &ent->client->ps, ANIM_ET_REVIVE, qfalse, qtrue );

				AICast_StateChange( cs, AISTATE_RELAXED );
				cs->enemyNum = -1;

				if (ent->aiTeam == 1) { 
					svParams.spawnedThisWaveFriendly++;
				} else { 
					svParams.spawnedThisWave++;
				}

			} else {
				// can't spawn yet, so set bbox back, and wait
				ent->r.maxs[2] = oldmaxZ;
				ent->client->ps.maxs[2] = ent->r.maxs[2];
			}
			trap_LinkEntity( ent );


}

// Load survival gamemode parameters from .surv file
void AI_LoadSurvivalTable( const char* mapname )
{
	int handle;
	pc_token_t token;

	handle = trap_PC_LoadSource( va( "maps/%s.surv", mapname ) );
	if ( !handle ) {
		G_Printf( S_COLOR_YELLOW "WARNING: Failed to load .surv file. Trying to load default.surv\n" );

		handle = trap_PC_LoadSource( "maps/default.surv" );

		if ( !handle ) {
			G_Printf( S_COLOR_RED "ERROR: Failed to load default.surv file\n" );
			return;
		}
	}

	memset( &svParams, 0, sizeof( svParams_t ) );

	// Find and parse parameter
	while ( 1 ) {
		if ( !trap_PC_ReadToken( handle, &token ) ) {
			break;
		}
		if ( !Q_stricmp( token.string, "survival" ) ) {
			BG_ParseSurvivalTable( handle );
			break;
		}
	}

	trap_PC_FreeSource( handle );
}

// Read survival parameters into aiDefaults from given file handle
// File handle position expected to be at opening brace of survival block
qboolean BG_ParseSurvivalTable(int handle)
{
	pc_token_t token;

	if (!trap_PC_ReadToken(handle, &token) || Q_stricmp(token.string, "{"))
	{
		PC_SourceError(handle, "expected '{'");
		return qfalse;
	}

	while (1)
	{
		if (!trap_PC_ReadToken(handle, &token))
		{
			break;
		}
		if (token.string[0] == '}')
		{
			break;
		}

		// float
		if (!Q_stricmp(token.string, "initialKillCountRequirement"))
		{
			if (!PC_Int_Parse(handle, &svParams.initialKillCountRequirement))
			{
				PC_SourceError(handle, "expected initialKillCountRequirement value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "initialSoldiersCount"))
		{
			if (!PC_Int_Parse(handle, &svParams.initialSoldiersCount))
			{
				PC_SourceError(handle, "expected initialSoldiersCount value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "initialEliteGuardsCount"))
		{
			if (!PC_Int_Parse(handle, &svParams.initialEliteGuardsCount))
			{
				PC_SourceError(handle, "expected initialEliteGuardsCount value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "initialBlackGuardsCount"))
		{
			if (!PC_Int_Parse(handle, &svParams.initialBlackGuardsCount))
			{
				PC_SourceError(handle, "expected initialBlackGuardsCount value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "initialVenomsCount"))
		{
			if (!PC_Int_Parse(handle, &svParams.initialVenomsCount))
			{
				PC_SourceError(handle, "expected initialVenomsCount value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "initialZombiesCount"))
		{
			if (!PC_Int_Parse(handle, &svParams.initialZombiesCount))
			{
				PC_SourceError(handle, "expected initialZombiesCount value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "initialWarriorsCount"))
		{
			if (!PC_Int_Parse(handle, &svParams.initialWarriorsCount))
			{
				PC_SourceError(handle, "expected initialWarriorsCount value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "initialProtosCount"))
		{
			if (!PC_Int_Parse(handle, &svParams.initialProtosCount))
			{
				PC_SourceError(handle, "expected initialProtosCount value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "initialPartisansCount"))
		{
			if (!PC_Int_Parse(handle, &svParams.initialPartisansCount))
			{
				PC_SourceError(handle, "expected initialPartisansCount value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "initialGhostsCount"))
		{
			if (!PC_Int_Parse(handle, &svParams.initialGhostsCount))
			{
				PC_SourceError(handle, "expected initialGhostsCount value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "initialPriestsCount"))
		{
			if (!PC_Int_Parse(handle, &svParams.initialPriestsCount))
			{
				PC_SourceError(handle, "expected initialPriestsCount value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "initialFlamersCount"))
		{
			if (!PC_Int_Parse(handle, &svParams.initialFlamersCount))
			{
				PC_SourceError(handle, "expected initialFlamersCount value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "soldiersIncrease"))
		{
			if (!PC_Int_Parse(handle, &svParams.soldiersIncrease))
			{
				PC_SourceError(handle, "expected soldiersIncrease value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "eliteGuardsIncrease"))
		{
			if (!PC_Int_Parse(handle, &svParams.eliteGuardsIncrease))
			{
				PC_SourceError(handle, "expected eliteGuardsIncrease value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "blackGuardsIncrease"))
		{
			if (!PC_Int_Parse(handle, &svParams.blackGuardsIncrease))
			{
				PC_SourceError(handle, "expected blackGuardsIncrease value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "venomsIncrease"))
		{
			if (!PC_Int_Parse(handle, &svParams.venomsIncrease))
			{
				PC_SourceError(handle, "expected venomsIncrease value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "zombiesIncrease"))
		{
			if (!PC_Int_Parse(handle, &svParams.zombiesIncrease))
			{
				PC_SourceError(handle, "expected zombiesIncrease value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "warriorsIncrease"))
		{
			if (!PC_Int_Parse(handle, &svParams.warriorsIncrease))
			{
				PC_SourceError(handle, "expected warriorsIncrease value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "protosIncrease"))
		{
			if (!PC_Int_Parse(handle, &svParams.protosIncrease))
			{
				PC_SourceError(handle, "expected protosIncrease value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "ghostsIncrease"))
		{
			if (!PC_Int_Parse(handle, &svParams.ghostsIncrease))
			{
				PC_SourceError(handle, "expected ghostsIncrease value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "priestsIncrease"))
		{
			if (!PC_Int_Parse(handle, &svParams.priestsIncrease))
			{
				PC_SourceError(handle, "expected priestsIncrease value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "flamersIncrease"))
		{
			if (!PC_Int_Parse(handle, &svParams.flamersIncrease))
			{
				PC_SourceError(handle, "expected flamersIncrease value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "maxSoldiers"))
		{
			if (!PC_Int_Parse(handle, &svParams.maxSoldiers))
			{
				PC_SourceError(handle, "expected maxSoldiers value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "maxEliteGuards"))
		{
			if (!PC_Int_Parse(handle, &svParams.maxEliteGuards))
			{
				PC_SourceError(handle, "expected maxEliteGuards value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "maxBlackGuards"))
		{
			if (!PC_Int_Parse(handle, &svParams.maxBlackGuards))
			{
				PC_SourceError(handle, "expected maxBlackGuards value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "maxVenoms"))
		{
			if (!PC_Int_Parse(handle, &svParams.maxVenoms))
			{
				PC_SourceError(handle, "expected maxVenoms value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "maxZombies"))
		{
			if (!PC_Int_Parse(handle, &svParams.maxZombies))
			{
				PC_SourceError(handle, "expected maxZombies value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "maxWarriors"))
		{
			if (!PC_Int_Parse(handle, &svParams.maxWarriors))
			{
				PC_SourceError(handle, "expected maxWarriors value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "maxProtos"))
		{
			if (!PC_Int_Parse(handle, &svParams.maxProtos))
			{
				PC_SourceError(handle, "expected maxProtos value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "maxGhosts"))
		{
			if (!PC_Int_Parse(handle, &svParams.maxGhosts))
			{
				PC_SourceError(handle, "expected maxGhosts value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "maxPriests"))
		{
			if (!PC_Int_Parse(handle, &svParams.maxPriests))
			{
				PC_SourceError(handle, "expected maxPriests value");
				return qfalse;
			}
		}		
		else if (!Q_stricmp(token.string, "maxFlamers"))
		{
			if (!PC_Int_Parse(handle, &svParams.maxFlamers))
			{
				PC_SourceError(handle, "expected maxFlamers value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "waveEg"))
		{
			if (!PC_Int_Parse(handle, &svParams.waveEg))
			{
				PC_SourceError(handle, "expected waveEg value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "waveBg"))
		{
			if (!PC_Int_Parse(handle, &svParams.waveBg))
			{
				PC_SourceError(handle, "expected waveBg value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "waveV"))
		{
			if (!PC_Int_Parse(handle, &svParams.waveV))
			{
				PC_SourceError(handle, "expected waveV value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "waveWarz"))
		{
			if (!PC_Int_Parse(handle, &svParams.waveWarz))
			{
				PC_SourceError(handle, "expected waveWarz value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "waveProtos"))
		{
			if (!PC_Int_Parse(handle, &svParams.waveProtos))
			{
				PC_SourceError(handle, "expected waveProtos value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "waveGhosts"))
		{
			if (!PC_Int_Parse(handle, &svParams.waveGhosts))
			{
				PC_SourceError(handle, "expected waveGhosts value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "wavePriests"))
		{
			if (!PC_Int_Parse(handle, &svParams.wavePriests))
			{
				PC_SourceError(handle, "expected wavePriests value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "waveFlamers"))
		{
			if (!PC_Int_Parse(handle, &svParams.waveFlamers))
			{
				PC_SourceError(handle, "expected waveFlamers value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "powerupDropChance"))
		{
			if (!PC_Int_Parse(handle, &svParams.powerupDropChance))
			{
				PC_SourceError(handle, "expected powerupDropChance value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "powerupDropChanceScavengerIncrease"))
		{
			if (!PC_Int_Parse(handle, &svParams.powerupDropChanceScavengerIncrease))
			{
				PC_SourceError(handle, "expected powerupDropChanceScavengerIncrease value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "treasureDropChance"))
		{
			if (!PC_Int_Parse(handle, &svParams.treasureDropChance))
			{
				PC_SourceError(handle, "expected treasureDropChance value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "treasureDropChanceScavengerIncrease"))
		{
			if (!PC_Int_Parse(handle, &svParams.treasureDropChanceScavengerIncrease))
			{
				PC_SourceError(handle, "expected treasureDropChanceScavengerIncrease value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "scoreHeadshotKill"))
		{
			if (!PC_Int_Parse(handle, &svParams.scoreHeadshotKill))
			{
				PC_SourceError(handle, "expected scoreHeadshotKill value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "scoreHit"))
		{
			if (!PC_Int_Parse(handle, &svParams.scoreHit))
			{
				PC_SourceError(handle, "expected scoreHit value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "scoreBaseKill"))
		{
			if (!PC_Int_Parse(handle, &svParams.scoreBaseKill))
			{
				PC_SourceError(handle, "expected scoreBaseKill value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "scoreKnifeBonus"))
		{
			if (!PC_Int_Parse(handle, &svParams.scoreKnifeBonus))
			{
				PC_SourceError(handle, "expected scoreKnifeBonus value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "friendlySpawnTime"))
		{
			if (!PC_Int_Parse(handle, &svParams.friendlySpawnTime))
			{
				PC_SourceError(handle, "expected friendlySpawnTime value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "defaultSpawnTime"))
		{
			if (!PC_Int_Parse(handle, &svParams.defaultSpawnTime))
			{
				PC_SourceError(handle, "expected defaultSpawnTime value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "egSpawnTime"))
		{
			if (!PC_Int_Parse(handle, &svParams.egSpawnTime))
			{
				PC_SourceError(handle, "expected egSpawnTime value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "bgSpawnTime"))
		{
			if (!PC_Int_Parse(handle, &svParams.bgSpawnTime))
			{
				PC_SourceError(handle, "expected bgSpawnTime value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "vSpawnTime"))
		{
			if (!PC_Int_Parse(handle, &svParams.vSpawnTime))
			{
				PC_SourceError(handle, "expected vSpawnTime value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "protoSpawnTime"))
		{
			if (!PC_Int_Parse(handle, &svParams.protoSpawnTime))
			{
				PC_SourceError(handle, "expected protoSpawnTime value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "warzSpawnTime"))
		{
			if (!PC_Int_Parse(handle, &svParams.warzSpawnTime))
			{
				PC_SourceError(handle, "expected warzSpawnTime value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "ghostSpawnTime"))
		{
			if (!PC_Int_Parse(handle, &svParams.ghostSpawnTime))
			{
				PC_SourceError(handle, "expected ghostSpawnTime value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "priestSpawnTime"))
		{
			if (!PC_Int_Parse(handle, &svParams.priestSpawnTime))
			{
				PC_SourceError(handle, "expected priestSpawnTime value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "flamerSpawnTime"))
		{
			if (!PC_Int_Parse(handle, &svParams.flamerSpawnTime))
			{
				PC_SourceError(handle, "expected flamerSpawnTime value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "aliveFriendliestoCallReinforce"))
		{
			if (!PC_Int_Parse(handle, &svParams.aliveFriendliestoCallReinforce))
			{
				PC_SourceError(handle, "expected aliveFriendliestoCallReinforce value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "maxPerks"))
		{
			if (!PC_Int_Parse(handle, &svParams.maxPerks))
			{
				PC_SourceError(handle, "expected maxPerks value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "maxPerksEng"))
		{
			if (!PC_Int_Parse(handle, &svParams.maxPerksEng))
			{
				PC_SourceError(handle, "expected maxPerksEng value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "armorPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.armorPrice))
			{
				PC_SourceError(handle, "expected armorPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "randomPerkPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.randomPerkPrice))
			{
				PC_SourceError(handle, "expected randomPerkPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "randomWeaponPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.randomWeaponPrice))
			{
				PC_SourceError(handle, "expected randomWeaponPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "secondchancePrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.secondchancePrice))
			{
				PC_SourceError(handle, "expected secondchancePrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "runnerPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.runnerPrice))
			{
				PC_SourceError(handle, "expected runnerPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "scavengerPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.scavengerPrice))
			{
				PC_SourceError(handle, "expected scavengerPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "fasthandsPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.fasthandsPrice))
			{
				PC_SourceError(handle, "expected fasthandsPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "doubleshotPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.doubleshotPrice))
			{
				PC_SourceError(handle, "expected doubleshotPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "resiliencePrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.resiliencePrice))
			{
				PC_SourceError(handle, "expected resiliencePrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "defaultPerkPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.defaultPerkPrice))
			{
				PC_SourceError(handle, "expected defaultPerkPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "knifePrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.knifePrice))
			{
				PC_SourceError(handle, "expected knifePrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "lugerPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.lugerPrice))
			{
				PC_SourceError(handle, "expected lugerPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "coltPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.coltPrice))
			{
				PC_SourceError(handle, "expected coltPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "silencerPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.silencerPrice))
			{
				PC_SourceError(handle, "expected silencerPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "tt33Price"))
		{
			if (!PC_Int_Parse(handle, &svParams.tt33Price))
			{
				PC_SourceError(handle, "expected tt33Price value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "revolverPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.revolverPrice))
			{
				PC_SourceError(handle, "expected revolverPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "akimboPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.akimboPrice))
			{
				PC_SourceError(handle, "expected akimboPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "hdmPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.hdmPrice))
			{
				PC_SourceError(handle, "expected hdmPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "dualtt33Price"))
		{
			if (!PC_Int_Parse(handle, &svParams.dualtt33Price))
			{
				PC_SourceError(handle, "expected dualtt33Price value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "mp40Price"))
		{
			if (!PC_Int_Parse(handle, &svParams.mp40Price))
			{
				PC_SourceError(handle, "expected mp40Price value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "stenPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.stenPrice))
			{
				PC_SourceError(handle, "expected stenPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "mp34Price"))
		{
			if (!PC_Int_Parse(handle, &svParams.mp34Price))
			{
				PC_SourceError(handle, "expected mp34Price value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "thompsonPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.thompsonPrice))
			{
				PC_SourceError(handle, "expected thompsonPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "ppshPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.ppshPrice))
			{
				PC_SourceError(handle, "expected ppshPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "mauserPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.mauserPrice))
			{
				PC_SourceError(handle, "expected mauserPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "mosinPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.mosinPrice))
			{
				PC_SourceError(handle, "expected mosinPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "delislePrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.delislePrice))
			{
				PC_SourceError(handle, "expected delislePrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "sniperriflePrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.sniperriflePrice))
			{
				PC_SourceError(handle, "expected sniperriflePrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "snooperScopePrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.snooperScopePrice))
			{
				PC_SourceError(handle, "expected snooperScopePrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "m1garandPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.m1garandPrice))
			{
				PC_SourceError(handle, "expected m1garandPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "g43Price"))
		{
			if (!PC_Int_Parse(handle, &svParams.g43Price))
			{
				PC_SourceError(handle, "expected g43Price value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "m1941Price"))
		{
			if (!PC_Int_Parse(handle, &svParams.m1941Price))
			{
				PC_SourceError(handle, "expected m1941Price value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "mp44Price"))
		{
			if (!PC_Int_Parse(handle, &svParams.mp44Price))
			{
				PC_SourceError(handle, "expected mp44Price value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "barPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.barPrice))
			{
				PC_SourceError(handle, "expected barPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "fg42Price"))
		{
			if (!PC_Int_Parse(handle, &svParams.fg42Price))
			{
				PC_SourceError(handle, "expected fg42Price value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "shotgunPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.shotgunPrice))
			{
				PC_SourceError(handle, "expected shotgunPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "auto5Price"))
		{
			if (!PC_Int_Parse(handle, &svParams.auto5Price))
			{
				PC_SourceError(handle, "expected auto5Price value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "mg42mPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.mg42mPrice))
			{
				PC_SourceError(handle, "expected mg42mPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "browningPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.browningPrice))
			{
				PC_SourceError(handle, "expected browningPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "panzerPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.panzerPrice))
			{
				PC_SourceError(handle, "expected panzerPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "flamerPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.flamerPrice))
			{
				PC_SourceError(handle, "expected flamerPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "teslaPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.teslaPrice))
			{
				PC_SourceError(handle, "expected teslaPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "venomPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.venomPrice))
			{
				PC_SourceError(handle, "expected venomPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "grenPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.grenPrice))
			{
				PC_SourceError(handle, "expected grenPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "pineapplePrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.pineapplePrice))
			{
				PC_SourceError(handle, "expected pineapplePrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "defaultWeaponPrice"))
		{
			if (!PC_Int_Parse(handle, &svParams.defaultWeaponPrice))
			{
				PC_SourceError(handle, "expected defaultWeaponPrice value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "intermissionTime"))
		{
			if (!PC_Int_Parse(handle, &svParams.intermissionTime))
			{
				PC_SourceError(handle, "expected intermissionTime value");
				return qfalse;
			}
			// string
		}
		else if (!Q_stricmp(token.string, "waveStartSound"))
		{
			// Parse the wave start sound path
			if (!PC_String_ParseNoAlloc(handle, svParams.waveStartSound, MAX_QPATH))
			{
				PC_SourceError(handle, "expected waveStartSound value");
				return qfalse;
			}
		}
		else if (!Q_stricmp(token.string, "waveEndSound"))
		{
			// Parse the wave end sound path
			if (!PC_String_ParseNoAlloc(handle, svParams.waveEndSound, MAX_QPATH))
			{
				PC_SourceError(handle, "expected waveEndSound value");
				return qfalse;
			}
		}
		else
		{
			PC_SourceError(handle, "unknown token '%s'", token.string);
			return qfalse;
		}
	}

	return qtrue;
}
