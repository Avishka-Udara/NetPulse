#ifndef NETPULSE_CORE_H
#define NETPULSE_CORE_H
#include <stdint.h>
#include <time.h>
typedef struct {
    int peak_start, peak_end, reset_day, reset_minute;
    double peak_gb, offpeak_gb;
    int position, offset;
    int widget_width, font_size, theme, transparent, show_totals, tray_only;
    uint64_t adapter;
} Config;
typedef struct { uint64_t down[2], up[2]; } Usage;
uint64_t proportional_bytes(uint64_t bytes,uint64_t part,uint64_t total);
void config_defaults(Config *c);
int parse_clock(const char *s, int *minute);
int is_peak(const Config *c, int minute);
time_t cycle_start(const Config *c, time_t now);
time_t cycle_next(const Config *c, time_t now);
/* Local wall-clock time controls plan boundaries. Byte splits preserve totals. */
void usage_split(const Config *c, time_t start, time_t end,
                 uint64_t down, uint64_t up, Usage *out);
/* False signals a reset; outputs still retain the unaffected direction's delta. */
int counter_delta(uint64_t old_down, uint64_t old_up, uint64_t down,
                  uint64_t up, uint64_t *delta_down, uint64_t *delta_up);
#endif
