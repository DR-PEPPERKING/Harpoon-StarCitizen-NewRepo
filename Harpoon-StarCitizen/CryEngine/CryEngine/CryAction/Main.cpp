// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryAction.h"

class CEngineModule_CryAction : public IGameFrameworkEngineModule
{
	CRYINTERFACE_BEGIN()
		CRYINTERFACE_ADD(Cry::IDefaultModule)
		CRYINTERFACE_ADD(IGameFrameworkEngineModule)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CEngineModule_CryAction, "CEngineModule_CryAction", "943C708C-F469-48AA-B573-D6A0FCE7B9CD"_cry_guid)

	virtual ~CEngineModule_CryAction()
	{
		m_pCryAction.reset();
	}

	//////////////////////////////////////////////////////////////////////////
	virtual const char* GetName() const override { return "CryAction"; }
	virtual const char* GetCategory() const override { return "CryEngine"; }

	//////////////////////////////////////////////////////////////////////////
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override
	{
		// Special case: cast away the const to maintain legacy compatibility for the game framework
		m_pCryAction.reset(new CCryAction(const_cast<SSystemInitParams&>(initParams)));
		return true;
	}

private:
	std::unique_ptr<CCryAction> m_pCryAction;
};

CRYREGISTER_SINGLETON_CLASS(CEngineModule_CryAction)