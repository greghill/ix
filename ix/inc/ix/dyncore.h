/*
 * dyncore.h -
 */

#pragma once

typedef int (*dune_worker_cb) (void *);
typedef int (*dune_worker_pending) (void *);

extern int dyncore_init(dune_worker_cb cb, dune_worker_pending pending);
