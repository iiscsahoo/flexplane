/*
 * emu_admission_core.c
 *
 *  Created on: July 12, 2014
 *      Author: aousterh
 */

#include "emu_admission_core.h"

#include <rte_errno.h>
#include <rte_string_fns.h>
#include <pthread.h>
#include <sched.h>

#include "admission_core_common.h"
#include "admission_log.h"
#include "../emulation/admitted.h"
#include "../emulation/emulation.h"
#include "../emulation/endpoint.h"
#include "../emulation/queue_managers/drop_tail.h"
#include "../emulation/queue_managers/dctcp.h"
#include "../emulation/packet.h"
#include "../emulation/router.h"
#include "../graph-algo/algo_config.h"

#define TIMESLOTS_PER_ONE_WAY_DELAY 4

struct emu_state g_emu_state;

struct rte_mempool* admitted_traffic_pool[NB_SOCKETS];
struct admission_log admission_core_logs[RTE_MAX_LCORE];
struct rte_ring *packet_queues[EMU_NUM_PACKET_QS];
struct dctcp_args args;

void emu_admission_init_global(struct rte_ring *q_admitted_out)
{
	int i;
	char s[64];
	struct rte_mempool *packet_mempool;
	uint32_t packet_size;

	/* setup for specific emulation algorithm */
	packet_size = EMU_ALIGN(sizeof(struct emu_packet)) + 0;

	/* allocate packet_mempool */
	uint32_t pool_index = 0;
	uint32_t socketid = 0;
	snprintf(s, sizeof(s), "packet_pool_%d", pool_index);
	packet_mempool =
			rte_mempool_create(s, PACKET_MEMPOOL_SIZE, /* num elements */
					packet_size, /* element size */
					PACKET_MEMPOOL_CACHE_SIZE, /* cache size */
					0, NULL, NULL, NULL, NULL, /* custom init, disabled */
					socketid, 0);
	if (packet_mempool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init packet mempool on socket %d: %s\n",
				socketid, rte_strerror(rte_errno));
	else
		RTE_LOG(INFO, ADMISSION,
				"Allocated packet mempool on socket %d - %lu bufs\n", socketid,
				(uint64_t) PACKET_MEMPOOL_SIZE);

	/* allocate admitted_traffic_mempool */
	pool_index = 0;
	if (admitted_traffic_pool[pool_index] == NULL) {
		snprintf(s, sizeof(s), "admitted_traffic_pool_%d", pool_index);
		admitted_traffic_pool[pool_index] =
				rte_mempool_create(s,
						ADMITTED_TRAFFIC_MEMPOOL_SIZE, /* num elements */
						sizeof(struct emu_admitted_traffic), /* element size */
						ADMITTED_TRAFFIC_CACHE_SIZE, /* cache size */
						0, NULL, NULL, NULL, NULL, /* custom init, disabled */
						socketid, 0);
		if (admitted_traffic_pool[pool_index] == NULL)
			rte_exit(EXIT_FAILURE,
					"Cannot init admitted traffic pool on socket %d: %s\n",
					socketid, rte_strerror(rte_errno));
		else
			RTE_LOG(INFO, ADMISSION,
					"Allocated admitted traffic pool on socket %d - %lu bufs\n",
					socketid, (uint64_t) ADMITTED_TRAFFIC_MEMPOOL_SIZE);
	}

	/* init log */
	for (i = 0; i < RTE_MAX_LCORE; i++)
		admission_log_init(&admission_core_logs[i]);

	/* init packet_queues */
	for (i = 0; i < EMU_NUM_PACKET_QS; i++) {
		snprintf(s, sizeof(s), "packet_q_%d", i);
		packet_queues[i] = rte_ring_create(s, PACKET_Q_SIZE, 0,
				RING_F_SP_ENQ | RING_F_SC_DEQ);
		if (packet_queues[i] == NULL)
			rte_exit(EXIT_FAILURE, "Cannot init packet_queues[%d]: %s\n", i,
					rte_strerror(rte_errno));
	}
	RTE_LOG(INFO, ADMISSION, "Initialized %d packet queues of size %d\n",
			EMU_NUM_PACKET_QS, PACKET_Q_SIZE);

	/* init emu_state */
    args.q_capacity = 512;
    args.K_threshold = 64;
	emu_init_state(&g_emu_state, (fp_mempool *) admitted_traffic_pool[0],
			(fp_ring *) q_admitted_out, (fp_mempool *) packet_mempool,
            (fp_ring **) packet_queues, R_DCTCP, &args, E_Simple, NULL);
}

int exec_emu_admission_core(void *void_cmd_p)
{
	struct admission_core_cmd *cmd = (struct admission_core_cmd *)void_cmd_p;
	uint32_t core_ind = cmd->admission_core_index;
	int ret;
	uint64_t logical_timeslot = cmd->start_timeslot;
	uint64_t start_time_first_timeslot, time_now, tslot;
        int64_t timeslots_behind;

	/* set thread priority to max */
/*	pthread_t this_thread = pthread_self();
	struct sched_param params;
	params.sched_priority = sched_get_priority_max(SCHED_FIFO);
	ret = pthread_setschedparam(this_thread, SCHED_FIFO, &params);
	if (ret != 0) {
	  ADMISSION_DEBUG("core %d admission %d failed to set thread realtime priority\n",
			  rte_lcore_id(), core_ind);
	} else {
	  ADMISSION_DEBUG("core %d admission %d successfully set thread realtime priority\n",
			  rte_lcore_id(), core_ind);
                          }*/

	ADMISSION_DEBUG("core %d admission %d starting allocations\n",
			rte_lcore_id(), core_ind);

	/* do allocation loop */
	time_now = fp_get_time_ns();
	tslot = (time_now * TIMESLOT_MUL) >> TIMESLOT_SHIFT;
	while (1) {
		/* check if we're behind */
		timeslots_behind = tslot - logical_timeslot +
				TIMESLOTS_PER_ONE_WAY_DELAY;
		if (timeslots_behind > TSLOTS_BEHIND_TOLERANCE) {
			/* skip timeslots to catch up */
			logical_timeslot += timeslots_behind;

			admission_log_core_skipped_tslots(timeslots_behind);
		}

		/* pace emulation so that timeslots arrive at endpoints just in time */
		while (tslot < logical_timeslot - TIMESLOTS_PER_ONE_WAY_DELAY) {
			admission_log_core_ahead();
			time_now = fp_get_time_ns();
			tslot = (time_now * TIMESLOT_MUL) >> TIMESLOT_SHIFT;
		}

		admission_log_allocation_begin(logical_timeslot,
					       start_time_first_timeslot);

		/* perform allocation */
		/* use a single core for now */
		emu_emulate(&g_emu_state);

		admission_log_allocation_end(logical_timeslot);

		logical_timeslot += 1;
	}

	return 0;
}
