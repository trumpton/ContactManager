# Project rules for Claude

## Commit messages

- Do not mention AI authorship in commit messages. That means no `Co-Authored-By`
  line naming Claude or any model, no `Claude-Session` line, no claude.ai links and
  no "Generated with" footer.
- End every commit message with this single trailer:

  Co-Authored-By: CC

## Test benches

- When a test bench (`autotest/`) is updated, record the release number at the start of any output it or its notes produce,
  such as a report, a summary or a run log: the short git commit hash of the code tested. Do not update a bench only to add this.

## Test log

- `documents/test-log.csv` records every test, one row per test, with these columns in this order:
  1. `test`: the test number, for example `AN-016` or `AC-021`;
  2. `date`: the date of the test run, as `YYYY-MM-DD`;
  3. `release`: the code release tested, as the short git commit hash of the code when the run started (blank if the test has not run);
  4. `result`: `PASS`, `FAIL` or `NOT RUN`;
  5. `elapsed`: how long the test took, written as described under "Elapsed times" below (blank if the test has not run, or has no time);
  6. `cm_notes`: notes or observations on bugs in ContactManager (blank if none);
  7. `testbench_notes`: notes or observations on bugs in the test bench (blank if none).
- A test the runner reports as BLOCKED (none of its checks could be made) is `NOT RUN`, with the reason in `testbench_notes`.
  A test with some checks blocked is recorded by its other checks, with the blocked ones named in `testbench_notes`.
- Quote any field that contains a comma or a double quotation mark.
- After each test run, update the file: replace the row of each test that was run with its new date, release and result, and add a
  row for any test that has none. Keep a note while it still applies, and clear it when it no longer does.
- If a batch stops before all its tests finish (it is killed, or aborts), log the tests that did finish, as `PASS` or `FAIL`,
  before anything else. The tests that did not finish stay `NOT RUN`, and the run is recorded as incomplete in `testbench_notes`.
- Elapsed times (column 5, `elapsed`). Write each time in one of these forms:
  - `x h xx m` for a time of an hour or more (whole minutes), for example `1 h 05 m`;
  - `xx.x m` for a time of a minute or more, for example `3.5 m` or `17.5 m`;
  - `xx.x s` for a time of 10 seconds or more, under a minute, for example `30.0 s`;
  - `x.xxx s` for a time under 10 seconds, for example `9.000 s` or `0.015 s`.
  A measured time is written as its range, `min - max`, with each end in its own form (`3.2 m - 4.9 m`). A single measurement is
  written once. A group run time, when the test ran with others and only the group's length is known, is written with a `~` in
  front (`~17.5 m`). An estimate is written with `~` in front and `?` after (`~30.0 s?`).
  - Time each test by its own finish, not the group's: when tests run in parallel, a test's time is when that test finished, so a
    test that finished after 5 minutes is 5 minutes even if the group ran for 20.
  - When a new measurement arrives, widen the range to include it (the smallest and largest times seen).
  - Never replace a measured time with a `~` time, and never write a `~` time over a measured one. A group time (`~`) may replace an
    estimate (`~...?`), and a measured time may replace either.
  - Each run, update the elapsed column for every test it ran.
- Commit the updated file with the other changes from the run.
