#include "mstp.h"
#include "mstp_metrics.h"
#include "mstp_defs.h"
#include "tim.h"

void atomic_inst();
void ici_resume();
void try_single_step();
void ici_restart();
void preemption();

uint32_t volatile   *tim2_CR1   = (uint32_t *) 0x40000000,
                    *tim2_SR    = (uint32_t *) 0x40000010,
                    *tim2_DIER  = (uint32_t *) 0x4000000C,
                    *tim2_EGR   = (uint32_t *) 0x40000014,
                    *tim2_CNT   = (uint32_t *) 0x40000024,
                    *tim2_ARR   = (uint32_t *) 0x4000002C,
                    *tim2_CCR1  = (uint32_t *) 0x40000034;

uint32_t volatile   *tim3_CR1   = (uint32_t *)  0x40000400,
                    *tim3_SR    = (uint32_t *)  0x40000410,
                    *tim3_DIER  = (uint32_t *)  0x4000040C,
                    *tim3_EGR   = (uint32_t *)  0x40000414,
                    *tim3_CNT   = (uint32_t *)  0x40000424,
                    *tim3_ARR   = (uint32_t *)  0x4000042C,
                    *tim3_CCR1  = (uint32_t *)  0x40000434;

uint32_t volatile   *tim7_CR1   = (uint32_t *)  0x40001400,
                    *tim7_SR    = (uint32_t *)  0x40001410,
                    *tim7_DIER  = (uint32_t *)  0x4000140C,
                    *tim7_EGR   = (uint32_t *)  0x40001414,
                    *tim7_CNT   = (uint32_t *)  0x40001424,
                    *tim7_ARR   = (uint32_t *)  0x4000142C,
                    *tim7_CCR1  = (uint32_t *)  0x40001434;

state_func state_functions[5] = {   atomic_inst, 
                                    ici_resume, 
                                    try_single_step, 
                                    ici_restart, 
                                    preemption
                                };

mstp_conf_t mstp_conf[2] = {
    /* NS configuration [0] */
    {
        .base_ISR_time          = BASE_ISR_TIME_NS,
        .base_clk               = BASE_CLK_NS,
        .base_clk_2_collide     = BASE_CLK_CONTENTION_NS,
        .streak_threshold       = STREAK_THRESHOLD,
        .ici_streak_threshold   = ICI_STREAK_THRESHOLD,
        .preemptions_threshold  = PREEMPTIONS_THRESHOLD,
        .base_inst_time         = BASE_INST_TIME,   
        .debug                  = MSTP_DEBUG,
    },
    /* S configuration [1] */
    {
        .base_ISR_time          = BASE_ISR_TIME_S,
        .base_clk               = BASE_CLK_S,
        .base_clk_2_collide     = BASE_CLK_CONTENTION_S,
        .streak_threshold       = STREAK_THRESHOLD,
        .ici_streak_threshold   = ICI_STREAK_THRESHOLD,
        .preemptions_threshold  = PREEMPTIONS_THRESHOLD,
        .base_inst_time         = BASE_INST_TIME,   
        .debug                  = MSTP_DEBUG,
    },
    /* S configuration FPU [2] */
    {
        .base_ISR_time          = BASE_ISR_FPU_S,
        .base_clk               = BASE_CLK_S,
        .base_clk_2_collide     = BASE_CLK_CONTENTION_S,
        .streak_threshold       = STREAK_THRESHOLD,
        .ici_streak_threshold   = ICI_STREAK_THRESHOLD,
        .preemptions_threshold  = PREEMPTIONS_THRESHOLD,
        .base_inst_time         = BASE_INST_TIME,   
        .debug                  = MSTP_DEBUG,
    }
};

#define NS_CONF     0
#define S_CONF      1
#define S_CONF_FPU  2

uint8_t mstp_conf_index = NS_CONF;

mstp_ctx_t mstp_ctx = {
    .streak                     = 0,
    .streak_mul_clks            = 0,
    .inst_time                  = 0,
    .inst_time_prev             = 0,
    .partial_step               = 0,
    .clk_it                     = 30,
    .clk_2_collide              = 0,
    .stack_clk_offset           = 0,
    .ici_restart_clk_it         = 0,
    .state_n                    = ATOMIC_INST,
    .state_p                    = ATOMIC_INST,
    .secure_stack_prev          = 0,
    .streak_overflow            = 0,
    .mul_clk_streak_overflow    = 0,
    .partial_step_saved         = 0,
    .ici_streak                 = 0,
    .lazy_stacking_en           = 0,
    .preemptions                = 0,
};

// This should also go to a structure representing LR 
lr_t lr_exc_return = {
    .secure_stack   = 0,
    .DCRS           = 0,
    .FTYpe          = 0,
    .Mode           = 0,
    .SPSEL          = 0,
    .ES             = 0,
};

// Is used in assembly code not put in structure
uint32_t lr_saved;

uint32_t volatile src           = 0x81;
uint32_t volatile dst           = 0;  
uint32_t volatile end           = 0;

// just for debugging purposes
register uint32_t sp            __asm("sp");

extern uint32_t stacked_pc_prev;
uint32_t prev = 0;

uint32_t count_its = 0;

__attribute__((optimize(0)))
__attribute__((used))
void single_step(void){
    //--------------------------------------------------------------------------
    // Analyze the LR EXC_RETURN Value
    //--------------------------------------------------------------------------
    // EXC_RETURN -> Definitive Guide to Arm Cortex-M23/M33 Processors, pag.354
    lr_exc_return.secure_stack    = ((lr_saved&0x7f)>>6)&0x1;
    lr_exc_return.DCRS            = ((lr_saved&0x7f)>>5)&0x1;
    lr_exc_return.FTYpe           = ((lr_saved&0x7f)>>4)&0x1;
    lr_exc_return.Mode            = ((lr_saved&0x7f)>>3)&0x1;
    lr_exc_return.SPSEL           = ((lr_saved&0x7f)>>2)&0x1;
    lr_exc_return.ES              = ((lr_saved&0x7f)>>0)&0x1;

    // Check if the secure code finish its execution and we are back to NS.
    if(mstp_ctx.secure_stack_prev == S_STACK && lr_exc_return.secure_stack != S_STACK){
        // mstp_conf.base_ISR_time = BASE_ISR_TIME;
        mstp_conf_index = NS_CONF; 
    }

    // Check if we are switching to secure code
    if(mstp_ctx.secure_stack_prev != S_STACK && lr_exc_return.secure_stack == S_STACK){
        mstp_conf_index = S_CONF; 
    }

    mstp_ctx.inst_time = *tim3_CNT - mstp_conf[mstp_conf_index].base_ISR_time;

    // mstp_ctx.inst_time < mstp_conf.base_inst_time
    if( mstp_ctx.inst_time == 0){
        mstp_ctx.inst_time = 1;
        mstp_conf[mstp_conf_index].base_ISR_time = mstp_conf[mstp_conf_index].base_ISR_time-1;
        // #ifdef VERBOSE_METRICS
        //     if(stacked_pc_prev != prev)
        //         printf("Zero: 0x%08x\r\n", stacked_pc_prev);
        // #endif
        prev = stacked_pc_prev;
    }

    //--------------------------------------------------------------------------
    // Single Step Logic (Algorithm to adjust next IT)
    //--------------------------------------------------------------------------
    state_functions[mstp_ctx.state_n]();
    
    //--------------------------------------------------------------------------
    // Adjust clk to interrupt and to collide based on stack frame size
    // - Shift interrupt clock to count for bigger stack frames (All regs + FPU)
    //--------------------------------------------------------------------------
    mstp_ctx.stack_clk_offset = 0;
    mstp_conf_index = NS_CONF; 
    mstp_conf[mstp_conf_index].base_ISR_time = BASE_ISR_TIME_NS;
    // FPU enable with lazy_stacking_en, if we don't use the FPU in this ISR,
    // the time is the same as the normal stack frame. 
    if(lr_exc_return.secure_stack == S_STACK){
        mstp_ctx.stack_clk_offset += S_STACK_CLK_OFFSET;
        mstp_conf_index = S_CONF; 
        mstp_conf[mstp_conf_index].base_ISR_time = BASE_ISR_TIME_S;
    }
    // Full stack frame with FPU
    if(lr_exc_return.FTYpe == EXTENDED_STACK && !mstp_ctx.lazy_stacking_en){
        mstp_ctx.stack_clk_offset += S_STACK_FPU_CLK_OFFSET_S;
        mstp_conf_index = S_CONF_FPU; 
        mstp_conf[mstp_conf_index].base_ISR_time = BASE_ISR_FPU_S;
    } 
    //--------------------------------------------------------------------------
    // Config Next It
    //--------------------------------------------------------------------------
    *tim3_SR  = 0; // Clean interrupt
    *tim3_CNT = 0;
    *tim3_CCR1 = (uint32_t)( (mstp_ctx.clk_it + mstp_ctx.stack_clk_offset) - 1); // Compare value

    //--------------------------------------------------------------------------
    // Config BUSted Gadget to amplify the ldr latency
    // This will be fired in assembly on the wrap code of this function to 
    // to increase precision 
    //--------------------------------------------------------------------------
    *tim2_CR1 &= ~(1<<0);
    *tim2_CNT = 0 - (mstp_ctx.clk_2_collide + mstp_ctx.stack_clk_offset);

    //--------------------------------------------------------------------------
    // Single Step Metrics 
    //--------------------------------------------------------------------------
    if(mstp_conf[mstp_conf_index].debug)
        process_metrics_debug((uint32_t *)sp, &mstp_ctx, &lr_exc_return);
    else
        process_metrics_production((uint32_t *)sp, &mstp_ctx, &lr_exc_return);
    
    //--------------------------------------------------------------------------
    // Check if we reach the end of our trace. We can compare it to the PC on
    // the stack to detect the end of the trace. We can do this because the the 
    // marker is on the M-Step memory space.
    // If we fint the marker, we won't fire next it. We have to do this because
    // even if we try to disable the timer on the interrupted code, it would be
    // ignored because the interrupt is already pending it will be taken anyways
    // and the timer will be enabled again here. 
    // So we have to detect the end and disable the timer interrupt to end the
    // single step.
    //--------------------------------------------------------------------------
    if (end) {
        *tim3_DIER &= ~(1<<1); // Disable the CC interrupt
        end = 0; // Clean end flag
    }

    mstp_ctx.secure_stack_prev = lr_exc_return.secure_stack;

}

extern uint32_t __cacheable_text_start__;
extern uint32_t __cacheable_text_end__;

#define REGION_NON_CACHEABLE 0UL
#define REGION_CACHEABLE     1UL

void setup_ns_mpu(void) {
    uint32_t base_cacheable  = (uint32_t)&__cacheable_text_start__;

    ARM_MPU_Disable();

    // REGION 0: Mark entire NS memory as Strongly Ordered Non-cacheable
    ARM_MPU_SetMemAttr(REGION_NON_CACHEABLE, ARM_MPU_ATTR_DEVICE_nGnRnE);

    MPU->RNR  = REGION_NON_CACHEABLE;
    MPU->RBAR = 0x8041400;
    MPU->RLAR = (base_cacheable - 1) | (REGION_NON_CACHEABLE << MPU_RLAR_AttrIndx_Pos) | MPU_RLAR_EN_Msk;

    ARM_MPU_Enable(MPU_CTRL_PRIVDEFENA_Msk); // No default memory map
}

__attribute__((optimize(0))) 
void trace(void (*victim)())
{
    //--------------------------------------------------------------------------
    // BUSTED GADGET -> Used to amplify the ldr latency
    //--------------------------------------------------------------------------
    int auto_reload = 1;
    int n_collisions = 1;

    //FPCCR, Floating-Point Context Control Register
    uint32_t *FPCCR = (uint32_t *) 0xE000EF34, lazy_stacking_en = 0;
    mstp_ctx.lazy_stacking_en = ((*FPCCR)>>30)&0x1; // Lazy stacking enabled?

    #ifdef NON_CACHEABLE
        setup_ns_mpu();
    #endif

    *tim2_CR1 &= ~(1<<0);
    *tim2_ARR = auto_reload; 
    *tim2_EGR |= 1<<0; // update preload regs
    *tim2_CNT = (auto_reload-1) - mstp_ctx.clk_2_collide; 
    *tim2_SR = 0; 
    *tim2_DIER = 0;
     
    // HAL_DMA_Start(&hdma_tim2_up, (uint32_t)&src, (uint32_t)tim7_CR1, n_collisions);
    HAL_DMA_Start(&hdma_tim2_up, (uint32_t)&src, (uint32_t)&dst, n_collisions);
    *tim2_DIER |= 1<<8;
    *tim2_CR1 |= (1<<2); //Only counter overflow generates event
    *tim2_CR1 |= (1<<3); //One pulse mode
    *tim2_CR1 |= (1<<0); // EN
    //--------------------------------------------------------------------------

    //--------------------------------------------------------------------------
    // M-Step GADGET -> Used to generate the single-step interrupt
    //--------------------------------------------------------------------------
    mstp_ctx.clk_it = mstp_conf[mstp_conf_index].base_clk;
    *tim3_CNT = 0;
    *tim3_SR = 0; // Clean update event generated by the timer initial config
    *tim3_DIER = 1<<1; 	 // CC interrupt enable
    *tim3_CCR1 = (uint32_t)((mstp_ctx.clk_it) - 1);
    *tim3_ARR = -1; // Max ARR
    
    // Fire 1st interrupt
    // *tim3_CR1 |= (1<<0); // EN    
    //--------------------------------------------------------------------------
    // Delay to land the interrupt in the first instruction of the victim
    MY_NOP(); 
    MY_NOP(); 
    MY_NOP(); 
    MY_NOP(); 
    victim();
    end = 1;
    //--------------------------------------------------------------------------
    // Wait for the trace to finish
    while(end);
    // Disable the timer
    *tim3_CR1 &= ~(1<<0); // EN
}
 
void atomic_inst(){
    mstp_ctx.clk_it = mstp_conf[mstp_conf_index].base_clk;
    mstp_ctx.clk_2_collide = mstp_conf[mstp_conf_index].base_clk_2_collide;
    // Count only interrupts generated in this state (Or the special case of
    // preemption)
    if(mstp_ctx.state_p == ATOMIC_INST){
        // Negative time means we are preempting ourselves. 
        if(mstp_ctx.inst_time == mstp_conf[mstp_conf_index].base_inst_time){
            mstp_ctx.streak++;
            mstp_ctx.streak_mul_clks = 0;
        }
        else { // Clean once we break the streak
            
            // Check if we are zero-stepping but with a instruction time bigger
            // than the base instruction time, i.e., 1 clk. This is a edge case
            // of some isntructions, e.g., ldr.w pc, which takes sucessivly 2 clks
            // but gets no progress. So we have to count it as a zero-step and 
            // go to the special states to handle long the streak of zero-steps.
            if(mstp_ctx.inst_time_prev == mstp_ctx.inst_time)
                mstp_ctx.streak_mul_clks++;

            mstp_ctx.streak = 0;
            
            // Last partial step of a ICI resume instruction. There is to options
            // that trigger this condition:
            // 1. We are on a normal ICI resume instruction and we did last partial
            //    and advance to the next instruction.
            // 2. We are on a ICI resume instruction with PC on the reglist and we
            //    did a partial step, but we are not making any progress. 
            // We have to count the number of ICI instructions in a row. If there
            // is to many, we are executing in a loop a ICI resume instruction
            // with PC on the reglist.
            if(mstp_ctx.streak_overflow){
                mstp_ctx.streak_overflow = 0;
                mstp_ctx.ici_streak++;
                if(mstp_ctx.ici_streak >= mstp_conf[mstp_conf_index].ici_streak_threshold){
                    // store the number of partial steps we did on the ICI resume
                    // in order to in the next time to do the last partial step
                    // with base clk + 2.
                    mstp_ctx.partial_step_saved = mstp_ctx.partial_step;     
                }
                mstp_ctx.partial_step = 0;
            } 
            else {
                // We are no more executing ICI resume instruction, so we can 
                // clean the ici_streak count.
                mstp_ctx.partial_step_saved = 0;
                mstp_ctx.ici_streak = 0;
            }

            mstp_ctx.ici_restart_clk_it = 0;
        }

        mstp_ctx.preemptions = 0;
        mstp_ctx.inst_time_prev = mstp_ctx.inst_time;
    }

    // End condition of a ICI_RESTART. There was a streak overflow, i.e., we were
    // zero-stepping, we just came from a ICI_RESTART state and now the instruction
    // time is bigger than the base instruction time
    if((mstp_ctx.state_p == ICI_RESTART) && 
       (mstp_ctx.inst_time > mstp_conf[mstp_conf_index].base_inst_time) &&
       mstp_ctx.mul_clk_streak_overflow ){
        mstp_ctx.ici_restart_clk_it = 0;
        mstp_ctx.streak_overflow = 0;
    }

    mstp_ctx.mul_clk_streak_overflow = 0;

    // Detect Zero-Step, long streak of instructions with no variation on
    // the execution time, i.e., stucked on the same instruction/execution stage.
    if(mstp_ctx.streak >= mstp_conf[mstp_conf_index].streak_threshold){
        mstp_ctx.streak = 0;
        mstp_ctx.state_n = ICI_RESUME;
        mstp_ctx.streak_overflow = 1;
    } 

  // Detect Zero-Step, long streak of instructions with no variation on
    // the execution time, i.e., stucked on the same instruction/execution stage.
    if(mstp_ctx.streak_mul_clks >= mstp_conf[mstp_conf_index].streak_threshold){
        mstp_ctx.streak_mul_clks = 0;
        mstp_ctx.state_n = ICI_RESTART;
        mstp_ctx.mul_clk_streak_overflow = 1;
    } 

    // Store Previous state
    mstp_ctx.state_p = ATOMIC_INST;
}
 
void ici_resume(){  
    // Remove clock bias due to the increment of the clk_it. We want to know the 
    // real instruction time. If we don't correct for the change in clk_it, we 
    // will see a fake increase in the instruction time. 
    if(mstp_ctx.clk_it > mstp_conf[mstp_conf_index].base_clk)
        mstp_ctx.inst_time = mstp_ctx.inst_time - (mstp_ctx.clk_it - mstp_conf[mstp_conf_index].base_clk);

    // If we tried a partial step and there was no progress, i.e., we are still
    // keeping with the streak, this is a ICI restart instruction
    if(mstp_ctx.inst_time <= mstp_conf[mstp_conf_index].base_inst_time && mstp_ctx.state_p == ICI_RESUME){ // i.e., we tried a partial step and there was no progress
        mstp_ctx.partial_step = 0;
        // Reset Clocks
        mstp_ctx.clk_it = mstp_conf[mstp_conf_index].base_clk;
        mstp_ctx.clk_2_collide = mstp_conf[mstp_conf_index].base_clk_2_collide;
        mstp_ctx.state_n = TRY_SINGLE_STEP;
    }
    else 
    // We don't know if this is the last partial step, so we have to assume it is
    // and we will check again the maximum streak. This is very time consuming, 
    // but it is the only way to guarantee that we are not stucked. And while
    // this could take a lot of time, ICI instructions are not that common.
    //
    // Only partial step if the instruction time is bigger than the base time but
    // and the previous instruction time is different, otherwise we went here
    // because we were zero-stepping on a instuction with time bigger than the
    // base time, so we have to try other clk_it
    if(mstp_ctx.inst_time > mstp_conf[mstp_conf_index].base_inst_time && 
       mstp_ctx.inst_time_prev != mstp_ctx.inst_time   ) { // i.e., partial step
        mstp_ctx.state_n = ATOMIC_INST;
        // We need to call here the next stage, to avoid one more it at clk + 1
    	atomic_inst();
    	return;
    } else  {
        mstp_ctx.clk_it++;

        // When we are in ICI resume, the time to return to the victim takes 1 
        // clock cycle more, because the CPU needs to do the context restore, so
        // we need to delay one clock the contention when we are doing partial
        // steps. 
        mstp_ctx.clk_2_collide++;
        mstp_ctx.partial_step++;
        
        // Check if we are doing partial steps on ICI resume, but we aren't single
        // stepping. This is a edge case of the ICI resume instruction when the PC
        // is on the reglist. e.g., pop {r0, r1, r2, r3, r4, r5, pc}
        // So if we have detected that the ici_streak_treshold was reached, it means
        // that the partial_step_saved holds some number indicating the number of
        // partial steps, so we just have now to try the last step with the base clk + 3
        if (mstp_ctx.partial_step_saved && (mstp_ctx.partial_step_saved == mstp_ctx.partial_step)){
            mstp_ctx.clk_it = mstp_conf[mstp_conf_index].base_clk + 3;
            mstp_ctx.clk_2_collide = mstp_conf[mstp_conf_index].base_clk_2_collide + 3;
            mstp_ctx.partial_step = 0;
            mstp_ctx.partial_step_saved = 0;
            mstp_ctx.ici_streak = 0;
            mstp_ctx.state_n = ATOMIC_INST;
        } 
    }

    // Store Previous state
    mstp_ctx.state_p = ICI_RESUME;
    mstp_ctx.inst_time_prev = mstp_ctx.inst_time;
}

// This is needed due to an edge case where the last partial-step of this 
// instruction ldmia.w sp!, {r7, lr} was 1. I didn't figured exactly why, but to 
// accommodate that, I'm doing an extra step of checking for zero-step between 
// ICI and IR states. If the 1 is due to the last partial-step we won't get a 
// zero-step. If it's due to the IR instruction, we will get a zero step and we 
// can go to IR state
void try_single_step(){
    mstp_ctx.clk_it = mstp_conf[mstp_conf_index].base_clk;
    mstp_ctx.clk_2_collide = mstp_conf[mstp_conf_index].base_clk_2_collide;

    // Count only interrupts generated in this state
    if(mstp_ctx.state_p == TRY_SINGLE_STEP){
        // Negative time means we are preempting ourselves. 
        // if( mstp_ctx.inst_time < mstp_conf[mstp_conf_index].base_inst_time){
        //     mstp_ctx.state_n = PREEMPTION;
        //     mstp_ctx.state_p = TRY_SINGLE_STEP;
        //     return;
        // }
        if(mstp_ctx.inst_time == mstp_conf[mstp_conf_index].base_inst_time){
            mstp_ctx.streak++;
            mstp_ctx.streak_mul_clks = 0;
        }
        else {// Clean once we break the streak
            mstp_ctx.streak = 0;

            if(mstp_ctx.inst_time_prev == mstp_ctx.inst_time)
                mstp_ctx.streak_mul_clks++;

        }

        mstp_ctx.preemptions = 0;
        mstp_ctx.inst_time_prev = mstp_ctx.inst_time;
    }

    // End condition of a multi-cycle zero-step detected. Go out of the loop, 
    // ICI_RESTART -> TRY_SINGLE_STEP -> ICI_RESTART -> TRY_SINGLE_STEP ....
    if(mstp_ctx.inst_time_prev != mstp_ctx.inst_time && mstp_ctx.mul_clk_streak_overflow){
        mstp_ctx.state_n = ATOMIC_INST;
        mstp_ctx.ici_restart_clk_it = 0;
    }
    
    mstp_ctx.mul_clk_streak_overflow = 0;

    // Detect Zero-Step, long streak of instructions with no variation on
    // the execution time, i.e., stucked on the same instruction/execution stage.
    if(mstp_ctx.streak_mul_clks >= mstp_conf[mstp_conf_index].streak_threshold){
        mstp_ctx.streak_mul_clks = 0;
        mstp_ctx.mul_clk_streak_overflow = 1;
        mstp_ctx.state_n = ICI_RESTART;
    }

    // Detect Zero-Step, long streak of instructions with no variation on
    // the execution time, i.e., stucked on the same instruction/execution stage.
    if(mstp_ctx.streak >= mstp_conf[mstp_conf_index].streak_threshold){
        mstp_ctx.streak = 0;
        mstp_ctx.state_n = ICI_RESTART;
    }

    // Store Previous state
    mstp_ctx.state_p = TRY_SINGLE_STEP;
}

void ici_restart(){
    mstp_ctx.clk_2_collide = mstp_conf[mstp_conf_index].base_clk_2_collide;
    if(!mstp_ctx.ici_restart_clk_it)
        // Check next clk_it to the one tested on ICI_RESUME
        mstp_ctx.clk_it++;
    else
        // Restore context 
        mstp_ctx.clk_it = mstp_ctx.ici_restart_clk_it + 1;

    // Save ici_restart context
    mstp_ctx.ici_restart_clk_it = mstp_ctx.clk_it;

    // Check single step, i.e., try to single-step 
    if(mstp_ctx.mul_clk_streak_overflow)
        mstp_ctx.state_n = TRY_SINGLE_STEP;
    else
        mstp_ctx.state_n = ATOMIC_INST;

    // Store Previous state
    mstp_ctx.state_p = ICI_RESTART;
}

// Detect if we got a preemption, and count the amount of preemption in a row
// if it is bigger then the defined threshold, it means we are stucked and 
// and we have to try again with other base clk
void preemption(){
    mstp_ctx.preemptions++;

    // Try with base clk + 1
    if(mstp_ctx.preemptions == mstp_conf[mstp_conf_index].preemptions_threshold){
        mstp_ctx.clk_it = mstp_conf[mstp_conf_index].base_clk + 1;
        mstp_ctx.clk_2_collide = mstp_conf[mstp_conf_index].base_clk_2_collide + 1;
    }

    // We already tried with base clk + 1, if that doesn't work to put out 
    // of preemption, do +2, +3, +4...which is the difference between the 
    // preemptions and preemptions_threshold
    if(mstp_ctx.preemptions > mstp_conf[mstp_conf_index].preemptions_threshold) {
        uint32_t diff = mstp_ctx.preemptions - mstp_conf[mstp_conf_index].preemptions_threshold;
        mstp_ctx.clk_it = mstp_conf[mstp_conf_index].base_clk + diff;
        mstp_ctx.clk_2_collide = mstp_conf[mstp_conf_index].base_clk_2_collide + diff;
    } 

    // mstp_ctx.state_n = ATOMIC_INST;
    // Return to the state that invoked the preemption
    mstp_ctx.state_n = mstp_ctx.state_p;

    // Store Previous state
    mstp_ctx.state_p = PREEMPTION;
}
