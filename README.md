# TPE-PROTOS

This project implements a SOCKSv5 proxy server.  The code now includes
basic administration helpers and runtime metrics collection.

## Administration helpers

* `admin_add_user(user, pass)`: append a new user credential to
  `src/authentication/users.txt`.

## Metrics API

`metrics.c` provides simple counters:

* total connections ever created
* current active connections
* total bytes transferred through the proxy

Call `metrics_get()` to retrieve a snapshot of these values.