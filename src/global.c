#include "global.h"
#include <stdbool.h>

// global.c Implements Settings Module and holds Global Constants
static bool settings_data[SETTINGS_COUNT] = { false };

void set_settings_flag(settings_flags flag) {
	settings_data[flag] = true;
}

bool get_settings_flag(settings_flags flag) {
	return settings_data[flag];
}
