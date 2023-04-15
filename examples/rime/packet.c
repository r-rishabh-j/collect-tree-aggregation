#include "contiki.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include "net/rime/collect.h"
#include "dev/leds.h"
#include "dev/button-sensor.h"
#include "net/netstack.h"
#include <stdio.h>
#include <stdlib.h>

#define MAX_HOPS 2
#define MAX_PACKET_LIST_SIZE 16
// #define DEBUG_LEVEL 1
#define ACK_RECEIVED 0x88
#define ACK_FLAGS_RTMETRIC_NEEDS_UPDATE 0x10
#define ACK_FLAGS_LIFETIME_EXCEEDED     0x20
#define ACK_FLAGS_DROPPED               0x40
#define ACK_FLAGS_CONGESTED             0x80

static struct collect_conn tc;
// static uint16_t last_hop_routing_metric;

struct packet_element
{
    uint8_t seq_no; // contains sequence number
    linkaddr_t origin; // origin of the packet
    struct collect_conn *connection_info; // stores connection information
};

// packet list contains recent packets
struct packet_element packet_list[MAX_PACKET_LIST_SIZE];

struct header_data
{
  uint8_t flags, dummy_var;
  uint16_t routing_metric; // contains routing metric information
};

int counter = 0; // list counter

/*---------------------------------------------------------------------------*/
PROCESS(example_collect_process, "Test collect process");
AUTOSTART_PROCESSES(&example_collect_process);
/*---------------------------------------------------------------------------*/

struct ack_msg {
  uint8_t flags, dummy;
  uint16_t rtmetric;
};

uint8_t ACK_MESSAGE_FLAG = 0;

static void
recv(const linkaddr_t *originator, uint8_t seqno, uint8_t hops)
{
    if (hops > MAX_HOPS)
    {
        struct ack_msg ACK;
        ACK_MESSAGE_FLAG|=ACK_FLAGS_DROPPED;
        memcpy(packetbuf_dataptr(), &ACK_MESSAGE_FLAG,sizeof(uint8_t));
        printf("Packet from %d.%d, seqno %d dropped since hops exceeded limit\nACK FLAG: 0x%02x\n", 
        originator->u8[0], originator->u8[1], seqno, ACK_MESSAGE_FLAG);
        ACK_MESSAGE_FLAG=0;
    }
    else
    {   
        printf("Sink got message from %d.%d, seqno %d, hops %d: len %d '%s'. Metric:%d\n",
               originator->u8[0], originator->u8[1],
               seqno, hops,
               packetbuf_datalen(),
               (char *)packetbuf_dataptr(), 1);

        printf("ACK: Node %d received message from %d - Message Recieved\n", 1, originator->u8[0]);
        ACK_MESSAGE_FLAG|=ACK_RECEIVED;
        memcpy(packetbuf_dataptr(), &ACK_MESSAGE_FLAG,sizeof(uint8_t));
        ACK_MESSAGE_FLAG=0;


        // if (counter < MAX_PACKET_LIST_SIZE)
        // {
        printf("Adding packet from %d.%d, seqno %d to packet list\n", originator->u8[0], originator->u8[1], seqno);
        struct packet_element packet;
        packet.seq_no = seqno;
        packet.origin = *originator;
        packet.connection_info = &tc;
        packet_list[(counter++)%MAX_PACKET_LIST_SIZE] = packet;

        int i=0;
        printf("PACKET-LIST: ");
        for(i=0;i<counter;i++){
            printf("|seq:%d, origin:%d", packet_list[i].seq_no, (packet_list[i].origin).u8[0]);
        }
        printf("\n");
        // }
    }
}
/*---------------------------------------------------------------------------*/
static const struct collect_callbacks callbacks = {recv};
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_collect_process, ev, data)
{
    static struct etimer periodic;
    static struct etimer et;

    PROCESS_BEGIN();

    collect_open(&tc, 130, COLLECT_ROUTER, &callbacks);

    if (linkaddr_node_addr.u8[0] == 1 &&
        linkaddr_node_addr.u8[1] == 0)
    {
        printf("I am sink\n");
        collect_set_sink(&tc, 1);
    }

    /* Allow some time for the network to settle. */
    etimer_set(&et, 120 * CLOCK_SECOND);
    PROCESS_WAIT_UNTIL(etimer_expired(&et));

    // printf("BUFF- %d\n", PACKETBUF_HDR_SIZE);

    while (1)
    {

        /* Send a packet every 30 seconds. */
        etimer_set(&periodic, CLOCK_SECOND * 30);
        etimer_set(&et, random_rand() % (CLOCK_SECOND * 30));

        PROCESS_WAIT_UNTIL(etimer_expired(&et));

        {
            static linkaddr_t oldparent;
            const linkaddr_t *parent;

            printf("Sending\n");
            packetbuf_clear();
            
            // set MAX_HOPS
            packetbuf_set_attr(PACKETBUF_ATTR_TTL, MAX_HOPS);

            packetbuf_set_datalen(sprintf(packetbuf_dataptr(), "%s", "Hello") + 1);

            collect_send(&tc, 15);

            // get routing metric
            struct header_data header_content;
            memcpy(&header_content, packetbuf_dataptr(), sizeof(struct header_data));
            printf("Routing metric from header - %d\n", header_content.routing_metric);

            parent = collect_parent(&tc);
            if (!linkaddr_cmp(parent, &oldparent))
            {
                if (!linkaddr_cmp(&oldparent, &linkaddr_null))
                {
                    printf("#L %d 0\n", oldparent.u8[0]);
                }
                if (!linkaddr_cmp(parent, &linkaddr_null))
                {
                    printf("#L %d 1\n", parent->u8[0]);
                }
                linkaddr_copy(&oldparent, parent);
            }
        }

        PROCESS_WAIT_UNTIL(etimer_expired(&periodic));
    }

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
