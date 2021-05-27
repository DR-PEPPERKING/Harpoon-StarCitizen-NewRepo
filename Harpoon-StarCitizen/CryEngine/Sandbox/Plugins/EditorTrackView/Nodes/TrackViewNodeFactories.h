// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CTrackViewTrack;
class CTrackViewAnimNode;
class CTrackViewNode;
class CEntityObject;
struct IAnimSequence;
struct IAnimNode;
struct IAnimTrack;

class CTrackViewAnimNodeFactory
{
public:
	CTrackViewAnimNode* BuildAnimNode(IAnimSequence* pSequence, IAnimNode* pAnimNode, CTrackViewNode* pParentNode, CEntityObject* pEntity);
};

class CTrackViewTrackFactory
{
public:
	CTrackViewTrack* BuildTrack(IAnimTrack* pTrack, CTrackViewAnimNode* pTrackAnimNode,
	                            CTrackViewNode* pParentNode, bool bIsSubTrack = false, unsigned int subTrackIndex = 0);
};
