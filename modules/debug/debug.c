#include "debug.h"

#include <stdarg.h>
#include <stdlib.h>

static struct
{
    debug_level_t level;
    int           enable_color;
    int           show_timestamp;
    const char   *module_filter;
} debug_config = {.level          = DEBUG_LEVEL_INFO,
                  .enable_color   = DEBUG_COLORS,
                  .show_timestamp = DEBUG_TIMESTAMP,
                  .module_filter  = NULL};

/* Color codes for terminal */
#define COLOR_BRIGHT_RED "\x1b[91m"
#define COLOR_BRIGHT_GREEN "\x1b[92m"
#define COLOR_BRIGHT_YELLOW "\x1b[93m"
#define COLOR_BRIGHT_BLUE "\x1b[94m"
#define COLOR_BRIGHT_MAGENTA "\x1b[95m"
#define COLOR_BRIGHT_CYAN "\x1b[96m"
#define COLOR_BRIGHT_WHITE "\x1b[97m"
#define COLOR_ORANGE "\x1b[38;5;214m"
#define COLOR_PURPLE "\x1b[38;5;129m"
#define COLOR_RESET "\x1b[0m"

void debug_init(void)
{
    debug_config.level          = DEBUG_LEVEL;
    debug_config.enable_color   = DEBUG_COLORS;
    debug_config.show_timestamp = DEBUG_TIMESTAMP;

    DEBUG_INFO(MODULE_MAIN, "Debug system initialized (Build: %s, Level: %d)", BUILD_TYPE,
               debug_config.level);
}

void debug_set_level(debug_level_t level)
{
    debug_config.level = level;
    DEBUG_DEBUG(MODULE_MAIN, "Debug level set to %d", level);
}

void debug_set_module_filter(const char *module_name)
{
    debug_config.module_filter = module_name;
    DEBUG_DEBUG(MODULE_MAIN, "Module filter set to: %s", module_name ? module_name : "NULL");
}

debug_level_t debug_get_level(void)
{
    return debug_config.level;
}

const char *debug_get_build_type(void)
{
    return BUILD_TYPE;
}

int debug_should_print(debug_level_t level, const char *module)
{
    /* Check if level is enabled */
    if (level > debug_config.level)
    {
        return 0;
    }

    /* Check module filter */
    if (debug_config.module_filter != NULL && strcmp(module, debug_config.module_filter) != 0)
    {
        return 0;
    }

    return 1;
}
void debug_print(debug_level_t level, const char *module, const char *file, int line,
                 const char *format, ...)
{
    /* Level strings and colors */
    char    message[512];  // Fixed buffer size
    va_list args;

    // FIRST: Process the variable arguments into our buffer
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    const char *level_str[] = {[DEBUG_LEVEL_ERROR] = "ERROR",
                               [DEBUG_LEVEL_WARN]  = "WARN",
                               [DEBUG_LEVEL_INFO]  = "INFO",
                               [DEBUG_LEVEL_DEBUG] = "DEBUG",
                               [DEBUG_LEVEL_TRACE] = "TRACE"};

    /* Improved color scheme - brighter and more distinct */
    const char *colors[] = {
        [DEBUG_LEVEL_ERROR] = COLOR_BRIGHT_RED,    // Bright Red
        [DEBUG_LEVEL_WARN]  = COLOR_ORANGE,        // Orange
        [DEBUG_LEVEL_INFO]  = COLOR_BRIGHT_GREEN,  // Bright Green
        [DEBUG_LEVEL_DEBUG] = COLOR_BRIGHT_CYAN,   // Bright Cyan (better than blue)
        [DEBUG_LEVEL_TRACE] = COLOR_PURPLE         // Purple
    };

    /* Timestamp */
    if (debug_config.show_timestamp)
    {
        time_t     now = time(NULL);
        struct tm *t   = localtime(&now);
        if (debug_config.enable_color)
        {
            printf(COLOR_BRIGHT_WHITE "[%02d:%02d:%02d]" COLOR_RESET " ", t->tm_hour, t->tm_min,
                   t->tm_sec);
        }
        else
        {
            printf("[%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);
        }
    }

    /* Color and level */
    if (debug_config.enable_color)
    {
        printf("%s%-5s%s ", colors[level], level_str[level], COLOR_RESET);
    }
    else
    {
        printf("%-5s ", level_str[level]);
    }

    /* Module, file, and line */
    const char *base_file = strrchr(file, '/');
    if (base_file)
        base_file++;
    else
        base_file = file;

    if (debug_config.enable_color)
    {
        printf(COLOR_BRIGHT_WHITE "[%s]" COLOR_RESET " [%s:%d] ", module, base_file, line);
    }
    else
    {
        printf("[%s] [%s:%d] ", module, base_file, line);
    }

    /* Print the pre-formatted message */
    if (debug_config.enable_color)
    {
        printf("%s%s%s", colors[level], message, COLOR_RESET);
    }
    else
    {
        printf("%s", message);
    }

    printf("\n");
    fflush(stdout);
}