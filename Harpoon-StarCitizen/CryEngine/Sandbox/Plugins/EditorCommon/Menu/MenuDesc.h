// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AbstractMenu.h"

#include <CryString/CryString.h>

#include <memory>
#include <vector>

class QCommandAction;

namespace MenuDesc
{

template<typename K> struct SItem;
template<typename K> struct SActionItem;
template<typename K> struct SMenuItem;

template<typename K>
struct SItemVisitor
{
	virtual ~SItemVisitor() {}
	virtual void Visit(const SItem<K>&)       {}
	virtual void Visit(const SActionItem<K>&) {}
	virtual void Visit(const SMenuItem<K>&)   {}
};

template<typename K>
struct SItem
{
	SItem(int priority, int section, const K& key)
		: m_pParent(nullptr)
		, m_priority(priority)
		, m_section(section)
		, m_key(key)
	{
	}

	virtual ~SItem() {}
	virtual void   Accept(SItemVisitor<K>& visitor, const K& key) const = 0;

	CAbstractMenu* FindMenu(CAbstractMenu* pRootMenu) const
	{
		std::vector<string> path;
		SMenuItem<K>* pIt = m_pParent;
		while (pIt)
		{
			path.push_back(pIt->m_name);
			pIt = pIt->m_pParent;
		}

		CAbstractMenu* pMenu = pRootMenu;
		while (!path.empty() && pMenu)
		{
			pMenu = pMenu->FindMenu(path.back().c_str());
			path.pop_back();
		}

		return pMenu;
	}

	SMenuItem<K>* m_pParent;
	int           m_priority;
	int           m_section;
	K             m_key;
};

template<typename K>
struct SActionItem : SItem<K>
{
	SActionItem(std::function<QCommandAction*()> retrieveActionCallback, int priority, int section, const K& key)
		: SItem<K>(priority, section, key)
		, m_retrieveActionCallback(retrieveActionCallback)
	{
	}

	virtual void Accept(SItemVisitor<K>& visitor, const K& key) const override
	{
		if (key == this->m_key)
		{
			visitor.Visit(*this);
		}
	}

	std::function<QCommandAction*()> m_retrieveActionCallback;
};

template<typename K>
struct SMenuItem : SItem<K>
{
	template<typename ... ARGS>
	SMenuItem(const char* szName, int priority, int section, const K& key, ARGS ... args)
		: SItem<K>(priority, section, key)
		, m_name(szName)
	{
		Init(std::forward<ARGS>(args) ...);
	}

	template<typename ... ARGS>
	void Init(std::unique_ptr<SItem<K>>&& item, ARGS&& ... args)
	{
		m_items.emplace_back(std::move(item));
		m_items.back()->m_pParent = this;
		Init(std::forward<ARGS>(args) ...);
	}

	void Init() // Recursion anchor.
	{
	}

	virtual void Accept(SItemVisitor<K>& visitor, const K& key) const override
	{
		if (this->m_key == key)
		{
			visitor.Visit(*this);
		}

		for (const auto& item : m_items)
		{
			item->Accept(visitor, key);
		}
	}

	const string                           m_name;
	std::vector<std::unique_ptr<SItem<K>>> m_items;
};

template<typename K>
std::unique_ptr<SActionItem<K>> AddAction(const K& key, int section, int priority, std::function<QCommandAction*()> retrieveActionCallback)
{
	return std::make_unique<SActionItem<K>>(retrieveActionCallback, priority, section, key);
}

template<typename K, typename ... ARGS>
std::unique_ptr<SMenuItem<K>> AddMenu(const K& key, int section, int priority, const char* szName, ARGS ... args)
{
	return std::make_unique<SMenuItem<K>>(szName, priority, section, key, std::forward<ARGS>(args) ...);
}

template<typename K>
struct SAddItemVisitor : SItemVisitor<K>
{
	SAddItemVisitor(CAbstractMenu* pRootMenu)
		: m_pRootMenu(pRootMenu)
	{
	}

	virtual void Visit(const SItem<K>&) override {}

	virtual void Visit(const SActionItem<K>& actionItem) override
	{
		QCommandAction* pAction = actionItem.m_retrieveActionCallback();
		CRY_ASSERT_MESSAGE(pAction, "Trying to add an unregistered action to editor menu. Make sure to call RegisterAction");

		CAbstractMenu* const pMenu = actionItem.FindMenu(m_pRootMenu);
		if (pMenu && !pMenu->ContainsAction(pAction))
		{
			pMenu->AddAction(pAction, actionItem.m_section, actionItem.m_priority);
		}
	}

	virtual void Visit(const SMenuItem<K>& menuItem) override
	{
		CAbstractMenu* const pMenu = menuItem.FindMenu(m_pRootMenu);
		if (pMenu && !pMenu->FindMenu(menuItem.m_name.c_str()))
		{
			pMenu->CreateMenu(menuItem.m_name.c_str(), menuItem.m_section, menuItem.m_priority);
		}
	}

	CAbstractMenu* const m_pRootMenu;
};

template<typename K>
struct SGetMenuNameVisitor : SItemVisitor<K>
{
	virtual void Visit(const SItem<K>&) override       {}
	virtual void Visit(const SActionItem<K>&) override {}

	virtual void Visit(const SMenuItem<K>& menuItem) override
	{
		m_name = menuItem.m_name;
	}

	string m_name;
};

template<typename K>
struct SGetMenuActionVisitor : SItemVisitor<K>
{
	virtual void Visit(const SItem<K>&) override {}
	virtual void Visit(const SActionItem<K>& menuItem) override
	{
		m_pAction = menuItem.m_retrieveActionCallback();
	}

	virtual void Visit(const SMenuItem<K>&) override {}

	QCommandAction* m_pAction{ nullptr };
};

template<typename K>
class CDesc
{
public:
	template<typename ... ARGS>
	void Init(std::unique_ptr<SItem<K>>&& item, ARGS&& ... args)
	{
		m_items.emplace_back(std::move(item));
		Init(std::forward<ARGS>(args) ...);
	}

	void Init() // Recursion anchor.
	{
	}

	void AddItem(CAbstractMenu* pRootMenu, const K& key)
	{
		SAddItemVisitor<K> v(pRootMenu);
		Accept(v, key);
	}

	const char* GetMenuName(const K& key) const
	{
		SGetMenuNameVisitor<K> v;
		Accept(v, key);
		return v.m_name.c_str();
	}

	QCommandAction* GetAction(const K& key) const
	{
		SGetMenuActionVisitor<K> v;
		Accept(v, key);
		return v.m_pAction;
	}

private:
	void Accept(SItemVisitor<K>& visitor, const K& key) const
	{
		for (const auto& item : m_items)
		{
			item->Accept(visitor, key);
		}
	}

private:
	std::vector<std::unique_ptr<SItem<K>>> m_items;
};

} // namespace MenuDesc
