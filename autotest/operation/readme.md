# Operation tests (area 3)

The bench for section 3 of `documents/test-specification.md`: 97 tests, all automated.

- **SS-001 to SS-015**: start-up and shutdown (3a).
- **MF-001 to MF-053**: by menu and function (3b). F9 is in area 1.
- **DL-001 to DL-030**: by dialog (3c). DL-006 is not in the specification (it moved to area 2).

**Status: written, not run.** The Python is compiled, and every test's run specs are built without starting the program
(92 tests build; the 5 blocked by design are listed below). The driver is syntax-checked against the application's headers,
but not built or linked. The first run is the first real test of the checks.

## Files

| File | What it is |
|---|---|
| `operation_tests.py` | The runner. `--list` shows the tests. Each test runs in a fresh HOME folder, one run after another. |
| `ops_common.py` | Shared code: home folders, the driver runner (including background copies and kills), the operations, the checks, and the registry. |
| `tests_startup.py` | SS-001 to SS-015. |
| `tests_menus.py` | MF-001 to MF-053. |
| `tests_dialogs.py` | DL-001 to DL-030. |
| `driver/driver.cpp` | The driver: a copy of the Google bench's driver, with the operations below added. |
| `driver/build_driver.sh` | Builds the driver against the application's objects. The application's own email and SMS are used. |

Nothing here reads or imports the archive. The fixture contacts come from `autotest/utilities/test_fixtures.json`.

## Set-up and running

1. Build the application (`documents/headless-test.md`, section 1).
2. Build the driver:

       sh autotest/operation/driver/build_driver.sh ~/build-contactmanager

   This writes `~/build-contactmanager/driver`.
3. Run the tests:

       python3 autotest/operation/operation_tests.py --list
       python3 autotest/operation/operation_tests.py                 # all 97, one after another
       python3 autotest/operation/operation_tests.py MF-013 DL-005   # some

The tests send no messages and reach no Google account, so they need nothing else. They run one after another, because
some use a second copy (SS-004) or a killed copy (SS-014). Each test's folder under `--out` holds its runs' files, and
`report.md` and `summary.json` are at the top.

A test is **PASS** when every check passes, **FAIL** when a check fails, **BLOCKED** when it cannot run here, and **PARTIAL**
when some checks pass and some are blocked. The blocked checks are written out in the report with their reasons.

## How a test is checked

The driver writes its results (`results.json`), its log (`driver.log`), and any dumps (`dump.json`) to each run's folder. A
dialog shows in the log as `dialog <class> "<title>"`, except when a test holds it open (see `hold` below). The checks read
these, plus the files the program wrote in the data folder.

## Driver operations added for these tests

The driver is the Google bench's, with four operations added and two records extended. Each is documented in `driver.cpp`.

- `hold {on}` leaves an ordinary dialog open (not answered), so that later operations can read it and press its buttons.
  The main window and the sync progress window are not held. `hold {on: false}` lets the open dialogs be answered.
- `press {name, button}` clicks the button with that text in the top dialog (or the main window), and records `<name>_pressed`.
- `widgets {name, objects, rows}` records the top window's title (`<name>_title`) and class (`<name>_class`), and each named
  control's value (`<name>_<object>`), its enabled and visible state, and with `rows`, the first item view's rows.
- `set_widget {name, object, text | checked | index}` sets one control in the top window. A spin box is set through its text,
  as a user types it, so text it refuses leaves the old value.
- `menu` also records `<name>_checked` (a checkable action's state), and the dump records each appointment's repeat fields.
- A key dialog that an operation opens is held too (only at a start is it answered before the first operation).
- `CM_TEST_LOGIN_ANSWER` names the button chosen for the question after three failed logins (SS-003 chooses Close).

## Blocked by design

| Test | Why |
|---|---|
| SS-013 | Running as root: file permissions are not enforced, so a read-only data folder can't be made. |
| MF-022, DL-002 | Identify Me is chosen in Setup from a list the bench cannot set. |
| MF-038 | Send Email Contact starts a mail command. The bench does not yet point it at a stand-in that records its arguments. |
| DL-010 | The duration arrows and To are dates in the program's own format, which the bench does not set. |

Other checks are blocked inside a test that otherwise runs: the Google sign-in code (DL-007), the matches and Enter in
Global Find (MF-015) and Advanced Find (DL-020), the report's entries (MF-042), the beep (MF-049, DL-027), the manual's Find
(MF-043, DL-024), the log window's focus (DL-023), the send with a key set (DL-022), and the end date of a repeat (DL-013).

## Expected failures and things to confirm

- **MF-013.** The File menu still has Test Function (ticket #119). The test fails until it is removed, as the spec expects.
- **The program's wording.** Several checks look for text in the log, the title of a window, or the text of a button. These
  come from the program's source at the time of writing, and are the first things to confirm on a run: the start-up
  "sender" warnings (SS-007), the unreadable-file report (SS-011), the missing ini file (SS-012), the sync error (SS-015),
  the transaction warnings (DL-021), the encryption warnings (DL-025, DL-026), and the Setup warnings (DL-005).
- **Rows and focus.** The contact list, the search results and the calendar's focus are read through the driver's widget
  records. Where a list is not an item view, the rows check reports that it cannot find the rows.
- **Process checks.** SS-004 and SS-014 depend on timing (the second copy's delay, and a kill after eight seconds). SS-008
  depends on the backup date setting (`backupdate`), which the program reads from its settings.
- **Files.** MF-040 and MF-041 look for the amount in any file of the data folder, because the program's transaction file is
  not named in the tests yet. MF-003 to MF-005 and DL-030 use the file dialog's own name box (`fileNameEdit`) and its buttons.

## Not covered by these tests

- Google, sync and messages (the areas 1 and 2 tests cover them). Nothing here sends a message, and the tests need no account.
- The screen-reader checks (section 4), which need a person.
