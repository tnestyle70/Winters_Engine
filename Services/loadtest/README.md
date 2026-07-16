# Load-test scope

Use real test accounts only and never point these scripts at production without approval. `cmd/loadtest` is the dependency-free CI/local runner; `account_replay.js` is the k6 staging profile.

The staged gates are 50, 100, 250, 500, and 1,000 scheduled RPS per endpoint. Advance only while at least 95% of the requested schedule is generated, error rate stays below 1%, p95 stays below 250 ms, no requests are dropped, DB/Redis connection pools remain below their limits, and the host is not CPU-saturated. Record the first failing gate as the current capacity boundary rather than extrapolating it.

`cmd/loadtest` accepts the bearer token through `LOADTEST_BEARER_TOKEN`. Use `LOADTEST_BODY` for JSON request bodies on Windows PowerShell, which avoids native-command quote rewriting. The runner exits nonzero when schedule coverage, error rate, or p95 misses its gate.

Replay control-plane requests and replay bytes are measured separately. `/replay/me` and presign calls hit the Go service; `.wrpl` PUT/GET bytes go directly between game server/client and S3/MinIO. Reporting S3 byte throughput as ECS API throughput would be misleading.

The UDP game server needs its own headless-client ramp and 30 Hz tick-budget evidence. HTTP RPS does not prove concurrent game-session capacity.
