#ifndef __GPS_HEADER_H__
#define __GPS_HEADER_H__

#include "app_config.h"

typedef union { double gps_timestamp; uint32_t timestamp_parts[2]; } gps_timestamp_t;

void gps_task(void *args);
gps_timestamp_t gps_request_timestamp(void);
void gps_get_llh(float *lat, float *lon, float *ht);

#endif  // __GPS_HEADER_H__
