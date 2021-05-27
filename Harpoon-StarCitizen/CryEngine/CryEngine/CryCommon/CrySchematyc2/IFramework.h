// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryGame/IGame.h>
#include <CryGame/IGameFramework.h>
#include <CryExtension/ClassWeaver.h>
#include <CrySerialization/Forward.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_Delegate.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_Signalv2.h>
#include <CrySchematyc2/IBaseEnv.h>
#include <CrySchematyc2/ILibRegistry.h>

namespace Schematyc2 {

struct ICompiler;
struct IDomainContext;
struct IEnvRegistry;
struct ILibRegistry;
struct ILog;
struct ILogRecorder;
struct IObjectManager;
struct IScriptRegistry;
struct ISerializationContext;
struct IStringPool;
struct ITimerSystem;
struct IUpdateScheduler;
struct IValidatorArchive;
struct IResourceCollectorArchive;
struct IGameResourceList;

struct SDomainContextScope;
struct SGUID;
struct SSerializationContextParams;
struct SValidatorArchiveParams;

class CUpdateRelevanceContext;

DECLARE_SHARED_POINTERS(IDomainContext)
DECLARE_SHARED_POINTERS(ISerializationContext)
DECLARE_SHARED_POINTERS(IValidatorArchive)
DECLARE_SHARED_POINTERS(IResourceCollectorArchive)
DECLARE_SHARED_POINTERS(IGameResourceList)

typedef TemplateUtils::CDelegate<SGUID()> GUIDGenerator;
typedef TemplateUtils::CSignalv2<void ()> EnvRefreshSignal;

struct SFrameworkSignals
{
	EnvRefreshSignal envRefresh;
};

struct IFramework : public Cry::IDefaultModule
{
	CRYINTERFACE_DECLARE_GUID(IFramework, "{C2D28CFF-542F-448E-9499-653C4077F28E}"_cry_guid);

	// TODO : Clean up this interface!
	virtual void                         SetGUIDGenerator(const GUIDGenerator& guidGenerator) = 0; // Deprecated
	virtual SGUID                        CreateGUID() const = 0;                                   // Deprecated

	virtual IStringPool&                 GetStringPool() = 0;  // Deprecated
	virtual IEnvRegistry&                GetEnvRegistry() = 0; // Deprecated
	virtual SchematycBaseEnv::IBaseEnv&  GetBaseEnv() = 0;     // Deprecated
	virtual ILibRegistry&                GetLibRegistry() = 0; // Deprecated

	virtual const char*                  GetFileFormat() const = 0;         // Deprecated
	virtual const char*                  GetRootFolder() const = 0;         // Deprecated
	virtual const char*                  GetOldScriptsFolder() const = 0;   // Deprecated
	virtual const char*                  GetOldScriptExtension() const = 0; // Deprecated

	virtual const char*                  GetScriptsFolder() const = 0;     // Deprecated
	virtual const char*                  GetSettingsFolder() const = 0;    // Deprecated
	virtual const char*                  GetSettingsExtension() const = 0; // Deprecated

	virtual bool                         IsExperimentalFeatureEnabled(const char* szFeatureName) const = 0;

	virtual IScriptRegistry&             GetScriptRegistry() = 0;  // Deprecated
	virtual ICompiler&                   GetCompiler() = 0;        // Deprecated
	virtual IObjectManager&              GetObjectManager() = 0;   // Deprecated
	virtual ILog&                        GetLog() = 0;             // Deprecated
	virtual ILogRecorder&                GetLogRecorder() = 0;     // Deprecated
	virtual IUpdateScheduler&            GetUpdateScheduler() = 0; // Deprecated
	virtual ITimerSystem&                GetTimerSystem() = 0;     // Deprecated

	virtual ISerializationContextPtr     CreateSerializationContext(const SSerializationContextParams& params) const = 0; // Deprecated
	virtual IDomainContextPtr            CreateDomainContext(const SDomainContextScope& scope) const = 0;                 // Deprecated
	virtual IValidatorArchivePtr         CreateValidatorArchive(const SValidatorArchiveParams& params) const = 0;         // Deprecated

	virtual IGameResourceListPtr         CreateGameResoucreList() const = 0;                                           // Deprecated
	virtual IResourceCollectorArchivePtr CreateResourceCollectorArchive(IGameResourceListPtr pResourceList) const = 0; // Deprecated

	virtual void                         RefreshLogFileSettings() = 0; // Deprecated
	virtual void                         RefreshEnv() = 0;             // Deprecated

	virtual SFrameworkSignals& Signals() = 0; // Deprecated

	virtual void               PrePhysicsUpdate() = 0;
	virtual void               Update() = 0;
	virtual void               SetUpdateRelevancyContext(CUpdateRelevanceContext& context) = 0;
};

} // ~Schematyc2 namespace
