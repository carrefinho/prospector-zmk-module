#include <lvgl.h>

#if defined(CONFIG_PROSPECTOR_STATUS_SCREEN_CLASSIC)
#include "layouts/classic/status_screen.c"
#elif defined(CONFIG_PROSPECTOR_STATUS_SCREEN_RADII)
#include "layouts/radii/status_screen.c"
#elif defined(CONFIG_PROSPECTOR_STATUS_SCREEN_FIELD)
#include "layouts/field/status_screen.c"
#elif defined(CONFIG_PROSPECTOR_STATUS_SCREEN_OPERATOR)
#include "layouts/operator/status_screen.c"
#else
#error "No status screen layout selected"
#endif
