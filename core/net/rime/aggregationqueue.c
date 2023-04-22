#include "sys/ctimer.h"
#include "packetqueue.h"
#include "sys/clock.h"
#include "contiki.h"

struct queueElement
{
  int Eid;
  char srcList[100];
  uint32_t expirationTIme;
  struct queueElement* prev;
  struct queueElement* next;
  struct queuebuf* q;
  struct data_msg_hdr* hdr_data;
};

void aggregateCustomQueue(struct queueElement **Head)
{
  struct queueElement *head = *Head;
  if (head == NULL)
  {
    return;
  }

  struct queueElement *ptr = head;
  while (ptr != NULL)
  {
    struct queueElement *it = ptr->next;
    while (it != NULL)
    {
      if (it->Eid == ptr->Eid)
      {


        // saving the ptr information for que buff and headers that are to be freed
        struct queuebuf* toFreeq=it->q;
        struct data_msg_hdr* toFreeHdr=it->hdr_data;
        
        // making a bigger source list.
        sprintf(ptr->srcList, "%s,%s", ptr->srcList, it->srcList);

        // updating the expiration time
        if (ptr->expirationTIme > it->expirationTIme)
        {
          ptr->expirationTIme = it->expirationTIme;

          toFreeq = ptr->q;
          toFreeHdr = ptr->hdr_data;

          // overwriting the queue pointer
          ptr->q = it->q;
          ptr->hdr_data = it->hdr_data;

          
        }

        queuebuf_free(toFreeq);
        free(toFreeHdr);

        // we need to delete this element now.
        struct queueElement *toDel = it;

        // updating the next and previous ptrs
        it->prev->next = it->next;

        if (it->next != NULL)
        {
          it->next->prev = it->prev;
        }
        it = it->next;

        // deleting the allocated memory for the current node
        free(toDel);
      }
      else
      {
        it = it->next;
      }
    }
    ptr = ptr->next;
  }

  *Head = head;
}

struct queueElement *pushCustomQueue(struct queueElement *head, int Eid, char srcList[100], long expirationTime, struct queuebuf *q, struct data_msg_hdr* hdr)
{

  uint32_t time_in_mill = clock_time() * (1000 / CLOCK_SECOND);

  // adding the expiration time to current time in miliseconds
  expirationTime+=time_in_mill;

  if(head==NULL)
  {
    // inserting the first element into the queue
    head = (struct queueElement *)malloc(sizeof(struct queueElement));
    head->prev = NULL;
    head->next = NULL;
    head->expirationTIme = expirationTime;
    head->Eid = Eid;
    head->q = q;
    head->hdr_data = hdr;
    sprintf(head->srcList, "%s", srcList);
    return head;
  }

  // making a new head of the queue (inserting the new element into it)
  struct queueElement *newHead = head;
  newHead = (struct queueElement *)malloc(sizeof(struct queueElement));
  newHead->prev = NULL;
  head->prev = newHead;
  newHead->next = head;
  newHead->expirationTIme = expirationTime;
  head = newHead;
  head->Eid = Eid;
  head->q = q;
  head->hdr_data = hdr;
  sprintf(head->srcList, "%s", srcList);
  return head;
}

struct queueElement *popCustomQueue(struct queueElement **Head)
{
  struct queueElement *newList = NULL;

  struct queueElement *head = *Head;
  struct queueElement *ptr = head;

  while (ptr != NULL)
  {

    uint32_t time_in_mill = clock_time() * (1000 / CLOCK_SECOND);

    // current time in miliseconds
    long currentTime = time_in_mill;

    // if (ptr->expirationTIme <= currentTime)
    if (1)
    {
      if (ptr == head)
      {
        head = head->next;
      }
      else
      {
        ptr->prev->next = ptr->next;
        ptr->next->prev = ptr->prev;
      }
      struct queueElement *toGoNext = ptr->next;
      ptr->next = newList;
      ptr->prev = NULL;
      if (newList != NULL)
      {
        newList->prev = ptr;
      }
      newList = ptr;

      ptr = toGoNext;
    }
    else
    {
      ptr = ptr->next;
    }
  }

  *Head = head;

  return newList;
}
