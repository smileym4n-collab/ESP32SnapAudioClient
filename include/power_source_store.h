#pragma once

#include "power_source.h"

app_config::PowerSource loadPowerSourcePreference();
void savePowerSourcePreference(app_config::PowerSource source);
