/*
    Copyright 2008 Sam Chapin

    This file is part of Gospel.

    Gospel is free software: you can redistribute it and/or modify
    it under the terms of version 3 of the GNU General Public License
    as published by the Free Software Foundation.

    Gospel is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Gospel.  If not, see <http://www.gnu.org/licenses/>.
*/

#define _GNU_SOURCE
#include <unistd.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sched.h>
#include <linux/futex.h>
#include <sys/time.h>

#include "death.h"

// Compare *futex to expectedValue and, if they're equal, set *futex to newValue, atomically.
// Return the original value of *futex.
int compareAndExchange(volatile int *futex, int expectedValue, int newValue) {
  int oldValue;
  asm volatile ("movl %3,%%eax; lock cmpxchgl %2,%1; movl %%eax,%0"
               : "=r" (oldValue)
               : "m" (*futex), "r" (newValue), "r" (expectedValue)
               : "eax", "cc"
               );
  return oldValue;
}

// Add increment to *futex and return the original value of *futex, atomically.
int exchangeAndAdd(volatile int *futex, int increment) {
  asm volatile ("lock xaddl %0, %1" : : "r" (increment), "m" (*futex));
}

void increment(volatile int *futex) {
  asm volatile ("lock incl %0" : : "m" (*futex));
}
void decrement(volatile int *futex) {
  asm volatile ("lock decl %0" : : "m" (*futex));
}

// Returns true if wait was successful, or false if *futex did not have the expected value.
int futexWait(volatile int *futex, int expectedValue) {
  if (syscall(SYS_futex, futex, FUTEX_WAIT, expectedValue, 0) == -1) {
    if (errno = EWOULDBLOCK) return 0;
    die("Weird error while attempting to wait on a futex.");
  }
  return -1;
}

void futexWake(volatile int *futex, int threadsToWake) {
  if (syscall(SYS_futex, futex, FUTEX_WAKE, threadsToWake) == -1)
    die("Weird error while attempting to wake up a thread.");
}
