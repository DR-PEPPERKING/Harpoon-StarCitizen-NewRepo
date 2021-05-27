// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProjectLoader.h"

#include "Impl.h"

#include <CryAudioImplWwise/GlobalData.h>
#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/XML/IXml.h>

namespace ACE
{
namespace Impl
{
namespace Wwise
{
string const g_parametersFolderName = "Game Parameters";
string const g_statesFolderName = "States";
string const g_switchesFolderName = "Switches";
string const g_eventsFolderName = "Events";
string const g_environmentsFolderName = "Master-Mixer Hierarchy";
string const g_soundBanksFolderName = "SoundBanks";
string const g_soundBanksInfoFileName = "SoundbanksInfo.xml";
ControlId g_soundBanksFolderId = g_invalidControlId;

//////////////////////////////////////////////////////////////////////////
EItemType TagToItemType(char const* const szTag)
{
	EItemType type = EItemType::None;

	if (_stricmp(szTag, "GameParameter") == 0)
	{
		type = EItemType::Parameter;
	}
	else if (_stricmp(szTag, "Event") == 0)
	{
		type = EItemType::Event;
	}
	else if (_stricmp(szTag, "AuxBus") == 0)
	{
		type = EItemType::AuxBus;
	}
	else if (_stricmp(szTag, "WorkUnit") == 0)
	{
		type = EItemType::WorkUnit;
	}
	else if (_stricmp(szTag, "StateGroup") == 0)
	{
		type = EItemType::StateGroup;
	}
	else if (_stricmp(szTag, "SwitchGroup") == 0)
	{
		type = EItemType::SwitchGroup;
	}
	else if (_stricmp(szTag, "Switch") == 0)
	{
		type = EItemType::Switch;
	}
	else if (_stricmp(szTag, "State") == 0)
	{
		type = EItemType::State;
	}
	else if (_stricmp(szTag, "Folder") == 0)
	{
		type = EItemType::VirtualFolder;
	}
	else
	{
		type = EItemType::None;
	}

	return type;
}

//////////////////////////////////////////////////////////////////////////
string BuildPath(IItem const* const pIItem)
{
	string buildPath = "";

	if (pIItem != nullptr)
	{
		IItem const* const pParent = pIItem->GetParent();

		if (pParent != nullptr)
		{
			buildPath = BuildPath(pParent) + "/" + pIItem->GetName();
		}
		else
		{
			buildPath = pIItem->GetName();
		}
	}

	return buildPath;
}

//////////////////////////////////////////////////////////////////////////
CProjectLoader::CProjectLoader(string const& projectPath, string const& soundbanksPath, string const& localizedBanksPath, CItem& rootItem, ItemCache& itemCache)
	: m_rootItem(rootItem)
	, m_itemCache(itemCache)
	, m_projectPath(projectPath)
{
	// Wwise places all the Work Units in the same root physical folder but some of those WU can be nested
	// inside each other so to be able to reference them in the right order we build a cache with the path and UIDs
	// so that we can load them in the right order (i.e. the top most parent first)
	BuildFileCache(g_parametersFolderName);
	BuildFileCache(g_statesFolderName);
	BuildFileCache(g_switchesFolderName);
	BuildFileCache(g_eventsFolderName);
	BuildFileCache(g_environmentsFolderName);

	LoadFolder("", g_parametersFolderName, m_rootItem);
	LoadFolder("", g_statesFolderName, m_rootItem);
	LoadFolder("", g_switchesFolderName, m_rootItem);
	LoadFolder("", g_eventsFolderName, m_rootItem);
	LoadFolder("", g_environmentsFolderName, m_rootItem);

	CItem* const pSoundBanks = CreateItem(g_soundBanksFolderName, EItemType::PhysicalFolder, m_rootItem, EPakStatus::None);
	g_soundBanksFolderId = pSoundBanks->GetId();
	LoadSoundBanks(soundbanksPath, false, *pSoundBanks);
	LoadSoundBanks(localizedBanksPath, true, *pSoundBanks);

	if (pSoundBanks->GetNumChildren() == 0)
	{
		m_rootItem.RemoveChild(pSoundBanks);
		ItemCache::const_iterator const it(m_itemCache.find(pSoundBanks->GetId()));

		if (it != m_itemCache.end())
		{
			m_itemCache.erase(it);
		}

		delete pSoundBanks;
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadSoundBanks(string const& folderPath, bool const isLocalized, CItem& parent)
{
	_finddata_t fd;
	intptr_t const handle = gEnv->pCryPak->FindFirst(folderPath + "/*.*", &fd);

	if (handle != -1)
	{
		EItemFlags const flags = isLocalized ? EItemFlags::IsLocalized : EItemFlags::None;

		do
		{
			string const name = fd.name;

			if ((name != ".") && (name != "..") && !name.empty())
			{
				if ((fd.attrib & _A_SUBDIR) == 0)
				{
					if ((_stricmp(PathUtil::GetExt(name), "bnk") == 0) && (name.compareNoCase("Init.bnk") != 0))
					{
						string const fullName = folderPath + "/" + name;
						ControlId const id = CryAudio::StringToId(fullName);
						CItem* const pItem = stl::find_in_map(m_itemCache, id, nullptr);

						if (pItem == nullptr)
						{

							EPakStatus const pakStatus = gEnv->pCryPak->IsFileExist(fullName.c_str(), ICryPak::eFileLocation_OnDisk) ? EPakStatus::OnDisk : EPakStatus::None;
							auto const pSoundBank = new CItem(name, id, EItemType::SoundBank, flags, pakStatus, fullName);

							parent.AddChild(pSoundBank);
							m_itemCache[id] = pSoundBank;
						}
					}
				}
			}
		}
		while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

		gEnv->pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadFolder(string const& folderPath, string const& folderName, CItem& parent)
{
	_finddata_t fd;
	ICryPak* const pCryPak = gEnv->pCryPak;
	string const fullFolderPath = m_projectPath + "/" + folderPath + "/" + folderName;
	intptr_t const handle = pCryPak->FindFirst(m_projectPath + "/" + folderPath + "/" + folderName + "/*.*", &fd);

	if (handle != -1)
	{
		EPakStatus const folderPakStatus = gEnv->pCryPak->IsFileExist(fullFolderPath.c_str(), ICryPak::eFileLocation_OnDisk) ? EPakStatus::OnDisk : EPakStatus::None;
		CItem* const pParent = CreateItem(folderName, EItemType::PhysicalFolder, parent, folderPakStatus);

		do
		{
			string const name = fd.name;

			if ((name != ".") && (name != "..") && !name.empty())
			{
				if ((fd.attrib & _A_SUBDIR) != 0)
				{
					LoadFolder(folderPath + "/" + folderName, name, *pParent);
				}
				else
				{
					string const fullPath = m_projectPath + "/" + folderPath + "/" + folderName + "/" + name;
					EPakStatus const pakStatus = gEnv->pCryPak->IsFileExist(fullPath.c_str(), ICryPak::eFileLocation_OnDisk) ? EPakStatus::OnDisk : EPakStatus::None;

					LoadWorkUnitFile(folderPath + "/" + folderName + "/" + name, *pParent, pakStatus);
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadWorkUnitFile(const string& filePath, CItem& parent, EPakStatus const pakStatus)
{
	XmlNodeRef const rootNode = GetISystem()->LoadXmlFromFile(m_projectPath + "/" + filePath);

	if (rootNode.isValid())
	{
		uint32 const fileId = CryAudio::StringToId(rootNode->getAttr("ID"));

		if (m_filesLoaded.count(fileId) == 0)
		{
			// Make sure we've loaded any work units we depend on before loading
			if (rootNode->haveAttr("RootDocumentID"))
			{
				uint32 const parentDocumentId = CryAudio::StringToId(rootNode->getAttr("RootDocumentID"));

				if (m_items.count(parentDocumentId) == 0)
				{
					// File hasn't been processed so we load it
					FilesCache::const_iterator const it(m_filesCache.find(parentDocumentId));

					if (it != m_filesCache.end())
					{
						LoadWorkUnitFile(it->second, parent, pakStatus);
					}
					else
					{
						CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "[Audio Controls Editor] [Wwise] Couldn't find Work Unit which file %s depends on. Are you missing wwise project files?.", filePath.c_str());
					}
				}
			}

			// Each files starts with the type of item and then the WorkUnit
			int const childCount = rootNode->getChildCount();

			for (int i = 0; i < childCount; ++i)
			{
				LoadXml(rootNode->getChild(i)->findChild("WorkUnit"), parent, pakStatus);
			}

			m_filesLoaded.insert(fileId);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadXml(XmlNodeRef const& rootNode, CItem& parent, EPakStatus const pakStatus)
{
	if (rootNode.isValid())
	{
		CItem* pItem = &parent;
		EItemType const type = TagToItemType(rootNode->getTag());

		if (type != EItemType::None)
		{
			string const name = rootNode->getAttr("Name");
			uint32 const itemId = CryAudio::StringToId(rootNode->getAttr("ID"));

			// Check if this item has not been created before. It could have been created in
			// a different Work Unit as a reference
			Items::const_iterator const it(m_items.find(itemId));

			if (it != m_items.end())
			{
				pItem = it->second;
			}
			else
			{
				pItem = CreateItem(name, type, parent, pakStatus);
				m_items[itemId] = pItem;
			}
		}

		XmlNodeRef const childNode = rootNode->findChild("ChildrenList");

		if (childNode.isValid())
		{
			int const childCount = childNode->getChildCount();
			for (int i = 0; i < childCount; ++i)
			{
				LoadXml(childNode->getChild(i), *pItem, pakStatus);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CItem* CProjectLoader::CreateItem(const string& name, EItemType const type, CItem& parent, EPakStatus const pakStatus)
{
	string const path = BuildPath(&parent);
	string const fullPathName = path + "/" + name;

	// The id is always the path of the item from the root of the wwise project
	ControlId const id = CryAudio::StringToId(fullPathName);

	CItem* pItem = stl::find_in_map(m_itemCache, id, nullptr);

	if (pItem == nullptr)
	{
		switch (type)
		{
		case EItemType::WorkUnit:
			{
				pItem = new CItem(name, id, type, EItemFlags::IsContainer, pakStatus, m_projectPath + fullPathName + ".wwu");
				break;
			}
		case EItemType::PhysicalFolder:
			{
				if (id != g_soundBanksFolderId)
				{
					pItem = new CItem(name, id, type, EItemFlags::IsContainer, pakStatus, m_projectPath + fullPathName);
				}
				else
				{
					pItem = new CItem(name, id, type, EItemFlags::IsContainer);
				}

				break;
			}
		case EItemType::VirtualFolder: // Intentional fall-through.
		case EItemType::SwitchGroup:   // Intentional fall-through.
		case EItemType::StateGroup:
			{
				pItem = new CItem(name, id, type, EItemFlags::IsContainer, pakStatus);
				break;
			}
		default:
			{
				pItem = new CItem(name, id, type, EItemFlags::None, pakStatus);
				break;
			}
		}

		parent.AddChild(pItem);
		m_itemCache[id] = pItem;
	}

	return pItem;
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::BuildFileCache(string const& folderPath)
{
	_finddata_t fd;
	ICryPak* const pCryPak = gEnv->pCryPak;
	intptr_t const handle = pCryPak->FindFirst(m_projectPath + "/" + folderPath + "/*.*", &fd);

	if (handle != -1)
	{
		do
		{
			string const name = fd.name;

			if ((name != ".") && (name != "..") && !name.empty())
			{
				if (fd.attrib & _A_SUBDIR)
				{
					BuildFileCache(folderPath + "/" + name);
				}
				else
				{
					string const path = folderPath + "/" + name;
					XmlNodeRef const rootNode = GetISystem()->LoadXmlFromFile(m_projectPath + "/" + path);

					if (rootNode.isValid())
					{
						m_filesCache[CryAudio::StringToId(rootNode->getAttr("ID"))] = path;
					}
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}
} // namespace Wwise
} // namespace Impl
} // namespace ACE
