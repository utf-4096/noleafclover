/*
 Copyright (c) 2013 yvt
 
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

#include <Core/Settings.h>
#include <Core/Strings.h>

#include "GunCasing.h"
#include "IRenderer.h"
#include "GameMap.h"
#include "Client.h"
#include "World.h"
#include "IAudioDevice.h"
#include <stdlib.h>
#include "ParticleSpriteEntity.h"
#include "IAudioChunk.h"

//Bullet on ground time limit
SPADES_SETTING(opt_brassTime, "5.0");
//Maximum distance for detailed particles
SPADES_SETTING(opt_particleNiceDist, "");

namespace spades {
	namespace client{
		GunCasing::GunCasing(Client *client,
							 IModel *model,
							 IAudioChunk *dropSound,
							 IAudioChunk *waterSound,
				  Vector3 pos,
				  Vector3 dir,
							 Vector3 flyDir):
		client(client),
		renderer(client->GetRenderer()), model(model),
		dropSound(dropSound), waterSound(waterSound){
			
			if(dropSound)
				dropSound->AddRef();
			if(waterSound)
				waterSound->AddRef();
			
			Vector3 up = MakeVector3(0, 0, 1);
			Vector3 right = Vector3::Cross(dir, up).Normalize();
			up = Vector3::Cross(right, dir);
			
			matrix = Matrix4::FromAxis(right, dir, up, pos);
			
			up = MakeVector3(0, 0, -1);
			rotAxis=Vector3::Cross(up, flyDir.Normalize());
			
			groundTime = 0.f;
			onGround = false;
			vel = flyDir * 10.f;
			rotSpeed = 40.f;
		}
		GunCasing::~GunCasing(){
			if(dropSound)
				dropSound->Release();
			if(waterSound)
				waterSound->Release();
		}
		static Vector3 RandomAxis(){
			Vector3 v;
			v.x = GetRandom() - GetRandom();
			v.y = GetRandom() - GetRandom();
			v.z = GetRandom() - GetRandom();
			return v.Normalize();
		}
		bool GunCasing::Update(float dt) 
		{
			if(onGround)
			{
				groundTime += dt;
				if (groundTime > float(opt_brassTime))
				{
					return false;
				}
				
				GameMap *map = client->GetWorld()->GetMap();
				if(!map->ClipWorld(groundPos.x, groundPos.y, groundPos.z)){
					return false;
				}
			}
			else
			{
				Matrix4 lastMat = matrix;
				
				matrix = matrix * Matrix4::Rotate(rotAxis, dt * rotSpeed);
				matrix = Matrix4::Translate(vel * dt) * matrix;
				vel.z += dt * 32.f;
				
				IntVector3 lp = matrix.GetOrigin().Floor();
				GameMap *m = client->GetWorld()->GetMap();
				
				if(lp.z >= 63)
				{
					// dropped into water
					float distance = (client->GetLastSceneDef().viewOrigin - matrix.GetOrigin()).GetLength();
					
					if(waterSound)
					{
						if (!client->IsMuted() && distance*4 < client->soundDistance)
						{
							IAudioDevice *dev = client->GetAudioDevice();
							AudioParam param;
							param.referenceDistance = .6f;
							param.pitch = .9f + GetRandom() * .2f;
							
							dev->Play(waterSound, lastMat.GetOrigin(),
									  param);
						}
						waterSound = NULL;
					}
					
					if (distance < 16.f * float(opt_particleNiceDist))
					{
						//int splats = rand() % 3;
						
						Handle<IImage> img = client->GetRenderer()->RegisterImage("Gfx/WhiteDisk.tga");
						
						Vector4 col = {0.7f, 0.7f, 0.5f, 0.5f};
						Vector3 pt = matrix.GetOrigin();
						pt.z = 62.99f;
						//for(int i = 0; i < splats; i++){
						ParticleSpriteEntity *ent =
						new ParticleSpriteEntity(client, img, col);
						ent->SetTrajectory(pt,
											MakeVector3(GetRandom()-GetRandom(),
														GetRandom()-GetRandom(),
														-GetRandom()) * 2.f,
											1.f, .4f);
						ent->SetRotation(GetRandom() * (float)M_PI * 2.f);
						ent->SetRadius(0.1f + GetRandom()*GetRandom()*0.1f);
						ent->SetLifeTime(1.f, 0.f, 0.f);
						client->AddLocalEntity(ent);
						//}
							
					}
					
					return false;
				}
				
				if(m->ClipWorld(lp.x,lp.y,lp.z)){
					// hit a wall
					
					IntVector3 lp2 = lastMat.GetOrigin().Floor();
					if (lp.z != lp2.z && ((lp.x == lp2.x && lp.y == lp2.y) ||
										  !m->ClipWorld(lp.x, lp.y, lp2.z))){
						vel.z = -vel.z;
						if(lp2.z < lp.z)
						{
							// ground hit
							if(vel.GetLength() < 0.5f + dt * 100.f && !dropSound)
							{
								// stick to ground
								onGround = true;
								groundPos = lp;
								
								// move to surface
								float z = matrix.GetOrigin().z;
								float shift = z - floorf(z);
								matrix = Matrix4::Translate(0,0,-shift) * matrix;
								
								// lie
								Vector3 v1 = matrix.GetAxis(0);
								Vector3 v2 = matrix.GetAxis(1);
								v1.z = 0; v2.z = 0;
								v1 = v1.Normalize();
								v2 = v2.Normalize();
								
								Vector3 v3 = Vector3::Cross(v1, v2).Normalize();
								
								v1 = Vector3::Cross(v2, v3).Normalize();
								
								matrix = Matrix4::FromAxis(v1, v2, v3, matrix.GetOrigin());
							}
							else
							{
								if(dropSound)
								{
									float distance = (client->GetLastSceneDef().viewOrigin - matrix.GetOrigin()).GetLength();
									if (!client->IsMuted() && distance*4 < client->soundDistance)
									{
										IAudioDevice *dev = client->GetAudioDevice();
										AudioParam param;
										param.referenceDistance = .6f;
									
										dev->Play(dropSound, lastMat.GetOrigin(),
												  param);
									}
									dropSound = NULL;
								}
							}
						}
					}else if(lp.x != lp2.x && ((lp.y == lp2.y && lp.z == lp2.z) ||
											  !m->ClipWorld(lp2.x, lp.y, lp.z)))
						vel.x = -vel.x;
					else if(lp.y != lp2.y && ((lp.x == lp2.x && lp.z == lp2.z) ||
											  !m->ClipWorld(lp.x, lp2.y, lp.z)))
						vel.y = -vel.y;
					else
						return false;
					if(!onGround){
						matrix = lastMat;
						vel *= .2f;
						rotAxis = RandomAxis();
						
						Vector3 r;
						r.x = GetRandom() - GetRandom();
						r.y = GetRandom() - GetRandom();
						r.z = GetRandom() - GetRandom();
						
						vel += r * 0.1f;
						
						rotSpeed *= .2f;
					}
				}
			}
			return true;
		}
		
		void GunCasing::Render3D() 
		{
			ModelRenderParam param;
			param.matrix = matrix * Matrix4::Scale(.015f);
			if (groundTime > float(opt_brassTime))
			{
				// sink
				float move = (groundTime - float(opt_brassTime) + 1.f) * 0.2f;
				param.matrix = Matrix4::Translate(0,0,move) * param.matrix;
			}
			renderer->RenderModel(model, param);
		}
	}
}
