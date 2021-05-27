// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "QViewportConsumer.h"
#include <vector>
#include <CrySerialization/Forward.h>

struct ICharacterInstance;
class QToolBar;
class QPropertyTreeLegacy;

namespace CharacterTool
{

using std::vector;
class CharacterToolForm;
class CharacterDocument;
class TransformPanel;
struct System;

struct SModeContext
{
	System*                system;
	CharacterToolForm*     window;
	CharacterDocument*     document;
	ICharacterInstance*    character;
	TransformPanel*        transformPanel;
	QToolBar*              toolbar;
	vector<QPropertyTreeLegacy*> layerPropertyTrees;
};

struct IViewportMode : public QViewportConsumer
{
	virtual ~IViewportMode() {}
	virtual void Serialize(Serialization::IArchive& ar)                         {}
	virtual void EnterMode(const SModeContext& context)                         {}
	virtual void LeaveMode()                                                    {}

	virtual void GetPropertySelection(vector<const void*>* selectedItems) const {}
	virtual void SetPropertySelection(const vector<const void*>& items)         {}
};

}
