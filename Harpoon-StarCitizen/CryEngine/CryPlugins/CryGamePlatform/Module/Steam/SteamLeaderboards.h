// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IPlatformLeaderboards.h"
#include "SteamTypes.h"
#include <steam/isteamuserstats.h>

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			class CLeaderboards final : public ILeaderboards
			{
			public:
				struct SQueuedEntryRequest
				{
					ELeaderboardDataRequest request;
					int minRange;
					int maxRange;
				};

				struct SQueuedUpdateRequest
				{
					ELeaderboardUploadScoreMethod method;
					int score;
					EScoreType scoreType;
					string name;
				};

				explicit CLeaderboards(CService& steamService);
				virtual ~CLeaderboards() = default;

				// ILeaderboards
				virtual void AddListener(IListener& listener) override { m_listeners.push_back(&listener); }
				virtual void RemoveListener(IListener& listener) override { stl::find_and_erase(m_listeners, &listener); }

				virtual void DownloadEntries(Identifier leaderboardId, ERequest request = ERequest::Global, int minRange = 1, int maxRange = 10, bool bTimeBased = false) override;
				virtual void UpdateScore(Identifier leaderboardId, int score, EScoreType scoreType, bool bForce = false) override;
				// ~ILeaderboards

			private:
				void FindOrCreateLeaderboard(const char* name, ELeaderboardSortMethod sortMethod = k_ELeaderboardSortMethodDescending, ELeaderboardDisplayType displayType = k_ELeaderboardDisplayTypeNumeric);
				void FindNextLeaderboardToUpdateScore();

				void OnFindLeaderboard(LeaderboardFindResult_t* pResult, bool bIOFailure);
				CCallResult<CLeaderboards, LeaderboardFindResult_t> m_callResultFindLeaderboard;

				void OnEntriesDownloaded(LeaderboardScoresDownloaded_t* pResult, bool bIOFailure);
				CCallResult<CLeaderboards, LeaderboardScoresDownloaded_t> m_callResultEntriesDownloaded;

			protected:
				CService& m_service;

				std::unique_ptr<SQueuedEntryRequest> m_pQueuedEntryRequest;
				std::vector<SQueuedUpdateRequest> m_queuedUpdateRequests;

				std::vector<IListener*> m_listeners;
			};
		}
	}
}