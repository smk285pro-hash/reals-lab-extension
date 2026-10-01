# Audio Lab stem latency audit

**Start time:** 2026-10-01 11:58 Asia/Saigon

## Initial purpose

Determine why separating a selected REAPER item and receiving its stems can take 30–37 minutes, then preserve the evidence, root causes, risks, and recommended implementation order for a later fix.

Scope:
- Reals Lab extension Audio Lab UI, bridge, network transport, and REAPER integration.
- Runtime logs and downloaded artifacts under `%APPDATA%/RealsLab`.
- Public API contract documented in `C:/Users/smk28/Desktop/reals media/reals/apps/stem-app/API_DOCS.md`.
- No code changes were made during this audit.

Constraints:
- The deployed Modal server implementation was not available in the extension repository.
- Live OpenAPI loading returned no content.
- `cmake --build --preset windows` and the GitNexus CLI were rejected by the MCP command allowlist before process launch, so no fresh build/test or graph analysis was completed.

## Strategy

1. Trace the complete flow from selected REAPER item to upload, polling, download, and track insertion.
2. Compare implementation assumptions with the API contract.
3. Measure real stage timings and artifact sizes from runtime logs.
4. Separate confirmed bottlenecks from unverified server-side hypotheses.
5. Rank fixes by latency, correctness, concurrency, and operability impact.

## Checklist

- [x] Read `PLAN.md`, `SPEC.md`, `DESIGN.md`, and `AGENTS.md`.
- [x] Inspect `LabApi`, `HttpClient`, `Bridge::runLabJob`, Audio Lab UI state, selected-item handling, and stem insertion.
- [x] Inspect runtime log `C:/Users/smk28/AppData/Roaming/RealsLab/reals_ext.log`.
- [x] Inspect actual downloaded stem and ZIP sizes/timestamps.
- [x] Compare slow 2026-10-01 runs with normal 2026-09-25 runs.
- [x] Review the documented public and Studio Session API flows.
- [ ] Inspect the deployed Modal backend implementation.
- [ ] Run a fresh Windows build and test suite.
- [ ] Run controlled upload/inference/download benchmarks against Modal.
- [ ] Validate every returned WAV header, duration, and expected byte count.

## Result

### Executive conclusion

The 30–37 minute wait is not caused by REAPER track creation. The dominant observed delay is downloading four stem files from the Modal API. The extension architecture amplifies the incident by downloading stems sequentially, allowing concurrent jobs, providing no total download deadline or byte-level progress, and downloading the ZIP only after all individual WAV files have already completed.

A separate high-impact issue is that a trimmed REAPER item is not rendered before upload. The extension uploads the entire underlying source file, so upload size, GPU inference duration, and result size can be much larger than the audible item segment.

### Verified runtime timings

#### Job `367ec5d1-cebf-4fe0-8724-bfdfc5229b3b`

| Stem | Size | Start | End | Duration | Approx. throughput |
|---|---:|---:|---:|---:|---:|
| bass | 11,993,088 B | 11:22:04 | 11:29:08 | 7m 04s | ~28 KB/s |
| drums | 13,041,664 B | 11:29:08 | 11:37:06 | 7m 58s | ~27 KB/s |
| other | 6,287,163 B | 11:37:06 | 11:43:46 | 6m 40s | ~16 KB/s |
| vocals | 7,598,360 B | 11:43:46 | 11:52:15 | 8m 29s | ~15 KB/s |

Total sequential stem download time: **30m 10s** for about **38.9 MB**.

REAPER folder/track insertion then completed in about **117 ms** (`11:52:15.470` to `11:52:15.587`).

The subsequent 3,825,854-byte ZIP took about **6m 35s** in the same degraded period.

#### Job `291c24e9-4369-452a-9811-a27b950ed658`

The four stem downloads ran from `11:35:06` to `12:12:41`: **37m 35s** for about **23.5 MB**. The vocals file alone remained active for about **21m 14s**.

Its ZIP later completed in about **11 seconds**, showing that the slow transfer was not stable and that the route recovered or behaved differently after the individual stem downloads.

#### Historical comparison

On 2026-09-25, individual four-stem result sets completed in roughly **20–30 seconds** after the first download began. The 2026-10-01 transfer rate is therefore a severe runtime regression or transient server/network egress incident, not normal expected behavior.

### Confirmed implementation findings

#### P0 — The full source file is uploaded instead of the selected item segment

Relevant code:
- `extension/src/reaper_plugin.cpp::getSelectedMediaItemPath`
- `extension/src/reaper_plugin.cpp::handleItemContextAction`

The extension obtains only `PCM_source::GetFileName()`. It records item position and length for later insertion but does not render or crop the selected item before calling `/api/v1/separate`.

Consequences:
- A 10-second item cut from a 4-minute source uploads and separates the full 4-minute file.
- Take start offset, play rate, fades, item/track FX, and other audible-item semantics are not represented in the uploaded audio.
- Upload, GPU inference, server storage, and result downloads are larger than necessary.

A correct fix must define the product contract first: separate the raw source region or the fully rendered audible item including take/track processing. The temporary result should start at offset zero; insertion must not blindly reapply the original source offset/play rate.

#### P0 — Four stem downloads are serialized

Relevant code:
- `bridge/src/Bridge.cpp::runLabJob`

The client loops over stem URLs and blocks on `LabApi::downloadToFile` one stem at a time. Total result latency is the sum of all four transfers.

The API already exposes `/api/export/stems-zip/{task_id}`, but the client downloads the ZIP only after all individual WAV files are available. The ZIP is not used to create REAPER tracks.

Preferred flow:
1. Job reaches `COMPLETE`.
2. Download one ZIP with progress/resume.
3. Validate and extract locally.
4. Insert extracted stems.
5. Fall back to bounded parallel individual downloads only if ZIP retrieval fails.

#### P0 — Concurrent jobs are not isolated

Evidence:
- The two 2026-10-01 jobs downloaded concurrently.
- `Bridge::runLabJob` spawns workers without a per-job queue or concurrency limit.
- The UI stores one global `labState.file`, `itemPosition`, `itemLength`, `lastStems`, and running flag.
- `renderLabResult` automatically calls `reaper.insertStemsToFolder` using the current global state rather than immutable state attached to the returned job.

Risk:
- Job A can complete after job B changes `labState`, causing A's stems to be inserted against or below B's item.
- Concurrent downloads compete for the same degraded link and can increase total latency.

Required fix:
- Introduce immutable `LabJobContext` keyed by client job ID/server task ID.
- Capture source item GUID, source path, rendered upload path, position, length, timestamps, and desired post-processing behavior.
- Route every progress/result/error event with that ID.
- Limit active separation jobs or use an explicit queue.

#### P0 — Download timeout and cancellation are incomplete

`Bridge::runLabJob` limits polling to 300 × 2 seconds, but that limit ends when the server reports `COMPLETE`. Result downloads have no overall deadline.

WinHTTP receive timeout is an inactivity timeout. A server that continuously sends tiny chunks can keep one download alive far beyond ten minutes, as observed.

Lab downloads do not receive a cancel token from the UI and do not expose a cancel operation for the active request.

#### P0 — A truncated download may be reported as successful

Relevant code:
- `core/src/net/HttpClient.cpp::readResponse`
- `core/src/net/HttpClient.cpp::downloadToFile`

If `WinHttpQueryDataAvailable` or `WinHttpReadData` fails, the loop can exit without setting `Response::error`. `downloadToFile` then treats a 2xx status plus a writable stream as success.

The implementation does not verify received bytes against `Content-Length`, parse the WAV after download, or use a checksum. The large size differences between stems are suspicious but do not alone prove truncation; WAV header/duration validation remains pending.

Required fix:
- Propagate every WinHTTP failure with `GetLastError()`.
- Track received bytes and verify `Content-Length` when present.
- Write to `*.part`, validate the result, then atomically rename.
- Validate WAV readability, duration tolerance, sample rate, channels, and nonzero content before dispatching `lab.result`.

#### P1 — Resume support is documented but not implemented

`HttpClient.h` says `downloadToFile` supports resume, while the implementation creates/truncates the destination and sends no `Range` header. A dropped transfer restarts from zero.

#### P1 — No useful download observability

The UI receives per-stem count percentages, not byte progress. It does not show:
- Upload bytes/rate.
- Queue/cold-start duration.
- GPU inference duration.
- Per-file download bytes/rate/ETA.
- Retry count.
- Total elapsed time.

This makes a transfer stalled at 10 KB/s look like an unexplained frozen job.

#### P1 — The extension uses the one-shot API inefficiently

The extension calls `/api/v1/separate`, which uploads the complete file for that operation. The documented Studio Session API supports uploading once, retaining a `task_id`, and running stems/chords/denoise against the same session.

This should be considered for repeated Audio Lab actions and batch workflows.

#### P1 — `sendToLab` is still a stub

`extension/src/reaper_plugin.cpp::sendToLab` only displays a queued toast and writes a log line. It does not start an Audio Lab operation. Routes relying on `reaper.lab` may appear queued without performing work.

#### P1 — Audio Lab happy-path coverage is missing

Existing tests cover only limited `LabApi` platform/link behavior. No identified tests cover:
- Upload → poll → download → insert.
- Slow or interrupted downloads.
- Content-Length mismatch.
- Resume.
- Multiple concurrent jobs.
- Mapping a result back to the correct REAPER item.
- Trimmed item rendering.

#### P2 — UI strings bypass project i18n policy

Audio Lab contains hardcoded Vietnamese progress text in C++/JavaScript, including upload/download stages. Project policy requires all UI strings to pass through `tr("key")` and live in `assets/i18n/`.

### Security finding

`%APPDATA%/RealsLab/config.json` stores the Agent API credential in plaintext. The credential value is intentionally omitted from this document.

Action required:
- Rotate/revoke the current credential.
- Store future credentials through Windows DPAPI/Credential Manager.
- Ensure diagnostics, logs, crash dumps, and configuration displays never expose raw credentials.

### Unverified server-side hypotheses

These require backend source access and controlled benchmarks:
- Modal egress or container/network throttling during the incident.
- A streaming endpoint producing data slowly instead of serving a completed static artifact.
- Cold starts or server contention before `COMPLETE`.
- Per-client throttling caused by concurrent long-lived downloads.
- Incorrect HTTP caching/range/content-length behavior.

The logs prove slow client-observed transfers but do not by themselves identify which server/network layer imposed the rate.

## Verification

Verified through:
- Direct source inspection of the extension working tree.
- Runtime timestamps from `reals_ext.log`.
- File sizes and filesystem timestamps for downloaded WAV/ZIP artifacts.
- Cross-case comparison between 2026-09-25 normal transfers and 2026-10-01 degraded transfers.
- API contract comparison against `API_DOCS.md`.

Not verified:
- Fresh build/test status.
- Deployed Modal implementation details.
- Exact upload and inference durations, because the current extension does not log stage boundaries.
- Whether the differing WAV sizes are valid encodings or silent truncation.

## Corroborating paths

- `bridge/src/Bridge.cpp`
- `core/include/reals/lab/LabApi.h`
- `core/src/lab/LabApi.cpp`
- `core/include/reals/net/HttpClient.h`
- `core/src/net/HttpClient.cpp`
- `extension/src/reaper_plugin.cpp`
- `ui-web/app.js`
- `SPEC.md` §§ 3–6
- `PLAN.md` Audio Lab decisions
- `%APPDATA%/RealsLab/reals_ext.log`
- `%APPDATA%/RealsLab/lab/`
- `C:/Users/smk28/Desktop/reals media/reals/apps/stem-app/API_DOCS.md`

## Decision

**Action:** Preserve this audit as the implementation baseline. Fixes have not started.

Recommended execution order:

1. Rotate the exposed plaintext credential and move secret storage to DPAPI.
2. Add immutable per-job context and serialize/queue stem jobs to remove wrong-item risk.
3. Render the exact selected item region to a temporary WAV before upload.
4. Change result retrieval to ZIP-first download and local extraction.
5. Add total deadlines, cancellation, retries, Range resume, `.part` files, byte verification, and WAV validation.
6. Add upload/inference/download stage telemetry with bytes, rate, ETA, and elapsed time.
7. Implement the real `sendToLab` route.
8. Add integration and adversarial tests for the complete Audio Lab path.
9. Benchmark Modal separately with one controlled file and record upload, queue, inference, and egress timings.
10. Move hardcoded Audio Lab UI strings into i18n resources.

Cross-references:
- `PLAN.md`
- `SPEC.md`

No production behavior was changed by this audit.
