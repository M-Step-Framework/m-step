#ifndef INC_M_STEP_METRICS_H_
#define INC_M_STEP_METRICS_H_

#include <stdint.h>
#include "mstp.h"
#include "armv8_decop_lib.h"

void process_metrics_debug(uint32_t, mstp_ctx_t *, lr_t *);
void process_metrics_production(uint32_t, mstp_ctx_t *, lr_t *);
void print_metrics();

#endif /* INC_M_STEP_H_ */
