/*
 * qlib.c              Matt Bishop		April 1, 2005
 *
 * Implement a very robust "queue of integers" module. This
 * code could safely be used in any program. This package allows
 * up to MAXQ queues of up to MAXELT elements of type QELT.
 *
 * Internal Representation:
 * An array of pointers queues[] contains pointers to each
 * queue; the pointer is NULL if the queue has not been created.
 * Creation is from the lowest index up. The queue structure
 * (type QUEUE) contains the queue array, a head index, a count
 * of elements, and a ticket number (see "External Representation" below).
 *
 * External Representation
 * All queues are referenced by "tickets" which (to the caller)
 * are simply integers. These tickets contain an index number and
 * a nonce. The index number refers to the element in queues[];
 * the nonce, to the version of the queue (we need this because
 * if a user is allocated a queue with ticket A, and deletes
 * that queue and creates a new one, the index in A will refer to
 * the new queue -- and we do not want that!)
 *
 * This code is for DEMONSTRATION purposes only. It emphasizes
 * robust programming. Real live production code would use other
 * constructs to improve efficiency (maximize use of pointers, perhaps ...)
 
 * This is a DYNAMIC VERSION of the queue implementation. 
 * The maximum number of queues in an array is expanded as necessary.
 * Also, number of elements in a queue is also dynamic.
 * 
 * Please refer to the other file for a dynamic version.

 */
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include "fqlib_dynamic.h"
#include "string.h"
/*
 * various macros
 */

//includes functions for debugging. Alternatively, use -DDEBUG when compiling
#define DEBUG			
#define IOFFSET	0x1221		/* used to hide index number in ticket */
#define NOFFSET	0x0502		/* used to hide nonce in ticket */
#define EMPTY	-1		/* illegal index to show nothing in queue */

/*
 * the queue structure
 */
typedef int QELT;		/* type of element being queued */
typedef struct queue {
	QTICKET ticket;		/* contains unique queue ID */
	QELT *que;       /* the actual queue */
	int head;		/* head index in que of the queue */
	int count;		/* number of elements in queue */
	int sz; 		/* current size of the queue*/
} QUEUE;

/*
 * error handling
 * all errors are returned as an integer code, and a string
 * amplifying the error is saved in here; this can then be printed
 */
char qe_errbuf[256] = "no error";	/* the error message buffer */
					/* macros to fill it */
#define ERRBUF(str) (void) strncpy(qe_errbuf, str, sizeof(qe_errbuf) - 1)
#define ERRBUF2(str, n)  (void) snprintf(qe_errbuf, sizeof(qe_errbuf), str, n)
#define ERRBUF3(str, n, m) (void) snprintf(qe_errbuf, sizeof(qe_errbuf), str, n, m)


// Queues that consists of multiple queues. 
static QUEUE **queues = NULL;		 
//nonce generator -- this MUST be non-zero always
static unsigned int noncectr = 1;	
int MAXQ = 0;  // stores the current maximum size of queues.

/*
 * generate a ticket number
 * this is an integer:
 *		index number + OFFSET,nonce
 * WARNING: both quantities must fit into 16 bits
 *
 * PARAMETERS:	int index	index number
 * RETURNED:	QTICKET		ticket of corresponding queue
 * ERRORS:	QE_INTINCON	* index + OFFSET is too big
 *				* nonce is too big
 *				* index is out of range
 *				(qe_errbuf has disambiguating string)
 * EXCEPTIONS:	none
 */
static QTICKET qtktref(unsigned int index)
{
	unsigned int high;	/* high 16 bits of ticket (index) */
	unsigned int low;	/* low 16 bits of ticket (nonce) */

	/* sanity check argument; called internally ... */
	if (index > MAXQ){
		ERRBUF3("qtktref: index %u too large (assumed less than %d)",
								index, MAXQ);
		return(QE_INTINCON);
	}

	/*
	 * get the high part of the ticket (index into queues array,
	 *   offset by some arbitrary amount)
	 * SANITY CHECK: be sure index + OFFSET fits into 16 bits as positive
	 */
	high = (index + IOFFSET)&0x7fff;
	if (high != index + IOFFSET){
		ERRBUF3("qtktref: index %u too large (assumed less than %u)",
						index, 0x7fff - IOFFSET);
		return(QE_INTINCON);
	}

	/*
	 * get the low part of the ticket (nonce)
	 * SANITY CHECK: be sure nonce fits into 16 bits
	 */
	low = (noncectr + NOFFSET) & 0xffff;
	if (low != (noncectr++ + NOFFSET) || low == 0){
		ERRBUF2("qtktref: generation number too large (max %u)\n",
							0xffff - NOFFSET);
		return(QE_INTINCON);
	}

	/* construct and return the ticket */
	return((QTICKET) ((high << 16) | low));
}

/*
	This function initialize the queues, if the queue is not created yet.
	This funciton is expected to be called from main, so the type is not static.

	RETURNED: return respective error code, and QE_NONE if no error.
*/
int initialize_queues (void) {

	if (queues != NULL){
		ERRBUF("queues is already initialized.");
		return (QE_ALREADY_INITIALIZED);
	}

	if ((queues = malloc(INITIAL_QUEUES_SIZE * sizeof(QUEUE*))) == NULL){
		ERRBUF("Cannot allocate memory for a new queues.");
		return (QE_NOROOM);
	}

	MAXQ = INITIAL_QUEUES_SIZE;
	for (int i = 0; i < MAXQ; i++) {
		queues[i] = NULL;
	}
	return (QE_NONE);
}

/*
	This function is expected to be called when the queues is full and the size 
	of the queues should be doubled.

	RETURNED: return respective error code, and QE_NONE if no error.

*/
static int double_queues_size(void){

	// Check that the queue is indeed full
	int i = 0;
	while (i < MAXQ){
		if (queues[i]== NULL){
			break;
		}
		i++;
	}
	
	if (i != MAXQ){
		ERRBUF("queues is not full and should not be doubled.");
		return (QE_NOT_FULL);
	}
	
	// Double the size of the queues
	// Note if realloc fails it does not free old pointer. Need to manualy handle.
	QUEUE** old_ptr = queues;
	if ((queues = realloc(queues, sizeof(QUEUE*) * MAXQ * 2)) == NULL) {
		ERRBUF("Cannot allocate memory for a new queues.");
		free(old_ptr);  // Free the old memory block if realloc fails
		return (QE_NOROOM);
	}

	// Initialize the new (second half) array elements to NULL
	for (int i = MAXQ; i < MAXQ * 2; i++) {
		queues[i] = NULL;
	}

	MAXQ *= 2;

	return (QE_NONE);
}


/*
 * check a ticket number and turn it into an index
 *
 * PARAMETERS:	QTICKET qno	queue ticket from the user
 * RETURNED:	int		index from the ticket
 * ERRORS:	QE_BADTICKET	queue ticket is invalid because:
 *				* index out of range [0 .. MAXQ)
 *				* index is for unused slot
 *				* nonce is of old queue
 *				(qe_errbuf has disambiguating string)
 * 		QE_INTINCON	queue is internally inconsistent beacuse:
 *				* exactly one of head, count is uninitialized
 *				* nonce is 0
 *				(qe_errbuf has disambiguating string)
 * EXCEPTIONS:	none
 */
static int readref(QTICKET qno)
{
	register unsigned index;	/* index of current queue */
	register QUEUE *q;		/* pointer to queue structure */

	/* get the index number and check it for validity */
	index = ((qno >> 16) & 0xffff) - IOFFSET;
	if (index >= MAXQ){
		ERRBUF3("readref: index %u exceeds %d", index, MAXQ);
		return(QE_BADTICKET);
	}
	if (queues[index] == NULL){
		ERRBUF2("readref: ticket refers to unused queue index %u",index);
		return(QE_BADTICKET);
	}

	/*
	 * you have a valid index; now validate the nonce; note we
	 * store the ticket in the queue, so just check that (same
	 * thing)
	 */
	if (queues[index]->ticket != qno){
		ERRBUF3("readref: ticket refers to old or uncreated queue (new=%u, old=%u)", 
				((queues[index]->ticket)&0xffff) - IOFFSET,
				(qno&0xffff) - NOFFSET);
		return(QE_BADTICKET);
	}

	/*
	 * check for internal consistencies
	 */
	if ((q = queues[index])->head < 0 || q->head >= q->sz ||
		q->count < 0 || q->count > q->sz){
		ERRBUF3("readref: internal inconsistency: head=%u,count=%u",
					q->head, q->count);
		return(QE_INTINCON);
	}
	if (((q->ticket)&0xffff) == 0){
		ERRBUF("readref: internal inconsistency: nonce=0");
		return(QE_INTINCON);
	}

	/* all's well -- return index */
	return(index);
}



/*
 * create a new queue
 *
 * PARAMETERS:	none
 * RETURNED:	QTICKET		token (if > 0); error number (if < 0)
 * ERRORS:	QE_BADPARAM	parameter is NULL
 *		QE_TOOMANYQS	too many queues allocated already
 *		QE_NOROOM	no memory to allocate new queue
 *				(qe_errbuf has descriptive string)
		QE_ALREADY_INITIALIZED if a queues is already initialized
 * EXCEPTIONS:	none
 */
QTICKET create_queue(void)
{
	register int cur;	/* index of current queue */
	register int tkt;	/* new ticket for current queue */
	
	// initialize the queues if it is not already initailized
	if (queues == NULL){
		int initialize_result; /*result of initializing queues*/
	
		if (QE_ISERROR(initialize_result = initialize_queues()))
			return (initialize_result);
	}

	/* check for array full */
	for(cur = 0; cur < MAXQ; cur++){
		if (queues[cur] == NULL)
			break;
	}
	// if queues is full, then double the queues size.
	if (cur == MAXQ){
		int double_queues_result; /*result of doubling queues size*/
		if (QE_ISERROR(double_queues_result = double_queues_size())){
			return(double_queues_result);
		}
	}

	/* allocate a new queue, and the que inside queues */
	if ((queues[cur] = malloc(sizeof(QUEUE))) == NULL){
		ERRBUF("create_queue: malloc: no more memory");
		return(QE_NOROOM);
	}

	queues[cur]->que = malloc(sizeof(QELT) * INITIAL_QUE_SIZE);
	if (queues[cur]->que == NULL) {
		ERRBUF("create_queue: malloc: no more memory for que array");
		free(queues[cur]); // Free the already allocated QUEUE structure
		return(QE_NOROOM);
	}
	queues[cur]->sz = INITIAL_QUE_SIZE;

	/* generate ticket */
	if (QE_ISERROR(tkt = qtktref(cur))){
		/* error in ticket generation -- abend procedure */
		(void) free(queues[cur]);
		queues[cur] = NULL;
		return(tkt);
	}

	/* now initialize queue entry */
	queues[cur]->head = queues[cur]->count = 0;
	queues[cur]->ticket = tkt;

	return(tkt);
}

/*
 * delete an existing queue
 *
 * PARAMETERS:	QTICKET qno	ticket for the queue to be deleted
 * RETURNED:	int		error code
 * ERRORS:	QE_BADPARAM	parameter refers to deleted, unallocated,
 *				or invalid queue (from readref()).
 * 		QE_INTINCON	queue is internally inconsistent (from
 *				readref()).
 * EXCEPTIONS:	none
 */
int delete_queue(QTICKET qno)
{
	register int cur;	/* index of current queue */

	/*
	 * check that qno refers to an existing queue;
	 * readref sets error code
	 */
	if (QE_ISERROR(cur = readref(qno)))
		return(cur);

    // First, free the dynamically allocated que array within the queue
    if (queues[cur]->que != NULL) {
        free(queues[cur]->que);
        queues[cur]->que = NULL; // Good practice to set the pointer to NULL after freeing
    }

	// Now, free the QUEUE structure itself.
	free(queues[cur]);
	queues[cur] = NULL;

	return(QE_NONE);
}

/*
	This funciton exapnds the queue when it is full.
	It updates its pointer inside queues[], and also move all its current
	content to the new exapnded queue.
*/

static int double_que_size (QTICKET qno){

	register int cur;	/* index of current queue */
	register QUEUE *q;	/* pointer to queue structure */

	/*
	 * check that qno refers to an existing queue;
	 * readref sets error code
	 */
	if (QE_ISERROR(cur = readref(qno)))
		return(cur);

	QELT *old_que = queues[cur]->que;
	if ((queues[cur]->que = realloc(queues[cur]->que, 
					sizeof(QELT)*queues[cur]->sz*2)) == NULL){
		ERRBUF("Cannot allocate memory for a new queue.");
		free(old_que);
		return(QE_NOROOM);
	}

	queues[cur]->sz *= 2;
	return (QE_NONE);
}
/*
 * add an element to an existing queue
 *
 * PARAMETERS:	QTICKET qno	ticket for the queue involved
 *		int n		element to be appended
 * RETURNED:	int		error code
 * ERRORS:	QE_BADPARAM	parameter refers to deleted, unallocated,
 *				or invalid queue (from readref()).
 * 		QE_INTINCON	queue is internally inconsistent (from
 *				readref()).
 *		QE_TOOFULL	queue has MAXELT elements and a new one can't
 *				be added
 * EXCEPTIONS:	none
 */

int put_on_queue(QTICKET qno, int n)
{
	register int cur;	/* index of current queue */
	register QUEUE *q;	/* pointer to queue structure */

	/*
	 * check that qno refers to an existing queue;
	 * readref sets error code
	 */
	if (QE_ISERROR(cur = readref(qno)))
		return(cur);

	/*
	 * add new element to tail of queue
	 */
	if ((q = queues[cur])->count == q->sz){
		int double_que_result;
		/* queue is full; need to expand the queue */
		if (QE_ISERROR(double_que_result = double_que_size(qno))){
			return (double_que_result);		
		}
	}else{
		/* append element to end */
		// Note the use of module arithmetic prevents integer overflow.
		q->que[(q->head+q->count)%q->sz] = n;
		/* one more in the queue */
		q->count++;
	}

	return(QE_NONE);
}

/*
 * take an element off the front of an existing queue
 *
 * PARAMETERS:	QTICKET qno	ticket for the queue involved
 * RETURNED:	int		error code
 * ERRORS:	QE_BADPARAM	bogus parameter because:
 *				* parameter refers to deleted, unallocated,
 *				  or invalid queue (from readref())
 *				* pointer points to NULL address for
 *				  returned element
 *				(qe_errbuf has descriptive string)
 * 		QE_INTINCON	queue is internally inconsistent (from
 *				readref()).
 *		QE_EMPTY	queue has no elements so none can be retrieved
 * EXCEPTIONS:	none
 */
int take_off_queue(QTICKET qno)
{
	register int cur;	/* index of current queue */
	register QUEUE *q;	/* pointer to queue structure */
	register int n;		/* index of element to be returned */

	/*
	 * check that qno refers to an existing queue;
	 * readref sets error code
	 */
	if (QE_ISERROR(cur = readref(qno)))
		return(cur);

	/*
	 * now pop the element at the head of the queue
	 */
	if ((q = queues[cur])->count == 0){
		/* it's empty */
		ERRBUF("take_off_queue: queue empty");
		return(QE_EMPTY);
	}
	else{
		/* get the last element */
		q->count--;
		n = q->head;
		q->head = (q->head + 1) % q->sz;
		return(q->que[n]);
	}

	/* should never reach here (sure ...) */
	ERRBUF("take_off_queue: reached end of routine despite no path there");
	return(QE_INTINCON);

}

/********** D E B U G     D E B U G     D E B U G      D E B U G ************/
#ifdef DEBUG
/*
 * list elements of queue to stdout (DEBUGGING ONLY)
 *
 * PARAMETERS:	QTICKET qno	ticket for the queue to be listed
 * RETURNED:	int		error code
 * ERRORS:	QE_BADPARAM	parameter refers to deleted, unallocated,
 *				or invalid queue (from readref())
 * 		QE_INTINCON	queue is internally inconsistent (from
 *				readref()).
 * EXCEPTIONS:	none
 */
int list_queue(QTICKET qno)
{
	register int cur;	/* index of current queue */
	register int l;		/* index of queue elements */
	register int m;		/* index of last element in queue */
	register QUEUE *q;	/* pointer to queue structure */

	/*
	 * check that qno refers to an existing queue;
	 * readref sets error code
	 */
	if (QE_ISERROR(cur = readref(qno)))
		return(cur);

	/*
	 * list the queue's contents
	 */
	/* print header (queue index and nonce) */
	q = queues[cur];
	printf("queue (index=%u, nonce=%u, count=%d, start=%d): ",
				cur, qno&0xffff, q->count, q->head);
	/* print the queue */
	if ((l = q->head) != EMPTY){
		m = (q->head + q->count) % q->sz;
		do{
			printf("%d ", q->que[l]);
			l = (l + 1) % q->sz;
		} while(l != m);
	}
	putchar('\n');

	return(QE_NONE);
}


/*
	This function is expected to be called at the end of every unit tests.
	Note inside the function, there is nothing to "check", therefore the return
	value is void.
*/
void reset_queues() {

	
    if (queues != NULL) {
        // Free all allocated queues
        for (int i = 0; i < MAXQ; i++) {
            if (queues[i] != NULL) {
                if (queues[i]->que != NULL) {
                    free(queues[i]->que);
                    queues[i]->que = NULL; 
                }
                free(queues[i]);
                queues[i] = NULL;
            }
        }

        // Free the queues array itself
        free(queues);
        queues = NULL;
    }

    // Reset the nonce counter and MAXQ to its initial value
    noncectr = 1;
    MAXQ = INITIAL_QUEUES_SIZE;
}

void reset_error_status(void){
	strncpy(qe_errbuf, "no error",sizeof(qe_errbuf));
}
#endif