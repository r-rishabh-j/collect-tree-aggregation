#include "sys/ctimer.h"
#include "packetqueue.h"


struct queueElement{
  int Eid;
  char srcList[100];
  long expirationTIme;
  struct queueElement* prev;
  struct queueElement* next;
};

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


struct queueElement* aggregateCustomQueue(struct queueElement *head)
{
  if(head==NULL)
  {
    return head;
  }

  struct queueElement* ptr=head;
  while(ptr!=NULL)
  {
    struct queueElement* it=ptr->next;
    while(it!=NULL)
    {
      if(it->Eid==ptr->Eid)
      {
        //making a bigger source list.
        sprintf(ptr->srcList,"%s,%s",ptr->srcList,it->srcList);
        
        // updating the expiration time 
        if(ptr->expirationTIme<it->expirationTIme)
        {
          ptr->expirationTIme=it->expirationTIme;
        }

        // we need to delete this element now.
        struct queueElement* toDel=it;

        // updating the next and previous ptrs
        it->prev->next = it->next;
        
        if(it->next!=NULL)
        {
          it->next->prev = it->prev;
        }
        it=it->next;
        
        // deleting the allocated memory for the current node
        free(toDel);
      }
      else
      {
        it=it->next;
      }
    }
    ptr=ptr->next;
  }

  return head;
}

struct queueElement* pushCustomQueue(struct queueElement *head,int Eid,char srcList[100],long expirationTime)
{
  if(head==NULL)
  {
    // inserting the first element into the queue
    head = (struct queueElement *)malloc(sizeof(struct queueElement));
    head->prev=NULL;
    head->next=NULL;
    head->expirationTIme=expirationTime;
    return head;
  }

  // making a new head of the queue (inserting the new element into it)
  struct queueElement *newHead = head;
  newHead = (struct queueElement *)malloc(sizeof(struct queueElement));
  newHead->prev = NULL;
  head->prev = newHead;
  newHead->next=head;
  newHead->expirationTIme=expirationTime;
  head = newHead;

  return head;
}

struct queueElement* popCustomQueue(struct queueElement *head)
{
  struct queueElement *newList=NULL;

  struct queueElement *ptr=head;
  
  while(ptr!=NULL)
  {
    long currentTime;
    if(ptr->expirationTIme<=currentTime)
    {
      if(ptr==head)
      {
        head=head->next;
      }
      else
      {
        ptr->prev->next=ptr->next;
        ptr->next->prev=ptr->prev;
      }
      struct queueElement *toGoNext=ptr->next;
      ptr->next=newList;
      ptr->prev=NULL;
      if(newList!=NULL)
      {
        newList->prev=ptr;
      }
      newList=ptr;

      ptr=toGoNext;
    }
    else
    {
      ptr=ptr->next;
    }
  }
}