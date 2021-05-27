// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if !defined(D3DBuffer)
	#define D3DBuffer void
#endif
#if !defined(D3DShaderResource)
	#define D3DShaderResource void
#endif
#if !defined(D3DSamplerState)
	#define D3DSamplerState void
#endif

#include <CryCore/Platform/CryWindows.h>
#include <CryMemory/IDefragAllocator.h>
#include "../XRenderD3D9/DeviceManager/DeviceResources.h"

////////////////////////////////////////////////////////////////////////////////////////
// Usage hints
enum BUFFER_USAGE
{
	BU_IMMUTABLE = 0,             // For data that never, ever changes
	BU_STATIC,                    // For long-lived data that changes infrequently (every n-frames)
	BU_DYNAMIC,                   // For short-lived data that changes frequently (every frame)
	BU_TRANSIENT,                 // For very short-lived data that can be considered garbage after first usage
	BU_TRANSIENT_RT,              // For very short-lived data that can be considered garbage after first usage
	BU_WHEN_LOADINGTHREAD_ACTIVE, // yes we can ... because renderloadingthread frames not synced with mainthread frames
	BU_MAX
};

////////////////////////////////////////////////////////////////////////////////////////
// Binding flags
enum BUFFER_BIND_TYPE
{
	BBT_VERTEX_BUFFER = 0,
	BBT_INDEX_BUFFER,
	BBT_CONSTANT_BUFFER,
	BBT_MAX
};

typedef uintptr_t buffer_handle_t;
typedef uint32    item_handle_t;

////////////////////////////////////////////////////////////////////////////////////////
struct SDescriptorBlock
{
	SDescriptorBlock(uint32 id)
		: blockID(id)
		, pBuffer(NULL)
		, size(0)
		, offset(~0u)
	{}

	const uint32 blockID;

	void*         pBuffer;
	buffer_size_t size;
	buffer_size_t offset;
};

#if (CRY_RENDERER_VULKAN >= 10)
struct SDescriptorSet
{
	SDescriptorSet()
		: vkDescriptorSet(VK_NULL_HANDLE)
	{
	}

	VkDescriptorSet vkDescriptorSet;
};
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Constant buffer wrapper class
class CConstantBuffer
{
public:
	CDeviceBuffer* m_buffer;
#if CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS
	void*          m_allocator;
#endif
	void*          m_base_ptr;
	item_handle_t  m_handle;
	buffer_size_t  m_offset;
	buffer_size_t  m_size;
	int32          m_nRefCount;
	int8           m_nUpdCount;
	union
	{
		struct
		{
			uint8 m_used              : 1;
			uint8 m_lock              : 1;
			uint8 m_dynamic           : 1;
		};

		uint8 m_clearFlags;
	};

	CConstantBuffer(uint32 handle);
	~CConstantBuffer();

	inline void AddRef()
	{
		CryInterlockedIncrement(&m_nRefCount);
	}

	inline void Release()
	{
		if (CryInterlockedDecrement(&m_nRefCount) == 0)
			ReturnToPool();
	}
	
	inline void SetDebugName(const char* name) const
	{
		if (!m_buffer) return;
		// If we have CB direct access we only get a range-fragment instead of our own object which we can name
#if !CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS
		return m_buffer->SetDebugName(name);
#endif
	}

	inline D3DBuffer* GetD3D() const
	{
		return m_buffer->GetBuffer();
	}

	inline D3DBuffer* GetD3D(buffer_size_t* offset, buffer_size_t* size) const
	{
		*offset = m_offset;
		*size = m_size;
		return m_buffer->GetBuffer();
	}

	inline uint64 GetCode() const
	{
#if CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS
		uint64 code = reinterpret_cast<uintptr_t>(m_buffer) ^ SwapEndianValue((uint64)m_offset, true);
		return code;
#else
		return reinterpret_cast<uint64>(m_buffer);
#endif
	}

	void* BeginWrite();
	void  EndWrite(bool requires_flush = false);
	bool  UpdateBuffer(const void* src, buffer_size_t size, buffer_size_t offset = 0, int numDataBlocks = 1); // See CDeviceManager::UploadContents for details on numDataBlocks

	bool  IsNullBuffer() const { return m_size == 0; }

private:
	void  ReturnToPool();
};
typedef _smart_ptr<CConstantBuffer> CConstantBufferPtr;

// Wrapper on the Constant buffer, allowing using it from outside of renderer and RenderThread
class CGraphicsDeviceConstantBuffer : public IGraphicsDeviceConstantBuffer
{
public:
	CGraphicsDeviceConstantBuffer() : m_size(0), m_bDirty(false) {}
	virtual void       SetData(const uint8* data, size_t size) override;
	// Should only be called from the thread that can create and update constant buffers
	CConstantBufferPtr GetConstantBuffer();
	CConstantBufferPtr GetNullConstantBuffer();
	buffer_size_t      GetSize() const { return m_size; }

protected:
	virtual void DeleteThis() const override { delete this; }

private:
	buffer_size_t                                      m_size;
	stl::aligned_vector<uint8, CRY_PLATFORM_ALIGNMENT> m_data;
	CConstantBufferPtr                                 m_pConstantBuffer;
	bool                                               m_bDirty;

	static CryCriticalSection                          s_accessLock;
};

class CDeviceBufferManager
{
	////////////////////////////////////////////////////////////////////////////////////////
	// Debug consistency functions
	// Should only be called from the below befriended function! Please do not abuse!

	friend class CGuardedDeviceBufferManager;

	void            PinItem_Locked(buffer_handle_t);
	void            UnpinItem_Locked(buffer_handle_t);
	buffer_handle_t Create_Locked(BUFFER_BIND_TYPE type, BUFFER_USAGE usage, buffer_size_t size, bool bUsePool=true);
	void            Destroy_Locked(buffer_handle_t);
	void*           BeginRead_Locked(buffer_handle_t handle);
	void*           BeginWrite_Locked(buffer_handle_t handle);
	void            EndReadWrite_Locked(buffer_handle_t handle);
	bool            UpdateBuffer_Locked(buffer_handle_t handle, const void*, buffer_size_t, buffer_size_t = 0);
	buffer_size_t   Size_Locked(buffer_handle_t);
	
	CConstantBuffer* CreateConstantBufferRaw(buffer_size_t size, bool dynamic = true, bool needslock = false);

public:
	CDeviceBufferManager();
	~CDeviceBufferManager();

	// Get access to the singleton instance of device buffer manager
	static CDeviceBufferManager* Instance();

	// PPOL_ALIGNMENT is 128 in general, which is a bit much, we use the smallest alignment necessary for fast mem-copies
#if 0
	static        buffer_size_t GetBufferAlignmentForStreaming();
#else
	static inline buffer_size_t GetBufferAlignmentForStreaming()
	{
		return CRY_PLATFORM_ALIGNMENT;
	}
#endif

	template<typename var_size_t>
	static inline var_size_t AlignBufferSizeForStreaming(var_size_t size)
	{
		// Align the allocation size up to the configured allocation alignment
		var_size_t alignment = GetBufferAlignmentForStreaming();
		return (size + (alignment - 1)) & ~(alignment - 1);
	}

	template<typename var_size_t>
	static inline var_size_t AlignElementCountForStreaming(var_size_t numElements, var_size_t sizeOfElement)
	{
		const var_size_t missing = AlignBufferSizeForStreaming(sizeOfElement * numElements) - (sizeOfElement * numElements);
		if (missing)
			numElements += (missing + (sizeOfElement - 1)) / sizeOfElement;
		return numElements;
	}

	////////////////////////////////////////////////////////////////////////////////////////
	// Initialization and destruction and high level update functionality
	bool Init();
	void Update(uint32 frameId, bool called_during_loading);
	void ReleaseEmptyBanks(uint32 frameId);
	void Sync(uint32 frameId);
	bool Shutdown();

	////////////////////////////////////////////////////////////////////////////////////////
	// ConstantBuffer creation
	inline CConstantBufferPtr CreateConstantBuffer(buffer_size_t size, bool dynamic = true, bool needslock = false)
	{
		CConstantBufferPtr result;
		result.Assign_NoAddRef(CreateConstantBufferRaw(size, dynamic, needslock));
		return result;
	}

	// null buffers
	static CConstantBufferPtr GetNullConstantBuffer();
	static CGpuBuffer* GetNullBufferTyped();
	static CGpuBuffer* GetNullBufferStructured();


	////////////////////////////////////////////////////////////////////////////////////////
	// Descriptor blocks
	SDescriptorBlock* CreateDescriptorBlock(buffer_size_t size);
	void              ReleaseDescriptorBlock(SDescriptorBlock* pBlock);

#if (CRY_RENDERER_VULKAN >= 10)
	SDescriptorSet* AllocateDescriptorSet(const VkDescriptorSetLayout& descriptorSetLayout);
	void ReleaseDescriptorSet(SDescriptorSet *pDescriptorSet);
#endif

	////////////////////////////////////////////////////////////////////////////////////////
	// Locks the global devicebuffer lock
	void LockDevMan();
	void UnlockDevMan();

	////////////////////////////////////////////////////////////////////////////////////////
	// Pin/Unpin items for async access outside of the renderthread
	void PinItem(buffer_handle_t);
	void UnpinItem(buffer_handle_t);

	// Returns the size in bytes of the allocation
	buffer_size_t Size(buffer_handle_t);

	////////////////////////////////////////////////////////////////////////////////////////
	// Buffer Resource creation methods
	//
	buffer_handle_t Create(BUFFER_BIND_TYPE type, BUFFER_USAGE usage, buffer_size_t size, bool bUsePool=true);
	void            Destroy(buffer_handle_t);

	////////////////////////////////////////////////////////////////////////////////////////
	// Manual IO operations
	//
	// Note: it's an error to NOT end an IO operation with EndReadWrite!!!
	//
	// Note: If you are writing (updating) a buffer only partially, please be aware that the
	//       the contents of the untouched areas might be undefined as a copy-on-write semantic
	//       ensures that the updating of buffers does not synchronize with the GPU at any cost.
	//
	void* BeginRead(buffer_handle_t handle);
	void* BeginWrite(buffer_handle_t handle);
	void  EndReadWrite(buffer_handle_t handle);
	bool UpdateBuffer(buffer_handle_t handle, const void*, buffer_size_t, buffer_size_t = 0);

	template<class T>
	bool UpdateBuffer(buffer_handle_t handle, stl::aligned_vector<T, CRY_PLATFORM_ALIGNMENT>& vData)
	{
		buffer_size_t size = buffer_size_t(vData.size() * sizeof(T));
		return UpdateBuffer(handle, &vData[0], AlignBufferSizeForStreaming(size));
	}

	////////////////////////////////////////////////////////////////////////////////////////
	// Get Stats back from the devbuffer
	bool GetStats(BUFFER_BIND_TYPE, BUFFER_USAGE, SDeviceBufferPoolStats &);

	////////////////////////////////////////////////////////////////////////////////////////
	// Retrieval of the backing d3d buffers
	//
	// Note: Getting vertexbuffer for an index buffer or vice-versa is not an error!
	//
	// Note: Do not store the returned device buffer pointer outside of the function performing
	//       calls below.

	D3DBuffer*       GetD3D  (buffer_handle_t, buffer_size_t* offset);
	D3DBuffer*       GetD3D  (buffer_handle_t, buffer_size_t* offset, buffer_size_t* size);
	D3DVertexBuffer* GetD3DVB(buffer_handle_t, buffer_size_t*);
	D3DIndexBuffer*  GetD3DIB(buffer_handle_t, buffer_size_t*);
};

class CGuardedDeviceBufferManager : public NoCopy
{
	CDeviceBufferManager* m_pDevBufMan;
public:

	explicit CGuardedDeviceBufferManager(CDeviceBufferManager* pDevMan)
		: m_pDevBufMan(pDevMan)
	{ m_pDevBufMan->LockDevMan(); }

	~CGuardedDeviceBufferManager() { m_pDevBufMan->UnlockDevMan(); }

	static inline buffer_size_t GetBufferAlignForStreaming()                                                    { return CDeviceBufferManager::GetBufferAlignmentForStreaming(); }
	template<typename var_size_t>
	static inline    var_size_t AlignBufferSizeForStreaming(var_size_t size)                                    { return CDeviceBufferManager::AlignBufferSizeForStreaming(size); }
	template<typename var_size_t>
	static inline    var_size_t AlignElementCountForStreaming(var_size_t numElements, var_size_t sizeOfElement) { return CDeviceBufferManager::AlignElementCountForStreaming(numElements, sizeOfElement); }

	////////////////////////////////////////////////////////////////////////////////////////
	// Pin/Unpin items for async access outside of the renderthread
	void PinItem(buffer_handle_t handle)   { m_pDevBufMan->PinItem_Locked(handle); }
	void UnpinItem(buffer_handle_t handle) { m_pDevBufMan->UnpinItem_Locked(handle); }

	////////////////////////////////////////////////////////////////////////////////////////
	// Buffer Resource creation methods
	//
	buffer_handle_t Create(BUFFER_BIND_TYPE type, BUFFER_USAGE usage, buffer_size_t size) { return m_pDevBufMan->Create_Locked(type, usage, size); }
	void            Destroy(buffer_handle_t handle)                                       { return m_pDevBufMan->Destroy_Locked(handle); }

	////////////////////////////////////////////////////////////////////////////////////////
	// Manual IO operations
	//
	// Note: it's an error to NOT end an IO operation with EndReadWrite!!!
	//
	// Note: If you are writing (updating) a buffer only partially, please be aware that the
	//       the contents of the untouched areas might be undefined as a copy-on-write semantic
	//       ensures that the updating of buffers does not synchronize with the GPU at any cost.
	//
	void*            BeginRead(buffer_handle_t handle)                                         { return m_pDevBufMan->BeginRead_Locked(handle); }
	void*            BeginWrite(buffer_handle_t handle)                                        { return m_pDevBufMan->BeginWrite_Locked(handle); }
	void             EndReadWrite(buffer_handle_t handle)                                      { m_pDevBufMan->EndReadWrite_Locked(handle); }
	bool             UpdateBuffer(buffer_handle_t handle, const void* src, buffer_size_t size) { return m_pDevBufMan->UpdateBuffer_Locked(handle, src, size); }

	D3DBuffer*       GetD3D(buffer_handle_t handle, buffer_size_t* offset)                     { return m_pDevBufMan->GetD3D(handle, offset); }
	D3DVertexBuffer* GetD3DVB(buffer_handle_t handle, buffer_size_t* offset)                   { return static_cast<D3DVertexBuffer*>(m_pDevBufMan->GetD3D(handle, offset)); }
	D3DIndexBuffer*  GetD3DIB(buffer_handle_t handle, buffer_size_t* offset)                   { return static_cast<D3DIndexBuffer*>(m_pDevBufMan->GetD3D(handle, offset)); }
};

class SRecursiveSpinLock
{
	volatile LONG     m_lock;
	volatile threadID m_owner;
	volatile uint16   m_counter;

	enum { SPIN_COUNT = 10 };

public:

	SRecursiveSpinLock()
		: m_lock()
		, m_owner()
		, m_counter()
	{}

	~SRecursiveSpinLock() {}

	void Lock()
	{
		threadID threadId = CryGetCurrentThreadId();
		int32 iterations = 0;
retry:
		IF (CryInterlockedCompareExchange(&(this->m_lock), 1L, 0L) == 0L, 1)
		{
			assert(m_owner == 0u && m_counter == 0u);
			m_owner = threadId;
			m_counter = 1;
		}
		else
		{
			IF (m_owner == threadId, 1)
				++m_counter;
			else
			{
				CrySleep((1 & isneg(SPIN_COUNT - iterations++)));
				goto retry;
			}
		}
	}

	bool TryLock()
	{
		threadID threadId = CryGetCurrentThreadId();
		IF (CryInterlockedCompareExchange(&m_lock, 1L, 0L) == 0L, 1)
		{
			assert(m_owner == 0u && m_counter == 0u);
			m_owner = threadId;
			m_counter = 1;
			return true;
		}
		else
		{
			IF (m_owner == threadId, 1)
			{
				++m_counter;
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	void Unlock()
	{
		assert(m_owner == CryGetCurrentThreadId() && m_counter != 0u);
		IF ((m_counter -= 1) == 0u, 1)
		{
			m_owner = 0u;
			m_lock = 0L;
			MemoryBarrier();
		}
	}
};

class SRecursiveSpinLocker
{
	SRecursiveSpinLock* lock;
public:
	SRecursiveSpinLocker(SRecursiveSpinLock* _lock)
		: lock(_lock)
	{
		lock->Lock();
	}
	~SRecursiveSpinLocker() { lock->Unlock(); }
};
#define SREC_AUTO_LOCK(x) SRecursiveSpinLocker _lock(&(x))

class CConditonalDevManLock
{
	CDeviceBufferManager* m_pDevBufMan;
	int                   m_Active;
public:
	explicit CConditonalDevManLock(CDeviceBufferManager* DevMan, int active)
		: m_pDevBufMan(DevMan)
		, m_Active(active)
	{
		if (m_Active)
			m_pDevBufMan->LockDevMan();
	}

	~CConditonalDevManLock()
	{
		if (m_Active)
			m_pDevBufMan->UnlockDevMan();
	}
};

class CGpuBuffer : NoCopy, public CResourceBindingInvalidator
{

public:
	CGpuBuffer(CDeviceBuffer* devBufToOwn = nullptr)
		: m_pDeviceBuffer(nullptr)
		, m_elementCount(0)
		, m_elementSize(0)
		, m_eFlags(0)
		, m_eMapMode(D3D11_MAP(0))
		, m_bLocked(false)
	{
		if (devBufToOwn)
		{
			OwnDevBuffer(devBufToOwn);
		}
	}

	virtual ~CGpuBuffer();

	void           Create(buffer_size_t numElements, buffer_size_t elementSize, DXGI_FORMAT elementFormat, uint32 flags, const void* pData);
	void           Release();

	inline SBufferLayout GetLayout() const
	{
		const SBufferLayout Layout =
		{
			m_eFormat,
			m_elementCount,
			static_cast<uint16>(m_elementSize),
			/* TODO: change FT_... to CDeviceObjectFactory::... */
			m_eFlags,
		};

		return Layout;
	}

	void           OwnDevBuffer(CDeviceBuffer* pDeviceTex);
	void           UpdateBufferContent(const void* pData, buffer_size_t nSize);
	void*          Lock();
	void           Unlock(buffer_size_t nSize);

	bool           IsNullBuffer()    const { return m_elementCount == 0; }
	bool           IsAvailable ()    const { return m_pDeviceBuffer != nullptr;  }
	CDeviceBuffer* GetDevBuffer()    const { return m_pDeviceBuffer; }
	uint32         GetFlags()        const { return m_eFlags; }
	buffer_size_t  GetElementCount() const { return m_elementCount; }

#if DURANGO_USE_ESRAM
	bool IsESRAMResident();
	bool AcquireESRAMResidency(CDeviceResource::EResidencyCoherence residencyEntry);
	bool ForfeitESRAMResidency(CDeviceResource::EResidencyCoherence residencyExit);
	bool TransferESRAMAllocation(CGpuBuffer* target);
#endif

	inline void    SetDebugName(const char* name) const { m_pDeviceBuffer->SetDebugName(name); }

	//////////////////////////////////////////////////////////////////////////

private:
	CDeviceBuffer*     AllocateDeviceBuffer(const void* pInitialData) const;
	void               ReleaseDeviceBuffer(CDeviceBuffer*& pDeviceBuffer) const;

	void               PrepareUnusedBuffer();

	CDeviceBuffer*                   m_pDeviceBuffer;

	buffer_size_t                    m_elementCount;
	buffer_size_t                    m_elementSize;
	uint32                           m_eFlags;

	D3D11_MAP                        m_eMapMode;
	DXGI_FORMAT                      m_eFormat;
	
	bool                             m_bLocked;

#if DURANGO_USE_ESRAM
	uint32                           m_nESRAMSize = 0;
	uint32                           m_nESRAMAlignment;
#endif

};

////////////////////////////////////////////////////////////////////////////////////////
static inline bool StreamBufferData(void* dst, const void* src, size_t sze)
{
	bool requires_flush = true;

#if 0
	assert(!(size_t(src) & (CRY_PLATFORM_ALIGNMENT - 1U)));
	assert(!(size_t(dst) & (CRY_PLATFORM_ALIGNMENT - 1U)));
	assert(!(size_t(sze) & (CRY_PLATFORM_ALIGNMENT - 1U)));
	//	assert(!(size_t(sze) & (SPoolConfig::POOL_ALIGNMENT - 1U)));
#endif

#if CRY_PLATFORM_SSE2
	if ((((uintptr_t)dst | (uintptr_t)src | sze) & 0xf) == 0u)
	{
		__m128* d = (__m128*)dst;
		const __m128* s = (const __m128*)src;

		size_t regs = sze / sizeof(__m128);
		size_t offs = 0;

		while (regs & (~7))
		{
			__m128 r0 = _mm_load_ps((const float*)(s + offs + 0));
			__m128 r1 = _mm_load_ps((const float*)(s + offs + 1));
			__m128 r2 = _mm_load_ps((const float*)(s + offs + 2));
			__m128 r3 = _mm_load_ps((const float*)(s + offs + 3));
			__m128 r4 = _mm_load_ps((const float*)(s + offs + 4));
			__m128 r5 = _mm_load_ps((const float*)(s + offs + 5));
			__m128 r6 = _mm_load_ps((const float*)(s + offs + 6));
			__m128 r7 = _mm_load_ps((const float*)(s + offs + 7));
			_mm_stream_ps((float*)(d + offs + 0), r0);
			_mm_stream_ps((float*)(d + offs + 1), r1);
			_mm_stream_ps((float*)(d + offs + 2), r2);
			_mm_stream_ps((float*)(d + offs + 3), r3);
			_mm_stream_ps((float*)(d + offs + 4), r4);
			_mm_stream_ps((float*)(d + offs + 5), r5);
			_mm_stream_ps((float*)(d + offs + 6), r6);
			_mm_stream_ps((float*)(d + offs + 7), r7);

			regs -= 8;
			offs += 8;
		}

		while (regs)
		{
			__m128 r0 = _mm_load_ps((const float*)(s + offs + 0));
			_mm_stream_ps((float*)(d + offs + 0), r0);

			regs -= 1;
			offs += 1;
		}

	#if !CRY_PLATFORM_ORBIS // Defer SFENCE until submit
		_mm_sfence();
	#endif
		requires_flush = false;
	}
	else
#endif
	{
		cryMemcpy(dst, src, sze, MC_CPU_TO_GPU);
	}

	return requires_flush;
}

static inline bool FetchBufferData(void* dst, const void* src, size_t sze)
{
	bool requires_flush = true;

#if 0
	assert(!(size_t(src) & (CRY_PLATFORM_ALIGNMENT - 1U)));
	assert(!(size_t(dst) & (CRY_PLATFORM_ALIGNMENT - 1U)));
	assert(!(size_t(sze) & (CRY_PLATFORM_ALIGNMENT - 1U)));
	//	assert(!(size_t(sze) & (SPoolConfig::POOL_ALIGNMENT - 1U)));
#endif

#if CRY_PLATFORM_SSE2
	if ((((uintptr_t)dst | (uintptr_t)src | sze) & 0xf) == 0u)
	{
		__m128* d = (__m128*)dst;
		const __m128* s = (const __m128*)src;

		size_t regs = sze / sizeof(__m128);
		size_t offs = 0;

		while (regs & (~7))
		{
			__m128 r0 = _mm_load_ps((const float*)(s + offs + 0));
			__m128 r1 = _mm_load_ps((const float*)(s + offs + 1));
			__m128 r2 = _mm_load_ps((const float*)(s + offs + 2));
			__m128 r3 = _mm_load_ps((const float*)(s + offs + 3));
			__m128 r4 = _mm_load_ps((const float*)(s + offs + 4));
			__m128 r5 = _mm_load_ps((const float*)(s + offs + 5));
			__m128 r6 = _mm_load_ps((const float*)(s + offs + 6));
			__m128 r7 = _mm_load_ps((const float*)(s + offs + 7));
			_mm_store_ps((float*)(d + offs + 0), r0);
			_mm_store_ps((float*)(d + offs + 1), r1);
			_mm_store_ps((float*)(d + offs + 2), r2);
			_mm_store_ps((float*)(d + offs + 3), r3);
			_mm_store_ps((float*)(d + offs + 4), r4);
			_mm_store_ps((float*)(d + offs + 5), r5);
			_mm_store_ps((float*)(d + offs + 6), r6);
			_mm_store_ps((float*)(d + offs + 7), r7);

			regs -= 8;
			offs += 8;
		}

		while (regs)
		{
			__m128 r0 = _mm_load_ps((const float*)(s + offs + 0));
			_mm_store_ps((float*)(d + offs + 0), r0);

			regs -= 1;
			offs += 1;
		}

		requires_flush = false;
	}
	else
#endif
	{
		cryMemcpy(dst, src, sze, MC_GPU_TO_CPU);
	}

	return requires_flush;
}
