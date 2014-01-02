
#ifndef CONTROLLER_H_
#define CONTROLLER_H_

#include <stdint.h>

#define CONTROLLER_SEND_TIMEOUT_NS (4*1000*1000)

/* Specifications for controller thread */
struct comm_core_cmd {
	uint64_t start_time;
	uint64_t end_time;

	uint64_t tslot_len; /**< Length of a time slot */
	uint32_t tslot_offset; /**< How many offsets in the future the controller allocates */
};

/**
 * Initializes global data used by comm cores
 */
void comm_init_global_structs(uint64_t first_time_slot);

/**
 * Initializes a single core to be a comm core
 */
void comm_init_core(uint16_t lcore_id);

void exec_comm_core(struct comm_core_cmd * cmd);


#endif /* CONTROLLER_H_ */