/*
 * Copyright (c) 2006, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         Tree-based hop-by-hop reliable data collection
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

/**
 * \addtogroup rimecollect
 * @{
 */

#include <stdlib.h>
#include "contiki.h"
#include "net/netstack.h"
#include "net/rime/rime.h"
#include "net/rime/collect.h"
#include "net/rime/collect-neighbor.h"
#include "net/rime/collect-link-estimate.h"
#include "packetqueue.h"
#include "dev/radio-sensor.h"
#include "lib/random.h"
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include "aggregationqueue.c"
#include <stdlib.h>
// #include "packetqueue.h"

static const struct packetbuf_attrlist attributes[] =
    {
        COLLECT_ATTRIBUTES
            PACKETBUF_ATTR_LAST};

/* The recent_packets list holds the sequence number, the originator,
   and the connection for packets that have been recently
   forwarded. This list is maintained to avoid forwarding duplicate
   packets. */
#define NUM_RECENT_PACKETS 16

linkaddr_t mote_address;
int mode_id = -1;

uint8_t distance_to_sink=2;

struct recent_packet
{
    struct collect_conn *conn;
    linkaddr_t originator;
    uint8_t eseqno;
};

static struct recent_packet recent_packets[NUM_RECENT_PACKETS];
static uint8_t recent_packet_ptr;

/* This is the header of data packets. The header comtains the routing
   metric of the last hop sender. This is used to avoid routing loops:
   if a node receives a packet with a lower routing metric than its
   own, it drops the packet. */

struct data_msg_hdr
{
    uint8_t flags, dummy;
    uint16_t rtmetric;
};

/* This is the header of ACK packets. It contains a flags field that
   indicates if the node is congested (ACK_FLAGS_CONGESTED), if the
   packet was dropped (ACK_FLAGS_DROPPED), if a packet was dropped due
   to its lifetime was exceeded (ACK_FLAGS_LIFETIME_EXCEEDED), and if
   an outdated rtmetric was detected
   (ACK_FLAGS_RTMETRIC_NEEDS_UPDATE). The flags can contain any
   combination of the flags. The ACK header also contains the routing
   metric of the node that sends tha ACK. This is used to keep an
   up-to-date routing state in the network. */

struct ack_msg
{
    uint8_t flags, dummy;
    uint16_t rtmetric;
};

#define ACK_FLAGS_CONGESTED 0x80
#define ACK_FLAGS_DROPPED 0x40
#define ACK_FLAGS_LIFETIME_EXCEEDED 0x20
#define ACK_FLAGS_RTMETRIC_NEEDS_UPDATE 0x10

/* These are configuration knobs that normally should not be
   tweaked. MAX_MAC_REXMITS defines how many times the underlying CSMA
   MAC layer should attempt to resend a data packet before giving
   up. The MAX_ACK_MAC_REXMITS defines how many times the MAC layer
   should resend ACK packets. The REXMIT_TIME is the lowest
   retransmission timeout at the network layer. It is exponentially
   increased for every new network layer retransmission. The
   FORWARD_PACKET_LIFETIME is the maximum time a packet is held in the
   forwarding queue before it is removed. The MAX_SENDING_QUEUE
   specifies the maximum length of the output queue. If the queue is
   full, incoming packets are dropped instead of being forwarded. */
#ifdef COLLECT_CONF_MAX_MAC_REXMITS
#define MAX_MAC_REXMITS COLLECT_CONF_MAX_MAC_REXMITS
#else /* COLLECT_CONF_MAX_MAC_REXMITS */
#define MAX_MAC_REXMITS 2
#endif /* COLLECT_CONF_MAX_MAC_REXMITS */

#ifdef COLLECT_CONF_MAX_ACK_MAC_REXMITS
#define MAX_ACK_MAC_REXMITS COLLECT_CONF_MAX_ACK_MAC_REXMITS
#else /* COLLECT_CONF_MAX_ACK_MAC_REXMITS */
#define MAX_ACK_MAC_REXMITS 5
#endif /* COLLECT_CONF_MAX_ACK_MAC_REXMITS */

#define REXMIT_TIME (CLOCK_SECOND * 32 / NETSTACK_RDC_CHANNEL_CHECK_RATE)
#define FORWARD_PACKET_LIFETIME_BASE REXMIT_TIME * 2
#define MAX_SENDING_QUEUE 3 * QUEUEBUF_NUM / 4
#define MIN_AVAILABLE_QUEUE_ENTRIES 4
#define KEEPALIVE_REXMITS 8
#define MAX_REXMITS 31

MEMB(send_queue_memb, struct packetqueue_item, MAX_SENDING_QUEUE);

/* These specifiy the sink's routing metric (0) and the maximum
   routing metric. If a node has routing metric zero, it is the
   sink. If a node has the maximum routing metric, it has no route to
   a sink. */
#define RTMETRIC_SINK 0
#define RTMETRIC_MAX COLLECT_MAX_DEPTH

/* Here we define what we mean with a significantly improved
   rtmetric. This is used to determine when a new parent should be
   chosen over an old parent and when to begin more rapidly advertise
   a new rtmetric. */
#define SIGNIFICANT_RTMETRIC_PARENT_CHANGE (COLLECT_LINK_ESTIMATE_UNIT + \
                                            COLLECT_LINK_ESTIMATE_UNIT / 2)

/* This defines the maximum hops that a packet can take before it is
   dropped. */
#ifdef COLLECT_CONF_MAX_HOPLIM
#define MAX_HOPLIM COLLECT_CONF_MAX_HOPLIM
#else /* COLLECT_CONF_MAX_HOPLIM */
#define MAX_HOPLIM 15
#endif /* COLLECT_CONF_MAX_HOPLIM */

/* Proactive probing: when there are no packets in the send
   queue, the system periodically sends a dummy packet to potential
   parents, i.e., neighbors with a lower rtmetric than we have but for
   which we do not yet have a link quality estimate. */
#ifdef COLLECT_CONF_PROACTIVE_PROBING_INTERVAL
#define PROACTIVE_PROBING_INTERVAL (random_rand() % (2 * COLLECT_CONF_PROACTIVE_PROBING_INTERVAL))
#else /* COLLECT_CONF_PROACTIVE_PROBING_INTERVAL */
#define PROACTIVE_PROBING_INTERVAL (random_rand() % CLOCK_SECOND * 60)
#endif /* COLLECT_CONF_PROACTIVE_PROBING_INTERVAL */
#define PROACTIVE_PROBING_REXMITS 15

/* The ANNOUNCEMENT_SCAN_TIME defines for how long the Collect
   implementation should listen for announcements from other nodes
   when it requires a route. */
#ifdef ANNOUNCEMENT_CONF_PERIOD
#define ANNOUNCEMENT_SCAN_TIME ANNOUNCEMENT_CONF_PERIOD
#else /* ANNOUNCEMENT_CONF_PERIOD */
#define ANNOUNCEMENT_SCAN_TIME CLOCK_SECOND
#endif /* ANNOUNCEMENT_CONF_PERIOD */

/* Statistics structure */
struct
{
    uint32_t foundroute;
    uint32_t newparent;
    uint32_t routelost;

    uint32_t acksent;
    uint32_t datasent;

    uint32_t datarecv;
    uint32_t ackrecv;
    uint32_t badack;
    uint32_t duprecv;

    uint32_t qdrop;
    uint32_t rtdrop;
    uint32_t ttldrop;
    uint32_t ackdrop;
    uint32_t timedout;
} stats;

/* Debug definition: draw routing tree in Cooja. */
#define DRAW_TREE 0
#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/* Forward declarations. */
static void send_queued_packet(struct collect_conn *c);
static void retransmit_callback(void *ptr);
static void retransmit_not_sent_callback(void *ptr);
static void set_keepalive_timer(struct collect_conn *c);

/*---------------------------------------------------------------------------*/
struct queueElement *aggregation_head = NULL;
struct ctimer aggregation_timer;
struct ctimer pop_timer;

/*---------------------------------------------------------------------------*/

#define AGGREGATION_INTERVAL 300
#define POP_INTERVAL 500

static void aggregationCaller()
{
    printf("AGGREGATING\n");
    aggregateCustomQueue(&aggregation_head);
    ctimer_restart(&aggregation_timer);
}

void add_packet_to_recent_packets(struct collect_conn *tc);

static void push_to_packetqueue(struct collect_conn *tc)
{
    printf("HAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n");
    if (packetqueue_len(&tc->send_queue) <= MAX_SENDING_QUEUE - MIN_AVAILABLE_QUEUE_ENTRIES &&
        packetqueue_enqueue_packetbuf(&tc->send_queue,
                                      FORWARD_PACKET_LIFETIME_BASE *
                                          packetbuf_attr(PACKETBUF_ATTR_MAX_REXMIT),
                                      tc))
    {
        add_packet_to_recent_packets(tc);
        printf("SENDING PAKCET\n");
        send_queued_packet(tc);
    }
    // else
    // {
    //     send_ack(tc, &ack_to, ackflags | ACK_FLAGS_DROPPED | ACK_FLAGS_CONGESTED);
    //     PRINTF("%d.%d: packet dropped: no queue buffer available\n",
    //            linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
    //     stats.qdrop++;
    // }
}

static void popAggregationQueueCaller(struct collect_conn *tc)
{
    printf("POP CUSTOM QUEUE\n");
    printf("AGg LIST len: %d\n", agg_list_len(aggregation_head));
    struct queueElement *popped = popCustomQueue(&aggregation_head);
    if (popped == NULL)
    {
        printf("NULLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL\n");
    }
    struct queuebuf *q = queuebuf_new_from_packetbuf();
    while (popped != NULL)
    {
        packetbuf_clear();
        queuebuf_to_packetbuf(popped->q);
        char *dataptr = packetbuf_dataptr();
        packetbuf_clear_data();
        packetbuf_hdralloc(sizeof(struct data_msg_hdr));
        memcpy(dataptr, popped->hdr_data, sizeof(struct data_msg_hdr));
        printf("ID: %d, SRC: %s\n", popped->Eid, popped->srcList);
        packetbuf_set_datalen(sprintf(packetbuf_dataptr(), "ID:%d|%s", popped->Eid, popped->srcList) + 1);
        push_to_packetqueue(tc);
        free(popped->hdr_data);
        queuebuf_free(popped->q);
        struct queueElement *nextPtr = popped->next;
        free(popped);
        popped = nextPtr;
    }
    queuebuf_to_packetbuf(q);
    queuebuf_free(q);

    ctimer_restart(&pop_timer);
}

/*---------------------------------------------------------------------------*/

/*
Use the packet on the head of the packet list and aggregate it with packets of same event ID
in the queue.
*/
// static void aggregate_queue(struct collect_conn *c)
// {
//     struct packqueue_item *head = list_head(c->send_queue.list);
// }

// struct call_arg
// {
//     struct ctimer lifetimer;
//     int data;
// };

// static void print_ctimer(struct call_arg *arg)
// {
//     printf("CTIMER-CALL\n");
//     // ctimer_stop(lifetimer);
//     ctimer_restart(&arg->lifetimer);
//     // ctimer_set(lifetimer, 5000, print_ctimer, lifetimer);
// }

// static void print_buf()
// {
//     char *dataptr = (char *)packetbuf_dataptr();
//     // printf("DATAAAAAAA: '%s'\n", (char *)packetbuf_dataptr() + 4);
// }

/*---------------------------------------------------------------------------*/
/**
 * This function computes the current rtmetric by adding the last
 * known rtmetric from our parent with the link estimate to the
 * parent.
 *
 */
static uint16_t
rtmetric_compute(struct collect_conn *tc)
{
    struct collect_neighbor *n;
    uint16_t rtmetric = RTMETRIC_MAX;

    /* This function computes the current rtmetric for this node. It
       uses the rtmetric of the parent node in the tree and adds the
       current link estimate from us to the parent node. */

    /* The collect connection structure stores the address of its
       current parent. We look up the neighbor identification struct in
       the collect-neighbor list. */
    n = collect_neighbor_list_find(&tc->neighbor_list, &tc->parent);

    /* If n is NULL, we have no best neighbor. Thus our rtmetric is
       then COLLECT_RTMETRIC_MAX. */
    if (n == NULL)
    {
        rtmetric = RTMETRIC_MAX;
    }
    else
    {
        /* Our rtmetric is the rtmetric of our parent neighbor plus
           the expected transmissions to reach that neighbor. */
        rtmetric = collect_neighbor_rtmetric_link_estimate(n);
    }

    return rtmetric;
}
/*---------------------------------------------------------------------------*/
/**
 * This function is called when the route advertisements need to be
 * transmitted more rapidly.
 *
 */
static void
bump_advertisement(struct collect_conn *c)
{
#if !COLLECT_ANNOUNCEMENTS
    neighbor_discovery_start(&c->neighbor_discovery_conn, c->rtmetric);
#else
    announcement_bump(&c->announcement);
#endif /* !COLLECT_ANNOUNCEMENTS */
}
/*---------------------------------------------------------------------------*/
/**
 * This function is called to update the current parent node. The
 * parent may change if new routing information has been found, for
 * example if a new node with a lower rtmetric and link estimate has
 * appeared.
 *
 */
static void
update_parent(struct collect_conn *tc)
{
    struct collect_neighbor *current;
    struct collect_neighbor *best;

    /* We grab the collect_neighbor struct of our current parent. */
    current = collect_neighbor_list_find(&tc->neighbor_list, &tc->parent);

    /* We call the collect_neighbor module to find the current best
       parent. */
    best = collect_neighbor_list_best(&tc->neighbor_list);

    /* We check if we need to switch parent. Switching parent is done in
       the following situations:

       * We do not have a current parent.
       * The best parent is significantly better than the current parent.

       If we do not have a current parent, and have found a best parent,
       we simply use the new best parent.

       If we already have a current parent, but have found a new parent
       that is better, we employ a heuristic to avoid switching parents
       too often. The new parent must be significantly better than the
       current parent. Being "significantly better" is defined as having
       an rtmetric that is has a difference of at least 1.5 times the
       COLLECT_LINK_ESTIMATE_UNIT. This is derived from the experience
       by Gnawali et al (SenSys 2009). */

    if (best != NULL)
    {
        linkaddr_t previous_parent;

        if (DRAW_TREE)
        {
            linkaddr_copy(&previous_parent, &tc->parent);
        }

        if (current == NULL)
        {
            /* New parent. */
            PRINTF("update_parent: new parent %d.%d\n",
                   best->addr.u8[0], best->addr.u8[1]);
            linkaddr_copy(&tc->parent, &best->addr);
            stats.foundroute++;
            bump_advertisement(tc);
        }
        else
        {
            if (DRAW_TREE)
            {
                PRINTF("#A e=%d\n", collect_neighbor_link_estimate(best));
            }
            if (collect_neighbor_rtmetric_link_estimate(best) +
                    SIGNIFICANT_RTMETRIC_PARENT_CHANGE <
                collect_neighbor_rtmetric_link_estimate(current))
            {

                /* We switch parent. */
                PRINTF("update_parent: new parent %d.%d (%d) old parent %d.%d (%d)\n",
                       best->addr.u8[0], best->addr.u8[1],
                       collect_neighbor_rtmetric(best),
                       tc->parent.u8[0], tc->parent.u8[1],
                       collect_neighbor_rtmetric(current));
                linkaddr_copy(&tc->parent, &best->addr);
                stats.newparent++;
                /* Since we now have a significantly better or worse rtmetric than
                   we had before, we let our neighbors know this quickly. */
                bump_advertisement(tc);

                if (DRAW_TREE)
                {
                    PRINTF("#A e=%d\n", collect_neighbor_link_estimate(best));
                    /*          {
                      int i;
                      int etx = 0;
                      PRINTF("#A l=");
                      for(i = 0; i < 8; i++) {
                        PRINTF("%d ", best->le.history[(best->le.historyptr - 1 - i) & 7]);
                        etx += current->le.history[i];
                      }
                      PRINTF("\n");
                      }*/
                }
            }
            else
            {
                if (DRAW_TREE)
                {
                    PRINTF("#A e=%d\n", collect_neighbor_link_estimate(current));
                    /*          {
                      int i;
                      int etx = 0;
                      PRINTF("#A l=");
                      for(i = 0; i < 8; i++) {
                        PRINTF("%d ", current->le.history[(current->le.historyptr - 1 - i) & 7]);
                        etx += current->le.history[i];
                      }
                      PRINTF("\n");
                      }*/
                }
            }
        }
        if (DRAW_TREE)
        {
            if (!linkaddr_cmp(&previous_parent, &tc->parent))
            {
                if (!linkaddr_cmp(&previous_parent, &linkaddr_null))
                {
                    PRINTF("#L %d 0\n", previous_parent.u8[0]);
                }
                PRINTF("#L %d 1\n", tc->parent.u8[0]);
            }
        }
    }
    else
    {
        /* No parent. */
        if (!linkaddr_cmp(&tc->parent, &linkaddr_null))
        {
            if (DRAW_TREE)
            {
                PRINTF("#L %d 0\n", tc->parent.u8[0]);
            }
            stats.routelost++;
        }
        linkaddr_copy(&tc->parent, &linkaddr_null);
    }
}
/*---------------------------------------------------------------------------*/
/**
 * This function is called whenever there is a chance that the routing
 * metric has changed. The function goes through the list of neighbors
 * to compute the new routing metric. If the metric has changed, it
 * notifies neighbors.
 *
 *
 */
static void
update_rtmetric(struct collect_conn *tc)
{
    PRINTF("update_rtmetric: tc->rtmetric %d\n", tc->rtmetric);

    /* We should only update the rtmetric if we are not the sink. */
    if (tc->rtmetric != RTMETRIC_SINK)
    {
        uint16_t old_rtmetric, new_rtmetric;

        /* We remember the current (old) rtmetric for later. */
        old_rtmetric = tc->rtmetric;

        /* We may need to update our parent node so we do that now. */
        update_parent(tc);

        /* We compute the new rtmetric. */
        new_rtmetric = rtmetric_compute(tc);

        /* We sanity check our new rtmetric. */
        if (new_rtmetric == RTMETRIC_SINK)
        {
            /* Defensive programming: if the new rtmetric somehow got to be
               the rtmetric of the sink, there is a bug somewhere. To avoid
               destroying the network, we simply will not assume this new
               rtmetric. Instead, we set our rtmetric to maximum, to
               indicate that we have no sane route. */
            new_rtmetric = RTMETRIC_MAX;
        }

        /* We set our new rtmetric in the collect conn structure. Then we
           decide how we should announce this new rtmetric. */
        tc->rtmetric = new_rtmetric;

        if (tc->is_router)
        {
            /* If we are a router, we update our advertised rtmetric. */
#if COLLECT_ANNOUNCEMENTS
            announcement_set_value(&tc->announcement, tc->rtmetric);
#else  /* COLLECT_ANNOUNCEMENTS */
            neighbor_discovery_set_val(&tc->neighbor_discovery_conn, tc->rtmetric);
#endif /* COLLECT_ANNOUNCEMENTS */
        }
        PRINTF("%d.%d: new rtmetric %d\n",
               linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
               tc->rtmetric);

        /* We got a new, working, route we send any queued packets we may have. */
        if (old_rtmetric == RTMETRIC_MAX && new_rtmetric != RTMETRIC_MAX)
        {
            PRINTF("Sending queued packet because rtmetric was max\n");
            send_queued_packet(tc);
        }
        if (DRAW_TREE)
        {
            if (old_rtmetric != new_rtmetric)
            {
                PRINTF("#A rt=%d,p=%d\n", tc->rtmetric, tc->parent.u8[0]);
            }
        }
    }
}
/*---------------------------------------------------------------------------*/
static int
enqueue_dummy_packet(struct collect_conn *c, int rexmits)
{
    struct collect_neighbor *n;

    packetbuf_clear();
    packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_ID, c->eseqno - 1);
    packetbuf_set_addr(PACKETBUF_ADDR_ESENDER, &linkaddr_node_addr);
    packetbuf_set_attr(PACKETBUF_ATTR_HOPS, 1);
    packetbuf_set_attr(PACKETBUF_ATTR_TTL, 1);
    packetbuf_set_attr(PACKETBUF_ATTR_MAX_REXMIT, rexmits);

    PRINTF("%d.%d: enqueueing dummy packet %d, max_rexmits %d\n",
           linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
           packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID),
           packetbuf_attr(PACKETBUF_ATTR_MAX_REXMIT));

    /* Allocate space for the header. */
    packetbuf_hdralloc(sizeof(struct data_msg_hdr));

    n = collect_neighbor_list_find(&c->neighbor_list, &c->parent);
    if (n != NULL)
    {
        return packetqueue_enqueue_packetbuf(&c->send_queue,
                                             FORWARD_PACKET_LIFETIME_BASE * rexmits,
                                             c);
    }
    return 0;
}
/*---------------------------------------------------------------------------*/
static void
send_packet(struct collect_conn *c, struct collect_neighbor *n)
{
    clock_time_t time;

    PRINTF("Sending packet to %d.%d, %d transmissions\n",
           n->addr.u8[0], n->addr.u8[1],
           c->transmissions);
    /* Defensive programming: if a bug in the MAC/RDC layers will cause
       it to not call us back, we'll set up the retransmission timer
       with a high timeout, so that we can cancel the transmission and
       send a new one. */
    time = 16 * REXMIT_TIME;
    ctimer_set(&c->retransmission_timer, time,
               retransmit_not_sent_callback, c);
    c->send_time = clock_time();

    unicast_send(&c->unicast_conn, &n->addr);
}
/*---------------------------------------------------------------------------*/
static void
proactive_probing_callback(void *ptr)
{
    struct collect_conn *c = ptr;
    struct packetqueue_item *i;

    ctimer_set(&c->proactive_probing_timer, PROACTIVE_PROBING_INTERVAL,
               proactive_probing_callback, ptr);

    /* Only do proactive link probing if we are not the sink and if we
       have a route. */
    if (c->rtmetric != RTMETRIC_SINK && c->rtmetric != RTMETRIC_MAX)
    {
        /* Grab the first packet on the send queue to see if the queue is
           empty or not. */
        i = packetqueue_first(&c->send_queue);
        if (i == NULL)
        {
            /* If there are no packets to send, we go through the list of
               neighbors to find a potential parent for which we do not have a
               link estimate and send a dummy packet to it. This allows us to
               quickly gauge the link quality of neighbors that we do not
               currently use as parents. */
            struct collect_neighbor *n;

            /* Find the neighbor with the lowest number of estimates. */
            for (n = list_head(collect_neighbor_list(&c->neighbor_list));
                 n != NULL; n = list_item_next(n))
            {
                if (n->rtmetric + COLLECT_LINK_ESTIMATE_UNIT < c->rtmetric &&
                    collect_link_estimate_num_estimates(&n->le) == 0)
                {
                    linkaddr_t current_parent;

                    PRINTF("proactive_probing_callback: found neighbor with no link estimate, %d.%d\n",
                           n->addr.u8[LINKADDR_SIZE - 2], n->addr.u8[LINKADDR_SIZE - 1]);

                    linkaddr_copy(&current_parent, &c->parent);
                    linkaddr_copy(&c->parent, &n->addr);
                    if (enqueue_dummy_packet(c, PROACTIVE_PROBING_REXMITS))
                    {
                        send_queued_packet(c);
                    }
                    linkaddr_copy(&c->parent, &current_parent);
                    return;
                }
            }
        }
        PRINTF("%d.%d: nothing on queue\n",
               linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
        return;
    }
}

/**/
// aggregation functions
// ID:<event_id>|mote_id1, mote_id2, ...
void aggregate_buffer(char *a, char *b, char *out)
{
    // char* token_a = strtok(a, "|");
    // char* token_b = strtok(b, "|");
    // char *list_a = strchr(a, '|') + 1;
    char *list_b = strchr(b, '|') + 1;
    sprintf(out, "%s,%s", a, list_b);
}

int get_event_id(char *buf)
{
    char id_str[30];
    buf = buf + 3;
    sprintf(id_str, "%s", buf);
    // printf("ID1: %s/", id_str);
    char *last = strchr(id_str, '|');
    *last = 0;
    int id = atoi(id_str);
    // printf("ID is %d\n", id);
    return id;
}

void get_mote_list(char *buf, char *out)
{
    char *list = strchr(buf, '|') + 1;
    strcpy(out, list);
}

/**/
// static void aggregate_queue(struct collect_conn *c)
// {
//     struct packqueue_item *head = list_head(c->send_queue.list);
//     struct queuebuf* q =
//     // c->send_queue;

// }
/*---------------------------------------------------------------------------*/
/**
 * This function is called when a queued packet should be sent
 * out. The function takes the first packet on the output queue, adds
 * the necessary packet attributes, and sends the packet to the
 * next-hop neighbor.
 *
 */
static void
send_queued_packet(struct collect_conn *c)
{
    struct packetqueue_item *i;

    /* If we are currently sending a packet, we do not attempt to send
       another one. */
    if (c->sending)
    {
        PRINTF("%d.%d: queue, c is sending\n",
               linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
        return;
    }

    /* Grab the first packet on the send queue. */
    i = packetqueue_first(&c->send_queue);
    // while (i != NULL)
    // {
    struct queuebuf *q;
    struct collect_neighbor *n;
    struct data_msg_hdr hdr;
    int max_mac_rexmits;
    if (i == NULL)
    {
        PRINTF("%d.%d: nothing on queue\n",
               linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);

        return;
    }

    /* We should send the first packet from the queue. */
    q = packetqueue_queuebuf(i);
    if (q != NULL)
    {
        /* Place the queued packet into the packetbuf. */
        queuebuf_to_packetbuf(q);

        /* Pick the neighbor to which to send the packet. We use the
        parent in the n->parent. */
        n = collect_neighbor_list_find(&c->neighbor_list, &c->parent);

        if (n != NULL)
        {

            /* If the connection had a neighbor, we construct the packet
            buffer attributes and set the appropriate flags in the
            Collect connection structure and send the packet. */

            PRINTF("%d.%d: sending packet to %d.%d with eseqno %d\n",
                   linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                   n->addr.u8[0], n->addr.u8[1],
                   packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID));

            /* Mark that we are currently sending a packet. */
            c->sending = 1;

            /* Remember the parent that we sent this packet to. */
            linkaddr_copy(&c->current_parent, &c->parent);

            /* This is the first time we transmit this packet, so set
            transmissions to zero. */
            c->transmissions = 0;

            /* Remember that maximum amount of retransmissions we should
            make. This is stored inside a packet attribute in the packet
            on the send queue. */
            c->max_rexmits = packetbuf_attr(PACKETBUF_ATTR_MAX_REXMIT);

            /* Set the packet attributes: this packet wants an ACK, so we
            sent the PACKETBUF_ATTR_RELIABLE flag; the MAC should retry
            MAX_MAC_REXMITS times; and the PACKETBUF_ATTR_PACKET_ID is
            set to the current sequence number on the connection. */
            packetbuf_set_attr(PACKETBUF_ATTR_RELIABLE, 1);

            max_mac_rexmits = c->max_rexmits > MAX_MAC_REXMITS ? MAX_MAC_REXMITS : c->max_rexmits;
            packetbuf_set_attr(PACKETBUF_ATTR_MAX_MAC_TRANSMISSIONS, max_mac_rexmits);
            packetbuf_set_attr(PACKETBUF_ATTR_PACKET_ID, c->seqno);

            stats.datasent++;

            /* Copy our rtmetric into the packet header of the outgoing
            packet. */
            memset(&hdr, 0, sizeof(hdr));
            hdr.rtmetric = c->rtmetric;
            memcpy(packetbuf_dataptr(), &hdr, sizeof(struct data_msg_hdr));
            // packetbuf_hdralloc(sizeof(struct data_msg_hdr));
            // memcpy(packetbuf_hdrptr(), &hdr, sizeof(struct data_msg_hdr));

            /* Send the packet. */
            send_packet(c, n);
        }
        else
        {
#if COLLECT_ANNOUNCEMENTS
#if COLLECT_CONF_WITH_LISTEN
            PRINTF("listen\n");
            announcement_listen(1);
            ctimer_set(&c->transmit_after_scan_timer, ANNOUNCEMENT_SCAN_TIME,
                       send_queued_packet, c);
#else  /* COLLECT_CONF_WITH_LISTEN */
            if (c->is_router)
            {
                announcement_set_value(&c->announcement, RTMETRIC_MAX);
                announcement_bump(&c->announcement);
            }
#endif /* COLLECT_CONF_WITH_LISTEN */
#endif /* COLLECT_ANNOUNCEMENTS */
        }
    }
    // }
}
/*---------------------------------------------------------------------------*/
/**
 * This function is called to retransmit the first packet on the send
 * queue.
 *
 */
static void
retransmit_current_packet(struct collect_conn *c)
{
    struct queuebuf *q;
    struct collect_neighbor *n;
    struct packetqueue_item *i;
    struct data_msg_hdr hdr;
    int max_mac_rexmits;

    /* Grab the first packet on the send queue, which is the one we are
       about to retransmit. */
    i = packetqueue_first(&c->send_queue);
    if (i == NULL)
    {
        PRINTF("%d.%d: nothing on queue\n",
               linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
        /* No packet on the queue, so there is nothing for us to send. */
        return;
    }

    /* Get hold of the queuebuf. */
    q = packetqueue_queuebuf(i);
    if (q != NULL)
    {
        update_rtmetric(c);

        /* Place the queued packet into the packetbuf. */
        queuebuf_to_packetbuf(q);

        /* Pick the neighbor to which to send the packet. If we have found
           a better parent while we were transmitting this packet, we
           chose that neighbor instead. If so, we need to attribute the
           transmissions we made for the parent to that neighbor. */
        if (!linkaddr_cmp(&c->current_parent, &c->parent))
        {
            /*      struct collect_neighbor *current_neighbor;
            current_neighbor = collect_neighbor_list_find(&c->neighbor_list,
                                                          &c->current_parent);
            if(current_neighbor != NULL) {
              collect_neighbor_tx(current_neighbor, c->max_rexmits);
              }*/

            PRINTF("parent change from %d.%d to %d.%d after %d tx\n",
                   c->current_parent.u8[0], c->current_parent.u8[1],
                   c->parent.u8[0], c->parent.u8[1],
                   c->transmissions);

            linkaddr_copy(&c->current_parent, &c->parent);
            c->transmissions = 0;
        }
        n = collect_neighbor_list_find(&c->neighbor_list, &c->current_parent);

        if (n != NULL)
        {
            /* If the connection had a neighbor, we construct the packet
               buffer attributes and set the appropriate flags in the
               Collect connection structure and send the packet. */

            PRINTF("%d.%d: sending packet to %d.%d with eseqno %d\n",
                   linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                   n->addr.u8[0], n->addr.u8[1],
                   packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID));

            /* Mark that we are currently sending a packet. */
            c->sending = 1;
            packetbuf_set_attr(PACKETBUF_ATTR_RELIABLE, 1);
            max_mac_rexmits = c->max_rexmits - c->transmissions > MAX_MAC_REXMITS ? MAX_MAC_REXMITS : c->max_rexmits - c->transmissions;
            packetbuf_set_attr(PACKETBUF_ATTR_MAX_MAC_TRANSMISSIONS, max_mac_rexmits);
            packetbuf_set_attr(PACKETBUF_ATTR_PACKET_ID, c->seqno);

            /* Copy our rtmetric into the packet header of the outgoing
               packet. */
            memset(&hdr, 0, sizeof(hdr));
            hdr.rtmetric = c->rtmetric;
            memcpy(packetbuf_dataptr(), &hdr, sizeof(struct data_msg_hdr));
            // packetbuf_hdralloc(sizeof(struct data_msg_hdr));
            // memcpy(packetbuf_hdrptr(), &hdr, sizeof(struct data_msg_hdr));

            /* Send the packet. */
            send_packet(c, n);
        }
    }
}
/*---------------------------------------------------------------------------*/
static void
send_next_packet(struct collect_conn *tc)
{
    /* Remove the first packet on the queue, the packet that was just sent. */
    packetqueue_dequeue(&tc->send_queue);
    tc->seqno = (tc->seqno + 1) % (1 << COLLECT_PACKET_ID_BITS);

    /* Cancel retransmission timer. */
    ctimer_stop(&tc->retransmission_timer);
    tc->sending = 0;
    tc->transmissions = 0;

    PRINTF("sending next packet, seqno %d, queue len %d\n",
           tc->seqno, packetqueue_len(&tc->send_queue));

    /* Send the next packet in the queue, if any. */
    send_queued_packet(tc);
}
/*---------------------------------------------------------------------------*/
static void
handle_ack(struct collect_conn *tc)
{
    struct ack_msg msg;
    struct collect_neighbor *n;

    PRINTF("handle_ack: sender %d.%d current_parent %d.%d, id %d seqno %d\n",
           packetbuf_addr(PACKETBUF_ADDR_SENDER)->u8[0],
           packetbuf_addr(PACKETBUF_ADDR_SENDER)->u8[1],
           tc->current_parent.u8[0], tc->current_parent.u8[1],
           packetbuf_attr(PACKETBUF_ATTR_PACKET_ID), tc->seqno);
    if (linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_SENDER),
                     &tc->current_parent) &&
        packetbuf_attr(PACKETBUF_ATTR_PACKET_ID) == tc->seqno)
    {

        /*    PRINTF("rtt %d / %d = %d.%02d\n",
               (int)(clock_time() - tc->send_time),
               (int)CLOCK_SECOND,
               (int)((clock_time() - tc->send_time) / CLOCK_SECOND),
               (int)(((100 * (clock_time() - tc->send_time)) / CLOCK_SECOND) % 100));*/

        stats.ackrecv++;
        memcpy(&msg, packetbuf_dataptr(), sizeof(struct ack_msg));

        /* It is possible that we receive an ACK for a packet that we
           think we have not yet sent: if our transmission was received by
           the other node, but the link-layer ACK was lost, our
           transmission counter may still be zero. If this is the case, we
           play it safe by believing that we have sent MAX_MAC_REXMITS
           transmissions. */
        if (tc->transmissions == 0)
        {
            tc->transmissions = MAX_MAC_REXMITS;
        }
        PRINTF("Updating link estimate with %d transmissions\n",
               tc->transmissions);
        n = collect_neighbor_list_find(&tc->neighbor_list,
                                       packetbuf_addr(PACKETBUF_ADDR_SENDER));

        if (n != NULL)
        {
            collect_neighbor_tx(n, tc->transmissions);
            collect_neighbor_update_rtmetric(n, msg.rtmetric);
            update_rtmetric(tc);
        }

        PRINTF("%d.%d: ACK from %d.%d after %d transmissions, flags %02x, rtmetric %d\n",
               linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
               tc->current_parent.u8[0], tc->current_parent.u8[1],
               tc->transmissions,
               msg.flags,
               msg.rtmetric);

        /* The ack contains information about the state of the packet and
           of the node that received it. We do different things depending
           on whether or not the packet was dropped. First, we check if
           the receiving node was congested. If so, we add a maximum
           transmission number to its routing metric, which increases the
           chance that another parent will be chosen. */
        if (msg.flags & ACK_FLAGS_CONGESTED)
        {
            PRINTF("ACK flag indicated parent was congested.\n");
            if (n != NULL)
            {
                collect_neighbor_set_congested(n);
                collect_neighbor_tx(n, tc->max_rexmits * 2);
            }
            update_rtmetric(tc);
        }
        if ((msg.flags & ACK_FLAGS_DROPPED) == 0)
        {
            /* If the packet was successfully received, we send the next packet. */
            send_next_packet(tc);
        }
        else
        {
            /* If the packet was lost due to its lifetime being exceeded,
               there is not much more we can do with the packet, so we send
               the next one instead. */
            if ((msg.flags & ACK_FLAGS_LIFETIME_EXCEEDED))
            {
                send_next_packet(tc);
            }
            else
            {
                /* If the packet was dropped, but without the node being
                   congested or the packets lifetime being exceeded, we
                   penalize the parent and try sending the packet again. */
                PRINTF("ACK flag indicated packet was dropped by parent.\n");
                collect_neighbor_tx(n, tc->max_rexmits);
                update_rtmetric(tc);

                ctimer_set(&tc->retransmission_timer,
                           REXMIT_TIME + (random_rand() % (REXMIT_TIME)),
                           retransmit_callback, tc);
            }
        }

        /* Our neighbor's rtmetric needs to be updated, so we bump our
           advertisements. */
        if (msg.flags & ACK_FLAGS_RTMETRIC_NEEDS_UPDATE)
        {
            bump_advertisement(tc);
        }
        set_keepalive_timer(tc);
    }
    else
    {
        stats.badack++;
    }
}
/*---------------------------------------------------------------------------*/
static void
send_ack(struct collect_conn *tc, const linkaddr_t *to, int flags)
{
    struct ack_msg *ack;
    uint16_t packet_seqno = packetbuf_attr(PACKETBUF_ATTR_PACKET_ID);

    packetbuf_clear();
    packetbuf_set_datalen(sizeof(struct ack_msg));
    ack = packetbuf_dataptr();
    memset(ack, 0, sizeof(struct ack_msg));
    ack->rtmetric = tc->rtmetric;
    ack->flags = flags;

    packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, to);
    packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE, PACKETBUF_ATTR_PACKET_TYPE_ACK);
    packetbuf_set_attr(PACKETBUF_ATTR_RELIABLE, 0);
    packetbuf_set_attr(PACKETBUF_ATTR_ERELIABLE, 0);
    packetbuf_set_attr(PACKETBUF_ATTR_PACKET_ID, packet_seqno);
    packetbuf_set_attr(PACKETBUF_ATTR_MAX_MAC_TRANSMISSIONS, MAX_ACK_MAC_REXMITS);
    unicast_send(&tc->unicast_conn, to);

    PRINTF("%d.%d: collect: Sending ACK to %d.%d for %d (epacket_id %d)\n",
           linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
           to->u8[0], to->u8[1], packet_seqno,
           packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID));

    RIMESTATS_ADD(acktx);
    stats.acksent++;
}
/*-----------------------------------------------a----------------------------*/
void add_packet_to_recent_packets(struct collect_conn *tc)
{
    /* Remember that we have seen this packet for later, but only if
       it has a length that is larger than zero. Packets with size
       zero are keepalive or proactive link estimate probes, so we do
       not record them in our history. */
    if (packetbuf_datalen() > sizeof(struct data_msg_hdr))
    {
        recent_packets[recent_packet_ptr].eseqno =
            packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID);
        linkaddr_copy(&recent_packets[recent_packet_ptr].originator,
                      packetbuf_addr(PACKETBUF_ADDR_ESENDER));
        recent_packets[recent_packet_ptr].conn = tc;
        recent_packet_ptr = (recent_packet_ptr + 1) % NUM_RECENT_PACKETS;
    }
}
/*---------------------------------------------------------------------------*/
// static void push_to_main_pkt_queue(struct collect_conn *tc)
// {
//     if (packetqueue_len(&tc->send_queue) <= MAX_SENDING_QUEUE - MIN_AVAILABLE_QUEUE_ENTRIES)
//     {
//         add_packet_to_recent_packets(tc);
//         // send_ack(tc, &ack_to, ackflags);
//         printf("SENDING\n");
//         send_queued_packet(tc);

//         struct packetqueue_item *i;

//         /* Allocate a memory block to hold the packet queue item. */
//         struct packetqueue *q = &tc->send_queue;
//         i = memb_alloc(q->memb);

//         if (i == NULL)
//         {
//             return 0;
//         }

//         /* Allocate a queuebuf and copy the contents of the packetbuf into it. */
//         i->buf = queuebuf_new_from_packetbuf();

//         if (i->buf == NULL)
//         {
//             memb_free(q->memb, i);
//             return 0;
//         }

//         i->queue = q;
//         i->ptr = tc;

//         /* Setup a ctimer that removes the packet from the queue when its
//             lifetime expires. If the lifetime is zero, we do not set a
//             lifetimer. */
//         clock_time_t lifetime = FORWARD_PACKET_LIFETIME_BASE * packetbuf_attr(PACKETBUF_ATTR_MAX_REXMIT);
//         if (lifetime > 0)
//         {
//             ctimer_set(&i->lifetimer, lifetime, remove_queued_packet, i);
//         }

//         /* Add the item to the queue. */
//         list_add(*q->list, i);
//     }
// }
/*---------------------------------------------------------------------------*/
static void
node_packet_received(struct unicast_conn *c, const linkaddr_t *from)
{
    struct collect_conn *tc = (struct collect_conn *)((char *)c - offsetof(struct collect_conn, unicast_conn));
    int i;
    struct data_msg_hdr *hdr = (struct data_msg_hdr *)malloc(PACKETBUF_HDR_SIZE);
    uint8_t ackflags = 0;
    struct collect_neighbor *n;

    // print_buf();

    memcpy(hdr, packetbuf_dataptr(), sizeof(struct data_msg_hdr));
    // memcpy(&hdr, packetbuf_hdrptr(), sizeof(struct data_msg_hdr));
    printf("RTMETRIC: %d\n", hdr->rtmetric);

    /* First update the neighbors rtmetric with the information in the
       packet header. */
    PRINTF("node_packet_received: from %d.%d rtmetric %d\n",
           from->u8[0], from->u8[1], hdr->rtmetric);
    // printf("node_packet_received: from %d.%d rtmetric %d\n",
    //        from->u8[0], from->u8[1], hdr.rtmetric);
    n = collect_neighbor_list_find(&tc->neighbor_list,
                                   packetbuf_addr(PACKETBUF_ADDR_SENDER));
    if (n != NULL)
    {
        collect_neighbor_update_rtmetric(n, hdr->rtmetric);
        update_rtmetric(tc);
    }

    /* To protect against sending duplicate packets, we keep a list of
       recently forwarded packet seqnos. If the seqno of the current
       packet exists in the list, we immediately send an ACK and drop
       the packet. */
    if (packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) ==
        PACKETBUF_ATTR_PACKET_TYPE_DATA)
    {
        // printf("DATA PACKET HAIIIII!!\n");
        linkaddr_t ack_to;
#if DEBUG
        uint8_t packet_seqno;
#endif

        stats.datarecv++;

        /* Remember to whom we should send the ACK, since we reuse the
           packet buffer and its attributes when sending the ACK. */
        linkaddr_copy(&ack_to, packetbuf_addr(PACKETBUF_ADDR_SENDER));
#if DEBUG
        packet_seqno = packetbuf_attr(PACKETBUF_ATTR_PACKET_ID);
#endif

        /* If the queue is more than half filled, we add the CONGESTED
           flag to our outgoing acks. */
        if (DRAW_TREE)
        {
            PRINTF("#A s=%d\n", packetqueue_len(&tc->send_queue));
        }
        if (packetqueue_len(&tc->send_queue) >= MAX_SENDING_QUEUE / 2)
        {
            ackflags |= ACK_FLAGS_CONGESTED;
        }

        for (i = 0; i < NUM_RECENT_PACKETS; i++)
        {
            if (recent_packets[i].conn == tc &&
                recent_packets[i].eseqno == packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID) &&
                linkaddr_cmp(&recent_packets[i].originator,
                             packetbuf_addr(PACKETBUF_ADDR_ESENDER)))
            {
                /* This is a duplicate of a packet we recently received, so we
                   just send an ACK. */
                PRINTF("%d.%d: found duplicate packet from %d.%d with seqno %d, via %d.%d\n",
                       linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                       recent_packets[i].originator.u8[0], recent_packets[i].originator.u8[1],
                       packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID),
                       packetbuf_addr(PACKETBUF_ADDR_SENDER)->u8[0],
                       packetbuf_addr(PACKETBUF_ADDR_SENDER)->u8[1]);
                send_ack(tc, &ack_to, ackflags);
                stats.duprecv++;
                return;
            }
        }

        /* If we are the sink, the packet has reached its final
           destination and we call the receive function. */
        if (tc->rtmetric == RTMETRIC_SINK)
        {
            struct queuebuf *q;

            add_packet_to_recent_packets(tc);
            // printf("AT SINK\n");

            /* We first send the ACK. We copy the data packet to a queuebuf
               first. */
            q = queuebuf_new_from_packetbuf();
            if (q != NULL)
            {
                send_ack(tc, &ack_to, 0);
                queuebuf_to_packetbuf(q);
                queuebuf_free(q);
            }
            else
            {
                PRINTF("%d.%d: collect: could not send ACK to %d.%d for %d: no queued buffers\n",
                       linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                       ack_to.u8[0], ack_to.u8[1],
                       packet_seqno);
                stats.ackdrop++;
            }

            PRINTF("%d.%d: sink received packet %d from %d.%d via %d.%d\n",
                   linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                   packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID),
                   packetbuf_addr(PACKETBUF_ADDR_ESENDER)->u8[0],
                   packetbuf_addr(PACKETBUF_ADDR_ESENDER)->u8[1],
                   from->u8[0], from->u8[1]);

            packetbuf_hdrreduce(sizeof(struct data_msg_hdr));
            /* Call receive function. */
            if (packetbuf_datalen() > 0 && tc->cb->recv != NULL)
            {
                tc->cb->recv(packetbuf_addr(PACKETBUF_ADDR_ESENDER),
                             packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID),
                             packetbuf_attr(PACKETBUF_ATTR_HOPS));
            }
            return;
        }
        else if (packetbuf_attr(PACKETBUF_ATTR_TTL) > 1 &&
                 tc->rtmetric != RTMETRIC_MAX)
        {

            /*
            Push packet into aggregation queue
            */

            /* If we are not the sink, we forward the packet to our best
               neighbor. First, we make sure that the packet comes from a
               neighbor that has a higher rtmetric than we have. If not, we
               have a loop and we inform the sender that its rtmetric needs
               to be updated. Second, we set our rtmetric in the outgoing
               packet to let the next hop know what our rtmetric is. Third,
               we update the hop count and ttl. */

            if (hdr->rtmetric <= tc->rtmetric)
            {
                ackflags |= ACK_FLAGS_RTMETRIC_NEEDS_UPDATE;
            }

            packetbuf_set_attr(PACKETBUF_ATTR_HOPS,
                               packetbuf_attr(PACKETBUF_ATTR_HOPS) + 1);
            packetbuf_set_attr(PACKETBUF_ATTR_TTL,
                               packetbuf_attr(PACKETBUF_ATTR_TTL) - 1);

            PRINTF("%d.%d: packet received from %d.%d via %d.%d, sending %d, max_rexmits %d\n",
                   linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                   packetbuf_addr(PACKETBUF_ADDR_ESENDER)->u8[0],
                   packetbuf_addr(PACKETBUF_ADDR_ESENDER)->u8[1],
                   from->u8[0], from->u8[1], tc->sending,
                   packetbuf_attr(PACKETBUF_ATTR_MAX_REXMIT));

            /* We try to enqueue the packet on the outgoing packet queue. If
               we are able to enqueue the packet, we send a positive ACK. If
               we are unable to enqueue the packet, we send a negative ACK
               to inform the sender that the packet was dropped due to
               memory problems. We first check the size of our sending queue
               to ensure that we always have entries for packets that
               are originated by this node. */
            char *dataptr = (char *)packetbuf_dataptr();
            if (dataptr[0] != 'I')
            {
                dataptr += 4;
            }
            printf("DATAPTR- %s\n", dataptr);
            int id = get_event_id(dataptr);
            printf("EVENT-ID - %d ", id);
            char mote_list[100];
            get_mote_list(dataptr, mote_list);
            printf("MOTE-LIST: %s\n", mote_list);
            struct queuebuf *q = queuebuf_new_from_packetbuf();
            printf("BEFORE HE\n");

            double rt = tc->rtmetric;
            rt = rt/RTMETRIC_MAX;
            long exp_time = -(long)(((double)1300)*rt) + 1700;
            printf("EXPIRATION TIME : %ld\n",exp_time);
            if (q != NULL)
            {
                printf("PUSHING TO AGG QUEUE\n");
                aggregation_head = pushCustomQueue(aggregation_head, id, mote_list, exp_time, q, hdr);
                printf("2AGg LIST len: %d\n", agg_list_len(aggregation_head));
                send_ack(tc, &ack_to, 0);
            }

            // if (packetqueue_len(&tc->send_queue) <= MAX_SENDING_QUEUE - MIN_AVAILABLE_QUEUE_ENTRIES &&
            //     packetqueue_enqueue_packetbuf(&tc->send_queue,
            //                                   FORWARD_PACKET_LIFETIME_BASE *
            //                                       packetbuf_attr(PACKETBUF_ATTR_MAX_REXMIT),
            //                                   tc))
            // {
            //     add_packet_to_recent_packets(tc);
            //     send_ack(tc, &ack_to, ackflags);
            //     printf("SENDING\n");
            //     send_queued_packet(tc);
            // }
            // else
            // {
            //     send_ack(tc, &ack_to, ackflags | ACK_FLAGS_DROPPED | ACK_FLAGS_CONGESTED);
            //     PRINTF("%d.%d: packet dropped: no queue buffer available\n",
            //            linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
            //     stats.qdrop++;
            // }
            // struct queuebuf *q;
            // if (q != NULL)
            // {
            // // queuebuf_to_packetbuf(q);
            // send_ack(tc, &ack_to, 0);
            // queuebuf_free(q);
            // }
        }
        else if (packetbuf_attr(PACKETBUF_ATTR_TTL) <= 1)
        {
            PRINTF("%d.%d: packet dropped: ttl %d\n",
                   linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                   packetbuf_attr(PACKETBUF_ATTR_TTL));
            send_ack(tc, &ack_to, ackflags | ACK_FLAGS_DROPPED | ACK_FLAGS_LIFETIME_EXCEEDED);
            stats.ttldrop++;
        }
    }
    else if (packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) ==
             PACKETBUF_ATTR_PACKET_TYPE_ACK)
    {
        PRINTF("Collect: incoming ack %d from %d.%d (%d.%d) seqno %d (%d)\n",
               packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE),
               packetbuf_addr(PACKETBUF_ADDR_SENDER)->u8[0],
               packetbuf_addr(PACKETBUF_ADDR_SENDER)->u8[1],
               tc->current_parent.u8[0],
               tc->current_parent.u8[1],
               packetbuf_attr(PACKETBUF_ATTR_PACKET_ID),
               tc->seqno);
        handle_ack(tc);
        stats.ackrecv++;
    }
    return;
}
/*---------------------------------------------------------------------------*/
static void
timedout(struct collect_conn *tc)
{
    struct collect_neighbor *n;
    PRINTF("%d.%d: timedout after %d retransmissions to %d.%d (max retransmissions %d): packet dropped\n",
           linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], tc->transmissions,
           tc->current_parent.u8[0], tc->current_parent.u8[1],
           tc->max_rexmits);
    PRINTF("%d.%d: timedout after %d retransmissions to %d.%d (max retransmissions %d): packet dropped\n",
           linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], tc->transmissions,
           tc->current_parent.u8[0], tc->current_parent.u8[1],
           tc->max_rexmits);

    tc->sending = 0;
    n = collect_neighbor_list_find(&tc->neighbor_list,
                                   &tc->current_parent);
    if (n != NULL)
    {
        collect_neighbor_tx_fail(n, tc->max_rexmits);
    }
    update_rtmetric(tc);
    send_next_packet(tc);
    set_keepalive_timer(tc);
}
/*---------------------------------------------------------------------------*/
static void
node_packet_sent(struct unicast_conn *c, int status, int transmissions)
{
    struct collect_conn *tc = (struct collect_conn *)((char *)c - offsetof(struct collect_conn, unicast_conn));

    /* For data packets, we record the number of transmissions */
    if (packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) == PACKETBUF_ATTR_PACKET_TYPE_DATA)
    {
        tc->transmissions += transmissions;
        PRINTF("tx %d\n", tc->transmissions);
        PRINTF("%d.%d: MAC sent %d transmissions to %d.%d, status %d, total transmissions %d\n",
               linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
               transmissions,
               tc->current_parent.u8[0], tc->current_parent.u8[1],
               status, tc->transmissions);
        if (tc->transmissions >= tc->max_rexmits)
        {
            timedout(tc);
            stats.timedout++;
        }
        else
        {
            clock_time_t time = REXMIT_TIME / 2 + (random_rand() % (REXMIT_TIME / 2));
            PRINTF("retransmission time %lu\n", time);
            ctimer_set(&tc->retransmission_timer, time,
                       retransmit_callback, tc);
        }
    }
}
/*---------------------------------------------------------------------------*/
/**
 * This function is called from a ctimer that is setup when a packet
 * is first transmitted. If the MAC layer signals that the packet is
 * sent, the ctimer will be stopped before this function is called. If
 * this function ends up being called, we add the maximum number of
 * MAC layer transmissions to the transmission count, and call the
 * retransmit function.
 */
static void
retransmit_not_sent_callback(void *ptr)
{
    struct collect_conn *c = ptr;

    PRINTF("retransmit not sent, %d transmissions\n", c->transmissions);
    c->transmissions += MAX_MAC_REXMITS + 1;
    retransmit_callback(c);
}
/*---------------------------------------------------------------------------*/
/**
 * This function is called from a ctimer that is setup when a packet
 * is sent. The purpose of this function is to either retransmit the
 * current packet, or timeout the packet. The descision is made
 * depending on how many times the packet has been transmitted. The
 * ctimer is set up in the function node_packet_sent().
 */
static void
retransmit_callback(void *ptr)
{
    struct collect_conn *c = ptr;

    PRINTF("retransmit, %d transmissions\n", c->transmissions);
    if (c->transmissions >= c->max_rexmits)
    {
        timedout(c);
        stats.timedout++;
    }
    else
    {
        c->sending = 0;
        retransmit_current_packet(c);
    }
}
/*---------------------------------------------------------------------------*/
#if !COLLECT_ANNOUNCEMENTS
static void
adv_received(struct neighbor_discovery_conn *c, const linkaddr_t *from,
             uint16_t rtmetric)
{
    struct collect_conn *tc = (struct collect_conn *)((char *)c - offsetof(struct collect_conn, neighbor_discovery_conn));
    struct collect_neighbor *n;

    n = collect_neighbor_list_find(&tc->neighbor_list, from);

    if (n == NULL)
    {
        collect_neighbor_list_add(&tc->neighbor_list, from, rtmetric);
        if (rtmetric == RTMETRIC_MAX)
        {
            bump_advertisement(tc);
        }
    }
    else
    {
        /* Check if the advertised rtmetric has changed to
           RTMETRIC_MAX. This may indicate that the neighbor has lost its
           routes or that it has rebooted. In either case, we bump our
           advertisement rate to allow our neighbor to receive a new
           rtmetric from us. If our neighbor already happens to have an
           rtmetric of RTMETRIC_MAX recorded, it may mean that our
           neighbor does not hear our advertisements. If this is the case,
           we should not bump our advertisement rate. */
        if (rtmetric == RTMETRIC_MAX &&
            collect_neighbor_rtmetric(n) != RTMETRIC_MAX)
        {
            bump_advertisement(tc);
        }
        collect_neighbor_update_rtmetric(n, rtmetric);
        PRINTF("%d.%d: updating neighbor %d.%d, etx %d\n",
               linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
               n->addr.u8[0], n->addr.u8[1], rtmetric);
    }

    update_rtmetric(tc);
}
#else
static void
received_announcement(struct announcement *a, const linkaddr_t *from,
                      uint16_t id, uint16_t value)
{
    struct collect_conn *tc = (struct collect_conn *)((char *)a - offsetof(struct collect_conn, announcement));
    struct collect_neighbor *n;

    n = collect_neighbor_list_find(&tc->neighbor_list, from);

    if (n == NULL)
    {
        /* Only add neighbors that have an rtmetric that is lower than
           ours. */
        if (value < tc->rtmetric)
        {
            collect_neighbor_list_add(&tc->neighbor_list, from, value);
            PRINTF("%d.%d: new neighbor %d.%d, rtmetric %d\n",
                   linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                   from->u8[0], from->u8[1], value);
        }
        if (value == RTMETRIC_MAX && tc->rtmetric != RTMETRIC_MAX)
        {
            bump_advertisement(tc);
        }
    }
    else
    {
        /* Check if the advertised rtmetric has changed to
           RTMETRIC_MAX. This may indicate that the neighbor has lost its
           routes or that it has rebooted. In either case, we bump our
           advertisement rate to allow our neighbor to receive a new
           rtmetric from us. If our neighbor already happens to have an
           rtmetric of RTMETRIC_MAX recorded, it may mean that our
           neighbor does not hear our advertisements. If this is the case,
           we should not bump our advertisement rate. */
        if (value == RTMETRIC_MAX &&
            collect_neighbor_rtmetric(n) != RTMETRIC_MAX)
        {
            bump_advertisement(tc);
        }
        collect_neighbor_update_rtmetric(n, value);
        PRINTF("%d.%d: updating neighbor %d.%d, etx %d\n",
               linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
               n->addr.u8[0], n->addr.u8[1], value);
    }

    update_rtmetric(tc);

#if !COLLECT_CONF_WITH_LISTEN
    if (value == RTMETRIC_MAX &&
        tc->rtmetric != RTMETRIC_MAX)
    {
        if (tc->is_router)
        {
            announcement_bump(&tc->announcement);
        }
    }
#endif /* COLLECT_CONF_WITH_LISTEN */
}
#endif /* !COLLECT_ANNOUNCEMENTS */
/*---------------------------------------------------------------------------*/
static const struct unicast_callbacks unicast_callbacks = {node_packet_received,
                                                           node_packet_sent};
#if !COLLECT_ANNOUNCEMENTS
static const struct neighbor_discovery_callbacks neighbor_discovery_callbacks =
    {adv_received, NULL};
#endif /* !COLLECT_ANNOUNCEMENTS */

void set_distance(linkaddr_t address)
{
    // Get the route entry for the sink
    // collect_depth

    // Calculate the path length from the current node to the sink
    uint8_t path_length = collect_depth(linkaddr_cmp(&linkaddr_null, &address))+1; // +1 to include the current node
    printf("DISTANCE IS %hhu\n", path_length);
    distance_to_sink = path_length;
}

void collect_open(struct collect_conn *tc, uint16_t channels,
                  uint8_t is_router, linkaddr_t address,
                  const struct collect_callbacks *cb)
{
    unicast_open(&tc->unicast_conn, channels + 1, &unicast_callbacks);
    channel_set_attributes(channels + 1, attributes);
    tc->rtmetric = RTMETRIC_MAX;
    tc->cb = cb;
    tc->is_router = is_router;
    tc->seqno = 10;
    tc->eseqno = 0;
    LIST_STRUCT_INIT(tc, send_queue_list);
    collect_neighbor_list_new(&tc->neighbor_list);
    tc->send_queue.list = &(tc->send_queue_list);
    tc->send_queue.memb = &send_queue_memb;
    collect_neighbor_init();
    /*CTIMER FOR QUEUE AGGREGATION*/
    mote_address = address;
    mode_id = address.u8[0];
    ctimer_set(&aggregation_timer, AGGREGATION_INTERVAL, aggregationCaller, NULL);
    ctimer_set(&pop_timer, POP_INTERVAL, popAggregationQueueCaller, tc);

#if !COLLECT_ANNOUNCEMENTS
    neighbor_discovery_open(&tc->neighbor_discovery_conn, channels,
                            CLOCK_SECOND * 4,
                            CLOCK_SECOND * 60,
#ifdef COLLECT_CONF_BROADCAST_ANNOUNCEMENT_MAX_TIME
                            COLLECT_CONF_BROADCAST_ANNOUNCEMENT_MAX_TIME,
#else
                            CLOCK_SECOND * 600UL,
#endif
                            &neighbor_discovery_callbacks);
    neighbor_discovery_start(&tc->neighbor_discovery_conn, tc->rtmetric);
#else /* !COLLECT_ANNOUNCEMENTS */
    announcement_register(&tc->announcement, channels,
                          received_announcement);
#if !COLLECT_CONF_WITH_LISTEN
    if (tc->is_router)
    {
        announcement_set_value(&tc->announcement, RTMETRIC_MAX);
    }
#endif /* COLLECT_CONF_WITH_LISTEN */
#endif /* !COLLECT_ANNOUNCEMENTS */

    ctimer_set(&tc->proactive_probing_timer, PROACTIVE_PROBING_INTERVAL,
               proactive_probing_callback, tc);
}
/*---------------------------------------------------------------------------*/
static void
send_keepalive(void *ptr)
{
    struct collect_conn *c = ptr;

    set_keepalive_timer(c);

    /* Send keepalive message only if there are no pending transmissions. */
    if (c->sending == 0 && packetqueue_len(&c->send_queue) == 0)
    {
        if (enqueue_dummy_packet(c, KEEPALIVE_REXMITS))
        {
            PRINTF("%d.%d: sending keepalive\n",
                   linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
            send_queued_packet(c);
        }
    }
}
/*---------------------------------------------------------------------------*/
static void
set_keepalive_timer(struct collect_conn *c)
{
    if (c->keepalive_period != 0)
    {
        ctimer_set(&c->keepalive_timer, (c->keepalive_period / 2) + (random_rand() % (c->keepalive_period / 2)),
                   send_keepalive, c);
    }
    else
    {
        ctimer_stop(&c->keepalive_timer);
    }
}
/*---------------------------------------------------------------------------*/
void collect_set_keepalive(struct collect_conn *c, clock_time_t period)
{
    c->keepalive_period = period;
    set_keepalive_timer(c);
}
/*---------------------------------------------------------------------------*/
void collect_close(struct collect_conn *tc)
{
#if COLLECT_ANNOUNCEMENTS
    announcement_remove(&tc->announcement);
#else
    neighbor_discovery_close(&tc->neighbor_discovery_conn);
#endif /* COLLECT_ANNOUNCEMENTS */
    unicast_close(&tc->unicast_conn);
    while (packetqueue_first(&tc->send_queue) != NULL)
    {
        packetqueue_dequeue(&tc->send_queue);
    }
}
/*---------------------------------------------------------------------------*/
void collect_set_sink(struct collect_conn *tc, int should_be_sink)
{
    if (should_be_sink)
    {
        tc->is_router = 1;
        tc->rtmetric = RTMETRIC_SINK;
        PRINTF("collect_set_sink: tc->rtmetric %d\n", tc->rtmetric);
        bump_advertisement(tc);

        /* Purge the outgoing packet queue. */
        while (packetqueue_len(&tc->send_queue) > 0)
        {
            packetqueue_dequeue(&tc->send_queue);
        }

        /* Stop the retransmission timer. */
        ctimer_stop(&tc->retransmission_timer);
    }
    else
    {
        tc->rtmetric = RTMETRIC_MAX;
    }
#if COLLECT_ANNOUNCEMENTS
    announcement_set_value(&tc->announcement, tc->rtmetric);
#endif /* COLLECT_ANNOUNCEMENTS */
    update_rtmetric(tc);

    bump_advertisement(tc);

    if (DRAW_TREE)
    {
        PRINTF("#A rt=0,p=0\n");
    }
}
/*---------------------------------------------------------------------------*/
int collect_send(struct collect_conn *tc, int rexmits)
{
    struct collect_neighbor *n;
    int ret;

    // struct call_arg *arg;

    packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_ID, tc->eseqno);

    /* Increase the sequence number for the packet we send out. We
       employ a trick that allows us to see that a node has been
       rebooted: if the sequence number wraps to 0, we set it to half of
       the sequence number space. This allows us to detect reboots,
       since if a sequence number is less than half of the sequence
       number space, the data comes from a node that was recently
       rebooted. */

    tc->eseqno = (tc->eseqno + 1) % (1 << COLLECT_PACKET_ID_BITS);

    if (tc->eseqno == 0)
    {
        tc->eseqno = ((int)(1 << COLLECT_PACKET_ID_BITS)) / 2;
    }
    packetbuf_set_addr(PACKETBUF_ADDR_ESENDER, &linkaddr_node_addr);
    packetbuf_set_attr(PACKETBUF_ATTR_HOPS, 1);

    packetbuf_set_attr(PACKETBUF_ATTR_TTL, MAX_HOPLIM);

    if (rexmits > MAX_REXMITS)
    {
        packetbuf_set_attr(PACKETBUF_ATTR_MAX_REXMIT, MAX_REXMITS);
    }
    else
    {
        packetbuf_set_attr(PACKETBUF_ATTR_MAX_REXMIT, rexmits);
    }

    PRINTF("%d.%d: originating packet %d, max_rexmits %d\n",
           linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
           packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID),
           packetbuf_attr(PACKETBUF_ATTR_MAX_REXMIT));

    if (tc->rtmetric == RTMETRIC_SINK)
    {
        packetbuf_set_attr(PACKETBUF_ATTR_HOPS, 0);
        if (tc->cb->recv != NULL)
        {
            tc->cb->recv(packetbuf_addr(PACKETBUF_ADDR_ESENDER),
                         packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID),
                         packetbuf_attr(PACKETBUF_ATTR_HOPS));
        }
        return 1;
    }
    else
    {

        /* Allocate space for the header. */
        packetbuf_hdralloc(sizeof(struct data_msg_hdr));

        if (packetqueue_enqueue_packetbuf(&tc->send_queue,
                                          FORWARD_PACKET_LIFETIME_BASE *
                                              packetbuf_attr(PACKETBUF_ATTR_MAX_REXMIT),
                                          tc))
        {
            send_queued_packet(tc);
            ret = 1;
        }
        else
        {
            PRINTF("%d.%d: drop originated packet: no queuebuf\n",
                   linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
            PRINTF("%d.%d: drop originated packet: no queuebuf\n",
                   linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
            ret = 0;
        }

        n = collect_neighbor_list_find(&tc->neighbor_list, &tc->parent);
        if (n != NULL)
        {
            PRINTF("%d.%d: sending to %d.%d\n",
                   linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                   n->addr.u8[0], n->addr.u8[1]);
        }
        else
        {
            PRINTF("%d.%d: did not find any neighbor to send to\n",
                   linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
#if COLLECT_ANNOUNCEMENTS
#if COLLECT_CONF_WITH_LISTEN
            PRINTF("listen\n");
            announcement_listen(1);
            ctimer_set(&tc->transmit_after_scan_timer, ANNOUNCEMENT_SCAN_TIME,
                       send_queued_packet, tc);
#else  /* COLLECT_CONF_WITH_LISTEN */
            if (tc->is_router)
            {
                announcement_set_value(&tc->announcement, RTMETRIC_MAX);
                announcement_bump(&tc->announcement);
            }
#endif /* COLLECT_CONF_WITH_LISTEN */
#endif /* COLLECT_ANNOUNCEMENTS */

            /*      if(packetqueue_enqueue_packetbuf(&tc->send_queue,
                                             FORWARD_PACKET_LIFETIME_BASE *
                                             packetbuf_attr(PACKETBUF_ATTR_MAX_REXMIT),
                                             tc)) {
        return 1;
            } else {
              PRINTF("%d.%d: drop originated packet: no queuebuf\n",
                     linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
              PRINTF("%d.%d: drop originated packet: no queuebuf\n",
                     linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
                     }*/
        }
    }
    return ret;
}
/*---------------------------------------------------------------------------*/
int collect_depth(struct collect_conn *tc)
{
    return tc->rtmetric;
}
/*---------------------------------------------------------------------------*/
const linkaddr_t *
collect_parent(struct collect_conn *tc)
{
    return &tc->current_parent;
}
/*---------------------------------------------------------------------------*/
void collect_purge(struct collect_conn *tc)
{
    collect_neighbor_list_purge(&tc->neighbor_list);
    linkaddr_copy(&tc->parent, &linkaddr_null);
    update_rtmetric(tc);
    if (DRAW_TREE)
    {
        PRINTF("#L %d 0\n", tc->parent.u8[0]);
    }
    linkaddr_copy(&tc->parent, &linkaddr_null);
}
/*---------------------------------------------------------------------------*/
void collect_print_stats(void)
{
    PRINTF("collect stats foundroute %lu newparent %lu routelost %lu acksent %lu datasent %lu datarecv %lu ackrecv %lu badack %lu duprecv %lu qdrop %lu rtdrop %lu ttldrop %lu ackdrop %lu timedout %lu\n",
           stats.foundroute, stats.newparent, stats.routelost,
           stats.acksent, stats.datasent, stats.datarecv,
           stats.ackrecv, stats.badack, stats.duprecv,
           stats.qdrop, stats.rtdrop, stats.ttldrop, stats.ackdrop,
           stats.timedout);
}
/*---------------------------------------------------------------------------*/
/** @} */
