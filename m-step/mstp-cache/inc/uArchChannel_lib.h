#ifndef U_ARCH_CHANNEL_LIB_H
#define U_ARCH_CHANNEL_LIB_H

#include <stdint.h>

#define SYSTICK_AVAILABLE

//------------------------------------------------------------------------------
// I_CACHE Configurations STM32L552
//------------------------------------------------------------------------------
#define STM32L552

#define I_CACHE_SIZE 8192
#define I_CACHE_LINE_SIZE 16
#define I_N_WAYS 2
#define I_N_CACHE_SETS 256
#define I_REPLACEMENT_POLICY PLRU

//------------------------------------------------------------------------------
// Prime+Probe Functions
//------------------------------------------------------------------------------
void prime();
void prime_2();

#ifdef SYSTICK_AVAILABLE
  uint32_t probe_all();
  uint32_t probe_line(uint8_t line, uint8_t way);
#else
  uint32_t probe_all_s();
  uint32_t probe_line_s(uint8_t line, uint8_t way);
#endif

//------------------------------------------------------------------------------
// Trojan Functions
//------------------------------------------------------------------------------
// touch_s uses the same block of jumps of the prime 1
void touch_s(uint8_t secret);
// touch_ns uses the same block of jumps of the prime 2
void touch_ns(uint8_t secret);
//------------------------------------------------------------------------------

#endif
