# Load Balancer

A load balancer built from scratch in C++, implementing L4 (TCP) and L7 (HTTP-aware)
proxying, health checks, connection draining, and multiple balancing algorithms.

## Goal

Given N backend servers of varying health and capacity, route client traffic to keep
every healthy backend within a reasonable load band, detect and route around failures
within a bounded time, and do all of this without dropping in-flight requests —
with benchmarks to prove it, not just claims.

## Why this project

Built to go deeper on load balancing internals, inspired by researching Maglev / AWS
NLB & ALB and my networking coursework at UCSD (TCP reliability, flow control,
congestion control, router design), and working on real system project.

## Roadmap / Milestones

- [ ] **M1 — Minimal L4 load balancer**
Simple backend that listen on a port and return hello message
  Accepts TCP connections, forwards bytes to a backend chosen via round robin.
  No health checks yet — just prove routing works end-to-end.

  Progress:
  - [x] Mock backend: listens on a port, replies "Hello from <id>" (and `/health`).
  - [x] Backend pool + round robin: `std::vector<Server>` and an
    `atomic<size_t>` counter; `pick_server()` returns the next backend.
  - [x] Outbound leg: `connect_to_server()` opens a TCP socket to the chosen
    backend (`socket` → `inet_pton`/`htons` → `connect`). Verified end-to-end
    with a throwaway `main` that sends one request and prints the reply.

  Next steps to finish M1:
  - [ ] Listener: `make_listener(8080)` (`socket` → `setsockopt(SO_REUSEADDR)`
    → `bind` → `listen`, backlog 128). LB's front door.
  - [ ] Accept loop: replace the test `main` with `while (accept) handle(fd)`.
  - [ ] `handle_connection()`: forward client → backend, then backend → client
    (the test's read/write loop, writing to `client_fd` instead of stdout).
  - [ ] Prove routing: `for i in {1..6}; do curl -s localhost:8080; done`
    should rotate backend1/2/3/1/2/3.

  Known follow-ups (tracked, not blocking M1):
  - Single-threaded for now → `std::thread(handle_connection, fd).detach()`
    once rotation works (this is where the atomic counter starts to matter).
  - One `read()` per request assumes the whole request fits in one chunk →
    replace with two independent pump loops + `shutdown(SHUT_WR)` for correct
    L4 byte streaming.
  - `signal(SIGPIPE, SIG_IGN)` so a client hanging up mid-response can't kill
    the process.

- [ ] **M2 — Health checks**
  Background thread pings each backend on an interval. Backend marked unhealthy
  after N consecutive failures, healthy again after N consecutive successes
  (avoids flapping). Dead backends stop receiving new connections.

- [ ] **M3 — Multiple balancing algorithms**
  Round robin, least-connections, weighted round robin, consistent hashing
  (for session affinity). Benchmark each against uneven backend capacity.

- [ ] **M4 — Connection draining**
  Backend can be marked "draining" — stays in the pool for in-flight requests,
  excluded from new routing decisions. Simulates safe rolling deploys.

- [ ] **M5 — L7 HTTP-aware layer**
  Parse HTTP requests. Add path-based routing, header inspection, sticky
  sessions via cookies. Compare overhead vs. the L4 version.

- [ ] **M6 — Benchmark & document**
  Load test with `wrk`/`hey`. Report p50/p99 latency, throughput, and LB
  overhead vs. hitting a backend directly. Publish results in this README.

## Key Decisions

| Decision | Choice | Reasoning |
|---|---|---|
| Language | C++ | Same sockets API as prior networking coursework (C); RAII removes manual cleanup bugs; forces real reasoning about concurrency/memory instead of getting it for free (as with Go). Trade-off: async I/O is rougher — starting thread-per-connection, may move to epoll-based event loop later for scale. |
| Concurrency model | Thread-per-connection (initial) | Simple and correct first; documents a clear evolution story toward an event loop if revisited. |
| Layer scope | Build both L4 and L7 | L4 (TCP passthrough) is fast but blind to content; L7 (HTTP-aware) enables smart routing at higher CPU cost. Building both lets me quantify the trade-off instead of asserting it. |
| Test backends | Lightweight mock HTTP servers, N processes on separate ports, Dockerized | Real servers/VMs are unnecessary infra overhead for this problem. Docker Compose makes the whole system reproducible with one command and makes it trivial to kill/slow a single backend on demand for failure-mode demos. |
| Failure injection | Mock backends expose `/simulate?delay=`, `/simulate?fail=`, `/kill` | Needed to test and demo health checks, draining, and degraded-backend scenarios — not just the happy path. |

## Getting Started

```bash
git clone <repo-url>
cd loadbalancer
docker-compose up
```

This spins up N mock backends and the load balancer. See `/benchmarks` for
scripts to reproduce the load test results below.

## Benchmark Results

_TBD — filled in as milestones are completed._

## Repo Structure

```
/lb              # load balancer source (C++)
/mock-backend    # lightweight test backend server
/benchmarks      # wrk/hey scripts + results
```