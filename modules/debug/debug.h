#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>
#include <string.h>
#include <time.h>

/* Debug levels */
typedef enum
{
    DEBUG_LEVEL_NONE = 0,
    DEBUG_LEVEL_ERROR,
    DEBUG_LEVEL_WARN,
    DEBUG_LEVEL_INFO,
    DEBUG_LEVEL_DEBUG,
    DEBUG_LEVEL_TRACE
} debug_level_t;

/* Debug modules */
typedef enum
{
    MODULE_MAIN,
    MODULE_LVGL,
    MODULE_UI,
    MODULE_GAME,
    MODULE_SOUND,
    MODULE_LED,
    MODULE_GPIO,
    MODULE_INPUT,
    MODULE_ANIMATION,
    MODULE_LOGIC,
    MODULE_HAL,   // Added HAL module
    MODULE_VIDEO, // Video playback (ffplay)
    MODULE_OTA,   // OTA firmware update
    MODULE_RADIO, // PGA Tour Radio background audio
    MODULE_COUNT  // Keep this as the last element
} debug_module_t;

/* Build type detection */
#ifdef DEBUG
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DEBUG_LEVEL_DEBUG
#endif
#define BUILD_TYPE "Debug"
#else
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DEBUG_LEVEL_WARN
#endif
#define BUILD_TYPE "Release"
#endif

/* Defaults if not set by CMake */
#ifndef DEBUG_COLORS
#define DEBUG_COLORS 1
#endif

#ifndef DEBUG_TIMESTAMP
#define DEBUG_TIMESTAMP 1
#endif

/* Project-specific module names - now as string constants */
static const char *DEBUG_MODULE_NAMES[MODULE_COUNT] = {
    "MAIN", "LVGL", "UI", "GAME", "SOUND", "LED", "GPIO", "INPUT", "ANIMATION", "LOGIC",
    "HAL",   // Added HAL module name
    "VIDEO", // Video playback module
    "OTA",   // OTA firmware update
    "RADIO"  // PGA Tour Radio background audio
};

/* Function prototypes */
void          debug_init(void);
void          debug_set_level(debug_level_t level);
void          debug_set_module_filter(const char *module_name);
debug_level_t debug_get_level(void);
const char   *debug_get_build_type(void);

/* Core debug macro with compile-time elimination */
#define DEBUG_PRINT(level, module, format, ...)                                    \
    do                                                                             \
    {                                                                              \
        if (debug_should_print(level, module))                                     \
        {                                                                          \
            debug_print(level, module, __FILE__, __LINE__, format, ##__VA_ARGS__); \
        }                                                                          \
    } while (0)

/* Level-specific macros */
#if DEBUG_LEVEL >= DEBUG_LEVEL_ERROR
#define DEBUG_ERROR(module, format, ...) \
    DEBUG_PRINT(DEBUG_LEVEL_ERROR, DEBUG_MODULE_NAMES[module], format, ##__VA_ARGS__)
#else
#define DEBUG_ERROR(module, format, ...) ((void)0)
#endif

#if DEBUG_LEVEL >= DEBUG_LEVEL_WARN
#define DEBUG_WARN(module, format, ...) \
    DEBUG_PRINT(DEBUG_LEVEL_WARN, DEBUG_MODULE_NAMES[module], format, ##__VA_ARGS__)
#else
#define DEBUG_WARN(module, format, ...) ((void)0)
#endif

#if DEBUG_LEVEL >= DEBUG_LEVEL_INFO
#define DEBUG_INFO(module, format, ...) \
    DEBUG_PRINT(DEBUG_LEVEL_INFO, DEBUG_MODULE_NAMES[module], format, ##__VA_ARGS__)
#else
#define DEBUG_INFO(module, format, ...) ((void)0)
#endif

#if DEBUG_LEVEL >= DEBUG_LEVEL_DEBUG
#define DEBUG_DEBUG(module, format, ...) \
    DEBUG_PRINT(DEBUG_LEVEL_DEBUG, DEBUG_MODULE_NAMES[module], format, ##__VA_ARGS__)
#else
#define DEBUG_DEBUG(module, format, ...) ((void)0)
#endif

#if DEBUG_LEVEL >= DEBUG_LEVEL_TRACE
#define DEBUG_TRACE(module, format, ...) \
    DEBUG_PRINT(DEBUG_LEVEL_TRACE, DEBUG_MODULE_NAMES[module], format, ##__VA_ARGS__)
#else
#define DEBUG_TRACE(module, format, ...) ((void)0)
#endif

/* Internal functions */
int  debug_should_print(debug_level_t level, const char *module);
void debug_print(debug_level_t level, const char *module, const char *file, int line,
                 const char *format, ...);

#endif /* DEBUG_H */