// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IAudioInterfacesCommonData.h"

/**
 * @namespace CryAudio
 * @brief Most parent audio namespace used throughout the entire engine.
 */
namespace CryAudio
{
class CObjectTransformation;

/**
 * @struct CryAudio::IListener
 * @brief A struct exposing public methods to interact with audio listeners.
 */
struct IListener
{
	// <interfuscator:shuffle>
	/** @cond */
	virtual ~IListener() = default;
	/** @endcond */

	/**
	 * Gets the listener's unique id.
	 * @return id of the listener.
	 */
	virtual ListenerId GetId() const = 0;

	/**
	 * Sets the listener's transformation.
	 * @param transformation - constant reference to an object holding the transformation to apply.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void SetTransformation(CTransformation const& transformation, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Sets this audio listener's name.
	 * Is used only during production whenever the owning entity's name is changed to adjust corresponding audio listeners as well.
	 * @param szName - name to set.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void SetName(char const* const szName, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;
	// </interfuscator:shuffle>
};
} // namespace CryAudio
