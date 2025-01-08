#include <esp_log.h>

#define printe(format, ...) ESP_LOG_LEVEL_LOCAL(ESP_LOG_ERROR, "CivicAlert", format, ##__VA_ARGS__)
#define printw(format, ...) ESP_LOG_LEVEL_LOCAL(ESP_LOG_WARN, "CivicAlert", format, ##__VA_ARGS__)
#define print(format, ...) ESP_LOG_LEVEL_LOCAL(ESP_LOG_INFO, "CivicAlert", format, ##__VA_ARGS__)
#define printd(format, ...) ESP_LOG_LEVEL_LOCAL(ESP_LOG_DEBUG, "CivicAlert", format, ##__VA_ARGS__)
#define printv(format, ...) ESP_LOG_LEVEL_LOCAL(ESP_LOG_VERBOSE, "CivicAlert", format, ##__VA_ARGS__)
