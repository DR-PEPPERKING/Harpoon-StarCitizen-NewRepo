// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IPlatformUserGeneratedContent.h"
#include "SteamTypes.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			class CUserGeneratedContent
				: public IUserGeneratedContent
			{
			public:
				CUserGeneratedContent(CService& steamService, AppId_t appId, IUserGeneratedContent::Identifier id);
				virtual ~CUserGeneratedContent() = default;

				// IUserGeneratedContent
				virtual IUserGeneratedContent::Identifier GetIdentifier() override { return m_id; }

				virtual bool SetTitle(const char* newTitle) override;
				virtual bool SetDescription(const char* newDescription) override;
				virtual bool SetVisibility(IUserGeneratedContent::EVisibility visibility) override;
				virtual bool SetTags(const char* *pTags, int numTags) override;
				virtual bool SetContent(const char* contentFolderPath) override;
				virtual bool SetPreview(const char* previewFilePath) override;

				virtual void CommitChanges(const char* changeNote) override;
				// ~IUserGeneratedContent

				void StartPropertyUpdate();

			protected:
				CService& m_service;
				AppId_t m_appId = k_uAppIdInvalid;
				IUserGeneratedContent::Identifier m_id;

				UGCUpdateHandle_t m_updateHandle;

				void OnContentUpdated(SubmitItemUpdateResult_t* pResult, bool bIOFailure);
				CCallResult<CUserGeneratedContent, SubmitItemUpdateResult_t> m_callResultContentUpdated;
			};
		}
	}
}