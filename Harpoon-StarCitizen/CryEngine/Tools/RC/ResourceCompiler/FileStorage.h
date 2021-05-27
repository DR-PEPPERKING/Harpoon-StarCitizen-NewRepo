// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

//! An associative container with unique keys.
class CFileStorage
{
public:
	CFileStorage(const string& path) :m_path(path) {}

	void SetFiles(const string& key, const std::vector<string>& files) const;

	const void CopyFiles(const string& key, const string& destinationFolder, std::vector<string>& copiedFiles) const;

private:
	string m_path;
};

