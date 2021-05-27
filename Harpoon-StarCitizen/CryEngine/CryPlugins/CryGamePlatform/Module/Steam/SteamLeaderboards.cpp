// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "SteamLeaderboards.h"
#include "SteamService.h"
#include "SteamUserIdentifier.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			CLeaderboards::CLeaderboards(CService& steamService)
				: m_service(steamService)
			{

			}

			void CLeaderboards::FindOrCreateLeaderboard(const char* name, ELeaderboardSortMethod sortMethod, ELeaderboardDisplayType displayType)
			{
				ISteamUserStats* pSteamUserStats = SteamUserStats();
				if (!pSteamUserStats)
				{
					return;
				}
				SteamAPICall_t hSteamAPICall = pSteamUserStats->FindOrCreateLeaderboard(name, sortMethod, displayType);
				m_callResultFindLeaderboard.Set(hSteamAPICall, this, &CLeaderboards::OnFindLeaderboard);
			}

			void CLeaderboards::OnFindLeaderboard(LeaderboardFindResult_t* pResult, bool bIOFailure)
			{
				ISteamUserStats* pSteamUserStats = SteamUserStats();

				if (!pResult->m_bLeaderboardFound || bIOFailure || !pSteamUserStats)
				{
					return;
				}

				if (m_pQueuedEntryRequest != nullptr)
				{
					SteamAPICall_t hSteamAPICall = pSteamUserStats->DownloadLeaderboardEntries(pResult->m_hSteamLeaderboard, m_pQueuedEntryRequest->request, m_pQueuedEntryRequest->minRange, m_pQueuedEntryRequest->maxRange);
					m_callResultEntriesDownloaded.Set(hSteamAPICall, this, &CLeaderboards::OnEntriesDownloaded);

					m_pQueuedEntryRequest.reset();
				}

				if (!m_queuedUpdateRequests.empty())
				{
					const SQueuedUpdateRequest& request = m_queuedUpdateRequests[0];
					int scoreDetails[1] = { static_cast<int>(request.scoreType) };
					pSteamUserStats->UploadLeaderboardScore(pResult->m_hSteamLeaderboard, request.method, request.score, scoreDetails, 1);

					m_queuedUpdateRequests.erase( m_queuedUpdateRequests.begin() );
					FindNextLeaderboardToUpdateScore();
				}
			}

			void CLeaderboards::DownloadEntries(Identifier leaderboardId, ERequest request, int minRange, int maxRange, bool bTimeBased)
			{
				if (SteamUserStats() == nullptr)
					return;

				m_pQueuedEntryRequest = stl::make_unique<SQueuedEntryRequest>();
				m_pQueuedEntryRequest->request = (ELeaderboardDataRequest)request;
				m_pQueuedEntryRequest->minRange = minRange;
				m_pQueuedEntryRequest->maxRange = maxRange;

				// If score is time based then sort leaderboard ascending and display in milliseconds, otherwise descending and numeric.
				ELeaderboardSortMethod sortMethod = bTimeBased ? k_ELeaderboardSortMethodAscending : k_ELeaderboardSortMethodDescending;
				ELeaderboardDisplayType displayType = bTimeBased ? k_ELeaderboardDisplayTypeTimeMilliSeconds : k_ELeaderboardDisplayTypeNumeric;

				FindOrCreateLeaderboard(leaderboardId.szName, sortMethod, displayType);
			}

			void CLeaderboards::OnEntriesDownloaded(LeaderboardScoresDownloaded_t* pResult, bool bIOFailure)
			{
				LeaderboardEntry_t leaderboardEntry;
				int32 details[3];

				CAccount* pAccount = m_service.GetLocalAccount();

				ISteamUserStats* pSteamUserStats = SteamUserStats();
				if (!pSteamUserStats)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Steam user stats service not available");
					return;
				}
				ISteamFriends* pSteamFriends = SteamFriends();
				if (!pSteamFriends)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Steam friends service not available");
					return;
				}

				for (int i = 0; i < pResult->m_cEntryCount; i++)
				{
					if (pSteamUserStats->GetDownloadedLeaderboardEntry(pResult->m_hSteamLeaderboardEntries, i, &leaderboardEntry, details, 3))
					{
						for (IListener* pListener : m_listeners)
						{
							Identifier leaderboardIdentifier;
							leaderboardIdentifier.szName = pSteamUserStats->GetLeaderboardName(pResult->m_hSteamLeaderboard);

							const CSteamID userID = ExtractSteamID(pAccount->GetIdentifier());
							const bool isCurrentUser = leaderboardEntry.m_steamIDUser == userID;
							pListener->OnEntryDownloaded(leaderboardIdentifier, pSteamFriends->GetFriendPersonaName(leaderboardEntry.m_steamIDUser), leaderboardEntry.m_nGlobalRank, leaderboardEntry.m_nScore, (EScoreType)details[0], isCurrentUser);
						}
					}
				}
			}

			void CLeaderboards::UpdateScore(Identifier leaderboardId, int score, EScoreType scoreType, bool bForce)
			{
				if (SteamUserStats() == nullptr)
				{
					return;
				}

				SQueuedUpdateRequest request;
				request.method = bForce ? k_ELeaderboardUploadScoreMethodForceUpdate : k_ELeaderboardUploadScoreMethodKeepBest;
				request.score = score;
				request.scoreType = scoreType;
				request.name = leaderboardId.szName;
				m_queuedUpdateRequests.push_back( request );
				if (m_queuedUpdateRequests.size()==1)
					FindNextLeaderboardToUpdateScore();
			}


			void CLeaderboards::FindNextLeaderboardToUpdateScore()
			{
				if (!m_queuedUpdateRequests.empty())
				{
					const SQueuedUpdateRequest& request = m_queuedUpdateRequests[0];

					// If score is time based then sort leaderboard ascending and display in milliseconds, otherwise descending and numeric.
					ELeaderboardSortMethod sortMethod = (request.scoreType == EScoreType::Time) ? k_ELeaderboardSortMethodAscending : k_ELeaderboardSortMethodDescending;
					ELeaderboardDisplayType displayType = (request.scoreType == EScoreType::Time) ? k_ELeaderboardDisplayTypeTimeMilliSeconds : k_ELeaderboardDisplayTypeNumeric;

					FindOrCreateLeaderboard(request.name.c_str(), sortMethod, displayType);
				}
			}
		}
	}
}