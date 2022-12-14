/*
 Copyright (c) 2013 yvt
 based on code of pysnip (c) Mathias Kaerlev 2011-2012.
 
 This file is part of OpenSpades.
 
 OpenSpades is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 OpenSpades is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with OpenSpades.  If not, see <http://www.gnu.org/licenses/>.
 
 */

#include "Client.h"

#include <Core/Settings.h>
#include <Core/Strings.h>

#include "IAudioChunk.h"
#include "IAudioDevice.h"

#include "ClientUI.h"
#include "PaletteView.h"
#include "LimboView.h"
#include "MapView.h"
#include "Corpse.h"

#include "World.h"
#include "Weapon.h"

#include "NetClient.h"

using namespace std;

SPADES_SETTING(cg_mouseSensitivity, "1");
SPADES_SETTING(cg_mouseCrouchScale, "0.75");
SPADES_SETTING(cg_zoomedMouseSensScale, "0.75");
SPADES_SETTING(cg_mouseExpPower, "1");
SPADES_SETTING(cg_invertMouseY, "0");


SPADES_SETTING(cg_holdAimDownSight, "0");

//Chameleon: easier with SMG
SPADES_SETTING(cg_keyWeaponMode, "x");
//Chameleon: easier with SMG
SPADES_SETTING(weap_burstFire, "0");
SPADES_SETTING(weap_burstRounds, "3");

SPADES_SETTING(cg_keyAttack, "LeftMouseButton");
SPADES_SETTING(cg_keyAltAttack, "RightMouseButton");
SPADES_SETTING(cg_keyToolSpade, "1");
SPADES_SETTING(cg_keyToolBlock, "2");
SPADES_SETTING(cg_keyToolWeapon, "3");
SPADES_SETTING(cg_keyToolGrenade, "4");
SPADES_SETTING(cg_keyReloadWeapon, "r");
SPADES_SETTING(cg_keyFlashlight, "f");
SPADES_SETTING(cg_keyLastTool, "");

SPADES_SETTING(cg_keyMoveLeft, "a");
SPADES_SETTING(cg_keyMoveRight, "d");
SPADES_SETTING(cg_keyMoveForward, "w");
SPADES_SETTING(cg_keyMoveBackward, "s");
SPADES_SETTING(cg_keyJump, "Space");
SPADES_SETTING(cg_keyCrouch, "Control");
SPADES_SETTING(cg_keySprint, "Shift");
SPADES_SETTING(cg_keySneak, "v");

SPADES_SETTING(cg_keyCaptureColor, "e");
SPADES_SETTING(cg_keyGlobalChat, "t");
SPADES_SETTING(cg_keyTeamChat, "y");
SPADES_SETTING(cg_keyChangeMapScale, "m");
SPADES_SETTING(cg_keyToggleMapZoom, "n");
SPADES_SETTING(cg_keyScoreboard, "Tab");
SPADES_SETTING(cg_keyLimbo, "l");

SPADES_SETTING(cg_keyScreenshot, "0");
SPADES_SETTING(cg_keySceneshot, "9");
SPADES_SETTING(cg_keySaveMap, "8");

SPADES_SETTING(cg_switchToolByWheel, "1");

SPADES_SETTING(cg_debugCorpse, "0");

SPADES_SETTING(cg_alerts, "1");

SPADES_SETTING(cg_manualFocus, "0");
SPADES_SETTING(cg_keyAutoFocus, "MiddleMouseButton");

//Chameleon: absolute maximum sound distance. footstep&other sound limit is 0.75 times this
SPADES_SETTING(snd_maxDistance, "150");

//bincoulars zoom. used with nades
SPADES_SETTING(v_binocsZoom, "2");

namespace spades {
	namespace client {
		
		bool Client::WantsToBeClosed() {
			return readyToClose;
		}
		
		bool FirstPersonSpectate = false;
		
		void Client::Closing() {
			SPADES_MARK_FUNCTION();
		}

		
		bool Client::NeedsAbsoluteMouseCoordinate() {
			SPADES_MARK_FUNCTION();
			if(scriptedUI->NeedsInput()) {
				return true;
			}
			if(!world) {
				// now loading.
				return true;
			}
			if(IsLimboViewActive()) {
				return true;
			}
			return false;
		}
		
		void Client::MouseEvent(float x, float y) 
		{
			SPADES_MARK_FUNCTION();
			
			if(scriptedUI->NeedsInput()) {
				scriptedUI->MouseEvent(x, y);
				return;
			}
			
			if(IsLimboViewActive()){
				limbo->MouseEvent(x, y);
				return;
			}
			
			if(IsFollowing())
			{
				SPAssert(world != nullptr);
				/*
				 if(world->GetLocalPlayer() &&
				 world->GetLocalPlayer()->GetTeamId() >= 2 &&
				 followingPlayerId == world->GetLocalPlayerIndex()){
				 // invert dir
				 x = -x; y = -y;
				 }
				 */
				
				x = -x;
				if (!cg_invertMouseY)
					y = -y;
				
				followYaw -= x * 0.003f;
				followPitch -= y * 0.003f;
				if(followPitch < -M_PI*.45f) followPitch = -static_cast<float>(M_PI)*.45f;
				if(followPitch > M_PI*.45f) followPitch = static_cast<float>(M_PI) * .45f;
				followYaw = fmodf(followYaw, static_cast<float>(M_PI)*2.f);
			}
			else if(world && world->GetLocalPlayer())
			{
				Player *p = world->GetLocalPlayer();
				float aimDownState = GetAimDownState();
				if(p->IsAlive())
				{
					float rollFac = 1;
					rollFac *= 2.f - p->GetHealth()*0.015f; //from 1 to 2
					rollFac *= fmax(1.f, 2.f - soundDistance*0.0625f); //from 1 to 2
					rollFac *= 1.f + p->spreadAdd*0.25f; //from 1 to 2

					x /= GetAimDownZoomScale();
					y /= GetAimDownZoomScale();

					//Chameleon: mouse inertia
					if (mouseInertia > 0.01f)
					{
						//SPLog("MouseEvent: %f, %f", x, y);
						//add speed to mouse acording to input x and y and multiplied by mouseInertia value
						mouseX += x*mouseInertia;
						mouseY += y*mouseInertia;
						//we will add missing speed in Client::Update(dt)
						x -= x*mouseInertia;
						y -= y*mouseInertia;
					}

					//Chameleon: weapon visual lag
					{
						/*weapX -= x*0.00025f*(1.f - aimDownState);
						weapY -= y*0.00025f*(1.f - aimDownState);*/
						weapX -= x*0.000015625f*(GetAimDownZoomScale() + 1)*(rollFac + 4)*(aimDownState*2 - 1);
						weapY += y*0.000015625f*(GetAimDownZoomScale() + 1)*(rollFac + 4);
					}

					float rad = x * x + y * y;
					if(rad > 0.f) 
					{
						if((float)cg_mouseExpPower < 0.001f || isnan((float)cg_mouseExpPower)) 
						{
							SPLog("Invalid cg_mouseExpPower value, resetting to 1.0");
							cg_mouseExpPower = 1.f;
						}
						float factor = renderer->ScreenWidth() * .1f;
						factor *= factor;
						rad /= factor;
						rad = powf(rad, (float)cg_mouseExpPower * 0.5f - 0.5f);
						
						// shouldn't happen...
						if(isnan(rad)) rad = 1.f;
						
						x *= rad;
						y *= rad;
					}
					
					if(aimDownState > 0.f) 
					{
						float scale = cg_zoomedMouseSensScale;
						scale = scale*aimDownState;
						x *= scale;
						y *= scale;
					}
					
					x *= (float)cg_mouseSensitivity;
					y *= (float)cg_mouseSensitivity;
					
					if(cg_invertMouseY)
						y = -y;

					if (p->crouching)
					{
						x *= (float)cg_mouseCrouchScale;
						y *= (float)cg_mouseCrouchScale;
					}
					
					
					
					//Chameleon: drunk cam, affected by health and soundDistance
					//old one; the new one is "rollfac"
					//so: input * health(from 1 to 0.25, as health goes 0 to 100) * soundDistance(from 3 to 1, as soundDistance goes 0 to >16)
					/*if (3-soundDistance*0.125f > 1)
						mouseRoll += x*0.001f*(1.f - world->GetLocalPlayer()->GetHealth()*0.0075f)*(3-soundDistance*0.125f);
					else
						mouseRoll += x*0.001f*(1.f - world->GetLocalPlayer()->GetHealth()*0.0075f); */

					mouseRoll += x*0.0005f*rollFac;

					//Chameleon: freeaim
					mouseYaw -= x*0.003f;
					mousePitch += y*0.003f;
					
					p->Turn(x * 0.003f, y * 0.003f);
				}
			}
		}
		void Client::MouseEventInertia(float x, float y) 
		{
			SPADES_MARK_FUNCTION();

			
			//if (scriptedUI->NeedsInput())
			//{
			//	return;
			//}

			//if (IsLimboViewActive())
			//{
			//	return;
			//}

			if (IsFollowing())
			{
				return;
			}

			else if (world && world->GetLocalPlayer())
			{
				Player *p = world->GetLocalPlayer();
				float aimDownState = GetAimDownState();
				if (p->IsAlive())
				{
					float rollFac = 1;
					rollFac *= 2.f - p->GetHealth()*0.015f; //from 1 to 2
					rollFac *= fmax(1.f, 2.f - soundDistance*0.0625f); //from 1 to 2
					rollFac *= 1.f + p->spreadAdd*0.25f; //from 1 to 2

					//Chameleon: weapon visual lag
					{
						/*weapX -= x*0.00025f*(1.f - aimDownState);
						weapY -= y*0.00025f*(1.f - aimDownState);*/
						weapX -= x*0.000015625f*(GetAimDownZoomScale() + 1)*(rollFac + 4)*(aimDownState*2 - 1);
						weapY += y*0.000015625f*(GetAimDownZoomScale() + 1)*(rollFac + 4);
					}

					//SPLog("MouseEventInertia: %f, %f", x, y);

					//x /= GetAimDownZoomScale();
					//y /= GetAimDownZoomScale();

					float rad = x * x + y * y;
					if (rad > 0.f)
					{
						if ((float)cg_mouseExpPower < 0.001f || isnan((float)cg_mouseExpPower))
						{
							SPLog("Invalid cg_mouseExpPower value, resetting to 1.0");
							cg_mouseExpPower = 1.f;
						}
						float factor = renderer->ScreenWidth() * .1f;
						factor *= factor;
						rad /= factor;
						rad = powf(rad, (float)cg_mouseExpPower * 0.5f - 0.5f);

						// shouldn't happen...
						if (isnan(rad)) rad = 1.f;

						x *= rad;
						y *= rad;
					}

					if (aimDownState > 0.f)
					{
						float scale = cg_zoomedMouseSensScale;
						scale = scale*aimDownState;
						x *= scale;
						y *= scale;
					}

					x *= (float)cg_mouseSensitivity;
					y *= (float)cg_mouseSensitivity;

					if (cg_invertMouseY)
						y = -y;

					if (p->crouching)
					{
						x *= (float)cg_mouseCrouchScale;
						y *= (float)cg_mouseCrouchScale;
					}

					//Chameleon: drunk cam, affected by health and soundDistance
					//old one; the new one is "rollfac"
					//so: input * health(from 1 to 0.25, as health goes 0 to 100) * soundDistance(from 3 to 1, as soundDistance goes 0 to >16)
					/*if (3-soundDistance*0.125f > 1)
					mouseRoll += x*0.001f*(1.f - world->GetLocalPlayer()->GetHealth()*0.0075f)*(3-soundDistance*0.125f);
					else
					mouseRoll += x*0.001f*(1.f - world->GetLocalPlayer()->GetHealth()*0.0075f); */

					mouseRoll += x*0.0005f*rollFac;

					//Chameleon: freeaim
					mouseYaw -= x*0.003f;
					mousePitch += y*0.003f;

					p->Turn(x * 0.003f, y * 0.003f);
				}
			}
		}
		
		void Client::WheelEvent(float x, float y) {
			SPADES_MARK_FUNCTION();
			
			if(scriptedUI->NeedsInput()) {
				scriptedUI->WheelEvent(x, y);
				return;
			}
			
			if(y > .5f) {
				KeyEvent("WheelDown", true);
				KeyEvent("WheelDown", false);
			}else if(y < -.5f){
				KeyEvent("WheelUp", true);
				KeyEvent("WheelUp", false);
			}
		}
		
		void Client::TextInputEvent(const std::string &ch){
			SPADES_MARK_FUNCTION();
			
			if (scriptedUI->NeedsInput() && !scriptedUI->isIgnored(ch)) {
				scriptedUI->TextInputEvent(ch);
				return;
			}
			
			// we don't get "/" here anymore
		}
		
		void Client::TextEditingEvent(const std::string &ch,
									  int start, int len) {
			SPADES_MARK_FUNCTION();
			
			if (scriptedUI->NeedsInput() && !scriptedUI->isIgnored(ch)) {
				scriptedUI->TextEditingEvent(ch, start, len);
				return;
			}
		}
		
		bool Client::AcceptsTextInput() {
			SPADES_MARK_FUNCTION();
			
			if(scriptedUI->NeedsInput()) {
				return scriptedUI->AcceptsTextInput();
			}
			return false;
		}
		
		AABB2 Client::GetTextInputRect() {
			SPADES_MARK_FUNCTION();
			if(scriptedUI->NeedsInput()) {
				return scriptedUI->GetTextInputRect();
			}
			return AABB2();
		}
		
		static bool CheckKey(const std::string& cfg,
							 const std::string& input) {
			if(cfg.empty())
				return false;

			static const std::string space1("space");
			static const std::string space2("spacebar");
			static const std::string space3("spacekey");

			if(EqualsIgnoringCase(cfg, space1) ||
				 EqualsIgnoringCase(cfg, space2) ||
				 EqualsIgnoringCase(cfg, space3))
			{

				if(input == " ")
					return true;
			}
			else
			{
				if(EqualsIgnoringCase(cfg, input))
						return true;
			}
			return false;
		}
		
		void Client::KeyEvent(const std::string& name, bool down){
			SPADES_MARK_FUNCTION();
			
			if(scriptedUI->NeedsInput()) {
				if(!scriptedUI->isIgnored(name)) {
					scriptedUI->KeyEvent(name, down);
				}else{
					if(!down) {
						scriptedUI->setIgnored("");
					}
				}
				return;
			}
			
			if(name == "Escape"){
				if(down){
					if(inGameLimbo){
						inGameLimbo = false;
					}else{
						if(GetWorld() == nullptr){
							// no world = loading now.
							// in this case, abort download, and quit the game immediately.
							readyToClose = true;
						}
						else {
							scriptedUI->EnterClientMenu();
						}
					}
				}
			}else if(world)
			{
				if(IsLimboViewActive())
				{
					if(down)
					{
						limbo->KeyEvent(name);
					}
					return;
				}
				if(IsFollowing())
				{
					if(CheckKey(cg_keyAttack, name))
					{
						if(down)
						{
							if(world->GetLocalPlayer()->GetTeamId() >= 2 ||
							   time > lastAliveTime + 1.3f)
								FollowNextPlayer(false);
						}
						return;
					}
					else if(CheckKey(cg_keyAltAttack, name))
					{
						if(down)
						{
							if(world->GetLocalPlayer()->GetTeamId() >= 2)
							{
								// spectating
								followingPlayerId = world->GetLocalPlayerIndex();
							}
							else if(time > lastAliveTime + 1.3f)
							{
								// dead
								FollowNextPlayer(true);
							}
						}
						return;
					}
				}
				if(world->GetLocalPlayer())
				{
					Player *p = world->GetLocalPlayer();

					if(p->IsAlive() && p->GetTool() == Player::ToolBlock && down) 
					{
						if(paletteView->KeyInput(name))
						{
							return;
						}
					}
					
					if(cg_debugCorpse)
					{
						if(name == "p" && down)
						{
							Corpse *corp;
							Player *victim = world->GetLocalPlayer();
							corp = new Corpse(renderer, map, victim);
							corp->AddImpulse(victim->GetFront() * 32.f);
							corpses.emplace_back(corp);
							
							if(corpses.size() > corpseHardLimit){
								corpses.pop_front();
							}else if(corpses.size() > corpseSoftLimit){
								RemoveInvisibleCorpses();
							}
						}
					}
					if(CheckKey(cg_keyMoveLeft, name)){
						playerInput.moveLeft = down;
						keypadInput.left = down;
						if(down) playerInput.moveRight = false;
						else playerInput.moveRight = keypadInput.right;
					}else if(CheckKey(cg_keyMoveRight, name)){
						playerInput.moveRight = down;
						keypadInput.right = down;
						if(down) playerInput.moveLeft = false;
						else playerInput.moveLeft = keypadInput.left;
					}else if(CheckKey(cg_keyMoveForward, name)){
						playerInput.moveForward = down;
						keypadInput.forward = down;
						if(down) playerInput.moveBackward = false;
						else playerInput.moveBackward = keypadInput.backward;
					}else if(CheckKey(cg_keyMoveBackward, name)){
						playerInput.moveBackward = down;
						keypadInput.backward = down;
						if(down) playerInput.moveForward = false;
						else playerInput.moveForward = keypadInput.forward;
					}
					//Chameleon
					else if(CheckKey(cg_keyCrouch, name))
					{
						playerInput.crouch = down;
					}
					else if(CheckKey(cg_keySprint, name))
					{
						playerInput.sprint = down;
						if(down)
						{
							//if(world->GetLocalPlayer()->IsToolWeapon())
							{
								weapInput.secondary = false;
							}
						}
					}
					else if(CheckKey(cg_keySneak, name)){
						playerInput.sneak = down;
					}
					else if(CheckKey(cg_keyJump, name))
					{
						if(down)
						{
							if (world->GetLocalPlayer()->IsToolWeapon())
							{
								weapInput.secondary = false;
							}
							//FirstPersonSpectate = !FirstPersonSpectate; unused - spectator is always 1st person
						}
						playerInput.jump = down;
					}
					//Chameleon
					else if (CheckKey(cg_keyWeaponMode, name) && down)
					{
						if (p->IsAlive())
						{
							if (p->GetWeaponType() == SMG_WEAPON && p->IsToolWeapon())
							{
								Handle<IAudioChunk> chunk = audioDevice->RegisterSound("Sounds/Weapons/WeaponMode.wav");
								audioDevice->PlayLocal(chunk, AudioParam());

								if (MaxShots == -1 && down)
								{
									MaxShots = 1;
								}
								else if (MaxShots == 1 && down && (bool)weap_burstFire)
								{
									MaxShots = (int)weap_burstRounds;
								}
								else if (down)
								{
									MaxShots = -1;
								}
								//SPLog("MaxShots: toggle %i", MaxShots);
							}
							if (p->GetWeaponType() == RIFLE_WEAPON && p->IsToolWeapon())
							{
								scopeView = !scopeView;
							}
						}

						if (p->GetWeaponType() != SMG_WEAPON)
						{
							MaxShots = 1;
						}
					}
					else if(CheckKey(cg_keyAttack, name))
					{
						//Chameleon: semi auto fire
						//world->GetLocalPlayer()->GetWeaponType() != SMG_WEAPON //SPLog("CHECK 3--------------t");
						/*if (p->IsToolWeapon() && semiAuto != -1)
						{
							heldTriggerAfterLastShot = down;
						}*/
						if (!down)
						{
							world->GetListener()->SetShotsFired(0);
						}
						weapInput.primary = down;
					}
					else if(CheckKey(cg_keyAltAttack, name))
					{
						auto lastVal = weapInput.secondary;
						
						if(world->GetLocalPlayer()->IsToolWeapon() && (!cg_holdAimDownSight))
						{
							if(down && !playerInput.sprint && !world->GetLocalPlayer()->GetWeapon()->IsReloading())
							{
								weapInput.secondary = !weapInput.secondary;
							}
						}
						//grenade binocs
						else if (world->GetLocalPlayer()->GetTool() == Player::ToolGrenade)
						{
							if (down && !playerInput.sprint && (int)v_binocsZoom != -1)
							{
								if (!cg_holdAimDownSight)
									weapInput.secondary = !weapInput.secondary;
								else
									weapInput.secondary = down;
							}
							else if (!playerInput.sprint && (int)v_binocsZoom == -1)
							{
								weapInput.secondary = down;
							}
						}
						else
						{
							if(!playerInput.sprint)
							{
								weapInput.secondary = down;
							}
							else
							{
								weapInput.secondary = down; //changed from "down" to "false"
							}
						}
						if(world->GetLocalPlayer()->IsToolWeapon() && weapInput.secondary && !lastVal &&
						   world->GetLocalPlayer()->IsReadyToUseTool() &&
						   !world->GetLocalPlayer()->GetWeapon()->IsReloading()) 
						{
							AudioParam params;
							params.volume = 0.1f;
							//Chameleon: limits the distance of this sound LOW
							if (soundDistance == int(snd_maxDistance))
							{
								Handle<IAudioChunk> chunk = audioDevice->RegisterSound("Sounds/Weapons/Zoom.wav");
								audioDevice->PlayLocal(chunk, MakeVector3(.4f, -.3f, .5f), params);
							}							
						}

						//if (world->GetLocalPlayer()->IsToolWeapon()) //does not work at all
						//{
						//	//Unzoom when jumping
						//	if (world->GetLocalPlayer()->GetVelocity().z < -0.25f || world->GetLocalPlayer()->GetVelocity().z > 0.25f)
						//		weapInput.secondary = false;
						//}
						//if (world->GetLocalPlayer()->GetVelocity().z > 0.25f)
					}
					else if(CheckKey(cg_keyReloadWeapon, name) && down)
					{
						Weapon *w = world->GetLocalPlayer()->GetWeapon();
						if(w->GetAmmo() < w->GetClipSize() &&
						   w->GetStock() > 0 &&
						   (!world->GetLocalPlayer()->IsAwaitingReloadCompletion()) &&
						   (!w->IsReloading()) &&
						   world->GetLocalPlayer()->GetTool() == Player::ToolWeapon){
							if(world->GetLocalPlayer()->IsToolWeapon()){
                                if(weapInput.secondary) {
                                    // if we send WeaponInput after sending Reload,
                                    // server might cancel the reload.
                                    // https://github.com/infogulch/pyspades/blob/895879ed14ddee47bb278a77be86d62c7580f8b7/pyspades/server.py#343
                                    hasDelayedReload = true;
                                    weapInput.secondary = false;
                                    return;
                                }
							}
							world->GetLocalPlayer()->Reload();
							net->SendReload();
						}
					}
					else if(CheckKey(cg_keyToolSpade, name) && down)
					{
						if(world->GetLocalPlayer()->GetTeamId() < 2 &&
						   world->GetLocalPlayer()->IsAlive() &&
						   world->GetLocalPlayer()->IsToolSelectable(Player::ToolSpade))
						{
							SetSelectedTool(Player::ToolSpade);
							weapInput.secondary = false;
							weapInput.primary = false;
						}
					}
					else if(CheckKey(cg_keyToolBlock, name) && down)
					{
						if(world->GetLocalPlayer()->GetTeamId() < 2 &&
						   world->GetLocalPlayer()->IsAlive()
						   ){
							if(world->GetLocalPlayer()->IsToolSelectable(Player::ToolBlock)) 
							{
								SetSelectedTool(Player::ToolBlock);
								weapInput.secondary = false;
								weapInput.primary = false;
							}
							else
							{
								if(cg_alerts)
									ShowAlert(_Tr("Client", "No blocks"), AlertType::Error);
								else
									PlayAlertSound();
							}
						}
					}else if(CheckKey(cg_keyToolWeapon, name) && down){
						if(world->GetLocalPlayer()->GetTeamId() < 2 &&
						   world->GetLocalPlayer()->IsAlive()){
							if(world->GetLocalPlayer()->IsToolSelectable(Player::ToolWeapon)) 
							{
								SetSelectedTool(Player::ToolWeapon);
								weapInput.secondary = false;
								weapInput.primary = false;
							}
							else
							{
								if(cg_alerts)
									ShowAlert(_Tr("Client", "No ammo"), AlertType::Error);
								else
									PlayAlertSound();
							}
						}
					}else if(CheckKey(cg_keyToolGrenade, name) && down){
						if(world->GetLocalPlayer()->GetTeamId() < 2 &&
						   world->GetLocalPlayer()->IsAlive()){
							if(world->GetLocalPlayer()->IsToolSelectable(Player::ToolGrenade))
							{
								SetSelectedTool(Player::ToolGrenade);
								weapInput.secondary = false;
								weapInput.primary = false;
							}
							else
							{
								if(cg_alerts)
									ShowAlert(_Tr("Client", "No nades"), AlertType::Error);
								else
									PlayAlertSound();
							}
						}
					}else if(CheckKey(cg_keyLastTool, name) && down)
					{
						if(hasLastTool &&
						   world->GetLocalPlayer()->GetTeamId() < 2 &&
						   world->GetLocalPlayer()->IsAlive() &&
						   world->GetLocalPlayer()->IsToolSelectable(lastTool))
						{
							hasLastTool = false;
							SetSelectedTool(lastTool);
							weapInput.secondary = false;
							weapInput.primary = false;
						}
					}else if(CheckKey(cg_keyGlobalChat, name) && down){
						// global chat
						scriptedUI->EnterGlobalChatWindow();
						scriptedUI->setIgnored(name);
					}else if(CheckKey(cg_keyTeamChat, name) && down){
						// team chat
						scriptedUI->EnterTeamChatWindow();
						scriptedUI->setIgnored(name);
					}else if(name == "/" && down){
						// command
						scriptedUI->EnterCommandWindow();
						scriptedUI->setIgnored(name);
					}else if(CheckKey(cg_keyCaptureColor, name) && down){
						CaptureColor();
					}
					else if(CheckKey(cg_keyChangeMapScale, name) && down)
					{
						mapView->SwitchScale();
						//Chameleon: limits the distance of this sound HIGH
						if (soundDistance > int(snd_maxDistance) / 2.f)
						{
							Handle<IAudioChunk> chunk = audioDevice->RegisterSound("Sounds/Misc/SwitchMapZoom.wav");
							audioDevice->PlayLocal(chunk, AudioParam());
						}
					}
					else if(CheckKey(cg_keyToggleMapZoom, name) && down)
					{
						if(largeMapView->ToggleZoom())
						{
							//Chameleon: limits the distance of this sound HIGH
							if (soundDistance > int(snd_maxDistance) / 2.f)
							{
								Handle<IAudioChunk> chunk = audioDevice->RegisterSound("Sounds/Misc/OpenMap.wav");
								audioDevice->PlayLocal(chunk, AudioParam());
							}
						}
						else
						{
							//Chameleon: limits the distance of this sound HIGH
							if (soundDistance > int(snd_maxDistance) / 2.f)
							{
								Handle<IAudioChunk> chunk = audioDevice->RegisterSound("Sounds/Misc/CloseMap.wav");
								audioDevice->PlayLocal(chunk, AudioParam());
							}
						}
					}
					else if(CheckKey(cg_keyScoreboard, name))
					{
						scoreboardVisible = down;
					}
					else if(CheckKey(cg_keyLimbo, name) && down)
					{
						limbo->SetSelectedTeam(world->GetLocalPlayer()->GetTeamId());
						limbo->SetSelectedWeapon(world->GetLocalPlayer()->GetWeapon()->GetWeaponType());
						inGameLimbo = true;
					}
					else if(CheckKey(cg_keySceneshot, name) && down)
					{
						TakeScreenShot(true);
					}
					else if(CheckKey(cg_keyScreenshot, name) && down)
					{
						TakeScreenShot(false);
					}
					else if(CheckKey(cg_keySaveMap, name) && down)
					{
						TakeMapShot();
					}
					else if(CheckKey(cg_keyFlashlight, name) && down)
					{
						flashlightOn = !flashlightOn;
						flashlightOnTime = time;
						//Chameleon: limits the distance of this sound HIGH
						if (soundDistance > int(snd_maxDistance) / 2.f)
						{
							Handle<IAudioChunk> chunk = audioDevice->RegisterSound("Sounds/Player/Flashlight.wav");
							audioDevice->PlayLocal(chunk, AudioParam());
						}
					}
					else if(CheckKey(cg_keyAutoFocus, name) && down &&
							 (int)cg_manualFocus){
						autoFocusEnabled = true;
					}
					else if(down) 
					{
						bool rev = (int)cg_switchToolByWheel > 0;
						if(name == (rev ? "WheelDown":"WheelUp")) {
							if ((int)cg_manualFocus)
							{
								// When DoF control is enabled,
								// tool switch is overrided by focal length control.
								float dist = 1.f / targetFocalLength;
								dist = std::min(dist + 0.01f, 1.f);
								targetFocalLength = 1.f / dist;
								autoFocusEnabled = false;
							} 
							else if(cg_switchToolByWheel &&
									  world->GetLocalPlayer()->GetTeamId() < 2 &&
							   world->GetLocalPlayer()->IsAlive())
							{
								Player::ToolType t = world->GetLocalPlayer()->GetTool();
								do{
									switch(t){
										case Player::ToolSpade:
											t = Player::ToolGrenade;
											break;
										case Player::ToolBlock:
											t = Player::ToolSpade;
											break;
										case Player::ToolWeapon:
											t = Player::ToolBlock;
											break;
										case Player::ToolGrenade:
											t = Player::ToolWeapon;
											break;
									}
								}
								while(!world->GetLocalPlayer()->IsToolSelectable(t));
								SetSelectedTool(t);
								weapInput.secondary = false;
								weapInput.primary = false;
							}
						}
						else if(name == (rev ? "WheelUp":"WheelDown")) 
						{
							if ((int)cg_manualFocus) {
								// When DoF control is enabled,
								// tool switch is overrided by focal length control.
								float dist = 1.f / targetFocalLength;
								dist = std::max(dist - 0.01f, 1.f / 128.f); // limit to fog max distance
								targetFocalLength = 1.f / dist;
								autoFocusEnabled = false;
							} 
							else if(cg_switchToolByWheel &&
									  world->GetLocalPlayer()->GetTeamId() < 2 &&
							   world->GetLocalPlayer()->IsAlive())
							{
								Player::ToolType t = world->GetLocalPlayer()->GetTool();
								do
								{
									switch(t)
									{
										case Player::ToolSpade:
											t = Player::ToolBlock;
											break;
										case Player::ToolBlock:
											t = Player::ToolWeapon;
											break;
										case Player::ToolWeapon:
											t = Player::ToolGrenade;
											break;
										case Player::ToolGrenade:
											t = Player::ToolSpade;
											break;
									}
								}
								while(!world->GetLocalPlayer()->IsToolSelectable(t));
								SetSelectedTool(t);
								weapInput.secondary = false;
								weapInput.primary = false;
							}
						}
					}
				}
				else
				{
					// limbo
				}
			}
		}
		

		
	}
}
