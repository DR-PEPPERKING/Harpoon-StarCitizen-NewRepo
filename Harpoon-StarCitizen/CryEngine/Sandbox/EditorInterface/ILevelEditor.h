// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//! This is the main interface to the level editor. Add methods here as necessary for plugin functionality
// Note: This is meant to replace the deprecated GetDocument/CryEditDoc semantic
// We should aim for the LevelEditor to contain a Level, and remove the old semantics over time
struct ILevelEditor
{
	virtual ~ILevelEditor() {}

	//! Returns true if a level is loaded in the editor
	virtual bool IsLevelLoaded() = 0;
};
