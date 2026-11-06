# Project rules for Claude

## Commit messages

- Do not mention AI authorship in commit messages. That means no `Co-Authored-By`
  line naming Claude or any model, no `Claude-Session` line, no claude.ai links and
  no "Generated with" footer.
- End every commit message with this single trailer:

  Co-Authored-By: CC

## Test log

- `documents/test-log.csv` records every test, one row per test, with these columns in this order:
  1. `test`: the test number, for example `AN-016` or `AC-021`;
  2. `date`: the date of the test run, as `YYYY-MM-DD`;
  3. `result`: `PASS` or `FAIL`;
  4. `cm_notes`: notes or observations on bugs in ContactManager (blank if none);
  5. `testbench_notes`: notes or observations on bugs in the test bench (blank if none).
- Quote any field that contains a comma or a double quotation mark.
- After each test run, update the file: replace the row of each test that was run with its new date and result, and add a
  row for any test that has none. Keep a note while it still applies, and clear it when it no longer does.
- Commit the updated file with the other changes from the run.
