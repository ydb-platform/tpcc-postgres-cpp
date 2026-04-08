# coroutines_vs_threads

Measures the overhead of context switches: threads (OS scheduling, kernel transitions) vs coroutines (cooperative user-space scheduling).

The workload is a tight compute loop with a period-6 conditional branch pattern. Each worker runs for a short time slice, then yields (thread: `sleep_for`; coroutine: `co_await`). With many workers sharing few cores, thread mode pays kernel transition costs on every yield/wake cycle, while coroutine mode switches entirely in user space.

## Usage

```bash
# Thread mode: 1..64 concurrent threads, doubling
coroutines_vs_threads --from 1 --to 64

# Coroutine mode: 1..1024 coroutines on 4 pool threads
coroutines_vs_threads --from 1 --to 1024 --pool_size 4 --coro

# Custom time slice (500 µs)
coroutines_vs_threads --from 4 --to 256 --coro --pool_size 4 --slice_us 500
```

## Flags

| Flag | Default | Description |
|------|---------|-------------|
| `--from` | (required) | Starting concurrency count |
| `--to` | (required) | Ending concurrency count (step ×2) |
| `--coro` | false | Use coroutines instead of threads |
| `--pool_size` | 1 | Task-queue thread count (coroutine mode) |
| `--slice_us` | 1 | Microseconds a worker runs before yielding |
