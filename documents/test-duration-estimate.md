# ContactManager Test Duration Estimate

Companion to `documents/test-specification.md`. It gives how long one full cycle of the tests takes, by area, with the basis for each figure.

A cycle runs every test once. It takes about **12 hours 26 minutes to 16 hours 55 minutes** in total. Of that, about **4 hours 13 minutes to 4 hours 45 minutes** needs a person (the manual and screen-reader tests). The rest runs unattended, but area 1 needs the network and the disposable Google account, and area 2 needs the accelerated harness.

---

## 1. Summary

| Area | Tests | Automated | Person's time | Total (low to high) |
|---|---|---|---|---|
| 1 Google sync (live) | 42 new, 132 imported (one is U01) | 5 h 28 min to 8 h 02 min | none | 5 h 28 min to 8 h 02 min |
| 2 Announcements (accelerated) | 47 new | 1 h 59 min to 3 h 06 min | none | 1 h 59 min to 3 h 06 min |
| Unit suite (112 tests, areas 1, 2 and 3b) | 112 imported | 3 min to 5 min | none | 3 min to 5 min |
| 3a Start-up and shutdown | 15 new, 6 imported, 1 old-file test | 8 min | 45 min | 54 min to 56 min |
| 3b By menu and function | 53 new, 13 imported | 17 min | 1 h 20 min | 1 h 36 min to 1 h 42 min |
| 3c By dialog | 28 new, 12 imported (plus 2 accelerated new tests, DL-006 and DL-022) | 15 min | 56 min to 1 h 10 min | 1 h 11 min to 1 h 30 min |
| 4 Accessibility | 18 new, 2 imported | 3 min | 1 h 12 min to 1 h 30 min | 1 h 15 min to 1 h 33 min |
| **Total** | **205 new, 279 imported** | | **4 h 13 min to 4 h 45 min** | **12 h 26 min to 16 h 55 min** |

The unit suite is one build and one run, covering all 112 unit tests (84 in area 2, 16 in area 3b and 12 in area 1). The bench scenario L01 is the bench's own runner for the same suite, so it is not counted again.

---

## 2. Basis of the figures

### 2.1 Measured figures (from earlier runs)

| Measure | Value | Source |
|---|---|---|
| Median time per archive bench scenario | 77 s | 163 timed scenario runs in the scratch bench folder (`bench/runs/`), U01 excluded |
| 90th percentile per scenario | 102 s | As above |
| Longest archive scenario | 415 s (N108) | As above |
| U01 (purge, 250 contacts and 1,000 appointments, both halves) | 1,062 s (17 min 42 s) | Run recorded in `test-results/` (4 November) |
| Messaging scenarios at 60 times real time | 45 real minutes for 47 plan hours | `test-results/20261104-0900/` |

### 2.2 Assumed figures (to be confirmed at the first full cycle)

| Item | Assumption | Why |
|---|---|---|
| New Google test (area 1) | 3 to 5 minutes each | A sync takes about 77 s to 102 s. A test with a manual edit on Google takes longer. |
| Accelerated speed (area 2) | 360 times real time: 10 real seconds = 1 hour | Specification 0.7. A three-day run is 12 minutes. |
| Accelerated batches | 8 batches, each of about six scenarios on separate contacts, each run for three days, plus one combined three-day run (AN-047) | One batch is 12 minutes plus 1 minute to set up. The combined run is 13 minutes. DL-006 and DL-022 take about 2 minutes together. |
| Accelerated re-runs | Up to 1.5 times the batch time | Allows for one re-run of a check. |
| Clean-up | 15 to 30 minutes, before and after a full run | The clean-up deletes each record with a 0.2 s pause, and a full run leaves about 1,250 records. |
| Manual tests (3a to 3c) | 1.5 to 3 minutes each | Set-up, the action, and the check. |
| Manual accessibility tests (4) | 4 to 5 minutes each | With a screen reader, the tester listens through each dialog. |
| Unit suite | 3 to 5 minutes | Compiling the test file and linking it with the built application. The tests themselves run in well under a second. |

### 2.3 What each area counts

- **Area 1** counts the 42 new live tests and the 132 archive scenarios that use Google. U01 is one of the 132, and its measured time (17 min 42 s) is used instead of the median. The other 131 use the median (77 s) for the low figure and the 90th percentile (102 s) for the high figure. The clean-up is counted once.
- **Area 2** counts the 47 new accelerated tests. The announcement unit tests are in the unit suite line.
- **Areas 3a to 4** count the new manual tests, and the imported scenarios that the bench driver runs automatically (31 menu, dialog and start-up scenarios, and 2 accessibility scenarios).
- **The old-file test** (`autotest/archive/oldfile/`) is counted with area 3a, and takes a few seconds.

---

## 3. Notes on the figures

1. **The accelerated clock.** At 360 times real time, one real second is six minutes of program time. A test with an expected time to the minute can be up to one check late, which is six minutes. Specification 0.7 writes that tolerance in, and it means the runs cannot be shortened by going faster.
2. **Google limits.** The bench waits when Google rate-limits it, which adds time to a full run. Those waits are inside the measured figures.
3. **Re-runs.** A failure in area 1 is re-run once before it is reported. The high figure for area 1 allows for a second run of the failing tests.
4. **One cycle.** The figures are for one full cycle. Regression runs after each change are not estimated.
5. **First-time set-up.** Setting up the disposable account (`autotest/utilities/setup_test_account.py`) takes about 10 minutes of a person's time, plus the Google Cloud steps. It is done once, so it is not in the table.

---

## 4. Risks to the estimate

| Risk | Effect | Mitigation |
|---|---|---|
| Google changes its limits or latency | Area 1 takes longer, and the rate-limit tests (GS-036) change | Measure again at the first full cycle, and update this estimate. |
| The accelerated harness is not yet in the repository (test specification Appendix C, item 3) | Area 2 cannot be run from the repository until it is brought in | A prerequisite. The estimate assumes it is in place. |
| The screen-reader tests need a person to listen | The area 4 time depends on the tester | Schedule the tests with the tester, and record the time spent. |
| The "Both" tests in area 1 are run under each configuration | Area 1 runs more than once for those tests | The estimate counts each run. The high figure allows for the second configuration. |

---

## 5. Recording actual times

Each run records, in its evidence folder, the start and end time and the time for each test. The first full cycle replaces the assumed figures in section 2.2 with measured ones, and this estimate is updated with the date of that cycle.
