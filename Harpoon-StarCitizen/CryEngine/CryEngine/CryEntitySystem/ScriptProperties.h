// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct IScriptObject;
class XmlNodeRef;
struct IScriptTable;

//////////////////////////////////////////////////////////////////////////
// This class handles assignment of entity script properties from XML nodes
// to the script tables.
//////////////////////////////////////////////////////////////////////////
class CScriptProperties
{
public:
	bool SetProperties(XmlNodeRef& entityNode, IScriptTable* pEntityTable);
	void Assign(XmlNodeRef& propsNode, IScriptTable* pPropsTable);
};
