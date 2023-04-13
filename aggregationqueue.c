#include "sys/ctimer.h"
#include "packetqueue.h"

/*---------------------------------------------------------------------------*/
void packetqueue_init(struct packetqueue *q)
{
  list_init(*q->list);
  memb_init(q->memb);
}
/*---------------------------------------------------------------------------*/
static void remove_queued_packet(void *item)
{
  struct packetqueue_item *i = item;
  struct packetqueue *q = i->queue;

  list_remove(*q->list, i);
  queuebuf_free(i->buf);
  ctimer_stop(&i->lifetimer);
  memb_free(q->memb, i);
  /*  printf("removing queued packet due to timeout\n");*/
}
/*---------------------------------------------------------------------------*/
int packetqueue_enqueue_packetbuf(struct packetqueue *q, clock_time_t lifetime,
                                  void *ptr)
{
  struct packetqueue_item *i;

  /* Allocate a memory block to hold the packet queue item. */
  i = memb_alloc(q->memb);

  if (i == NULL)
  {
    return 0;
  }

  /* Allocate a queuebuf and copy the contents of the packetbuf into it. */
  i->buf = queuebuf_new_from_packetbuf();

  if (i->buf == NULL)
  {
    memb_free(q->memb, i);
    return 0;
  }

  i->queue = q;
  i->ptr = ptr;

  /* Setup a ctimer that removes the packet from the queue when its
     lifetime expires. If the lifetime is zero, we do not set a
     lifetimer. */
  if (lifetime > 0)
  {
    ctimer_set(&i->lifetimer, lifetime, remove_queued_packet, i);
  }

  /* Add the item to the queue. */
  list_add(*q->list, i);

  return 1;
}
/*---------------------------------------------------------------------------*/
struct packetqueue_item *
packetqueue_first(struct packetqueue *q)
{
  return list_head(*q->list);
}
/*---------------------------------------------------------------------------*/
void packetqueue_dequeue(struct packetqueue *q)
{
  struct packetqueue_item *i;

  i = list_head(*q->list);
  if (i != NULL)
  {
    list_remove(*q->list, i);
    queuebuf_free(i->buf);
    ctimer_stop(&i->lifetimer);
    memb_free(q->memb, i);
  }
}
/*---------------------------------------------------------------------------*/
int packetqueue_len(struct packetqueue *q)
{
  return list_length(*q->list);
}
/*---------------------------------------------------------------------------*/
struct queuebuf *
packetqueue_queuebuf(struct packetqueue_item *i)
{
  if (i != NULL)
  {
    return i->buf;
  }
  else
  {
    return NULL;
  }
}
/*---------------------------------------------------------------------------*/
void *
packetqueue_ptr(struct packetqueue_item *i)
{
  if (i != NULL)
  {
    return i->ptr;
  }
  else
  {
    return NULL;
  }
}
/*---------------------------------------------------------------------------*/
/** @} */
