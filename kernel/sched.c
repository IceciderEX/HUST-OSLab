/*
 * implementing the scheduler
 */

#include "sched.h"
#include "spike_interface/spike_utils.h"

process* ready_queue_head = NULL;
process* waiting_queue_head = NULL;

//
// insert a process, proc, into the END of ready queue.
//
void insert_to_ready_queue( process* proc ) {
  sprint( "going to insert process %d to ready queue.\n", proc->pid );
  // if the queue is empty in the beginning
  if( ready_queue_head == NULL ){
    proc->status = READY;
    proc->queue_next = NULL;
    ready_queue_head = proc;
    return;
  }

  // ready queue is not empty
  process *p;
  // browse the ready queue to see if proc is already in-queue
  for( p=ready_queue_head; p->queue_next!=NULL; p=p->queue_next )
    if( p == proc ) return;  //already in queue

  // p points to the last element of the ready queue
  if( p==proc ) return;
  p->queue_next = proc;
  proc->status = READY;
  proc->queue_next = NULL;

  return;
}

//
// insert a process, proc, into the END of waiting queue(BLOCK).
// added @lab3_challenge1
void insert_to_waiting_queue( process* proc ) {
  //sprint( "going to insert process %d to waiting queue.\n", proc->pid );
  // if the queue is empty in the beginning
  if( waiting_queue_head == NULL ){
    proc->status = BLOCKED;
    proc->queue_next = NULL;
    waiting_queue_head = proc;
    return;
  }

  // waiting queue is not empty
  process *p;
  // browse the ready queue to see if proc is already in-queue
  for( p=waiting_queue_head; p->queue_next!=NULL; p=p->queue_next )
    if( p == proc ) return;  //already in queue

  // p points to the last element of the ready queue
  if( p==proc ) return;
  p->queue_next = proc;
  proc->status = BLOCKED;
  proc->queue_next = NULL;

  // sprint("current waiting queue:\n");
  // for( p=waiting_queue_head; p->queue_next!=NULL; p=p->queue_next ){
  //   sprint("%d ", p->pid);
  // }
  // sprint("\n");

  return;
}


// wake up the waiting parent if there is any
// added @lab3_challenge1
void wake_up_parent(process* proc){
  process* p;
  //sprint("waking up proc:%d's parent\n", proc->pid);
  if(waiting_queue_head == NULL) return;
  for(p = waiting_queue_head;p != NULL;p = p->queue_next){
    //sprint("cur pid:%d\n", p->pid);
    if(p->pid == proc->parent->pid){
      process* parent = p;
      if(p == waiting_queue_head){
        waiting_queue_head = waiting_queue_head->queue_next;
        insert_to_ready_queue(parent);
        return;
      }
      p = p->queue_next;
      insert_to_ready_queue(parent);
      return;
    }
  }
}

//
// choose a proc from the ready queue, and put it to run.
// note: schedule() does not take care of previous current process. If the current
// process is still runnable, you should place it into the ready queue (by calling
// ready_queue_insert), and then call schedule().
//
extern process procs[NPROC];
void schedule() {
  if ( !ready_queue_head ){
    // by default, if there are no ready process, and all processes are in the status of
    // FREE and ZOMBIE, we should shutdown the emulated RISC-V machine.
    int should_shutdown = 1;

    for( int i=0; i<NPROC; i++ )
      if( (procs[i].status != FREE) && (procs[i].status != ZOMBIE) ){
        should_shutdown = 0;
        sprint( "ready queue empty, but process %d is not in free/zombie state:%d\n", 
          i, procs[i].status );
      }

    if( should_shutdown ){
      sprint( "no more ready processes, system shutdown now.\n" );
      shutdown( 0 );
    }else{
      panic( "Not handled: we should let system wait for unfinished processes.\n" );
    }
  }

  current = ready_queue_head;
  assert( current->status == READY );
  ready_queue_head = ready_queue_head->queue_next;

  current->status = RUNNING;
  sprint( "going to schedule process %d to run.\n", current->pid );
  switch_to( current );
}
