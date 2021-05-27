// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CrySystem/ConsoleRegistration.h>

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		float         SCvars::timeBudgetInSeconds;
		int           SCvars::debugDraw;
		int           SCvars::debugDrawZTestOn;
		float         SCvars::debugDrawLineThickness;
		int           SCvars::debugDrawAlphaValueOfDiscardedItems;
		int           SCvars::logQueryHistory;
		float         SCvars::timeBudgetExcessThresholdInPercent;
		int           SCvars::printTimeExcessWarningsToConsole;
		int           SCvars::roundRobinLimit;

		void SCvars::Register()
		{
			REGISTER_CVAR2("uqs_timeBudgetInSeconds", &timeBudgetInSeconds, 0.0005f, VF_NULL,
				"The total time-budget in seconds granted to update all running queries in a time-sliced fashion.");

			REGISTER_CVAR2("uqs_debugDraw", &debugDraw, 0, VF_NULL,
				"0/1: Draw details of running and finished queries on 2D screen as well as in the 3D world via debug geometry.");

			REGISTER_CVAR2("uqs_debugDrawZTestOn", &debugDrawZTestOn, 1, VF_NULL,
				"0/1: Disable/enable z-tests for 3D debug geometry.");

			REGISTER_CVAR2("uqs_debugDrawLineThickness", &debugDrawLineThickness, 1.0f, VF_NULL,
				"Thickness of all 3d lines that are used for debug drawing.");

			REGISTER_CVAR2("uqs_debugDrawAlphaValueOfDiscardedItems", &debugDrawAlphaValueOfDiscardedItems, 127, VF_NULL,
				"Alpha value for drawing items that didn't make it into the final result set of a query. Clamped to [0..255] internally.");

			REGISTER_CVAR2("uqs_logQueryHistory", &logQueryHistory, 0, VF_NULL,
				"0/1: Enable logging of past queries to draw them at a later time via 'uqs_debugDraw' set to 1.\n"
				"Pick the one to draw via PGDOWN/PGUP.");

			REGISTER_CVAR2("uqs_timeBudgetExcessThresholdInPercent", &timeBudgetExcessThresholdInPercent, 20.0f, VF_NULL,
				"Percentage of the granted time of a query that we allow to exceed before before taking counter-measures and issuing warnings.");

			REGISTER_CVAR2("uqs_printTimeExcessWarningsToConsole", &printTimeExcessWarningsToConsole, 1, VF_NULL,
				"0/1: Print warnings due to time excess of queries also to the console, not just to the query history.");

			REGISTER_CVAR2("uqs_roundRobinLimit", &roundRobinLimit, 4, VF_NULL,
				"Max. number of (non-hierarchical) queries that will get a chance to do some work per frame.\n"
				"A value of 0 or smaller has a special meaning:\n"
				"It will allow *all* currently running queries to do some work (at the expense of possible congestion).");
		}

		void SCvars::Unregister()
		{
			gEnv->pConsole->UnregisterVariable("uqs_timeBudgetInSeconds");
			gEnv->pConsole->UnregisterVariable("uqs_debugDraw");
			gEnv->pConsole->UnregisterVariable("uqs_debugDrawZTestOn");
			gEnv->pConsole->UnregisterVariable("uqs_debugDrawLineThickness");
			gEnv->pConsole->UnregisterVariable("uqs_debugDrawAlphaValueOfDiscardedItems");
			gEnv->pConsole->UnregisterVariable("uqs_logQueryHistory");
			gEnv->pConsole->UnregisterVariable("uqs_timeBudgetExcessThresholdInPercent");
			gEnv->pConsole->UnregisterVariable("uqs_printTimeExcessWarningsToConsole");
			gEnv->pConsole->UnregisterVariable("uqs_roundRobinLimit");
		}
	}
}
