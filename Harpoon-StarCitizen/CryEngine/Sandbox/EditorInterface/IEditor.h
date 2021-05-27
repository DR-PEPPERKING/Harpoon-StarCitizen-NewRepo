// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// disable virtual function override warnings, as MFC headers do not pass them.
#pragma warning(disable: 4264)
#pragma warning(disable: 4266)

#include <CryCore/functor.h>
#include <CryString/CryString.h>
#include <CryMath/Cry_Math.h>
#include <functional>

class CAssetManager;
class CBaseObject;
class CBroadcastManager;
class CConfigurationManager;
class CCryEditDoc;
class CFlowGraphManager;
class CHeightmap;
class CGameEngine;
class CLevelEditorSharedState;
class CMaterialManager;
class CPersonalizationManager;
class CPopupMenuItem;
class CPrefabManager;
class CPreferences;
class CRuler;
class CToolBarService;
class CTrayArea;
class CUIEnumsDatabase;
class CViewManager;
class CViewport;
class CWaitProgress;
class CWnd;
class QString;
class QWidget;

struct AABB;
struct I3DEngine;
struct IAIManager;
struct IBackgroundTaskManager;
struct ICommandManager;
struct IDataBaseItem;
struct IDataBaseManager;
struct IDisplayViewport;
struct IEditorClassFactory;
struct IEditorMaterial;
struct IEditorNotifyListener;
struct IExportManager;
struct IFileChangeMonitor;
struct IGizmoManager;
struct IIconManager;
struct ILevelEditor;
struct IMaterial;
struct IMovieSystem;
struct INotificationCenter;
struct IObjectManager;
struct IPane;
struct IPythonManager;
struct IRenderer;
struct IRenderNode;
struct IResourceSelectorHost;
struct ISelectionGroup;
struct ISourceControl;
struct ISystem;
struct IUndoManager;
struct IUriEventListener;
struct IViewportManager;
struct SObjectChangedContext;
struct SRayHitInfo;

enum ESystemConfigSpec;

namespace FileSystem
{
class CEnumerator;
}

namespace Cry
{
struct IProjectManager;
}

enum EReloadScriptsType
{
	eReloadScriptsType_None   = 0,
	eReloadScriptsType_Entity = BIT(0),
	eReloadScriptsType_Actor  = BIT(1),
	eReloadScriptsType_Item   = BIT(2),
	eReloadScriptsType_AI     = BIT(3),
	eReloadScriptsType_UI     = BIT(4),
};

// Global editor notify events.
enum EEditorNotifyEvent
{
	// Global events.
	eNotify_OnInit = 10,               // Sent after editor fully initialized.
	eNotify_OnQuit,                    // Sent before editor quits.
	eNotify_OnIdleUpdate,              // Sent every frame while editor is idle.

	// Document events.
	eNotify_OnBeginNewScene,           // Sent when the document is begin to be cleared.
	eNotify_OnEndNewScene,             // Sent after the document have been cleared.
	eNotify_OnBeginSceneOpen,          // Sent when document is about to be opened.
	eNotify_OnEndSceneOpen,            // Sent after document have been opened.
	eNotify_OnBeginSceneClose,         // Sent when document is about to be closed.
	eNotify_OnEndSceneClose,           // Sent after document have been closed.
	eNotify_OnBeginSceneSave,          // Sent when document is about to be saved.
	eNotify_OnEndSceneSave,            // Sent after document have been saved.
	eNotify_OnBeginLayerExport,        // Sent when a layer is about to be exported.
	eNotify_OnEndLayerExport,          // Sent after a layer have been exported.
	eNotify_OnClearLevelContents,      // Send when the level is cleared
	eNotify_OnMissionChange,           // Send when the current mission changes.
	eNotify_OnBeginLoad,               // Sent when the document is start to load.
	eNotify_OnEndLoad,                 // Sent when the document loading is finished
	eNotify_OnBeginExportToGame,       // Sent when the level starts to be exported to game
	eNotify_OnExportToGame,            // Sent when the level is exported to game
	eNotify_OnExportToGameFailed,      // Sent when the exporting level to game failed

	// Game related events.
	eNotify_OnBeginGameMode,           // Sent when editor goes to game mode.
	eNotify_OnEndGameMode,             // Sent when editor goes out of game mode.
	eNotify_OnEnableFlowSystemUpdate,  // Sent when game engine enables flowsystem updates
	eNotify_OnDisableFlowSystemUpdate, // Sent when game engine disables flowsystem updates

	// AI/Physics simulation related events.
	eNotify_OnBeginSimulationMode,     // Sent when simulation mode is started.
	eNotify_OnEndSimulationMode,       // Sent when editor goes out of simulation mode.

	// UI events.
	eNotify_OnUpdateViewports,         // Sent when editor needs to update data in the viewports.
	eNotify_OnMainFrameInitialized,    // Sent when editor main frame is initialized.

	eNotify_OnUpdateSequencer,             // Sent when editor needs to update the CryMannequin sequencer view.
	eNotify_OnUpdateSequencerKeys,         // Sent when editor needs to update keys in the CryMannequin track view.
	eNotify_OnUpdateSequencerKeySelection, // Sent when CryMannequin sequencer view changes selection of keys.

	eNotify_OnInvalidateControls,      // Sent when editor needs to update some of the data that can be cached by controls like combo boxes.

	// Object events.
	eNotify_OnSelectionChange,   // Sent when object selection change.
	eNotify_OnPlaySequence,      // Sent when editor start playing animation sequence.
	eNotify_OnStopSequence,      // Sent when editor stop playing animation sequence.

	// Task specific events.
	eNotify_OnTerrainRebuild,            // Sent when terrain was rebuilt (resized,...)
	eNotify_OnBeginTerrainRebuild,       // Sent when terrain begin rebuilt (resized,...)
	eNotify_OnEndTerrainRebuild,         // Sent when terrain end rebuilt (resized,...)
	eNotify_OnVegetationObjectSelection, // When vegetation objects selection change.
	eNotify_OnVegetationPanelUpdate,     // When vegetation objects selection change.

	eNotify_OnObjectHideMaskChange,     // Sent when editor changes types of objects to display

	eNotify_OnTimeOfDayChange,         // Time of day parameters where modified.

	eNotify_OnDataBaseUpdate,          // DataBase Library was modified.

	eNotify_OnBeginSWNewScene,          // Sent when SW document is begin to be cleared.
	eNotify_OnEndSWNewScene,            // Sent after SW document have been cleared.
	eNotify_OnBeginSWMoveTo,            // moveto operation was started
	eNotify_OnEndSWMoveTo,              // moveto operation completed
	eNotify_OnSWLockUnlock,             // Sent when commit, rollback or getting lock from segmented world
	eNotify_OnSWVegetationStatusChange, // When changed segmented world status of vegetation map

	eNotify_OnBeginUndoRedo,
	eNotify_OnEndUndoRedo,
	eNotify_CameraChanged, // When the active viewport camera was changed

	// Designer Object Specific events.
	eNotify_OnExportBrushes,           // For Designer objects, or objects using the Designer Tool.

	eNotify_OnTextureLayerChange,      // Sent when texture layer was added, removed or moved
	eNotify_OnBeginBindViewport,       // Sent when a viewport is about to be bound.

	eNotify_OnBeginLayoutResize, //  Sent when we begin changing the layout
	eNotify_OnEndLayoutResize,   //  Sent when we begin changing the layout
	eNotify_EditorConfigSpecChanged
};

//! Mouse events that viewport can send
enum EMouseEvent
{
	eMouseMove,
	eMouseLDown,
	eMouseLUp,
	eMouseLDblClick,
	eMouseRDown,
	eMouseRUp,
	eMouseRDblClick,
	eMouseMDown,
	eMouseMUp,
	eMouseMDblClick,
	eMouseWheel,
	eMouseFocusEnter,
	eMouseFocusLeave,
	eMouseEnter,
	eMouseLeave
};

enum EDragEvent
{
	eDragEnter,
	eDragLeave,
	eDragMove,
	eDrop
};

//! Global editor operation mode
enum EOperationMode
{
	eOperationModeNone = 0, // None
	eCompositingMode,       // Normal operation mode where objects are composited in the scene
	eModellingMode          // Geometry modeling mode
};

//////////////////////////////////////////////////////////////////////////
//DEPRECATED
//! Types of database items

enum EDataBaseItemType
{
	EDB_TYPE_MATERIAL,
	EDB_TYPE_PARTICLE,
	EDB_TYPE_ENTITY_ARCHETYPE,
	EDB_TYPE_PREFAB, // Deprecated since CE 5.6
	EDB_TYPE_GAMETOKEN,
	EDB_TYPE_FLARE
};

//! Viewports update flags for UpdateViews
enum UpdateConentFlags
{
	eUpdateHeightmap = 0x01,
	eUpdateStatObj   = 0x02,
	eUpdateObjects   = 0x04, //! Update objects in viewport.
	eRedrawViewports = 0x08  //! Just redraw viewports..
};
//////////////////////////////////////////////////////////////////////////

void CryFatalError(const char*, ...);

struct IEditor
{
	virtual ~IEditor() {}

	virtual ISystem*             GetSystem() = 0;

	virtual IMovieSystem*        GetMovieSystem() = 0;
	//! Access to class factory.
	virtual IEditorClassFactory* GetClassFactory() = 0;

	//! Registers a python command
	virtual IPythonManager* GetIPythonManager() = 0;

	//! Returns the instance of the asset manager
	virtual CAssetManager* GetAssetManager() = 0;

	//! Get Global Broadcast Manager.
	//! This broadcast manager exists for the lifetime of the application and is used for inter-editor communication.
	//! For more information about the hierarchical broadcasting system see BroadcastManager.h.
	virtual CBroadcastManager* GetGlobalBroadcastManager() = 0;
	virtual IExportManager*    GetExportManager() = 0;

	//! Access to commands manager.
	virtual ICommandManager* GetICommandManager() = 0;
	// Executes an Editor command.
	virtual void             ExecuteCommand(const char* sCommand, ...) = 0;

	virtual IUndoManager*    GetIUndoManager() = 0;

	//! Access to notification center
	virtual INotificationCenter* GetNotificationCenter() = 0;
	virtual CPreferences*        GetPreferences() = 0;

	// Access to personalization files manager
	virtual CPersonalizationManager* GetPersonalizationManager() = 0;

	//! Access to tray area
	virtual CTrayArea* GetTrayArea() = 0;

	//! Access to toolbar service for loading and storing editor specific toolbars
	virtual CToolBarService* GetToolBarService() = 0;

	//! Retrieve interface to the source control.
	virtual ISourceControl* GetSourceControl() = 0;

	//! Get project manager interface
	virtual Cry::IProjectManager* GetProjectManager() = 0;

	//! Gets the level editor primary interface
	virtual ILevelEditor*            GetLevelEditor() = 0;
	virtual CLevelEditorSharedState* GetLevelEditorSharedState() = 0;

	virtual void                     Notify(EEditorNotifyEvent event) = 0;
	//! Register Editor notifications listener.
	virtual void                     RegisterNotifyListener(IEditorNotifyListener* listener) = 0;
	//! Unregister Editor notifications listener.
	virtual void                     UnregisterNotifyListener(IEditorNotifyListener* listener) = 0;

	// Register Object Mode sub tools. Needed for tools defined within Sandbox project
	virtual void RegisterAllObjectModeSubTools() = 0;
	virtual void UnRegisterAllObjectModeSubTools() = 0;

	//! This folder is supposed to store Sandbox user settings and state
	virtual const char* GetUserFolder() = 0;

	//! Returns the path of the editors Master CD folder
	virtual string                 GetMasterCDFolder() = 0;
	virtual IResourceSelectorHost* GetResourceSelectorHost() = 0;

	//! Returns IconManager.
	virtual IIconManager* GetIconManager() = 0;

	virtual I3DEngine*    Get3DEngine() = 0;
	virtual IRenderer*    GetRenderer() = 0;

	virtual bool          IsMainFrameClosing() const = 0;
	virtual bool          IsInConsolewMode() = 0;
	virtual bool          IsInGameMode() = 0;
	virtual void          SetInGameMode(bool inGame) = 0;
	virtual bool          IsDevModeEnabled() const = 0; //should go to preferences?

	//Dockable pane management
	virtual IPane*              CreateDockable(const char* szClassName) = 0;
	virtual IPane*              FindDockable(const char* szClassName) = 0;
	virtual IPane*              FindDockableIf(const std::function<bool(IPane*, const string& /*className*/)>& predicate) = 0;
	virtual std::vector<IPane*> FindAllDockables(const char* szClassName) = 0;
	virtual void                RaiseDockable(IPane* pPane) = 0;

	//!Creates a preview widget for any file type, returns null if cannot be previewed
	virtual QWidget* CreatePreviewWidget(const QString& file, QWidget* pParent = nullptr) = 0;

	virtual void     PostOnMainThread(std::function<void()> task) = 0;

	//! Returns an interface to the viewport manager class
	virtual IViewportManager* GetViewportManager() = 0;

	//////////////////////////////////////////////////////////////////////////
	// DEPRECATED METHODS
	// DO NOT USE
	// TODO : move into another interface for clarity of what is deprecated and what is not
	//////////////////////////////////////////////////////////////////////////

	virtual CGameEngine*      GetGameEngine() = 0;
	virtual void              SetPlayerViewMatrix(const Matrix34& tm, bool bEyePos = true) = 0;

	virtual bool              IsSourceControlAvailable() = 0;

	virtual CUIEnumsDatabase* GetUIEnumsDatabase() = 0;

	//! Get DB manager that own items of specified type.
	virtual IDataBaseManager*       GetDBItemManager(EDataBaseItemType itemType) = 0;
	virtual void                    OpenAndFocusDataBase(EDataBaseItemType type, IDataBaseItem* pItem) = 0;

	virtual IBackgroundTaskManager* GetBackgroundTaskManager() = 0;

	virtual CConfigurationManager*  GetConfigurationManager() const = 0;

	virtual IAIManager*             GetAIManager() = 0;

	virtual CFlowGraphManager*      GetFlowGraphManager() = 0;

	//TODO: This is part of level editor
	virtual IGizmoManager* GetGizmoManager() = 0;

	virtual CRuler*        GetRuler() = 0;

	// Return true if editor in a state where document is ready to be modified.
	virtual bool         IsDocumentReady() const = 0;
	virtual CCryEditDoc* GetDocument() const = 0;
	virtual void         SetModifiedFlag(bool modified = true) = 0;

	//! Get access to object manager.
	virtual IObjectManager* GetObjectManager() = 0;

	//! Creates a new object.
	//! bInteractive: when true, will force visibility of the object type, will show and unfreeze the layer if necessary and select the object,
	//! intended for immediate interaction of the object
	virtual CBaseObject*           NewObject(const char* type, const char* file = nullptr, bool bInteractive = false) = 0;

	virtual void                   DeleteObject(CBaseObject* obj) = 0;
	virtual const ISelectionGroup* GetISelectionGroup() const = 0;
	virtual bool                   IsSelectionLocked() = 0;
	virtual CBaseObject*           GetSelectedObject() = 0;
	virtual CPrefabManager*        GetPrefabManager() = 0;
	virtual const char*            GetLevelName() = 0;
	virtual const char*            GetLevelPath() = 0;
	virtual bool                   StartObjectCreation(const char* type, const char* file = nullptr) = 0;
	virtual CHeightmap*            GetHeightmap() = 0;
	// end level editor methods

	virtual ESystemConfigSpec GetEditorConfigSpec() const = 0;

	virtual void              SetConsoleVar(const char* var, const int value) = 0;
	virtual void              SetConsoleVar(const char* var, const float value) = 0;
	virtual float             GetConsoleVar(const char* var) = 0;

	//! Adds a handler for native OS specific events. Useful for plugins that need access to specific OS messages.
	//! "message" is the OS specific message while "retval" is the return value for the message which only needed on Windows
	//! The way the interface works is identical to the qt based interface, check http://doc.qt.io/qt-5/qabstractnativeeventfilter.html
	virtual void AddNativeHandler(uintptr_t id, std::function<bool(void* message, long* retval)> callback) = 0;
	virtual void RemoveNativeHandler(uintptr_t id) = 0;

	virtual void ReduceMemory() = 0;

	//Both of these systems will be unified in a better interface
	virtual FileSystem::CEnumerator* GetFileSystemEnumerator() = 0;
	virtual IFileChangeMonitor*      GetFileMonitor() = 0;

	virtual void                     SetCurrentMissionTime(float time) = 0;
	virtual float                    GetCurrentMissionTime() = 0;

	//todo : remove functor
	typedef Functor2<CPopupMenuItem*, const CBaseObject*> TContextMenuExtensionFunc;
	virtual void              RegisterObjectContextMenuExtension(TContextMenuExtensionFunc func) = 0;

	virtual CMaterialManager* GetMaterialManager() = 0;

	//IUriEventListener interface
	virtual void RegisterURIListener(IUriEventListener* pListener, const char* szSchema) = 0;
	virtual void UnregisterURIListener(IUriEventListener* pListener, const char* szSchema) = 0;
	//~IUriEventListener interface

	virtual void  RequestScriptReload(const EReloadScriptsType& kind) = 0;

	virtual CWnd* OpenView(const char* sViewClassName) = 0;
	virtual CWnd* FindView(const char* sViewClassName) = 0;
	virtual void  SetActiveView(CViewport* viewport) = 0;

	//Returns the viewport manager, deprecate and replace with IViewportManager instead
	virtual CViewManager*     GetViewManager() = 0;

	virtual CViewport*        GetActiveView() = 0;
	virtual IDisplayViewport* GetActiveDisplayViewport() = 0;

	virtual void              UpdateViews(int flags = 0xFFFFFFFF, AABB* updateRegion = nullptr) = 0;
	virtual bool              IsUpdateSuspended() const = 0;
	virtual bool              IsGameInputSuspended() = 0;
	virtual void              ToggleGameInputSuspended() = 0;

	//Do not use, implement a newer version of this in FilePathUtil if necessary
	virtual bool ScanDirectory(const string& path, const string& fileSpec, std::vector<string>& filesOut, bool recursive = true) = 0;

	//! CBaseObject temporary methods, should most likely be moved to level editor
	virtual bool             PickObject(const Vec3& vWorldRaySrc, const Vec3& vWorldRayDir, SRayHitInfo& outHitInfo, CBaseObject* pObject) = 0;
	virtual void             AddWaitProgress(CWaitProgress* pProgress) = 0;
	virtual void             RemoveWaitProgress(CWaitProgress* pProgress) = 0;
	virtual void             OnObjectContextMenuOpened(CPopupMenuItem* pMenu, const CBaseObject* pObject) = 0;
	virtual IEditorMaterial* LoadMaterial(const string& name) = 0;
	virtual void             OnRequestMaterial(IMaterial* pMatInfo) = 0;
	virtual bool             IsCGroup(CBaseObject* pObject) = 0;
	virtual void             ResumeUpdateCGroup(CBaseObject* pObject) = 0;
	virtual bool             SuspendUpdateCGroup(CBaseObject* pObject) = 0;
	virtual void             SyncPrefabCPrefabObject(CBaseObject* pObject, const SObjectChangedContext& context) = 0;
	virtual bool             IsModifyInProgressCPrefabObject(CBaseObject* oObject) = 0;
	virtual bool             IsCPrefabObject(CBaseObject* pObject) = 0;
	virtual bool             IsGroupOpen(CBaseObject* pObject) = 0;
	virtual void             OpenGroup(CBaseObject* pObject) = 0;
	virtual void             RemoveMemberCGroup(CBaseObject* pGroup, CBaseObject* pObject) = 0;
	virtual float            GetTerrainElevation(float x, float y) = 0;
	virtual void             RegisterEntity(IRenderNode* pRenderNode) = 0;
	virtual void             UnRegisterEntityAsJob(IRenderNode* pRenderNode) = 0;
	virtual void             RegisterDeprecatedPropertyEditor(int propertyType, std::function<bool(const string&, string&)>& handler) = 0;
	virtual bool             EditDeprecatedProperty(int propertyType, const string& oldValue, string& newValueOut) = 0;
	virtual bool             CanEditDeprecatedProperty(int propertyType) = 0;
	virtual bool             IsInPreviewMode() = 0;
};

IEditor* GetIEditor();

//! Derive from this class if you want to register for getting global editor notifications.
//! Use IAutoEditorNotifyListener for convenience
struct IEditorNotifyListener
{
	bool m_bIsRegistered;

	IEditorNotifyListener() : m_bIsRegistered(false) {}
	virtual ~IEditorNotifyListener()
	{
		if (m_bIsRegistered)
			CryFatalError("Destroying registered IEditorNotifyListener");
	}

	//! called by the editor to notify the listener about the specified event.
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event) = 0;
};

//! Better implementation of IEditorNotifyListener with automatic registration and deregistration in constructor/destructor
struct IAutoEditorNotifyListener : public IEditorNotifyListener
{
	IAutoEditorNotifyListener()
		: IEditorNotifyListener()
	{
		GetIEditor()->RegisterNotifyListener(this);
	}

	~IAutoEditorNotifyListener()
	{
		GetIEditor()->UnregisterNotifyListener(this);
	}
};
