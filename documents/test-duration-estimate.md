# ContactManager Test Duration Estimate

Companion to `documents/test-specification.md`. It gives how long one full cycle of the tests takes, by area, with the basis for each figure.

A cycle runs every test once. It takes about **8 hours 30 minutes to 12 hours 24 minutes** of machine time and person time together. That excludes the capacity stress test in section 5 of the test specification (U01, 17 minutes 42 seconds), which is not part of a normal cycle. Only the screen-reader tests in area 4 need a person, about **1 hour 12 minutes to 1 hour 30 minutes**. Everything else is automated: area 1 needs the network and the disposable Google account, and area 2 needs the accelerated harness.

---

## 1. Summary

| Area | Prefixes | Tests | Automated | Person's time | Total (low to high) |
|---|---|---|---|---|---|
| 1 Google sync (live system access) | `GS`, `IG` | 24 tests (47 runs), 142 imported (U01 is in section 5) | 5 h 38 min to 8 h 26 min | none | 5 h 38 min to 8 h 26 min |
| 5 Capacity stress test (not part of the normal cycle) | `CS` (CS-001); the imported U01 keeps its archive ID | CS-001, the imported U01 | 17 min 42 s | none | 17 min 42 s, shown separately |
| 2 Announcements (offline, accelerated clock, stub transports) | `AN` | 48 new, in 10 parallel runs | 16 min to 17 min (the longest run) | none | 16 min to 17 min |
| 6 Unit suite | `UT-001` to `UT-114` | 114 imported | 3 min to 5 min | none | 3 min to 5 min |
| 3 Operation testing | `SS`, `MF`, `DL`, `IO`, `OF` | 97 new, 20 imported (IO-001 to IO-020), 1 old-file test (OF-001) | 1 h 18 min to 2 h 03 min | none | 1 h 18 min to 2 h 03 min |
| 4 Accessibility (manual, with automated checks) | `AC`, `IA` | 18 new, 2 imported | 3 min | 1 h 12 min to 1 h 30 min | 1 h 15 min to 1 h 33 min |
| **Total (cycle, excluding section 5)** | — | **187 new, 278 imported** | **7 h 18 min to 10 h 54 min** | **1 h 12 min to 1 h 30 min** | **8 h 30 min to 12 h 24 min** |

The unit suite is one build and one run, covering all 114 unit tests: 84 announcement tests (area 2), 22 calendar tests (area 3b) and 8 sync tests (area 1). The bench scenario L01 is the bench's runner for the same suite, so it is not counted again.

Area 3 is automated. Its figures are the operation runner's runs through the test driver, not a person's time. Each new test is one driver run, about 20 to 40 seconds with the program's start-up and the six fixture contacts. The imported scenarios are the bench's own runs, at the same measured times as area 1.

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
| Google requests within a bench driver run | median 38 s of run time, of which median 20 s is Google requests | 212 driver runs, scratch bench folder (see section 3) |

### 2.2 Assumed figures (to be confirmed at the first full cycle)

| Item | Assumption | Why |
|---|---|---|
| New Google test (area 1) | 3 to 5 minutes each | A sync takes about 77 s to 102 s. A test with a manual edit on Google takes longer. |
| Accelerated speed (area 2) | 360 times real time: 10 real seconds = 1 hour | Specification 0.7. A three-day run is 12 minutes. |
| Accelerated runs | Ten runs in parallel, each with its own data folder and clock (test specification section 2.0): A (3 virtual days, including AN-016, a daily repeat over three days), B (3), C (4), L (2), D (3), G1 (1), G2 (2), G3 (2), G4 (3), and N alongside A | Each run ends when its selected tests complete, or at four virtual days. The longest run sets the time: four virtual days is 16 minutes, plus about a minute to start. |
| Accelerated re-runs | A failed run is re-run once, alone, and the time is the longest run again | Keeps the set within the accepted 16 minutes plus one re-run of the longest run. |
| Clean-up (area 1) | 15 to 30 minutes, before and after a full run | The clean-up deletes each record with a 0.2 s pause, and a full run leaves about 1,250 records. |
| New operation test (area 3) | 20 s (low) to 40 s (high) each | One driver run: the program's start-up, the six fixture contacts, the test's keys or menu action, and the check. |
| Multi-run operation tests (area 3) | Five minutes in total | Tests with a restart, a second copy, a wait or a kill (SS-004, SS-006, SS-008, SS-014). |
| Screen-reader test (area 4) | 4 to 5 minutes each | A person listens through each control and dialog. |
| Unit suite | 3 to 5 minutes | Compiling the test file and linking it with the built application. The tests themselves run in well under a second. |

### 2.3 What each area counts

- **Area 1** counts the 47 live runs of the 24 tests (GS-001 to GS-041; the 18 merged IDs run as part of their tests, section 1 of `documents/test-specification.md`) and the 142 imported Google scenarios (`documents/imported-tests.md`). Each live run is 3 to 5 minutes, and each imported scenario is 77 s (median) to 102 s (90th percentile), plus 15 to 30 minutes of clean-up. These are the same figures as before, and they reproduce the earlier 5 h 09 min to 7 h 43 min for 42 tests and 131 scenarios.
- **Section 5 (capacity stress test)** counts U01 (CS-001) at its measured time, 17 min 42 s. It runs on the scratch account only, when capacity is in question.
- **Area 2** counts the 48 new accelerated tests (AN-001 to AN-048).
- **Area 3** counts the 97 new operation tests (SS: 15, MF: 53, DL: 29, including DL-022) at 20 to 40 seconds each, the five minutes for the multi-run tests, and the 20 imported operation scenarios (IO-001 to IO-020, see `documents/imported-tests.md`; the estimate's earlier figure was 31 SS 6, MF 13, DL 12) at the measured times, plus the old-file test. The durations for area 3 have not been recalculated for the change from 31 to 20 imported operation scenarios, since their per-scenario times are not yet measured; the figure above still uses 31.
- **Area 4** counts the 18 new screen-reader tests (manual) and the two imported accessibility scenarios (automated).

---

## 3. Why the Google tests take as long as they do

The Google tests take a median of 77 seconds each, and the longest takes over 6 minutes. This is measured from the bench's own records, not estimated. The breakdown comes from 212 driver runs and their request timings:

| Where the time goes | Measured |
|---|---|
| A driver run (the program starts, syncs, and dumps its data) | median 38 s |
| Of that, Google requests | median 20 s; 50 per cent of all the driver time |
| Calendar listings (`getCalendar`, one page at a time) | 537 pages, median 2.0 s each, 1,065 s in all (30 per cent of the request time) |
| One request per changed appointment (`getAppointment`) | 6,593 requests, median 246 ms each, 1,702 s in all (48 per cent of the request time) |
| Updates to contacts and appointments | 223 contact updates (median 1.7 s) and 498 appointment updates (median 0.45 s) |
| The rest of the scenario time, inside the scenario (not measured separately) | about 39 s: the bench's own checks and listings of the Google side. The clean-up before and after each scenario is not in the 77 s: it is timed by nothing, and is added to the total run time. |

Four things make each Google test slow:

1. **The scratch account is large.** It holds about 3,300 calendar events and 500 contacts, most of them cancelled tombstones from earlier runs. Every listing pages through all of them: a page takes a median of about 2 seconds, and a sync lists the whole window before it does any work.
2. **Each changed appointment is fetched on its own.** The sync asks for each changed appointment separately. The median request takes 246 ms, and a scenario with fifty changes spends about 12 seconds on it.
3. **The bench cleans up around every scenario.** Before and after each scenario, the bench lists every contact and every calendar event on the account, and then deletes its own records one request at a time. This is part of the 77 seconds, and it is not measured separately.
4. **Google's own limits.** The contact-delete limit is per minute, so a full purge is slow, and the app paces its uploads when Google rate-limits them (section 7.5 of the specification). These waits are small in the measured figures.

What would make them faster, in order of effect (these are suggestions for the bench, and are not done):

- Give the scratch account a fresh calendar, or purge its tombstones, so each listing is small.
- Use the calendar's search (`q=BENCH`) in the bench's own listing, so the cleanup lists only its own events.
- Track the IDs the bench creates in each scenario, and delete only those, instead of listing the whole account before and after.

---

## 4. Notes on the figures

1. **The accelerated clock.** At 360 times real time, one real second is six minutes of program time. A test with an expected time to the minute can be up to one check late, which is six minutes. Specification 0.7 writes that tolerance in, and it means the runs cannot be shortened by going faster.
2. **Google limits.** The bench waits when Google rate-limits it, which adds time to a full run. Those waits are inside the measured figures.
3. **Re-runs.** A failure in area 1 is re-run once before it is reported. The high figure for area 1 allows for a second run of the failing tests.
4. **One cycle.** The figures are for one full cycle. Regression runs after each change are not estimated.
5. **First-time set-up.** Setting up the disposable account (`autotest/utilities/setup_test_account.py`) takes about 10 minutes of a person's time, plus the Google Cloud steps. It is done once, so it is not in the table.
6. **U01 is in section 5.** U01 (250 contacts and 1,000 appointments, CS-001 in section 5 of the test specification) is a capacity stress test. Its 17 min 42 s is shown separately and is not in the cycle total.
7. **Volume scenarios could also move to section 5.** Four archive scenarios create most of their records as a volume test: C12 (40 contacts, median 140 s), X19 (20 contacts, 122 s), A10 (40 appointments, 94 s) and U02 (40 appointments, 78 s). Moving them would take about 7 minutes out of area 1. This has not been decided.

---

## 5. Risks to the estimate

| Risk | Effect | Mitigation |
|---|---|---|
| Google changes its limits or latency | Area 1 takes longer, and the rate-limit tests (GS-033, which carries GS-036) change | Measure again at the first full cycle, and update this estimate. |
| The scratch account keeps growing | Each Google listing takes longer (section 3) | Tidy the account before each cycle, or give the bench a fresh calendar. |
| The accelerated harness is not yet in the repository (test specification Appendix B, item 3) | Area 2 cannot be run from the repository until it is brought in | A prerequisite. The estimate assumes it is in place. |
| The operation runner is not finished | Area 3 is estimated, and not yet measured | Measure the first full run of area 3, and update the 20 to 40 second figure. |
| The screen-reader tests need a person to listen | The area 4 time depends on the tester | Schedule the tests with the tester, and record the time spent. |
| The "Both" tests in area 1 are run under each configuration | Area 1 runs more than once for those tests | The estimate counts each run. The high figure allows for the second configuration. |

---

## 6. Recording actual times

Each run records, in its evidence folder, the start and end time and the time for each test. The first full cycle replaces the assumed figures in section 2.2 with measured ones, and this estimate is updated with the date of that cycle.
