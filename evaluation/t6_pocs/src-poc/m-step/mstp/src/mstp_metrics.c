#include "mstp_metrics.h"

#define ZERO_BYTES  0
#define TWO_BYTES   2
#define FOUR_BYTES  4

uint32_t zero_steps = 0;
uint32_t partial_steps = 0;
uint32_t single_steps = 0;
uint32_t instruction_folding = 0;
uint32_t multi_steps = 0;
uint32_t stacked_pc_prev = 0;
uint32_t interrupts = 0;
uint32_t pc_chaning_inst = 0;

struct metrics_t  // This is not used
{
    uint32_t inst;
    uint32_t pc;
};

#define MAX_METRICS 10000
struct metrics_t metrics[MAX_METRICS];
uint32_t metric = 0;


void print_mstp_ctx(mstp_ctx_t *mstp_ctx) {
    printf("clk_it:         %d\r\n", mstp_ctx->clk_it);
    // printf("clk_2_collide: %d\r\n", mstp_ctx->clk_2_collide);
    printf("partial_step:   %d\r\n", mstp_ctx->partial_step);
    // printf("streak: %d\r\n", mstp_ctx->streak);
    printf("inst_time:      %d\r\n", mstp_ctx->inst_time);
    printf("ici_streak:     %d\r\n", mstp_ctx->ici_streak);
}

void clear_metrics() {
    zero_steps = 0;
    partial_steps = 0;
    single_steps = 0;
    instruction_folding = 0;
    multi_steps = 0;
    stacked_pc_prev = 0;
    interrupts = 0;
    metric = 0;
}

void print_metrics() {
    printf("\r\n");
    printf("Zero Steps    : %d\r\n", zero_steps);
    // printf("Partial Steps : %d\r\n", partial_steps);
    printf("Single Steps  : %d\r\n", single_steps);
    printf("Inst Folding  : %d\r\n", instruction_folding);
    printf("Multi Steps   : %d\r\n", multi_steps);
    printf("Interrupts    : %d\r\n", interrupts);
    for (uint32_t i = 0; i < metric; i++) {
        printf("PC: 0x%08x", metrics[i].pc);
        print_instruction(metrics[i].inst);
        printf("\r\n");
    }
    clear_metrics();
}

void register_metrics(uint32_t inst, uint32_t pc, uint32_t secure_stack) {
	#ifdef NS_METRICS
		if (metric < MAX_METRICS) {
			metrics[metric].inst = inst;
			metrics[metric].pc = pc;
			metric++;
		} else {
			printf("Metrics buffer full!\r\n");
		}
	#else
		if(secure_stack) {
			metrics[metric].inst = inst;
			metrics[metric].pc = pc;
			metric++;
		}
	#endif
}

void print_pc_time(uint32_t pc, mstp_ctx_t *mstp_ctx, char secure_stack) {
	// Just print S PCs
	if(secure_stack)
		printf("%02d\r\n", mstp_ctx->inst_time);
}

//--------------------------------------------------------------------------
// Single Step Metrics Production Mode (raw / unfiltered)
// - Unlike debug mode, we do NOT read the stacked PC to classify and drop
//   zero-steps (that read goes through the secure world). Every step taken on
//   the secure stack is emitted, producing the RAW production trace (zero-steps
//   kept). Analyze it with the raw templates: 1-analyze-trace.sh -r
//--------------------------------------------------------------------------
void process_metrics_production(uint32_t sp, mstp_ctx_t *mstp_ctx, lr_t *lr_exc_return){
	(void)sp;
	if(lr_exc_return->secure_stack == S_STACK)
		printf("%02d\r\n", mstp_ctx->inst_time);
}

//--------------------------------------------------------------------------
// Single Step Metrics
// - Get the Next Instruction PC to see we are single-stepping or not
//--------------------------------------------------------------------------
uint32_t memory_bkpt = 0;
void process_metrics_debug(uint32_t sp, mstp_ctx_t *mstp_ctx, lr_t *lr_exc_return){
	// Get PC on the stack frame
	uint32_t offset = 10;
	uint32_t stacked_pc = (*((uint32_t *) (sp + offset*4))) & ~0x1; // Mask to remove the thumb bit
	uint32_t secure_stack = 0;

    if(lr_exc_return->secure_stack == S_STACK) {
        // We are single-stepping S application, so we need to do a TZ call to 
        // get the PC on the stack frame. When the S world is interrupted, it 
        // will used the S stack frame. Even if we disable all S protections, we
        // are not able to get directly the SP. It is banked between states.
        uint32_t sp_s = GET_SP_S();
        // TFM PSP
        offset = 16; // Full S Stack Frame withouts FPU
        // TZ baremetal MSP
        // offset = 20; // Full S Stack Frame withouts FPU
		stacked_pc = ((uint32_t) READ_MEM_S((uint32_t *) (sp_s + offset*0x4))) & ~0x1; // Mask to remove the thumb bit

		if(lr_exc_return->FTYpe == EXTENDED_STACK && !mstp_ctx->lazy_stacking_en) {
			// We are single-stepping S application, so we need to do a TZ call to 
			// get the PC on the stack frame. When the S world is interrupted, it 
			// will used the S stack frame. Even if we disable all S protections, we
			// are not able to get directly the SP. It is banked between states.
			uint32_t sp_s = GET_SP_S();
			offset = 35; // Full S stack frame with FPU
			stacked_pc = ((uint32_t) READ_MEM_S((uint32_t *) (sp_s + offset*0x4))) & ~0x1; // Mask to remove the thumb bit
		}

		secure_stack = 1;
	} else 
		secure_stack = 0;

	// Check if the secure code finished its execution and we are back to NS. We 
	// have to still do this access through the secure world. Because the previous
	// instruction will be in the S while the next in NS
    if(mstp_ctx->secure_stack_prev == S_STACK && lr_exc_return->secure_stack != S_STACK){
		secure_stack = 1;
    }

    //debug porpuses, breakpoint memory adddress
    if( stacked_pc == memory_bkpt){
        int dummy = 0;
        dummy++;
    }

    // Ignore first It, because we cannot calculate the instruction distance, as
    // no instruction was executed yet
    if(stacked_pc_prev != 0) {
		// Check the zype of step we are doing based on the difference between the
		// consecutive PCs we got on the "m-step".
		switch (stacked_pc - stacked_pc_prev){
			//----------------------------------------------------------------------
			// instruction_distance = 0: We've to check if it's zero-step due to IR or
			// partial-step deu to ICI
			//----------------------------------------------------------------------
			case ZERO_BYTES:
				zero_steps++;
			break;

			//----------------------------------------------------------------------
			// instruction_distance = 2: Single-step
			//----------------------------------------------------------------------
			case TWO_BYTES:
				single_steps++;
				//TODO: Create a MSTP_LOG_VX("...", ...), which will wrap the printf
				// and allow for several levels of verbosity, similar to what
				// tfm has
				#ifdef VERBOSE_METRICS
					print_pc_time(stacked_pc_prev, mstp_ctx, secure_stack);
				#endif
			break;

			//----------------------------------------------------------------------
			// instruction_distance = 4: We have to check if it is single-step,
			// double-step or macro-fusion
			//----------------------------------------------------------------------
			case FOUR_BYTES:
				// Stacked PC is the address ot the next instruction
				uint32_t pc = stacked_pc-FOUR_BYTES;

				//TODO: Create a MSTP_LOG_VX("...", ...), which will wrap the printf
				// and allow for several levels of verbosity, similar to what
				// tfm has
				#ifdef VERBOSE_METRICS
					print_pc_time(stacked_pc_prev, mstp_ctx, secure_stack);
				#endif

				// Check if the executed instruction was a PC changing 
                // instruction, e.g.,  branch, pop pc, or ldr pc
                if(is_pc_chaning_inst(stacked_pc_prev, secure_stack))
                    pc_chaning_inst = 1;

				if(is_opcode_32(pc, secure_stack)) {
					// 32 bits instruction, thus single step
					single_steps++;
				} else {
					// If there was a 16-bit Branch, which jumped 4 bytes, it
					// could have jumped over another 16-bit instruction, so we
					// have to check if the instruction was a PC changing instruction
					// example:
					// 0x08000000: b 0x08000004
					// 0x08000002: mov r0, r1;  
					// 0x08000004: mov r3, r4; 
					if(pc_chaning_inst)
						single_steps++;
					else {
						// 16 bits instruction, increment the PC + 2, to analyze why that
						// instruction  was skipped
						pc = pc + 2;
	
						// Macro-fusion
						switch (decode_opcode(pc, secure_stack))
						{
						// These instructions take 0 clock cycles to execute, and are
						// dual issued (macro-fusion/instruction folding)
						// This is the case when the branches are not taken, so we can
						// see the stacked pc minus 2
						case BLE: case BCS: case BCC: case BMI: case BPL:
						case BVS: case BVC: case BHI: case BLS: case BGE:
						case BLT: case BGT: case BNE: case BEQ:
						case IT:  case NOP: case CPSID:
							instruction_folding++;
							single_steps++;
							break;
						default:
							multi_steps++;
							register_metrics(decode_opcode(pc, secure_stack), pc, secure_stack);
							break;
						}
					}
				}

				pc_chaning_inst = 0;
			break;

			//----------------------------------------------------------------------
			// instruction_distance > 4: If single-step is working as it should, the
			// source of this have to be branch instruction
			//----------------------------------------------------------------------
			default:
			//TODO: Create a MSTP_LOG_VX("...", ...), which will wrap the printf
			// and allow for several levels of verbosity, similar to what
			// tfm has
			#ifdef VERBOSE_METRICS	
				print_pc_time(stacked_pc_prev, mstp_ctx, secure_stack);
			#endif
				
                // Check if the executed instruction was a PC changing 
                // instruction, e.g.,  branch, pop pc, or ldr pc
                if(is_pc_chaning_inst(stacked_pc_prev, secure_stack))
                    pc_chaning_inst = 1;

                // Check if the executed instruction was a branch and was folded
                // together with other instruction (macro-fusion)
                if(is_opcode_32(stacked_pc_prev, secure_stack)){
                    uint32_t pc = stacked_pc_prev+FOUR_BYTES;
                    if(is_pc_chaning_inst(pc, secure_stack))
                        pc_chaning_inst = 1;
                } else {
                    uint32_t pc = stacked_pc_prev+TWO_BYTES;
                    if(is_pc_chaning_inst(pc, secure_stack))
                        pc_chaning_inst = 1;
                }

				// Decode instruction and check if it is A CPSID, i.e., disable
				// interrupts. If it is, we won't count the instruction as a
				// multi-step, because while we get multi-step, is not a failure
				// of the framework, but a constraint because the interrupts are
				// disabled. 
				if(decode_opcode(stacked_pc_prev, secure_stack) == CPSID)
					single_steps++;
                else if(pc_chaning_inst)
					single_steps++;
				else{
					multi_steps++;
					register_metrics(decode_opcode(stacked_pc_prev, secure_stack), stacked_pc_prev, secure_stack);
                }
				pc_chaning_inst = 0;
			break;
		}

		interrupts++;
    }
    stacked_pc_prev = stacked_pc;
}
