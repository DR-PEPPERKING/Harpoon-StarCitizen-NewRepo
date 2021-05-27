// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct bop_meshupdate;
struct EventPhysCollision;
struct EventPhysJointBroken;
struct EventPhysPostStep;
struct IFoliage;
struct IGeometry;
struct IMaterial;
struct IPhysicalEntity;
struct IPhysicalWorld;
struct IRenderMesh;
struct IStatObj;
struct pe_action_move_parts;
struct pe_status_pos;
struct phys_geometry;
struct SEntityEvent;
struct SEntityPhysicalizeParams;

class CEntity;

#include <CryEntitySystem/IEntityBasicTypes.h>

#if 0 // uncomment if more than 64 parts are needed and the mask can no longer fit into an int64
	#include <CryCore/BitMask.h>
typedef bitmask_t<bitmaskPtr, 4>    attachMask;
typedef bitmask_t<bitmaskBuf<4>, 4> attachMaskLoc;
	#define attachMask1 (bitmask_t<bitmaskOneBit, 4>(1))
#else
typedef uint32 attachMask, attachMaskLoc;
	#define attachMask1 1
#endif

// Implements physical behavior of the entity.
class CEntityPhysics final : public ISimpleEntityEventListener
{
public:
	CEntityPhysics() = default;
	virtual ~CEntityPhysics();

	// ISimpleEntityEventListener
	virtual void ProcessEvent(const SEntityEvent& event) override;
	// ~ISimpleEntityEventListener

	void SerializeXML(XmlNodeRef& entityNode, bool bLoading);
	bool NeedNetworkSerialize();
	void SerializeTyped(TSerialize ser, int type, int flags);
	void EnableNetworkSerialization(bool enable);
	//////////////////////////////////////////////////////////////////////////

	void             Serialize(TSerialize ser);

	void             GetLocalBounds(AABB& bbox) const;
	void             GetWorldBounds(AABB& bbox) const;

	void             Physicalize(SEntityPhysicalizeParams& params);
	IPhysicalEntity* GetPhysicalEntity() const { return m_pPhysicalEntity; }
	void             EnablePhysics(bool bEnable);
	bool             IsPhysicsEnabled() const;
	void             AddImpulse(int ipart, const Vec3& pos, const Vec3& impulse, bool bPos, float fAuxScale, float fPushScale = 1.0f);

	int              GetPartId0(int nSlot = 0);
	int              GetPhysAttachId();
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Called from physics events.
	//////////////////////////////////////////////////////////////////////////
	// Called back by physics PostStep event for the owned physical entity.
	void OnPhysicsPostStep(EventPhysPostStep* pEvent = 0);
	void AttachToPhysicalEntity(IPhysicalEntity* pPhysEntity);
	void CreateRenderGeometry(int nSlot, IGeometry* pFromGeom, bop_meshupdate* pLastUpdate = 0);
	void OnPhysicsStateChanged(int previousSimulationClass);
	//////////////////////////////////////////////////////////////////////////

	void              UpdateSlotGeometry(int nSlot, IStatObj* pStatObjNew = 0, float mass = -1.0f, int bNoSubslots = 1);
	void              AssignPhysicalEntity(IPhysicalEntity* pPhysEntity, int nSlot = -1);

	virtual bool      PhysicalizeFoliage(int iSlot);
	virtual void      DephysicalizeFoliage(int iSlot);
	virtual IFoliage* GetFoliage(int iSlot);

	int               AddSlotGeometry(int nSlot, SEntityPhysicalizeParams& params, int bNoSubslots = 1);
	void              RemoveSlotGeometry(int nSlot);

	void              MovePhysics(CEntityPhysics* dstPhysics);

	virtual void      GetMemoryUsage(ICrySizer* pSizer) const;
	void              ReattachSoftEntityVtx(IPhysicalEntity* pAttachToEntity, int nAttachToPart);

#if !defined(_RELEASE)
	static void       EnableValidation();
	static void       DisableValidation();
#endif

	void              PrepareForDeletion();
	void              OnCollision(const EventPhysCollision& collision, int sourceIndex);
	void              SendBreakEvent(EventPhysJointBroken* pEvent);
	void              OnGlobalEntityMaterialChanged(IMaterial* pMaterial);

private:
	IPhysicalWorld*  PhysicalWorld() const { return gEnv->pPhysicalWorld; }
	void             OnEntityXForm(EntityTransformationFlagsMask transformReasons);
	void             OnChangedPhysics(bool bEnabled);
	void             DestroyPhysicalEntity(bool bDestroyCharacters = true, int iMode = 0);

	void             PhysicalizeSimple(SEntityPhysicalizeParams& params);
	void             PhysicalizeLiving(SEntityPhysicalizeParams& params);
	void             PhysicalizeParticle(SEntityPhysicalizeParams& params);
	void             PhysicalizeSoft(SEntityPhysicalizeParams& params);
	void             AttachSoftVtx(IRenderMesh* pRM, IPhysicalEntity* pAttachToEntity, int nAttachToPart);
	void             PhysicalizeArea(SEntityPhysicalizeParams& params);
#if defined(USE_GEOM_CACHES)
	bool             PhysicalizeGeomCache(SEntityPhysicalizeParams& params);
#endif
	bool             PhysicalizeCharacter(SEntityPhysicalizeParams& params);
	bool             ConvertCharacterToRagdoll(SEntityPhysicalizeParams& params, const Vec3& velInitial);

	void             CreatePhysicalEntity(SEntityPhysicalizeParams& params);
	phys_geometry*   GetSlotGeometry(int nSlot);
	void             SyncCharacterWithPhysics();

	IPhysicalEntity* QueryPhyscalEntity(IEntity* pEntity) const;
	CEntity*         GetCEntity(IPhysicalEntity* pPhysEntity);

	void             ReleasePhysicalEntity();

	void             RemapChildAttachIds(CEntity* pent, attachMask& idmaskSrc, attachMask& idmaskDst, int* idmap);

	bool             TriggerEventIfStateChanged(IPhysicalEntity* pPhysEntity, const pe_status_pos* pPrevStatus) const;
	// Figures out render material at slot nSlot, and fills necessary data into the ppart output structure
	void             UpdateParamsFromRenderMaterial(int nSlot, IPhysicalEntity* pPhysEntity);

	void             AwakeOnRender(bool vRender);
	void             OnTimer(int id);
	void             OnEntityHiddenOrMadeInvisible();
	void             OnEntityUnhiddenOrMadeVisible();
	void             OnEntityUnhidden();
	void             OnChildEntityAttached(EntityId childEntityId);
	void             OnChildEntityDetached(EntityId childEntityId);

private:
	CEntity* GetEntity() const;

private:
	friend class CEntity;

	// Pointer to physical object.
	IPhysicalEntity* m_pPhysicalEntity = nullptr;
};
