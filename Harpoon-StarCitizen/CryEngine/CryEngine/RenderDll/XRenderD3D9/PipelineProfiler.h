// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DeviceManager/DeviceObjects.h"

class CRenderPipelineProfiler
{
public:
	enum EProfileSectionFlags
	{
		eProfileSectionFlags_RootElement = BIT(0),
		eProfileSectionFlags_MultithreadedSection = BIT(1)
	};
	
	CRenderPipelineProfiler();

	void                   Init();
	void                   Finish();
	void                   Display();

	void                   BeginFrame(const int frameID);
	void                   EndFrame();

	void                   BeginSection(const char* name);
	void                   EndSection(const char* name);

	uint32                 InsertMultithreadedSection(const char* name);
	void                   UpdateMultithreadedSection(uint32 index, bool bSectionStart, int numDIPs, int numPolys, bool bIssueTimestamp, CTimeValue deltaTimestamp, CDeviceCommandList* pCommandList);
	
	bool                   IsEnabled() const;
	void                   SetEnabled(bool enabled) { m_enabled = enabled; }
	void                   SetPaused(bool paused) { if ((m_paused = paused)) m_recordData = false; }

	const RPProfilerStats&                   GetBasicStats(ERenderPipelineProfilerStats stat, int nThreadID) { assert((uint32)stat < RPPSTATS_NUM); return m_basicStats[nThreadID][stat]; }
	const RPProfilerStats*                   GetBasicStatsArray(int nThreadID)                               { return m_basicStats[nThreadID]; }
	const DynArray<RPProfilerDetailedStats>* GetDetailedStatsArray(int nThreadID)                            { return &m_detailedStats[nThreadID]; }
	const SRenderStatistics::SFrameSummary&  GetFrameSummary(int nThreadID)                                  { return m_frameTimings[nThreadID]; }

protected:
	struct SProfilerSection
	{
		char            name[31]; 
		float           gpuTime;
		float           gpuTimeSmoothed;
		float           cpuTime;
		float           cpuTimeSmoothed;
		CTimeValue      startTimeCPU, endTimeCPU;
		uint32          startTimestamp, endTimestamp;
		CCryNameTSCRC   path;
		int             numDIPs, numPolys;
		int8            recLevel;   // Negative value means error in stack
		uint8           flags;
		uint16          pos;
	};

	struct SFrameData
	{
		enum { kMaxNumSections = CDeviceTimestampGroup::kMaxTimestamps / 2 };

		uint32                  m_numSections;
		int                     m_frameID;
		SProfilerSection        m_sections[kMaxNumSections];
		CDeviceTimestampGroup   m_timestampGroup;
		bool                    m_updated;

		SRenderStatistics::SFrameSummary m_frameTimings;

		// cppcheck-suppress uninitMemberVar
		SFrameData()
			: m_numSections(0)
			, m_frameID(0)
			, m_updated(false)
		{
			memset(m_sections, 0, sizeof(m_sections));
		}
	};

	struct SStaticElementInfo
	{
		uint nPos;
		bool bUsed;

		SStaticElementInfo(uint _nPos)
			: nPos(_nPos)
			, bUsed(false)
		{
		}
	};

protected:
	uint32 InsertSection(const char* name, uint32 profileSectionFlags = 0);
	
	bool FilterLabel(const char* name);
	void UpdateSectionTimesAndStats(uint32 frameDataIndex);
	void UpdateThreadTimings(uint32 frameDataIndex);
	
	void ResetBasicStats(RPProfilerStats* pBasicStats, bool bResetAveragedStats);
	void ResetDetailedStats(DynArray<RPProfilerDetailedStats>& pDetailedStats, bool bResetAveragedStats);
	void ComputeAverageStats(SFrameData& currData, SFrameData& prevData);
	void AddToStats(RPProfilerStats& outStats, SProfilerSection& section);
	void SubtractFromStats(RPProfilerStats& outStats, SProfilerSection& section);
	void UpdateStats(uint32 frameDataIndex);

	void DisplayOverviewStats(uint32 frameDataIndex) const;
	void DisplayDetailedPassStats(uint32 frameDataIndex) const;

private:
	SProfilerSection& FindSection(SFrameData& frameData, SProfilerSection& section);

protected:
	enum { kNumPendingFrames = MAX_FRAMES_IN_FLIGHT 
		+ 1 /* display is delayed by one frame */ 
		+ 1 /* timestamp group fence is issued after RT_EndFrame so it's technically part of the next frame */ };

	static_assert(kNumPendingFrames <= MAX_TIMESTAMP_GROUPS, "Not enough reservable Timestamp-Groups available for the PipelineProfiler");

	std::vector<uint32>               m_stack;
	SFrameData                        m_frameData[kNumPendingFrames];
	uint32                            m_frameDataIndex;
	SFrameData*                       m_frameDataRT;
	SFrameData*                       m_frameDataLRU;
	float                             m_avgFrameTime;
	bool                              m_enabled;
	bool                              m_paused;
	bool                              m_recordData;

	RPProfilerStats                   m_basicStats[RT_COMMAND_BUF_COUNT][RPPSTATS_NUM];
	DynArray<RPProfilerDetailedStats> m_detailedStats[RT_COMMAND_BUF_COUNT];
	SRenderStatistics::SFrameSummary  m_frameTimings[RT_COMMAND_BUF_COUNT],
	                                  m_frameTimingsSmoothed;

	// we take a snapshot every now and then and store it in here to prevent the text from jumping too much
	std::multimap<CCryNameTSCRC, SStaticElementInfo> m_staticNameList;
};
