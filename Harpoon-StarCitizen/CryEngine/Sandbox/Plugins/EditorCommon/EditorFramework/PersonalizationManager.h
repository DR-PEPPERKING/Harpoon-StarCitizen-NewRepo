// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
#include "EditorCommonAPI.h"
#include "Util/UserDataUtil.h"
#include <IEditor.h>

#include <QHash>
#include <QTimer>

#define SET_PERSONALIZATION_PROPERTY(module, propName, value) \
	GetIEditor()->GetPersonalizationManager()->SetProperty( # module, propName, value)

#define GET_PERSONALIZATION_PROPERTY(module, propName) \
	GetIEditor()->GetPersonalizationManager()->GetProperty( # module, propName)

#define GET_PERSONALIZATION_STATE(module) \
	GetIEditor()->GetPersonalizationManager()->GetState( # module)

class EDITOR_COMMON_API CPersonalizationManager : public CUserData
{
public:
	CPersonalizationManager();
	~CPersonalizationManager();

	/* There are limited types that are serialized correctly to JSon with QJsonDocument.
	 * http://doc.qt.io/qt-5/qjsonvalue.html#fromVariant has a list of supported types and states:
	 * "For all other QVariant types a conversion to a QString will be attempted. If the returned string
	 * is empty, a Null QJsonValue will be stored"
	 *
	 * Please add functionality for going from and to QVariant in EditorCommon/QtUtil
	 * and use them in combination with the personalization manager's get and set property*/
	void            SetProperty(const QString& moduleName, const QString& propName, const QVariant& value);
	const QVariant& GetProperty(const QString& moduleName, const QString& propName);
	bool            HasProperty(const QString& moduleName, const QString& propName) const;

	template<class T>
	bool GetProperty(const QString& moduleName, const QString& propName, T& outValue)
	{
		const QVariant& variant = GetProperty(moduleName, propName);
		if (variant.isValid() && variant.canConvert<T>())
		{
			outValue = variant.value<T>();
			return true;
		}
		return false;
	}

	// Returns engine defaults for the given module
	const QVariantMap& GetDefaultState(const QString& moduleName);

	// Returns user personalization for the given module
	void               SetState(const QString& moduleName, const QVariantMap& state);
	const QVariantMap& GetState(const QString& moduleName);

	//Project specific properties. Will store in the user folder of the sandbox and only be usable for this project
	//ex: recent files are only meaningful in the context of a single project
	void            SetProjectProperty(const QString& moduleName, const QString& propName, const QVariant& value);
	const QVariant& GetProjectProperty(const QString& moduleName, const QString& propName);
	bool            HasProjectProperty(const QString& moduleName, const QString& propName) const;

	//! Saves the cached information to the personalization file on the hard drive.
	void SavePersonalization() const;

private:
	typedef QHash<QString, QVariantMap> ModuleStateMap;

	static QVariant       ToVariant(const ModuleStateMap& map);
	static ModuleStateMap FromVariant(const QVariant& variant);

	// Loads engine defaults from disk
	void                  LoadDefaultState();

	// Save/Load user personalization to/from disk
	void                  SaveSharedState() const;
	void                  LoadSharedState();

	// Save/Load project specific personalization to/from disk
	void                  SaveProjectState() const;
	void                  LoadProjectState();

private:
	// Engine defaults for personalization. Ex. what features should be enabled by default
	ModuleStateMap m_defaultState;
	// User personalization during editor use. Ex. features the user has enabled/disabled
	ModuleStateMap m_sharedState;
	// Project specific personalization. Ex. last folder used for opening a specific asset type
	ModuleStateMap m_projectState;

	QTimer         m_saveSharedStateTimer;
	QTimer         m_saveProjectStateTimer;
};
