/* The main thread acquires locks A and B, then it creates two
   higher-priority threads.  Each of these threads blocks
   acquiring one of the locks and thus donate their priority to
   the main thread.  The main thread releases the locks in turn
   and relinquishes its donated priorities.
   
   Based on a test originally submitted for Stanford's CS 140 in
   winter 1999 by Matt Franklin <startled@leland.stanford.edu>,
   Greg Hutchins <gmh@leland.stanford.edu>, Yu Ping Hu
   <yph@cs.stanford.edu>.  Modified by arens. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/thread.h"

static thread_func a_thread_func;
static thread_func b_thread_func;

void
test_priority_donate_multiple (void) 
{
  struct lock a, b;

  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);

  /* Make sure our priority is the default. */
  ASSERT (thread_get_priority () == PRI_DEFAULT);

  lock_init (&a);
  lock_init (&b);

  lock_acquire (&a);
  lock_acquire (&b);

  thread_create ("a", PRI_DEFAULT + 1, a_thread_func, &a); // 32
  msg ("Main thread should have priority %d.  Actual priority: %d.", 
       PRI_DEFAULT + 1, thread_get_priority ());
//a => main => "a" , main이 32로 됨. (main이 current)
  thread_create ("b", PRI_DEFAULT + 2, b_thread_func, &b);
  msg ("Main thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 2, thread_get_priority ());
//b => main => "b" main이 33으로됨. (main이 current)
  lock_release (&b); // b holder = b, main = 32
  msg ("Thread b should have just finished.");
  msg ("Main thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 1, thread_get_priority ());
// b가 실행되고, main은 다시 32가 되어야함.
  lock_release (&a); // a holder = a, main = 31
  msg ("Thread a should have just finished.");
  msg ("Main thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT, thread_get_priority ());
}
// a가 실행되고, main은 다시 31이 되어야함.
static void
a_thread_func (void *lock_) 
{
  struct lock *lock = lock_;

  lock_acquire (lock); // main priority = 32 // a의 waiters상태는 지금 main => thread a
  msg ("Thread a acquired lock a.");
  lock_release (lock);
  msg ("Thread a finished.");
}

static void
b_thread_func (void *lock_) 
{
  struct lock *lock = lock_;

  lock_acquire (lock); // main priority = 33
  msg ("Thread b acquired lock b.");
  lock_release (lock);
  msg ("Thread b finished.");
}
