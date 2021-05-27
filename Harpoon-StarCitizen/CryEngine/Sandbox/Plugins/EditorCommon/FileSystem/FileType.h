// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QMetaType>
#include <QString>
#include <QVector>

/**
 * \brief statically describes a known file type of engine
 *
 * this structure should be statically allocated and never freed
 */
struct SFileType
{
	static const char*      trContext; ///< context for nameKey

	const char*             nameTrKey;        ///< translatable name for the file type
	QString                 iconPath;         ///< icon path used for visualizations
	QVector<QString>        folders;          ///< engine folders this file type exists in (empty means all folders)
	QString                 primaryExtension; ///< main extension (default for opening & saving the file)
	QVector<QString>        extraExtensions;  ///< additional file name extensions

	QString                 name() const; ///< \returns name for current translation

	static const SFileType* Unknown();
	static const SFileType* DirectoryType();
	static const SFileType* SymLink();

	void                    CheckValid() const;
};

Q_DECLARE_METATYPE(const SFileType*)
