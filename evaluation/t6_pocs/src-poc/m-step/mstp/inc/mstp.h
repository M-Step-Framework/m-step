 #ifndef INC_M_STEP_H_
 #define INC_M_STEP_H_
 
    #include "main.h"
    #include <stdint.h>
    #include <stdio.h>
    
    // Number of instructions to detect a zero-step
    #define STREAK_THRESHOLD    20

    // Max number of ICI instructions to detect ICI with PC on the reglist. A 
    // value greater than this means that we are executing a ICI instruction with
    // PC on the reglist on a loop. No progress is made.
    #define ICI_STREAK_THRESHOLD 4

    // Max number of preemptions in a row before we try another base clk
    #define PREEMPTIONS_THRESHOLD 5

    //--------------------------------------------------------------------------
    // M-Step configuration (TIM3_IRQHandler cached)
    //
    // Base configuration assuming 0 wait clocks latency, i.e., either all things
    // are cacheable or the frequency is low enough to not have any wait clocks.
    //--------------------------------------------------------------------------
    // Time to land an interrupt first instruction
    #define MIN_BASE_CLK 5
    #define MIN_BASE_CLK_CONTENTION 18

    // Clk offsets relative to BASE_CLK
    #define S_STACK_CLK_OFFSET 9
    #define S_STACK_FPU_CLK_OFFSET_NS 18 
    #define S_STACK_FPU_CLK_OFFSET_S 26

    // Unstacking + Stacking times
    #define MIN_ISR_TIME_NS 29 
    #define MIN_ISR_TIME_S  47
    #define MIN_ISR_FPU_S   64

    // Time of the "shortest" instruction e.g., mov r0, r1
    #define BASE_INST_TIME  1

    //--------------------------------------------------------------------------
    // All NS Non-cacheable 
    // TIM3 Cacheable 
    // S Cacheable 
    //--------------------------------------------------------------------------
    #define ALL_NS_NON_CACHEABLE_TIM3_CACHEABLE_S_CACHEABLE
    #ifdef ALL_NS_NON_CACHEABLE_TIM3_CACHEABLE_S_CACHEABLE
        // NS Non-cacheable
        #define BASE_CLK_OFFSET_NS 3
        #define BASE_CLK_CONTENTION_OFFSET_NS 3

        // S Cacheable
        #define BASE_CLK_OFFSET_S 0
        #define BASE_CLK_CONTENTION_OFFSET_S 0

        // NS Non-cacheable -> Unstacking + Stacking times 
        #define BASE_ISR_TIME_OFFSET_NS 3

        // NS Non-cacheable -> Unstacking + Stacking times 
        #define BASE_ISR_TIME_OFFSET_S 0
    #else
        // NS cacheable
        #define BASE_CLK_OFFSET_NS 0
        #define BASE_CLK_CONTENTION_OFFSET_NS 0

        // S Cacheable
        #define BASE_CLK_OFFSET_S 0
        #define BASE_CLK_CONTENTION_OFFSET_S 0

        // NS Cacheable -> Unstacking + Stacking times 
        #define BASE_ISR_TIME_OFFSET_NS 0

        // NS Non-cacheable -> Unstacking + Stacking times 
        #define BASE_ISR_TIME_OFFSET_S 0
    #endif

    //--------------------------------------------------------------------------
    // NS M-Step configuration 
    //--------------------------------------------------------------------------
    // Time to land an interrupt first instruction
    #define BASE_CLK_NS             MIN_BASE_CLK + BASE_CLK_OFFSET_NS
    #define BASE_CLK_CONTENTION_NS  MIN_BASE_CLK_CONTENTION + BASE_CLK_CONTENTION_OFFSET_NS

    // Unstacking + Stacking times
    #define BASE_ISR_TIME_NS    MIN_ISR_TIME_NS + BASE_ISR_TIME_OFFSET_NS

    //--------------------------------------------------------------------------
    // NS M-Step configuration 
    //--------------------------------------------------------------------------
    // Time to land an interrupt first instruction
    #define BASE_CLK_S             MIN_BASE_CLK + BASE_CLK_OFFSET_S
    #define BASE_CLK_CONTENTION_S  MIN_BASE_CLK_CONTENTION + BASE_CLK_CONTENTION_OFFSET_S

    // Unstacking + Stacking times
    #define BASE_ISR_TIME_S     MIN_ISR_TIME_S
    #define BASE_ISR_FPU_S      MIN_ISR_FPU_S
    //--------------------------------------------------------------------------

    #define S_STACK        1
    #define EXTENDED_STACK 0

    #define VERBOSE_METRICS

    #define DEBUG_OFF   0
    #define DEBUG_ON    1

    // Single toggle for all mstp configs. Default is debug:
    //   DEBUG_ON  -> debug trace (PC-based, zero-steps dropped; analyze with no flag) [default]
    //   DEBUG_OFF -> raw production trace (zero-steps kept; analyze with -r)
    #ifndef MSTP_DEBUG
        #define MSTP_DEBUG  DEBUG_ON
    #endif
    
    // The NS is non-cacheable, and the M-Step is not tunning yet to deal with 
    // non-cacheable victim code (not all edge cases are covered). Our victims
    // are tunning in cacheable memory on the S world, so by default we have the
    // logging of M-Step metrics of the NS world code disabled. 
    // #define NS_METRICS

    void trace(void (*victim)());
 
    typedef void (*state_func)();
    
    typedef enum {
        ATOMIC_INST,
        ICI_RESUME,
        TRY_SINGLE_STEP,
        ICI_RESTART,
        PREEMPTION
    } state_t;
    
    typedef struct {
        uint32_t base_ISR_time;
        uint32_t base_clk;
        uint32_t base_clk_2_collide;
        uint32_t streak_threshold;
        uint32_t ici_streak_threshold;
        uint32_t preemptions_threshold;
        uint32_t base_inst_time;
        uint8_t  debug;
    } mstp_conf_t;
    
    typedef struct {
        uint32_t streak; 
        uint32_t streak_mul_clks; 
        int32_t inst_time;
        int32_t inst_time_prev;
        uint32_t partial_step;
        uint32_t clk_it;
        uint32_t clk_2_collide;
        uint32_t stack_clk_offset;
        uint32_t ici_restart_clk_it;
        uint32_t ici_resume_pc_clk_it;
        uint32_t secure_stack_prev;
        uint32_t streak_overflow;
        uint32_t mul_clk_streak_overflow;
        uint32_t partial_step_saved;
        uint32_t ici_streak;
        uint32_t lazy_stacking_en;
        uint32_t preemptions;
        state_t  state_n;
        state_t  state_p;
    } mstp_ctx_t;

    typedef struct { 
        uint32_t secure_stack;
        uint32_t DCRS;
        uint32_t FTYpe;
        uint32_t Mode;
        uint32_t SPSEL;
        uint32_t ES;
    } lr_t;
    
 #endif /* INC_M_STEP_H_ */
 