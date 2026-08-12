/*
Streamiau Counter
Copyright (C) 2026, José Almeida <jose.afga@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>
#include "counter-dock.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

bool obs_module_load(void)
{
    CounterDock* streamiau_counter_dock = new CounterDock();
	bool success = obs_frontend_add_dock_by_id("streamiau-counter-dock_id", obs_module_text("Title"), streamiau_counter_dock);

	if (success) {
    	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
    	return true;
	}

	delete streamiau_counter_dock;
	return false;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "plugin unloaded");
}
