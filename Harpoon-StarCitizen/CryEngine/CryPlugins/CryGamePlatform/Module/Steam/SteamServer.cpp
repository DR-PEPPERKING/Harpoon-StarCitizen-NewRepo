// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "SteamServer.h"
#include "SteamUserIdentifier.h"

#include <CrySystem/IProjectManager.h>
#include <CrySystem/CryVersion.h>

#include <steam/steam_gameserver.h>

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			CServer::CServer(bool bLocal)
				: m_bLocal(bLocal)
			{
				ICVar* pPortVar = gEnv->pConsole->GetCVar("cl_serverport");

				int port = pPortVar->GetIVal();

				CryFixedStringT<32> gameVersionString = gEnv->pSystem->GetProductVersion().ToString();

				if (!SteamGameServer_Init(0, 8766, port, 27016, eServerModeAuthentication, gameVersionString.c_str()))
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "SteamGameServer_Init call failed!");
					return;
				}

				if (ISteamGameServer* pGameServer = SteamGameServer())
				{
					pGameServer->SetKeyValue("version", gameVersionString.c_str());

					char str[_MAX_PATH];
					CryGetCurrentDirectory(_MAX_PATH, str);

					pGameServer->SetProduct(gEnv->pSystem->GetIProjectManager()->GetCurrentProjectName());
					pGameServer->SetGameDescription(gEnv->pSystem->GetIProjectManager()->GetCurrentProjectName());

					pGameServer->SetDedicatedServer(gEnv->IsDedicated());

					pGameServer->LogOnAnonymous();

					pGameServer->EnableHeartbeats(true);
				}
			}

			CServer::~CServer()
			{
				if (ISteamGameServer* pGameServer = SteamGameServer())
				{
					pGameServer->EnableHeartbeats(false);

					// Disconnect from the steam servers
					pGameServer->LogOff();
				}

				// release our reference to the steam client library
				SteamGameServer_Shutdown();
			}

			IServer::Identifier CServer::GetIdentifier() const
			{
				if (ISteamGameServer* pGameServer = SteamGameServer())
					return pGameServer->GetSteamID().ConvertToUint64();

				return 0;
			}

			uint32 CServer::GetPublicIP() const
			{
				if (ISteamGameServer* pGameServer = SteamGameServer())
					return pGameServer->GetPublicIP();

				return 0;
			}

			const char* CServer::GetPublicIPString() const
			{
				uint32 publicIP = GetPublicIP();

				const int NBYTES = 4;
				uint8 octet[NBYTES];
				char* ipAddressFinal = new char[15];
				for (int i = 0; i < NBYTES; i++)
				{
					octet[i] = publicIP >> (i*  8);
				}
				cry_sprintf(ipAddressFinal, NBYTES, "%d.%d.%d.%d", octet[3], octet[2], octet[1], octet[0]);

				string sIP = string(ipAddressFinal);
				delete[] ipAddressFinal;

				return sIP;
			}

			uint16 CServer::GetPort() const
			{
				ICVar* pPortVar = gEnv->pConsole->GetCVar("cl_serverport");

				return pPortVar->GetIVal();
			}

			bool CServer::AuthenticateUser(uint32 clientIP, char* authData, int authDataLength, AccountIdentifier &userId)
			{
				if (ISteamGameServer* pGameServer = SteamGameServer())
				{
					CSteamID steamUserId;
					if (pGameServer->SendUserConnectAndAuthenticate(clientIP, authData, authDataLength, &steamUserId))
					{
						userId = CreateAccountIdentifier(steamUserId);
						return true;
					}
					else
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Steam] Steam authentication failure!");
				}
				else
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Steam] No game server found, returning authentication failure!");

				return false;
			}

			void CServer::SendUserDisconnect(const AccountIdentifier& userId)
			{
				if (ISteamGameServer* pGameServer = SteamGameServer())
				{
					pGameServer->SendUserDisconnect(ExtractSteamID(userId));
				}
			}
		}
	}
}