/*
 * RouterDriver.cc
 *
 *  Created on: Dec 21, 2014
 *      Author: yonch
 */

#include "RouterDriver.h"
#include <assert.h>
#include <stdint.h>
#include <time.h> /* for seeding the random number generator */
#include "../config.h"
#include "../router.h"
#include "../graph-algo/fp_ring.h"
#include "../graph-algo/platform.h"
#include "../api.h"
#include "../api_impl.h"
#include "../graph-algo/random.h"

#define ROUTER_MAX_BURST	(EMU_ROUTER_NUM_PORTS * 2)


RouterDriver::RouterDriver(Router* router, struct fp_ring* q_to_router,
		struct fp_ring* q_from_router, struct emu_admission_statistics* stat)
	: m_router(router),
	  m_q_to_router(q_to_router),
	  m_q_from_router(q_from_router),
	  m_stat(stat)
{
	seed_random(&m_random, time(NULL));
}

RouterDriver::~RouterDriver() {
	delete m_router;
}

/**
 * Emulate a timeslot at a single router
 */
void RouterDriver::step() {
	uint32_t i, j, n_pkts;
	struct emu_packet *pkt_ptrs[ROUTER_MAX_BURST];
	assert(ROUTER_MAX_BURST >= EMU_ROUTER_NUM_PORTS);

	/* fetch packets to send from router to endpoints */
#ifdef EMU_NO_BATCH_CALLS
	n_pkts = 0;
	for (uint32_t i = 0; i < EMU_ROUTER_NUM_PORTS; i++) {
		pkt_ptrs[n_pkts] = m_router->pull(i);

		if (pkt_ptrs[n_pkts] != NULL)
			n_pkts++;
	}
#else
	n_pkts = m_router->pull_batch(pkt_ptrs, EMU_ROUTER_NUM_PORTS);
#endif
	assert(n_pkts <= EMU_ROUTER_NUM_PORTS);
	/* send packets to endpoint groups */
	while (fp_ring_enqueue_bulk(m_q_from_router, (void **) &pkt_ptrs[0],
			n_pkts) == -ENOBUFS) {
		/* no space in ring. log and retry. */
		adm_log_emu_send_packets_failed(m_stat, n_pkts);
	}
	adm_log_emu_router_sent_packets(m_stat, n_pkts);


	/* fetch a batch of packets from the network */
	n_pkts = fp_ring_dequeue_burst(m_q_to_router,
			(void **) &pkt_ptrs, ROUTER_MAX_BURST);
	/* shuffle packets to ensure router doesn't discriminate against some
	 * endpoints */
	for (uint32_t i = n_pkts; i > 1; i--) {
		/* decide which packet should be in pkts_ptrs[i] between 0..i-1*/
		uint32_t j = random_int(&m_random, i);
		/* swap i and j, works okay even when i == j */
		struct emu_packet *tmp = pkt_ptrs[i-1];
		pkt_ptrs[i-1] = pkt_ptrs[j];
		pkt_ptrs[j] = tmp;
	}

	/* pass all incoming packets to the router */
#ifdef EMU_NO_BATCH_CALLS
	for (i = 0; i < n_pkts; i++) {
		m_router->push(pkt_ptrs[i]);
	}
#else
	m_router->push_batch(&pkt_ptrs[0], n_pkts);
#endif
}


