#include <lvgl.h>

#if defined(CONFIG_PROSPECTOR_STATUS_SCREEN_DEFAULT)
#include "layouts/default/status_screen.c"
#elif defined(CONFIG_PROSPECTOR_STATUS_SCREEN_ROUNDED_GRID)
#include "layouts/rounded_grid/status_screen.c"
#elif defined(CONFIG_PROSPECTOR_STATUS_SCREEN_WIND_MAP)
#include "layouts/wind_map/status_screen.c"
#else
#error "No status screen layout selected"
#endif
