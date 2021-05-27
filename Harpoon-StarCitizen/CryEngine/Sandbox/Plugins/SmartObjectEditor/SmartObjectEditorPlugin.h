// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "IPlugin.h"

class CSmartObjectEditorPlugin : public IPlugin, public IAutoEditorNotifyListener
{
public:
	CSmartObjectEditorPlugin();
	~CSmartObjectEditorPlugin();

	int32        GetPluginVersion() { return 1; }
	const char*  GetPluginName() { return "Sample Plugin"; }
	const char*  GetPluginDescription() { return "Plugin used as a code sample to demonstrate Sandbox's plugin system"; }

	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event) override;

private:

	bool OnEditDeprecatedProperty(int type, const string& oldValue, string& newValue) const;
	void ValidateSmartObjects() const;
};
