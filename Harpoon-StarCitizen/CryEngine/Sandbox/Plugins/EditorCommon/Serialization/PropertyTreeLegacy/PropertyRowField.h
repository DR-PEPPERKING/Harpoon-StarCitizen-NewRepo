/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once
#include <CrySerialization/yasli/Config.h>
#include "PropertyRow.h"
#include "IDrawContext.h"

class PROPERTY_TREE_API PropertyRowField : public PropertyRow
{
public:
	WidgetPlacement widgetPlacement() const override{ return WIDGET_VALUE; }
	int widgetSizeMin(const PropertyTreeLegacy* tree) const override;

	virtual int buttonCount() const{ return 0; }
	virtual property_tree::Icon buttonIcon(const PropertyTreeLegacy* tree, int index) const;
	virtual bool usePathEllipsis() const { return false; }
	virtual bool onActivateButton(int buttonIndex, const PropertyActivationEvent& e) { return false; }
	int hitButton(const PropertyTreeLegacy* tree, const Point& p) const;

	void redraw(IDrawContext& context) override;
	bool onActivate(const PropertyActivationEvent& e) override;
	bool onMouseDown(PropertyTreeLegacy* tree, Point point, bool& changed) override;
	void onMouseUp(PropertyTreeLegacy* tree, Point point) override;
protected:
	Rect fieldRect(const PropertyTreeLegacy* tree) const;

	mutable RowWidthCache widthCache_;
};
