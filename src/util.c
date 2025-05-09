// This file is part of CPU Energy Meter,
// a tool for measuring energy consumption of Intel CPUs:
// https://github.com/sosy-lab/cpu-energy-meter
//
// SPDX-FileCopyrightText: 2003 O'Reilly (Taken from "Secure Programming Cookbook for C and C++")
// SPDX-FileCopyrightText: 2015-2021 Dirk Beyer <https://www.sosy-lab.org>
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "util.h"

#include <err.h>
#include <grp.h>
#include <sys/capability.h>
#include <unistd.h>

static int debug_enabled = 0;

void enable_debug() {
  debug_enabled = 1;
}

int is_debug_enabled() {
  return debug_enabled;
}

/*
 * The documentation regarding the capabilities was taken from the linux manual pages (i.e.,
 * http://man7.org/linux/man-pages/man3/cap_get_proc.3.html and
 * http://man7.org/linux/man-pages/man3/cap_clear.3.html) [links from Dec. 14, 2017].
 *
 * Note that in order to execute the code on linux, the 'libcap-dev'-package needs to be available
 * on the working machine.
 */

void drop_capabilities() {
  // Allocate a capability state in working storage, set the state to that of the calling process,
  // and return a pointer to the newly created capability state.
  cap_t capabilities = cap_get_proc();
  if (capabilities == NULL) {
    err(1, "Getting capabilities of process failed");
  }

  // Clear all capability flags.
  if (cap_clear(capabilities)) {
    err(1, "cap_clear failed");
  }
  if (cap_set_proc(capabilities)) {
    err(1, "Dropping capabilities failed");
  }

  // Free the releasable memory, as the capability state in working storage is no longer required.
  if (cap_free(capabilities)) {
    err(1, "cap_free failed");
  }
}

/*
 * Documentation and source code for dropping and restoring the root privileges can be found at
 * https://www.safaribooksonline.com/library/view/secure-programming-cookbook/0596003943/ch01s03.html
 * [link from Nov. 28, 2017]
 */

void drop_root_privileges_by_id(uid_t uid, gid_t gid) {
  gid_t newgid = gid > 0 ? gid : getgid();
  gid_t oldgid = getegid();
  uid_t newuid = uid > 0 ? uid : getuid();
  uid_t olduid = geteuid();

  if (olduid != 0 && oldgid != 0) {
    DEBUG("Not changing UID because not running as root (uid=%d gid=%d).", olduid, oldgid);
    return; // currently not root, nothing can be done
  }

  /*
   * If root privileges are to be dropped, be sure to pare down the ancillary groups for the process
   * before doing anything else because the setgroups() system call requires root privileges.
   */
  if (!olduid) {
    setgroups(1, &newgid);
  }

  if (newgid != oldgid) {
    if (setregid(newgid, newgid) == -1) {
      err(1, "Changing group id of process failed");
    }
  }

  if (newuid != olduid) {
    if (setreuid(newuid, newuid) == -1) {
      err(1, "Changing user id of process failed");
    }
  }

  /* verify that the changes were successful */
  if (newgid != oldgid && (setegid(oldgid) != -1 || getegid() != newgid)) {
    errx(1, "Changing group id of process failed");
  }
  if (newuid != olduid && (seteuid(olduid) != -1 || geteuid() != newuid)) {
    errx(1, "Changing user id of process failed");
  }
}

int bind_cpu(int cpu, cpu_set_t *old_context) {
  cpu_set_t cpu_context;
  CPU_ZERO(&cpu_context);
  CPU_SET(cpu, &cpu_context);

  return bind_context(&cpu_context, old_context);
}

int bind_context(cpu_set_t *new_context, cpu_set_t *old_context) {
  if (old_context != NULL) {
    if (sched_getaffinity(0, sizeof(cpu_set_t), old_context) == -1) {
      warn("Could not retrieve CPU affinity of process");
      return -1;
    }
  }

  if (sched_setaffinity(0, sizeof(cpu_set_t), new_context) == -1) {
    warn("Could not set CPU affinity of process");
    return -1;
  }

  return 0;
}

int is_cpu_offline(int cpu) {
  cpu_set_t cpu_context;
  CPU_ZERO(&cpu_context);

  if (sched_getaffinity(0, sizeof(cpu_set_t), &cpu_context) == -1) {
    warn("Could not retrieve CPU affinity of process");
    return -1;
  }

  if (!CPU_ISSET(cpu, &cpu_context)) {
    warn("CPU %d is offline", cpu);
    return 1;
  }

  return 0;
}
