# Google sync tests (area 1)

The bench for section 1 of `documents/test-specification.md`: GS-001 to GS-041, in the merged groups of that section.

**Status: written, not run.** The bench has been compile-checked only. The first run is the first real test of it, and its
failures are findings to be read, not bench faults to be hidden.

## Files

| File | What it is |
|---|---|
| `googlesync_tests.py` | The 24 tests, the runner, and the checks. |
| `gs_common.py` | Helpers: Google calls (through `autotest/utilities`), fixture contacts and events on Google, home folders, running the driver, operation builders. |
| `driver/sync_driver.cpp` | The headless driver: starts the program, runs its operations, writes a dump of the contacts and appointments. |
| `driver/build_driver.sh` | Builds the driver against an existing build of the program. |

## Set-up

1. Set up the disposable Google account once, with `autotest/utilities/setup_test_account.py` (the account file is kept
   outside the repository).
2. Build the program, then the driver:

       sh autotest/googlesync/driver/build_driver.sh ~/build-contactmanager

   This writes `~/build-contactmanager/sync_driver`. Use `--driver` to point elsewhere.

## Running

    python3 autotest/googlesync/googlesync_tests.py --list
    python3 autotest/googlesync/googlesync_tests.py                  # all, one after another
    python3 autotest/googlesync/googlesync_tests.py GS-008           # one test on its own
    python3 autotest/googlesync/googlesync_tests.py GS-008 GS-010 --out ~/gs-run

Tests run one at a time, never in parallel (Google rate limits). Each test, and each configuration of it, starts from an
empty test account and clears up afterwards. The clean-up finds the bench's records by the test domain and the test
prefix; it does not list the whole account.

Evidence goes to `--out` (default: a timestamped folder in the home folder, outside the repository). Each test and
configuration has a folder with its homes, the driver's runs (ops, log, dump, results), `checks.txt` (every check, with
`pass`, `FAIL` or `BLOCKED`), and `summary.json` at the top.

### Configurations

A test marked for both configurations runs under **CM** (ContactManager is the master) and under **G** (Google is the
master). The test sets the master in each home before its first run.

## Checks that are blocked, and why

| Test | Check | Reason |
|---|---|---|
| GS-011 | Google shows contact 1 as hidden | The bench does not know how the program marks a hidden contact on Google. Confirm, then add the check. |
| GS-011 | No message for contact 1 | Messages are area 2 (AN-027). |
| GS-015 | The link to contact 1 on Google | The rule for the link the program writes to Google is not confirmed. The bench checks the program's side (the appointment's contact, read back from the dump). |
| GS-031 | Real network loss and a revoked grant | The bench injects them with `CM_TEST_FAULTS`. It does not disconnect the network or revoke the grant. |
| GS-032 | Malformed reply for contact 3 | The fault plan (`googleaccess_network.cpp`) has status and `closed` faults only. A malformed-reply fault needs adding there. |
| GS-033 | Pacing adapts | The bench does not read request timings from the log yet. |

## Interpretations to confirm

These are choices the bench makes where the specification and the program leave a detail open. Each is named in the test.

- **Tidy Records** is run as the driver's `tidy_full` (Tidy Contacts, as the menu runs it).
- **The window** is set with the keys `windowbefore` and `windowafter` (days). The ini key for the download window
  (`downloaddaysbefore`, ticket #118) is not yet read by the program, so GS-023 and GS-038 use the Setup window.
- **The link from a Google appointment to a contact** is made by the contact being an attendee of the Google event
  (GS-015, GS-017, GS-023, GS-038). Confirm the program's rule; if it differs, change `create_linked_event` in one place.
- **GS-008, Google master.** The specification says "delete contact 5 again" under Google master. The bench deletes it,
  syncs, then syncs again and checks that the contact is not brought back.
- **GS-012, encryption.** The encrypted check uses a home whose database is encrypted from its first run, not a plain
  home switched to encryption: the program does not convert files that already exist (specification, section 2.5).
- **GS-035 (20 contacts).** The test stops the sync with `w11=closed`, which is a connection closed by Google
  at the eleventh write. The contacts are 20 extra contacts in the test domain; the six fixture contacts are not used.
- **Report wording.** Checks on what the report says (GS-005, GS-031, GS-033, GS-025, GS-032) match words in the driver's
  log loosely. Tighten each once the program's report text is confirmed.

## GS-041 and the clean-up

The default clean-up (`autotest/utilities/clean_test_data.py`) finds the test records by search: the People search for
the test domain, and the Calendar search for the test prefix. It does not list the account. GS-041 checks this by
reading the utility's source: its listing calls may appear only in the `--all-data` branch.

## Driver change

The driver's contact dump now includes `hidden` and `deleted` flags for each contact (GS-008, GS-011). This needs a
rebuild of the driver before the next run.
