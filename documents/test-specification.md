# ContactManager Test Specification

Version: as at 5 November 2026.
Companions: `documents/specification.md` (what the program does, the reference for every test), `documents/test-duration-estimate.md` (how long the tests take), `documents/archive/test-specification-previous.md` (the earlier specification, with its results, kept for the record), and `autotest/utilities/readme.md` (the set-up and clean-up utilities).

---

## Contents

0. Scope, conventions and fixtures
1. Google sync (live system)
2. Announcements (offline, accelerated clock, stub transports)
3. Operation testing
   - 3a. Start-up and shutdown
   - 3b. By menu and function (F9 is covered in area 1)
   - 3c. By dialog
4. Accessibility (manual, with automated checks)
5. Capacity stress test (not part of the normal cycle)
6. Unit suite
- Appendix A. Traceability: specification section to tests
- Appendix B. Requirements that have no test yet

---

## 0. Scope, conventions and fixtures

### 0.1 Test IDs

Every test has a unique ID and a one-line title. The prefix is the area:

| Prefix | Area |
|---|---|
| `GS` | Google sync (area 1) |
| `AN` | Announcements (area 2) |
| `SS` | Start-up and shutdown (3a) |
| `MF` | By menu and function (3b) |
| `DL` | By dialog (3c) |
| `AC` | Accessibility (area 4) |
| `IG` | Imported Google sync scenarios from the archive bench (area 1) |
| `IA` | Imported accessibility scenarios from the archive bench (area 4) |
| `IO` | Imported operation scenarios from the archive bench (area 3) |
| `OF` | The imported old-file test (area 3) |
| `UT-001` to `UT-114` | Unit suite (section 6), one number for each unit test |

Each test has its own number within its area. The imported tests (`IG`, `IA` and `OF`) keep their archive IDs for reference, but the archive's IDs are not unique, so they are tracked by the IDs in `documents/imported-tests.md`.

### 0.2 Result states

| State | Meaning |
|---|---|
| Pass | Every expected result was observed. |
| Fail | An expected result was not observed. The evidence is kept with the run. |
| Blocked | The test could not run (for example the Google account is not set up, or the network is down). The reason is recorded. |
| Not run | Not run in this cycle. |

Each run records: the build (commit), the date and time, the test account, the files in the database folder before and after, the log, and any dump or report the test produces.

### 0.3 The two configurations

Area 1 runs each test under one or both configurations:

- **CM** — ContactManager is the master (Setup > Synchronisation Selection, for both calendar and contacts).
- **G** — Google is the master.

A test marked **Both** is run under both. The Setup choices are set before each run and recorded.

### 0.4 The six test contacts

Six contacts are used throughout the tests. They are the same for every area, so the results can be compared.

| No. | Name | Surname as stored | Email | Mobile | Date of birth | Contact by |
|---|---|---|---|---|---|---|
| 1 | Jack O'Neil | O'Neil | jack.oneil@sgc.contacmanagertest | +44 7700 900001 | 15 Jan 1980 | Email |
| 2 | Samantha Carter | Carter | samantha.carter@sgc.contacmanagertest | +44 7700 900002 | none | Email |
| 3 | Daniel Jackson | Jackson | daniel.jackson@sgc.contacmanagertest | +44 7700 900003 | 20 Mar 1975 | SMS |
| 4 | Tealc de Chulak | de Chulak | tealc.dechulak@sgc.contacmanagertest | +44 7700 900004 | none | SMS |
| 5 | George Hammond | Hammond | george.hammond@sgc.contacmanagertest | +44 7700 900005 | 2 Nov 1960 | Email and SMS |
| 6 | Vala Mal Doran | Mal Doran | vala.maldoran@sgc.contacmanagertest | +44 7700 900006 | none | Email and SMS |

Notes:
- Email addresses are `firstname.surname@sgc.contacmanagertest`, with the surname written without spaces or apostrophes (an address cannot hold them). Tealc's is `tealc.dechulak`, and Vala's is `vala.maldoran`.
- The domain is spelled as specified: `sgc.contacmanagertest`. Appendix B, item 1, asks for the spelling to be confirmed before the first live run, because the clean-up utility finds test records by this domain.
- Only odd-numbered contacts (1, 3, 5) have a date of birth.
- Phone numbers are in the range +44 7700 900000 to +44 7700 900999.
- The fixtures are in `autotest/utilities/test_fixtures.json`. The tests and the utilities read them from there.

### 0.5 Identifying and removing test records

Every record the tests create carries an identifier, so it can be found and removed on any account:

| Record | Identifier |
|---|---|
| Contact | Its email address is in the domain `sgc.contacmanagertest` |
| Appointment | Its summary starts with `TEST ` |

`autotest/utilities/clean_test_data.py` removes these by default. With `--all-data`, it removes everything on the account, and only after the account's address is typed in (0.6).

### 0.6 The Google account

- A **disposable** Google account is used for area 1. It must never be a real account.
- It is set up once with `autotest/utilities/setup_test_account.py`. That utility asks for the Google OAuth client, creates the two ContactManager passwords (the database password, which is the encryption key, and the login password), takes the authorisation through Google's device sign-in, and writes one account file outside the repository (`~/.config/contactmanager-test/test-account.json`, permissions 0600).
- Before each live run: `clean_test_data.py --dry-run`, then `clean_test_data.py` (default: test data only).
- `--all-data` is for a clean-up of the disposable account only. It needs the account's address typed in, and it cannot be used with `--yes`.

### 0.7 The accelerated clock

Area 2 runs on a clock that goes faster than real time: **10 real seconds = 1 hour**, that is 360 times real time. Three days (72 hours) take 12 minutes.

At 360 times, one real second is 6 minutes of program time. The program checks for due messages once per real second, so a message can be up to one check (6 minutes) late in program time. Area 2 expectations allow this: a message is **on time** when it is sent at the expected minute, or at the next check (within 6 minutes), and not later. Any later is a failure. A confirmation, an update and a cancellation are the exception: each may arrive up to **15 virtual minutes** after its expected time, and not later.

The clock is set by the test harness, through a shim that changes the time the program reads (the shim is in the scratch folder, with the harness: Appendix B, item 3). The program's own timers and time reads all follow it.

### 0.8 Stub transports

Area 2 does not send real email or text. The send functions are replaced by stubs that record each message (recipient, subject or text, body, time) and return a chosen result (sent, refused, timed out). The accelerated harness keeps its stub in a scratch folder: bringing that stub into the repository is a prerequisite for area 2 (Appendix B, item 3).

### 0.9 Run order and locations

Read `headless-test.md` (how to run the bench and the accelerated harness) and `gotchas.md` (what has gone wrong before) before running any area.

- Area 1 needs the Google account and the network. It runs last in any cycle, because it changes the account.
- Area 2 needs no network, and runs in the accelerated harness.
- Area 3 needs the program only.
- Area 4's manual tests need the program, a screen reader (NVDA, or the accessibility inspection tools of the platform), and a person to listen. Its automated checks (AC-019 to AC-029) need the program only.

The durations are estimated in `documents/test-duration-estimate.md`.

---

## 1. Google sync (live system access)

These tests use the six fixture contacts (0.4), a disposable account (0.6), and the two masters (0.3).

**How the tests are grouped.** Section 1 has three subsections, and each merge is inside one subsection:

| Subsection | Tests | Merged tests | Single tests (not merged) |
|---|---|---|---|
| 1.1 Contacts | 8 | GS-003, GS-005, GS-008, GS-012 | GS-001, GS-002, GS-010, GS-011 |
| 1.2 Appointments | 8 | GS-015, GS-017, GS-020, GS-023, GS-025 | GS-028, GS-029, GS-030 |
| 1.3 Interruptions, limits, tokens and copies | 8 | GS-031, GS-032, GS-033, GS-041 | GS-035, GS-038, GS-039, GS-040 |

The earlier draft had **42 tests**, GS-001 to GS-042. Eighteen of those draft tests run the same steps as another test, so they are **merged** into it: the test that is run also carries their checks, and they are not run on their own. The table below gives each merged ID and the test it is merged into. So the 24 tests above run 47 times in all (section 1.5), and all 42 IDs are still tracked: the 18 merged IDs have their own rows in `documents/test-log.csv`.

**Merged IDs.** Each of these is run as part of the test named in the second column.

| Merged ID | Merged into | What its checks cover |
|---|---|---|
| GS-004 | GS-003 | An email changed on Google reaches the app once, with no duplicate |
| GS-006 | GS-005 | When both sides change a field, Google as master keeps Google's version |
| GS-007 | GS-005 | An edit made only in the app is replaced by Google's copy, Google as master |
| GS-009 | GS-008 | A contact deleted in the app stays deleted, Google as master |
| GS-013 | GS-012 | With encryption on, no plain text is on disk after a sync |
| GS-014 | GS-012 | A deleted contact's history and to-do files stay together, and moving them back restores it |
| GS-016 | GS-015 | An appointment made on Google for contact 2 is downloaded once, linked to contact 2 |
| GS-018 | GS-017 | An appointment deleted in the app is deleted on Google |
| GS-019 | GS-017 | An appointment deleted on Google is removed in the app, Google as master |
| GS-021 | GS-020 | Deleting one occurrence of a repeat leaves the others |
| GS-022 | GS-020 | Splitting a repeat ("this and following") gives no duplicates |
| GS-024 | GS-023 | An archived appointment moved back into the window is restored from the archive |
| GS-026 | GS-025 | An appointment dated 1 January 1970 is not uploaded, and the others sync |
| GS-027 | GS-025 | An appointment organised by someone else is not uploaded across syncs |
| GS-034 | GS-031 | A refused sign-in tells the user to sign in again, and changes nothing locally |
| GS-036 | GS-033 | A rate limit on upload is retried with waits, and a failure gives its reason |
| GS-037 | GS-032 | A malformed reply for one contact skips that contact with a reason, and the others sync |
| GS-042 | GS-041 | The set-up's saved account is the one used, and the clean-up refuses any other account |


**Running one test on its own.** Every test can be run on its own, as its ID, for example GS-008. A merged test runs as a whole: GS-008 includes the checks of the test merged into it (deleting a contact when Google is master), so running GS-008 runs both. The only set-up a test needs is the fixture contacts (GS-001, creating contacts 1 to 6 and saving). No test relies on another test's result, except the steps of GS-020, which are in sequence on one repeat.

**Clean-up.** The clean-up removes the records a run created, by the IDs the run recorded or by a search for the test prefix. It never lists the whole account.

### 1.1 Contacts: creation, change, deletion and support files

| ID | Title | Procedure and expected result | Run |
|---|---|---|---|
| GS-001 | The six fixture contacts created in ContactManager reach Google once, with their values | Create contacts 1 to 6 with their fixture values, save, sync. Expect six contacts on Google with the name, email, phone and birthday (1, 3, 5 only). No duplicates on a second sync. | Both |
| GS-002 | A fixture contact made on Google downloads once, with its contact and history files written | Create contact 2 on Google. Sync. Expect one contact in the app, its files written, its Google ID stored, and a second sync adds nothing. | Both |
| GS-003 | Edits made in ContactManager and on Google each reach the other side in one sync, and are recorded in history. | Change contact 1's mobile in the app, and contact 2's email on Google. Sync (CM). Expect the new mobile on Google and the new email in the app, one copy of each, and a history entry on each contact. | CM |
| GS-005 | When both sides change the same field, the master's version is kept, and an edit made only in the app is replaced when Google is master. | Change contact 3's surname in the app and on Google, and sync (CM): expect the app's surname on Google, and the report to say the app's version was sent. Under Google master, make the same change and sync: expect Google's surname in the app and a history entry. Then, under Google master, change contact 4's phone in the app only, and sync: expect Google's phone in the app and no upload. | Both |
| GS-008 | A fixture contact deleted in the app is deleted on Google under either master, and its files move to the deleted folder. | Delete contact 5 in the app, sync, and run Tidy Records. Expect the contact gone from Google, and its contact, history and to-do files in `contact/deleted/`. Under Google master, delete contact 5 again in the app and sync: expect it gone from both, and not brought back. | Both |
| GS-010 | A fixture contact deleted on Google is removed from the app, not restored | Delete contact 6 on Google, sync. Expect the contact removed from the app and moved to the deleted folder. | Both |
| GS-011 | A hidden fixture contact stays on Google as hidden and is not messaged | Hide contact 1, sync. Expect Google to agree, and no message for contact 1 (area 2 checks the message). | Both |
| GS-012 | History and to-do files survive edits on Google, a deletion and a restore, both plain and encrypted. | Give contact 3 a history and a to-do item. Edit contact 3 on Google, sync and restart: expect both files unchanged except as the edit requires, and readable. Set `[database] encrypted=yes`, sync contacts 1 to 6, and search the database folder for their names and notes: expect no plain-text match. Delete contact 2 and run Tidy Records; move its three files back into `contact/`, and restart: expect the contact, history and to-do restored. | Both |

### 1.2 Appointments: upload, download and sync

| ID | Title | Procedure and expected result | Run |
|---|---|---|---|
| GS-015 | An appointment added in the app, or made on Google, reaches the other side once, linked to its contact. | Add an appointment for contact 1 in the app, and one for contact 2 on Google. Sync twice. Expect one event on Google linked to contact 1, and one appointment in the app linked to contact 2, with no copies on the second sync. | Both |
| GS-017 | An appointment moved or deleted on one side reaches the other. | Move contact 3's appointment on Google, and sync (CM): expect the new time in the app, as one entry. Delete contact 4's appointment in the app, and sync (CM): expect it gone from Google. Under Google master, delete another appointment for contact 4 on Google, and sync: expect it removed from the app and not restored. | Both |
| GS-020 | A weekly repeat is uploaded, one occurrence can be deleted, and the repeat can be split, with no duplicates. | Add a weekly repeat for contact 5, and sync: expect one repeating event on Google, listed on each week in the window. Delete one occurrence in the app, and sync: expect Google to omit that date and the others to stay. Change one occurrence and all that follow, and sync: expect two repeats on Google, the earlier unchanged and the later changed, and no duplicates. | Both |
| GS-023 | Appointments outside the calendar window are not downloaded, and an archived appointment is restored when it moves back into the window. | Put an appointment for contact 6 entirely before the window on Google, and sync: expect it absent from the app and still on Google. Archive an expired appointment, move it into the window on Google, and sync: expect it restored and gone from the archive. | Both |
| GS-025 | Appointments that cannot be uploaded are not uploaded, the report says why, and the others sync. | Add one appointment with no end time, one dated 1 January 1970, and a good one. Sync: expect only the good one on Google, and the report to name the two that were not uploaded and why. Create on Google an appointment whose organiser is another person, and sync twice (CM): expect no upload, and no copy in the app beyond its download. | Both |
| GS-028 | An appointment crossing the clocks going back keeps its London time on Google and in the app | Add a weekly 14:00 repeat across the end of British Summer Time. Sync. Expect 14:00 London time on both sides for each date. | Both |
| GS-029 | An appointment for a deleted contact is kept, shows as "Deleted contact", and is not changed on Google until edited | Delete contact 1 with an appointment. Sync. Expect the appointment kept in the app, shown as Deleted contact, and unchanged on Google. | Both |
| GS-030 | Two overlapping appointments for different contacts both reach Google, and neither is merged | Add two overlapping appointments for contacts 1 and 2. Sync. Expect two events on Google, with their own times. | Both |

### 1.3 Interruptions, limits, tokens and copies

| ID | Title | Procedure and expected result | Run |
|---|---|---|---|
| GS-031 | A sync that cannot run (no network, or a refused sign-in) changes no local file and reports why, and a later sync completes. | Disconnect the network, and sync: expect an error and no local change. Reconnect, and sync: expect success. Revoke the app's access on Google, and sync: expect the report to say to sign in again, and no local change. Sign in again, and sync: expect success. | Both |
| GS-032 | A bad entry or reply for one contact is reported with its reason, and the others sync. | Give contact 2 an email Google does not accept, and sync: expect contact 2 reported, not uploaded, and the others synced. Inject a malformed reply for contact 3, and sync: expect contact 3 skipped and reported, and the others unchanged. | Both |
| GS-033 | Google's server errors and rate limits are retried, with no duplicate record, and a failure gives its reason. | Inject a 500 reply on one upload, and rate-limit replies on others (fault plan), and sync: expect retries with waits and one record per contact on Google. Make the rate limit persist: expect the pacing to adapt, and, if the sync fails, the report to give the reason and the recovery step. | Both |
| GS-035 | A sync stopped part-way resumes from its progress file, with no upload repeated and none lost | Stop the program during uploads of 20 contacts. Restart, and sync. Expect each contact on Google once. | Both |
| GS-038 | Changing the calendar window does not delete appointments outside it, locally or on Google | Narrow the window in Setup, and sync. Expect appointments outside it kept on Google, and archived locally only where they have expired. | Both |
| GS-039 | Two copies of the program on one calendar each see the other's change, with no duplicates | Run two copies on one data folder's calendar. Change an appointment in one. Sync both. Expect the change in the other, and no duplicate. | Both |
| GS-040 | Switching the master between syncs, with edits on both sides, leaves one copy of each record | Edit contacts 1 and 2 on both sides, sync (CM). Switch to G, edit again, sync. Expect one copy of each record. | Both |
| GS-041 | The clean-up removes only the records of the run, and the set-up's saved account is the one used. | Run the set-up `--check`: expect the account to work. Run the clean-up with a token for another account: expect it to stop before any delete. Run the clean-up: expect no test records left, and the next sync to start from an empty account. The clean-up finds records by the IDs the run recorded, or by a search for the test prefix. It does not list the whole account. | Both |

### 1.4 Set-up and running

The bench is in `autotest/googlesync/`: `googlesync_tests.py` (the tests, their runner and checks), `gs_common.py` (the helpers: Google calls, fixture contacts and events on Google, home folders, running the driver) and `driver/` (the headless sync driver, which starts the program, runs its operations and writes a dump of the contacts and appointments).

1. Set up the disposable Google account once, with `autotest/utilities/setup_test_account.py`. The account file is kept outside the repository.
2. Build the program, then the driver: `sh autotest/googlesync/driver/build_driver.sh ~/build-contactmanager`. This writes `~/build-contactmanager/sync_driver`; use `--driver` to point elsewhere.

    python3 autotest/googlesync/googlesync_tests.py --list
    python3 autotest/googlesync/googlesync_tests.py                  # all, one after another
    python3 autotest/googlesync/googlesync_tests.py GS-008           # one test on its own
    python3 autotest/googlesync/googlesync_tests.py GS-008 GS-010 --out ~/gs-run

Tests run one at a time, never in parallel, because Google rate-limits them. Each test, and each configuration of it, starts from an empty test account and clears up afterwards. The clean-up finds the bench's records by the test domain and the test prefix. It does not list the whole account.

Evidence goes to `--out` (by default a timestamped folder in the home folder, outside the repository). Each test and configuration has a folder with its homes, the driver's runs (ops, log, dump, results), `checks.txt` (every check, marked `pass`, `FAIL` or `BLOCKED`), and a `summary.json` at the top.

### 1.5 Configurations and the number of runs

A test marked **Both** runs twice: under **CM** (ContactManager is the master) and under **G** (Google is the master). The test sets the master in each home before its first run. GS-003 is CM only. So the 24 tests make **47 runs**: 23 tests run twice, and GS-003 runs once.

### 1.6 Checks that are blocked, and why

Some checks cannot yet be made. The bench reports each as `BLOCKED` in `checks.txt`, not as a pass.

| Test | Check | Reason |
|---|---|---|
| GS-011 | Google shows contact 1 as hidden | The bench does not know how the program marks a hidden contact on Google. Confirm, then add the check. |
| GS-011 | No message for contact 1 | Messages are area 2 (AN-027). |
| GS-015 | The link to contact 1 on Google | The rule for the link the program writes to Google is not confirmed. The bench checks the program's side: the appointment's contact, read back from the dump. |
| GS-031 | Real network loss and a revoked grant | The bench injects them with `CM_TEST_FAULTS`. It does not disconnect the network or revoke the grant. |
| GS-032 | Malformed reply for contact 3 | The fault plan (`googleaccess_network.cpp`) has status and `closed` faults only. A malformed-reply fault needs adding there. |
| GS-033 | Pacing adapts | The bench does not yet read request timings from the log. |

### 1.7 Interpretations to confirm

These are the choices the bench makes where the specification and the program leave a detail open. Each is named in its test.

- **Tidy Records** is run as the driver's `tidy_full` (Tidy Contacts, as the menu runs it).
- **The window** is set with the keys `windowbefore` and `windowafter` (days). The ini key for the download window (`downloaddaysbefore`, ticket #118) is not yet read by the program, so GS-023 and GS-038 use the Setup window.
- **The link from a Google appointment to a contact** is made by the contact being an attendee of the Google event (GS-015, GS-017, GS-023, GS-038). The program's rule needs confirming. If it differs, change `create_linked_event` in one place.
- **GS-008, Google master.** The bench deletes contact 5 again, syncs, then syncs again and checks that the contact is not brought back.
- **GS-012, encryption.** The encrypted check uses a home whose database is encrypted from its first run, not a plain home switched to encryption, because the program does not convert files that already exist (section 2.5).
- **GS-035 (20 contacts).** The test stops the sync with `w11=closed`, a connection closed by Google at the eleventh write. The 20 contacts are extra contacts in the test domain; the six fixture contacts are not used.
- **Report wording.** The checks on what the report says (GS-005, GS-031, GS-033, GS-025, GS-032) match words in the driver's log loosely. Each is to be tightened once the program's report text is confirmed.

### 1.8 Clean-up, GS-041 and the driver

The default clean-up (`autotest/utilities/clean_test_data.py`) finds the test records by search: the People search for the test domain, and the Calendar search for the test prefix. It does not list the account. GS-041 checks this by reading the utility's source: its listing calls may appear only in the `--all-data` branch.

The driver's contact dump includes `hidden` and `deleted` flags for each contact (GS-008, GS-011). The driver must be rebuilt before the next run.

### 1.9 Imported Google sync tests

These tests were imported from the archive bench (`documents/imported-tests.md`), and are in `autotest/googlesync/tests_imported.py`. Each runs with the same driver and checks as the tests above, in the CM configuration (ContactManager is the master) unless its procedure says otherwise. Each test uses its own contacts and events, named for the test, so the clean-up finds them.

| ID | Title | Procedure and expected result | Run |
|---|---|---|---|
| IG-001 | Add a contact locally, no sync: Google unchanged | Add a contact in the app; do not sync. Expect no copy on Google, and the contact held in the app. | CM |
| IG-003 | Add then delete locally before any sync: nothing reaches Google | Add a contact, delete it, then sync. Expect no copy on Google and no live contact in the app. | CM |
| IG-007 | Same name on both sides before sync: both kept | A Google contact and an app contact with the same name, different phones; sync. Expect two contacts with the name on Google, both phones there, and two in the app. | CM |
| IG-010 | Out-of-sequence phones and emails: each value kept in some slot | A Google contact with three emails and four phones in odd slots; sync twice. Expect each value held in some slot in the app, and three emails still on Google after the second sync. | CM |
| IG-011 | Stray spaces and newlines in Google data: no spurious upload | A Google contact with a padded email and a multi-line address; sync. Expect the Google record unchanged (its etag), and the app holding the trimmed email. | CM |
| IG-012 | Large upload: 40 local contacts reach Google once | Add 40 contacts in the app; sync. Expect 40 contacts on Google, with no duplicates. | CM |
| IG-013 | Unicode and apostrophes in names round-trip exactly | A contact with a Unicode surname containing an apostrophe; sync. Expect Google to hold the name exactly, once. | CM |
| IG-014 | Email-only contact (no name) is uploaded with its email | A contact with an email and no name; sync. Expect one Google contact with that email. | CM |
| IG-015 | Repeated syncs with no changes do not write to Google | A Google contact; sync; then sync twice more. Expect its etag unchanged after the later syncs. | CM |
| IG-016 | Edit keeps URLs the app does not hold (option 1 regression) | A Google contact with two URLs, a nickname and a biography; sync; change the surname in the app; sync. Expect both URLs, the nickname and the biography kept on Google, and the new surname there. | CM |
| IG-017 | Add an appointment locally, no sync: Google unchanged | Add an appointment in the app; do not sync. Expect no copy on Google, and the appointment held in the app. | CM |
| IG-019 | Identical events on Google (duplicates): both kept, no growth on re-sync | Two identical events on Google; sync; sync again. Expect both in the app after each sync, and two on Google. | CM |
| IG-020 | Same event added locally and on Google before sync: both kept | An event made on Google and the same event added in the app; sync. Expect two copies on Google. | CM |
| IG-026 | Large upload: 40 local appointments reach Google once | Add 40 appointments in the app; sync. Expect 40 appointments on Google, with no duplicates. | CM |
| IG-028 | Stray spaces and newlines in a location: no spurious upload | A Google event with a padded multi-line location; sync. Expect the Google event unchanged (its etag), and the app holding the trimmed location. | CM |
| IG-029 | Local ID of a Google event is the same across syncs | A Google event; sync; sync again. Expect one entry each time, with the same local ID. | CM |
| IG-038 | Blank and whitespace-only name fields: no crash, no duplicate | Two contacts with blank names (one with a test-domain email); sync. Expect the run to complete, and no more than one Google copy of the nameless contact. | CM |
| IG-039 | Very long comment (6000 characters) round-trips intact | A Google contact with a 6000-character biography; sync. Expect the text unchanged on Google, and the full text in the app. | CM |
| IG-040 | Quotes, backslashes, ampersands, angle brackets, emoji and right-to-left text round-trip | A contact whose name and comment hold quotes, backslashes, ampersands, angle brackets, emoji and right-to-left text; sync. Expect the name held exactly on Google and in the app. | CM |
| IG-041 | Phone number formats (international, brackets, extensions) keep their digits | A Google contact with three phone formats; sync. Expect the digits unchanged on Google, and in the app without the (0) trunk zero. | CM |

---

## 2. Announcements (offline, accelerated clock, stub transports)

These tests run the program headless, with the stub transports (0.8), on the accelerated clock (0.7). Expected times are program times. A message is on time when sent at the expected time or at the next check (within 6 minutes). The tests are grouped into runs, as section 2.0 explains.

Defaults apply unless a test says otherwise: hold 5 minutes, send window 07:00 to 22:00, reminder 24 hours before, confirm-only 36 hours, limits 5 an hour and 10 a day.

### 2.0 How the announcement tests are run

The announcement tests run as **runs**. A run is one headless process of the program, with its own data folder, its own home folder and its own accelerated clock. Runs are independent, so **the runs listed below run in parallel**, and the whole set takes as long as the longest run: at most four virtual days, which is 16 minutes of real time at 360 times.

**Why the runs are grouped.** Two tests can share a run only if neither changes anything the other relies on. These things are shared by every test in a run, so each is kept to one run:
- a contact's state (email or text ticked, a deleted or hidden contact, a removed address);
- a global setting, template or sender in the ini file;
- the stub transport's failures;
- the default limits (5 an hour, 10 a day), or the raised limits (20000);
- a restart or a second copy of the program;
- the date a run starts from.

| Run | Tests | Why it is a separate run | Length |
|---|---|---|---|
| A (main) | AN-001 to AN-021, AN-024, AN-025, AN-047 | Plain tests on any fixture contact. Limits raised to 20000. | 3 virtual days |
| B (contact state) | AN-026 to AN-030 | Each changes one contact's state (deleted, hidden, a channel changed, no channel, no address). Each uses its own contact. | up to 3 virtual days |
| C (restarts) | AN-031 to AN-034, AN-043 | Each stops and restarts the program. The restart times are part of each test. | up to 4 virtual days |
| L (limits) | AN-022, AN-023 | Need the defaults (5 an hour, 10 a day). AN-023 also restarts the program. | up to 2 virtual days |
| D (dates) | AN-045, AN-046 | Each starts the clock on a date of its own (a clock change; New Year). | up to 3 virtual days |
| G1 (senders) | AN-036, AN-037 | Each blanks a sender in the ini file. | up to 1 virtual day |
| G2 (templates) | AN-038, AN-039, AN-044 | Each changes a message template. | up to 2 virtual days |
| G3 (transport) | AN-040, AN-041, AN-042 | Each makes the stub transport fail. | up to 2 virtual days |
| G4 (reminders) | AN-048 | Turns the reminders off for the whole run. | up to 3 virtual days |
| N (second copy) | AN-035 | Starts a second copy on run A's data folder while run A runs. The second copy holds no lock, so it sends nothing. | alongside run A |

**Selection.** The operator chooses which tests to run. By default every test is selected. A selected test starts only the run it belongs to, with only the selected tests in it. A request for AN-017 alone runs only AN-017, in run A.

**Stopping.** A run ends when every selected test in it has completed, or at four virtual days, whichever comes first. A test completes at the last time any of its expected messages is due, plus one hour of program time, to check that nothing else is sent. A test with no expected message completes one hour after its last step. So a test that completes after one virtual day ends its run after one virtual day.

**Unpicking the messages.** Two things identify each message, and both are used:
- **The test number in the subject.** Each test's appointment summary starts with its test number (for example `AN-017 Jack confirm`). When the environment variable `CM_TEST_MESSAGE_TAGS` is set, the program puts that number in place of `$TESTTAG` in a message's subject and body. The harness's message templates use `$TESTTAG`, so each message shows its test number, and can be reviewed by eye. Without the variable, `$TESTTAG` is not replaced and nothing changes. This is a test-only feature; the shipped program never sets the variable.
- **The recipient and the quoted appointment time.** The harness matches each message to its expected message by these, which is unique within a run because each test's appointment times are its own. This is the check; the tag is for review.

**Time.** The runs run in parallel, so the set takes about 16 minutes: the longest run, at four virtual days, plus start-up. That is the target the operator has accepted.

### 2.1 Creation, update and cancellation

| ID | Title | Procedure and expected result | Run |
|---|---|---|---|
| AN-001 | An appointment made for contact 1 at 10:00 is confirmed by email at 10:05 | Create at 10:00, three days ahead. Expect one email to contact 1 at 10:05, with the details. | Accelerated, run A |
| AN-002 | An appointment made for contact 3 is confirmed by text at 10:05 | As AN-001, for contact 3 (text only). Expect one text at 10:05, and no email. | Accelerated, run A |
| AN-003 | An appointment for contact 5 (both channels) is confirmed by email only | As AN-001, for contact 5. Expect one email at 10:05 and no text. | Accelerated, run A |
| AN-004 | On the day, an email-only contact gets one email from 07:00 | Appointment for contact 2 on day 2. Expect one email at 07:00 on day 2. | Accelerated, run A |
| AN-005 | On the day, a text-only contact gets one text from 07:00 | Appointment for contact 4 on day 2. Expect one text at 07:00 on day 2. | Accelerated, run A |
| AN-006 | On the day, a contact with both channels gets an email, then a text | Appointment for contact 6 on day 2. Expect one email, then one text, from 07:00 on day 2. | Accelerated, run A |
| AN-007 | A reminder is sent 24 hours before, once | Appointment for contact 2 at 15:00 on day 3. Expect one reminder email at 15:00 on day 2. | Accelerated, run A |
| AN-008 | An appointment made 30 hours before its start gets its confirmation only, with no reminder | Create for contact 4 at 09:00 on day 2, for 15:00 on day 3. Expect one confirmation, and no reminder. | Accelerated, run A |
| AN-009 | Edits within the hold give one confirmation with the final details | Create for contact 1; edit at +2 and +4 minutes. Expect one confirmation at +9 minutes with the final details. | Accelerated, run A |
| AN-010 | An edit and its reversal within the hold send nothing | Create for contact 1; change at +2, change back at +4. Expect one confirmation with the original details, at +9 minutes, or nothing if deleted before. | Accelerated, run A |
| AN-011 | An edit after confirmation sends one update with the final details, after the hold | Confirm; at +1 hour, move twice within the hold. Expect one update, with the final details, at the end of the hold. | Accelerated, run A |
| AN-012 | A cancellation after confirmation is sent at once, not held | Confirm; at +1 hour, delete. Expect a cancellation at once, on the same channel. | Accelerated, run A |
| AN-013 | An appointment created and deleted before confirmation sends nothing | Create at 10:00; delete at 10:01. Expect nothing sent. | Accelerated, run A |
| AN-014 | A title or location change alone sends no update | Confirm; change the title only. Expect nothing sent. | Accelerated, run A |
| AN-015 | A repeat occurrence removed after confirmation sends a cancellation for that date only | Weekly repeat for contact 2, confirmed; delete one occurrence. Expect one cancellation for that date, by email, and no other message. | Accelerated, run A |
| AN-016 | Each occurrence of a repeat is reminded once | Daily repeat for contact 5, made at 09:00 on day 0, first on day 1 at 10:00, with occurrences on days 1, 2 and 3. Expect one confirmation. The first occurrence gets no reminder, since it was confirmed less than 36 hours before its start (the reminder rule). Expect one reminder before each of the second and third occurrences, and one email and one text on each day, none twice. | Accelerated, run A |

### 2.2 Send window, silent period and backlogs

| ID | Title | Procedure and expected result | Run |
|---|---|---|---|
| AN-017 | An appointment made at 21:30 is confirmed at 07:00, when the window opens | Create at 21:30 for the next day. Expect the confirmation at 07:00, not before. | Accelerated, run A |
| AN-018 | A reminder due in the silent period is sent once, at 07:00 | Appointment whose reminder is due at 23:00. Expect the reminder at 07:00, once. | Accelerated, run A |
| AN-019 | Edits made overnight send one update at 07:00, with the final times | Confirm at 20:00; edit at 22:15, 22:30 and 23:30. Expect one update at 07:00 with the final times. (The edits are after the send window closes at 22:00, so none is sent overnight.) | Accelerated, run A |
| AN-020 | A send due at 22:00 waits until 07:00 | Create so that the confirmation is due at 22:00. Expect it sent at 07:00 the next day. | Accelerated, run A |
| AN-021 | A message due at 07:00 is sent at 07:00 | Create so that the confirmation is due at 07:00. Expect it sent at 07:00. | Accelerated, run A |
| AN-022 | Twelve appointments for contact 6 created overnight send five at 07:00, five at 08:00, and the rest once the daily limit clears | Create twelve for contact 6 at 23:00. Expect 5 at 07:00, 5 at 08:00, the last two after the daily limit clears. | Accelerated, run L |
| AN-023 | The limits hold across a restart | Send ten messages to contact 2 on day 1; restart at 12:00. Expect no further messages to contact 2 until the daily limit clears. | Accelerated, run L |

### 2.3 Expired, irrelevant and blocked messages

| ID | Title | Procedure and expected result | Run |
|---|---|---|---|
| AN-024 | An appointment already started gets no reminder | Appointment that starts at 10:00; the reminder time passes after its start. Expect no reminder. | Accelerated, run A |
| AN-025 | No message is sent for a past appointment | Appointment that ended yesterday. Run the clock forward. Expect nothing. | Accelerated, run A |
| AN-026 | A deleted contact's appointments send no messages | Contact 3 deleted; appointments made before. Expect nothing sent to contact 3. | Accelerated, run B |
| AN-027 | A hidden contact's appointments send no messages | Contact 4 hidden; appointments made. Expect nothing sent to contact 4. | Accelerated, run B |
| AN-028 | A channel changed after confirmation sends only what is pending on the new channel, and nothing again | Contact 5 confirmed by email; change to text only. Expect only pending messages on text, and no repeat of the email. | Accelerated, run B |
| AN-029 | A contact with neither channel ticked is sent nothing | Contact with no channel ticked. Expect no message of any kind. | Accelerated, run B |
| AN-030 | A confirmation falls back to text when email is ticked but the contact has no email address | Contact 1 with email ticked and its address removed, and text ticked with a mobile number. Create an appointment. Expect one text confirmation and no email (specification 5.2). | Accelerated, run B |

### 2.4 Restarts, shutdown and two copies

| ID | Title | Procedure and expected result | Run |
|---|---|---|---|
| AN-031 | A shutdown during a hold sends the message after restart, with the current details | Create; shut down at +2 minutes; restart at +20 minutes. Expect the confirmation at restart, with the details as they are then. | Accelerated, run C |
| AN-032 | A reminder missed while the program was off is not sent late once the appointment is on that day | Shut down from 11:00 the day before until 06:00 on the day; appointment at 15:00 on the day. Expect no reminder, and the on-the-day message from 07:00. | Accelerated, run C |
| AN-033 | A restart in the middle of a day does not repeat the on-the-day messages | Send on-the-day messages; restart at 12:00. Expect none repeated. | Accelerated, run C |
| AN-034 | Three days off with many edits send one message per appointment, not one per change | Edit ten appointments twenty times each while off; restart. Expect one message per appointment. | Accelerated, run C |
| AN-035 | A second copy on the same data folder sends no messages and shows the warning | Start a second copy. Expect the warning "Messages Not Sent" and no message from the second copy. | Both copies, alongside run N |

### 2.5 Setup, templates and sending failures

| ID | Title | Procedure and expected result | Run |
|---|---|---|---|
| AN-036 | A blank email sender sends no email, logs "sender missing", and adds nothing to history | Blank `[email-reminder] sender`. Expect no email, the log entry, the start-up warning, and no history entry. | Accelerated, run G1 |
| AN-037 | A blank text sender sends no text, logs "sender missing", and adds nothing to history | Blank `[sms-reminder] sender`. As AN-036 for texts. | Accelerated, run G1 |
| AN-038 | With no on-the-day template, the reminder template is used on the day | Remove `emailtoday`. Expect the reminder template's text on the day. | Accelerated, run G2 |
| AN-039 | A template file that cannot be read sends no confirmation, and the log says why | Point `emailconfirmation` at a missing file. Expect no confirmation email, the log entry, and the reminder and the on-the-day email still sent (they use their own templates). | Accelerated, run G2 |
| AN-040 | A send refused by the transport is retried after 5 minutes, and recorded once when it succeeds | Stub refuses the first two sends. Expect retries at +5 and +10 minutes, success on the third try, and one history entry. | Accelerated, run G3 |
| AN-041 | A send that always fails is given up after 10 tries, and the log says so | Stub refuses every send to one contact. Expect the confirmation tried ten times and then no more, and a log entry giving up. A given-up confirmation does not stop the reminder and the on-the-day message, which are each tried ten times and then given up in the same way. | Accelerated, run G3 |
| AN-042 | A text refused by the transport is not recorded as sent, and the reason is logged | Stub refuses one text with a reason. Expect the reason in the log, no history entry, and a retry. | Accelerated, run G3 |
| AN-043 | The SMS key expiry warning appears at start-up within 30 days of expiry, and not before | Set expiry 29 days ahead, start; then 31 days ahead, start. Expect a warning only for the first. | Accelerated, run C |
| AN-044 | A multi-line text template sends only its first line | Text template with two lines. Expect only the first line sent. | Accelerated, run G2 |
| AN-045 | A clock change in March keeps the reminder at 24 elapsed hours, and sends in local time | Made on 26 March 2027 at 10:00 for 12:00 on 28 March, so not confirm-only. Expect the confirmation at 10:05 on 26 March, a reminder at 11:00 on 27 March (24 elapsed hours, one hour earlier than wall-clock, across the clock change), and the on-the-day message from 07:00 on 28 March. | Accelerated, run D |
| AN-046 | A reminder and on-the-day message across New Year are sent on the right dates | Appointment for 1 January with reminder on 31 December. Expect both on the right days. | Accelerated, run D |
| AN-047 | Run A, as a whole: every expected message of the run is sent once, on time, and nothing unexpected is sent | Run A runs with its selected tests. Compare the messages sent with the expected list for those tests. Expect every expected message on time, once, and no extra. | Accelerated, run A |
| AN-048 | Setup's "Enable SMS and Email Reminders" off stops all automatic messages | Untick it. Create an appointment for contact 1 and run three days. Expect no message of any kind. Tick it again, create an appointment, and expect its confirmation at the hold's end. | Accelerated, run G4 |

---

## 3. Operation testing

Operation tests check the program without Google, sync or messages, unless a test says otherwise. **All of the tests in this section are automated.** Each one is run by the operation runner (`autotest/operation/operation_tests.py`), which starts the program headless through the test driver, presses the keys or triggers the menu item, and checks the result. The driver answers each dialog with its default button and writes the dialog's title to its log, so a dialog is checked by its title. A test passes when every expected result in its row is observed. A test that cannot run in the test environment is reported as blocked, with the reason. No test in this section needs a person to look at the screen or listen. The manual tests are the screen-reader checks in section 4. Section 4 also has automated checks, which need only the program.

### 3a. Start-up and shutdown

| ID | Title | Procedure and expected result | How run |
|---|---|---|---|
| SS-001 | A first start with no data folder creates the folder and asks for the key to be set | Delete the data folder; start. Expect the folder created and the key set-up offered. | Automated |
| SS-002 | A correct login password lets the program start the first time | Start; type the login password. Expect the contact list. | Automated |
| SS-003 | Three wrong passwords offer set-up or close, and close exits without changing data | Type a wrong password three times. Expect the choice; choose Close. Expect exit and unchanged data. | Automated |
| SS-004 | A second copy on the same folder shows the warning and still works for everything but messages | Start a second copy. Expect the warning and normal use of contacts. | Automated |
| SS-005 | Start-up lists overlapping appointments and changes nothing | Create two overlapping appointments; restart. Expect both listed and no change. | Automated |
| SS-006 | Start-up shows the SMS key warning only within 30 days of expiry | As AN-043, from the start-up side. | Automated |
| SS-007 | Start-up warns for each blank sender and logs it | Blank both senders; start. Expect a warning for each, and the log entries. | Automated |
| SS-008 | The first start on a day makes one backup zip, and a second start that day makes none | Start, close, start again. Expect one backup dated today. Run on five days: expect two zips kept. | Automated |
| SS-009 | Saved data is there after a restart | Add a contact, an appointment, a to-do item and a history entry; save; close; start. Expect all four. | Automated |
| SS-010 | Closing with unsaved edits asks Save, Discard or Cancel, and Cancel keeps the program open | Edit a contact without saving; close. Expect the question; choose Cancel. Expect the program open. | Automated |
| SS-011 | An unreadable contact file is reported; the other contacts load and the file is not overwritten | Corrupt one `.contact` file; start. Expect a report, the other contacts loaded, and the bad file unchanged. | Automated |
| SS-012 | Start with no ini file uses the defaults and reports that the file is missing | Remove the ini file; start. Expect the program to start, and the log to say the file was not found. | Automated |
| SS-013 | Start with a read-only data folder reports the error and loses no data | Make the folder read-only; start; edit; save. Expect an error and no loss of the stored data. | Automated |
| SS-014 | Data saved before a crash is there after it; unsaved edits are not | Save a contact; edit another without saving; kill the process; start. Expect the saved one, not the unsaved edit. | Automated |
| SS-015 | With no network, the program starts and works, and a sync reports the error | Disconnect; start; add a contact; sync. Expect normal start, and a sync error. | Automated |

### 3b. By menu and function

Each menu item is tested by choosing it from the menu, and by its key where it has one. F9 (Google Sync) is covered in area 1.

| ID | Title | Procedure and expected result | How run |
|---|---|---|---|
| MF-001 | File > Open lists the contacts, skips by surname letter, and has "create contact" at the top | Open the menu; press a surname letter; check the top entry. Expect the list, the skip, and the create entry. | Automated |
| MF-002 | File > Save (Ctrl+S) saves the open contact and its files | Edit; Ctrl+S. Expect the edit saved to the contact's file. | Automated |
| MF-003 | File > Export > Contacts writes a file with all contacts | Export. Expect a file with contacts 1 to 6. | Automated |
| MF-004 | File > Export > Appointments writes a file with all appointments | Export. Expect a file with the appointments in the window. | Automated |
| MF-005 | File > Import reads an export back without duplicates | Export; delete one contact; import. Expect the contact back, and no duplicates. | Automated |
| MF-006 | File > Tidy Records saves the screen's edit first, then shows its summary | Edit a contact on screen; Tidy Records. Expect the edit saved, and the summary. | Automated |
| MF-007 | File > Key and Password enables items according to whether the program is logged in | Check the items before and after logging in and out. Expect Set File Encryption Key and Change Login Password only when logged out, and Logout only when logged in. | Automated |
| MF-008 | File > Setup opens the Setup dialog (its controls are tested in 3c) | Open. Expect the Setup dialog. | Automated |
| MF-009 | File > Show > Deleted Appointments lists deleted appointments in braces, and the tick is off at start-up | Tick; check the list; restart; check the tick. Expect braces, and the tick off. | Automated |
| MF-010 | File > Show > Deleted Contacts and Hidden Contacts list those contacts | Tick each; check. Expect the deleted and hidden contacts listed. | Automated |
| MF-011 | File > Show > Logs opens a window that is not modal and shows new entries | Open; carry on working; watch the entries grow. Expect the window to stay open and update. | Automated |
| MF-012 | File > Exit closes the program after the unsaved-edit check | Exit with and without an edit. Expect the question only when there is an edit. | Automated |
| MF-013 | The File menu has no Test Function item | Open the File menu and look for Test Function. Expect none (ticket #119). | Automated |
| MF-014 | Find Here (Ctrl+F) and Find Next (F3) move through matches on the page | Search for a word that appears three times. Expect each F3 to move to the next. | Automated |
| MF-015 | Global Find (Ctrl+G) matches part of a name without case or accents, lists hidden contacts in brackets, and opens on Enter | Search "jac", "sam", an accented name; hide one contact. Expect matches, the hidden one in brackets, and Enter to open. | Automated |
| MF-016 | Advanced Global Find (Ctrl+Shift+G) opens its dialog and applies its criteria (see DL-020) | Open. Expect the dialog. | Automated |
| MF-017 | Email / SMS To Contacts lists contacts with an address for each channel | Open. Expect contacts with the right addresses. | Automated |
| MF-018 | Email / SMS To Non-Clients lists contacts not ticked Client | Open. Expect only non-clients. | Automated |
| MF-019 | Uncategorised Contacts lists contacts with no Type ticked | Open. Expect contacts with no Type. | Automated |
| MF-020 | Duplicate Contacts lists possible duplicates, and does not merge them (merging is Tidy Records) | Create two contacts with one address. Open. Expect both listed, not merged. | Automated |
| MF-021 | Duplicate Hidden Auto-Email Entries lists hidden contacts that share an address with a visible one | Hide one of two contacts sharing an address; open. Expect the pair listed. | Automated |
| MF-022 | Goto Me (Ctrl+Shift+O) opens the contact set as Identify Me | Set Identify Me; press the key. Expect that contact open. | Automated |
| MF-023 | Goto Journal (Ctrl+J) opens the journal in Easy Notepad, and a missing program gives an error | Press it with Easy Notepad present and absent. Expect the journal, then an error. | Automated |
| MF-024 | Goto To Do List (Ctrl+L) and Goto My To Do List (Ctrl+Shift+L) open the right lists | Press each. Expect the contact's list, then the user's. | Automated |
| MF-025 | Goto Today (Ctrl+T) selects the first entry from now on | Scroll away; press Ctrl+T. Expect the first entry from today. | Automated |
| MF-026 | Goto Date (Ctrl+D) opens the date box | Press. Expect the box with the cursor at the end. | Automated |
| MF-027 | Goto Contact Details (Alt+N, M, A, P, B) moves to each section of the form | Open a contact, enable editing; press each key. Expect the cursor in each section. | Automated |
| MF-028 | Insert Appointment After (Ctrl+I) adds an entry after the selected one, for the open contact | Select an entry; Ctrl+I. Expect the new entry after it, For the open contact. | Automated |
| MF-029 | Insert My Appointment After (Ctrl+Shift+I) adds an entry after the selected one, for the user | As MF-028, for the user. | Automated |
| MF-030 | New Appointment (Ctrl+N) opens the form for the open contact | Press. Expect the form, For set. | Automated |
| MF-031 | New My Appointment (Ctrl+Shift+N) opens the form for the user | Press. Expect the form, For the user. | Automated |
| MF-032 | Update Appointment (Ctrl+U) opens the selected entry's form | Select; press. Expect that entry's details. | Automated |
| MF-033 | Remove Appointment (Ctrl+R) deletes the selected entry after confirmation | Select; press; decline; press; accept. Expect no change on decline, and deletion on accept. | Automated |
| MF-034 | Edit History enables the history to be edited | Choose it. Expect history editing enabled for the open contact. | Automated |
| MF-035 | New Contact opens a blank contact | Choose it. Expect a blank Contact Details tab. | Automated |
| MF-036 | Contact Details enables editing of the open contact (Alt+E, D) | Choose it. Expect the form enabled. | Automated |
| MF-037 | Delete Contact asks for confirmation, then moves the contact to the deleted folder | Choose; decline; choose; accept. Expect no change, then the move. | Automated |
| MF-038 | Send EMail Contact (Ctrl+M) starts the mail command with the recipient and name filled in | Press. Expect the command with $TO and $NAME. | Automated |
| MF-039 | SMS Contact (Ctrl+Shift+M) opens the text form for the contact | Press. Expect the form for the contact. | Automated |
| MF-040 | Transaction Income (Ctrl++) adds an income entry to the contact's log | Press; enter an amount. Expect the entry in the log. | Automated |
| MF-041 | Transaction Expense (Ctrl+-) adds an expense entry to the contact's log | As MF-040, as an expense. | Automated |
| MF-042 | Transaction Report produces the year's report | Choose; enter the year. Expect the report with the entries. | Automated |
| MF-043 | Manual (F1) opens the manual, and F3 finds the next match | Press F1; search. Expect the manual, and the match. | Automated |
| MF-044 | About shows the program's name and version | Choose it. Expect name and version. | Automated |
| MF-045 | Typing a digit in the calendar opens the date box, and Escape closes it without moving | Press 2; Escape. Expect the list unchanged. | Automated |
| MF-046 | W and WW move the list a week and two weeks ahead, and T starts again from today | Press W, WW, T. Expect the moves. | Automated |
| MF-047 | N and P move between the open contact's entries and N wraps to the first | Open a contact with three entries; press N four times. Expect the wrap. | Automated |
| MF-048 | Space shows only the open contact's entries, and a second Space shows all | Press Space twice. Expect the filter, then all entries. | Automated |
| MF-049 | With no contact open, Space, N, P and the arrows beep and change nothing | Close all contacts; press each. Expect a beep and no change. | Automated |
| MF-050 | Overview (Alt+O) shows the open contact's summary | Open a contact; press Alt+O. Expect the summary. | Automated |
| MF-051 | History (Alt+Y) lists the contact's recorded changes and sent messages | Open a contact with history; press Alt+Y. Expect the entries. | Automated |
| MF-052 | To-Do List (Alt+L) saves its items with the contact | Add an item; save; restart. Expect the item. | Automated |
| MF-053 | Contact Details (Alt+D) is unavailable with no contact open, and available after Alt+E, D | Check both states. Expect them. | Automated |

### 3c. By dialog

| ID | Title | Procedure and expected result | How run |
|---|---|---|---|
| DL-001 | Setup saves each control on OK and discards the changes on Cancel | Change each control; Cancel; reopen; check. Then change and OK; check. | Automated |
| DL-002 | Setup's Identify Me choice is the contact Goto Me opens | Choose a contact; OK; press Ctrl+Shift+O. Expect that contact. | Automated |
| DL-003 | Setup's calendar and contacts masters are saved independently | Set calendar to G and contacts to CM; OK; reopen. Expect both kept. | Automated |
| DL-004 | Setup's window days accept whole numbers and refuse text | Enter 30, then text. Expect 30 kept, and the text refused. | Automated |
| DL-005 | Setup's SMS key expiry accepts day/month/year and refuses other text | Enter 25/01/2035 and 25/1/35; then 2035. Expect the first two accepted and the last refused. | Automated |
| DL-007 | Setup's Google Account Change shows the sign-in code, and Reset removes the account | Change; read the code; cancel. Reset. Expect the account removed. | Automated |
| DL-008 | The appointment form has For, Summary, From, To and Description, and Enter saves | Fill each field; Enter. Expect the entry saved with each value. | Automated |
| DL-009 | Escape in the appointment form cancels with no change | Change a field; Escape. Expect no change. | Automated |
| DL-010 | The duration arrows change To, and changing To changes the duration | Use up and down; then change To. Expect both kept in step. | Automated |
| DL-011 | A clash in the appointment form offers a change, and declining saves nothing | Create an overlap; choose not to change. Expect nothing saved. | Automated |
| DL-012 | A repeat keeps its type and interval, and a single appointment gets no repeat | Create a weekly repeat; reopen. Expect weekly. Create a single one; reopen. Expect none. | Automated |
| DL-013 | A repeat's end, by count or by date, is kept in the list and the form | Set count 3; reopen; set an end date. Expect each kept. | Automated |
| DL-014 | A bad email in the contact form stops the save, with focus in its box and nothing saved | Enter a bad address; Ctrl+S. Expect the box focused and no save. | Automated |
| DL-015 | A label typed after an address becomes the email's title | Type `name@domain.com (Work)`; save. Expect the title Work. | Automated |
| DL-016 | A phone typed with a (0) after the country code is saved in tidy form | Enter `+44 (0) 1234 567890`; save. Expect `+44 1234 567890`. | Automated |
| DL-017 | F2 changes a title, and a title with a colon is refused | Press F2 in an email title; enter a colon. Expect refused. | Automated |
| DL-018 | Clearing the only detail of a contact saves the change | Clear all details; save. Expect the change saved. | Automated |
| DL-019 | Quitting or switching contact with unsaved edits asks Save, Discard or Cancel | Edit; switch contact; Cancel; Discard. Expect each outcome. | Automated |
| DL-020 | Advanced Global Find applies its criteria, lists results, and opens on Enter | Set a criterion; search; Enter on a result. Expect the list and the contact open. | Automated |
| DL-021 | The transaction form checks the amount, and records income and expense | Enter text; then a number. Expect text refused, and the entry recorded. | Automated |
| DL-022 | The text form sends through the stub and records the send in history; a missing key gives a message | Send with the key set; then with it removed. Expect a send and history, then a message. | Automated |
| DL-023 | The Logs window updates while open, and closing it returns focus to the program | Open; add an entry; close. Expect the entry and the focus back. | Automated |
| DL-024 | The manual's Find and F3 work, and Escape closes it | Search; press Escape. Expect the search and the close. | Automated |
| DL-025 | The Set Encryption Key dialog refuses a mismatched key and a mismatched password | Enter two different keys; then two different passwords. Expect each refused with its message. | Automated |
| DL-026 | Change Password asks for the old password first, and refuses a wrong one | Enter a wrong old password. Expect refused. | Automated |
| DL-027 | The typed-date box moves the list for valid dates, and beeps and closes for text | Type 2/12/26; Enter. Then type "xyz". Expect the move, then the beep and close. | Automated |
| DL-028 | The tidy progress window shows its summary, and closes | Run Tidy Records. Expect the summary, then close. | Automated |
| DL-029 | Message boxes default to the safe answer, and Escape cancels without action | Delete a contact; press Escape. Expect no delete. | Automated |
| DL-030 | Export and Import file dialogs leave no file when cancelled | Export; cancel. Expect no file. | Automated |

### 3d. Imported operation tests

These tests were imported from the archive bench (`documents/imported-tests.md`). Each runs as an operation test, in an empty home folder, with the same results as above.

| ID | Title | Procedure and expected result | How run |
|---|---|---|---|
| IO-002 | Tidy Contacts merges two entries for one person when they share an address and do not conflict | Two entries for one person with the same address, one with a phone; run Tidy Contacts. Expect one live entry holding the address and the phone. (The note of the second entry is not yet checked: BLOCKED.) | Automated |
| IO-003 | Tidy Contacts does not merge two entries that share an address but have different names | Two entries with the same address and different names; run Tidy Contacts. Expect both entries still there, and nothing merged. | Automated |
| IO-004 | Quitting with edits asks Save, Discard or Cancel: Save refuses a bad entry, Discard keeps the stored contact | Edit a contact to a bad email; Save at the quit prompt is refused and the window stays open; Cancel keeps it open; Discard closes it. Expect the stored contact unchanged, with no bad email and no phone change. | Automated |
| IO-006 | Tidy Contacts lists an entry it cannot fix, leaves it alone, and still fixes the other entry on the contact | A contact with an email that has two labels and a phone with a label; run Tidy Contacts. Expect the email listed as unfixable and left as it was, and the phone fixed. | Automated |
| IO-007 | Double-clicking a calendar entry opens its contact, and keeps the calendar row and its focus | A calendar entry for a contact; click it. Expect that contact opened, the same calendar row kept, and focus on the calendar list. | Automated |
| IO-009 | A weekly appointment is listed on each week in the repeats window, and stored once | A weekly appointment; list the calendar. Expect one row for each week in the repeats window, and the appointment stored once. Selecting a repeat opens its contact. | Automated |
| IO-010 | An edit of a repeat changes its appointment; a move to another day moves the appointment and the focus with it | A weekly appointment; edit one occurrence's summary (the change is stored once; the focus stays on the repeat), then move that occurrence to the next day. Expect the appointment moved by one day, with the focus on it. | Automated |
| IO-011 | A birthday saved on a contact shows in the calendar straight away, with birthdays shown | Save a birthday on a contact, with birthdays shown; read the calendar without a restart. Expect the new date shown and the old one gone. (BLOCKED: the driver cannot set a birthday yet.) | Automated |
| IO-012 | A repeat whose source is older than the window is listed, and the source is not | A weekly appointment that started 120 days ago; list the calendar. Expect its repeats in the window listed, the source not listed, and the source stored once. | Automated |
| IO-014 | Goto Today selects the first entry that has not ended, including a repeat, rather than the end of the list | A weekly repeat that started three days ago; press Goto Today. Expect the entry four days ahead selected, not the end of the list. | Automated |
| IO-015 | Shortening a repeat from a later entry keeps the focus on the nearest entry of that repeat | A weekly repeat; shorten it from its fifteenth occurrence. Expect two entries left and the Add row, with focus on the nearest remaining entry of that repeat. | Automated |
| IO-016 | OK on the edit of a repeat that started 11 days ago closes the form, with no dialog | A repeat (every three weeks, 32 times) that started 11 days ago; edit it and press OK. Expect the form to close, with no dialog shown. | Automated |
| IO-018 | An appointment file moved into the archive is not loaded at start-up, and start-up leaves it in the archive | Save an appointment; move its file into the calendar's archive folder; start the program. Expect the appointment not loaded, and the file still in the archive. | Automated |
| IO-019 | A start with no to-do or history file opens and writes its data | Start the program in an empty home folder. Expect the program to start and write its data. | Automated |
| IO-020 | Notes in plain text: a contact's to-do list and history are saved as plain files, and a restart reads them back | Give a contact a to-do list and a history; save; restart. Expect plain `.todolist` and `.history` files holding the text, and the contact read back after the restart. | Automated |
| OF-001 | A contact file written by an older version keeps its meaning: its labels, addresses and phones load, and survive a save | Build `autotest/unit/oldfile/old_contact_test.cpp` (`autotest/unit/run-old-contact-test.sh`). Expect each check to pass: the three addresses in their slots, the old email and phone labels, a phone2 title kept as a label, and the file unchanged after a save. | Automated (test program) |

---

## 4. Accessibility (manual, with automated checks)

Accessibility tests come in two groups.

- **Manual (AC-001 to AC-018).** Carried out with a screen reader (NVDA or the platform's inspector) and with keyboard only. Each test states what a person should hear or see, and the expected result is recorded by the person running it. Their data is set up by an automated step, `autotest/accessibility/setup_manual_data.py`, which uploads the fixture contacts and appointments to the scratch Google account. It then asks the tester to set both masters to Google and run a sync in ContactManager, and waits for the tester to confirm before the manual tests begin.
- **Automated (AC-019 to AC-029, table 4a).** Each one supports a manual test. They check what the program gives a screen reader: the names of menus, tabs and controls, the focus, and the text of the screen reader text. They do not check that a screen reader speaks it well, or how it sounds, or that focus can be seen. Those stay with the manual tests.

| ID | Title | Procedure and expected result | Run |
|---|---|---|---|
| AC-001 | Every menu and submenu is reached from the keyboard, and each item is named | Walk the menus with Alt keys and arrows; the screen reader reads each name. Expect every item reached and named. | Manual, screen reader |
| AC-002 | Every tab is reached with its Alt key, and its name is read | Press each tab key. Expect the tab name read. | Manual, screen reader |
| AC-003 | Every control in every dialog is reached with Tab, and its label is read | For each dialog in 3c, Tab through. Expect every control reached and labelled. | Manual, screen reader |
| AC-004 | Show Screen Reader Text follows the focus and updates its values | Open it; move focus; change a value. Expect the dialog to follow. | Manual, screen reader |
| AC-005 | Calendar entries are read with their date, time and summary, at a usable length | Move through entries. Expect each read in full, and not padded with the program path (issue #22). | Manual, screen reader |
| AC-006 | Each field of the contact form is read with its label, title and section | Tab through each section. Expect the label, title and section announced. | Manual, screen reader |
| AC-007 | Moving through lists and failures give different sounds | Move; trigger a failure. Expect two distinct sounds. | Manual, listening |
| AC-008 | Error and warning messages take focus and are read | Trigger a warning. Expect it read at once. | Manual, screen reader |
| AC-009 | Escape closes every dialog and returns focus to the window that opened it | Open and close each dialog with Escape. Expect focus returned. | Manual |
| AC-010 | Every action has a key: double-clicking a calendar entry has the Enter equivalent | Select an entry; press Enter. Expect the same result as a double-click. | Manual |
| AC-011 | The typed-date box has a short name and description | Open it. Expect a short name and description read. | Manual, screen reader |
| AC-012 | Focus stays on the entry after a sync or Tidy Records | Select an entry; sync; Tidy. Expect the focus on the entry. | Manual |
| AC-013 | The manual is read and searched with a screen reader | Read a section; search. Expect the text and matches read. | Manual, screen reader |
| AC-014 | A key that does nothing gives audible feedback, not silence | Press keys with no contact open. Expect a beep. | Manual, listening |
| AC-015 | The sync progress bar's value and the log lines are readable | Sync. Expect the progress read at intervals. | Manual, screen reader |
| AC-016 | Dates and times are read with their format, and a bad entry is announced | Enter a bad date. Expect the bad entry announced. | Manual, screen reader |
| AC-017 | A state is shown in words, not only by colour or layout | Check deleted items (braces), disabled tabs and disabled menu items. Expect each to say so in words. | Manual |
| AC-018 | The keyboard focus is always visible | Tab through the window. Expect a visible focus on each control. | Manual |

### 4a. Automated checks of what a screen reader is given

Run by `autotest/accessibility/automated_tests.py`, through the operation driver. They need the program only: no Google account, no message. Each reads the program's own records (names, roles, focus, the screen reader text) and checks them against the expected result.

| ID | Title | Procedure and expected result | Run |
|---|---|---|---|
| AC-019 | Every menu and submenu item is named, and every top menu has its Alt letter (supports AC-001) | Read the menu bar. Expect every item to have a name, and every top menu to have a letter (&). | Automated |
| AC-020 | Every tab is named, and has its Alt letter (supports AC-002) | Read the main window's tabs. Expect each to have a name and a letter (&). | Automated |
| AC-021 | Every control in each dialog that Tab reaches has a name (supports AC-003) | Open, in turn, Setup, the appointment form, Advanced Global Find, the text form and the transaction form. For each, read the controls Tab reaches. Expect each to have a name. | Automated |
| AC-022 | Show Screen Reader Text follows the focus and updates its values (supports AC-004) | Open it. Select a calendar entry: expect its text to name the calendar list. Press Tab: expect the text to name the new control. | Automated |
| AC-023 | Calendar entries are read with their date, time and summary, at a usable length (supports AC-005) | Read the accessible text of three test appointments. Expect each to hold its time and summary, no program path, and 200 characters or fewer. | Automated |
| AC-024 | Each field of the contact form is read with its label and its title (supports AC-006) | Open a contact for editing. Expect each field (names, surname, email, home, work and mobile) to be reached and named, and each label and title to have text. | Automated |
| AC-025 | An error or warning takes the focus, so that it is read at once (supports AC-008) | Enter an invalid SMS key expiry in Setup and press OK. Expect the warning to be the active window, the focus to be in it, and its message to have text. | Automated |
| AC-026 | Escape closes every dialog and returns focus to the window that opened it (supports AC-009) | From the calendar list, open Setup, Advanced Global Find and About, and press Escape in each. Expect each to close and the focus to return to the calendar list. | Automated |
| AC-027 | Every action has a key: Enter on a calendar entry does what a double-click does (supports AC-010) | Double-click an entry, and note the contact, the entry and the windows open. Select the same entry and press Enter. Expect the same three. | Automated |
| AC-028 | The typed-date box has a short name and a description (supports AC-011) | Open the typed-date box. Expect a name of four words or fewer, and a description. | Automated |
| AC-029 | Focus stays on the entry after Tidy Records (supports the Tidy part of AC-012) | Select a calendar entry, run Tidy Records. Expect the focus on the calendar list and the same entry selected. The sync part of AC-012 stays manual. | Automated |

The automated checks cover the dialogs and controls named above, not every control in the program. A control that is not in those dialogs is checked only by the manual tests.

---

### 4b. Imported accessibility test

This test was imported from the archive bench (`documents/imported-tests.md`), and is run with the section 4a checks.

| ID | Title | Procedure and expected result | How run |
|---|---|---|---|
| IA-002 | The accessibility debug checkbox in the screen reader text dialog turns console tracing on and off | Open Show Screen Reader Text; tick its debug checkbox, then clear it. Expect the box to follow each change, and the console to log `AccessibleDebug on` and then `AccessibleDebug off`. | Automated |

## 5. Capacity stress test (not part of the normal cycle)

This is not part of the normal test cycle. It checks how the program and Google behave with a very large data set: 250 contacts and 1,000 appointments. It is run on the scratch account only, when the capacity of the sync is in question. It is not expected in routine runs, and the specification does not expect thousands of records in any other test.

| ID | Title | Procedure and expected result | How run |
|---|---|---|---|
| CS-001 | Purge the account, then upload 250 contacts and 1,000 appointments, half by the Google API and half by ContactManager, and leave them in place | The purge removes everything of each kind on the account, so run it only on the scratch account. Expect every contact and appointment on Google once, and the sync to finish within its time. Records are left in place. | Automated (bench), capacity only |

## 6. Unit suite

Reference `UT`. The unit suite is one build and one run of unit tests, numbered `UT-001` to `UT-114` in the order they run. It covers 114 unit tests: 84 announcement tests (area 2), 22 calendar tests (area 3b) and 8 sync tests (area 1). The numbers follow the order of the test functions in the file, and the group column gives each one's section in the file. The bench scenario L01 is the bench's runner for the suite, so it is not counted again. The suite is included in the cycle total in `documents/test-duration-estimate.md`.

| ID | Unit test (function in `tst_messages.cpp`) | Area | Group in the file |
|---|---|---|---|
| UT-001 | `reminderDueWithinTheReminderHours` | Announcements (area 2) | The reminder: 24 hours before |
| UT-002 | `reminderDueAtTheStartOfTheDayBefore` | Announcements (area 2) | The reminder: 24 hours before |
| UT-003 | `reminderDueForAnAppointmentThatHasStarted` | Announcements (area 2) | The reminder: 24 hours before |
| UT-004 | `reminderFollowsRemindHours` | Announcements (area 2) | The reminder: 24 hours before |
| UT-005 | `confirmOnlyJustInside36Hours` | Announcements (area 2) | The confirm-only window: 36 hours by default |
| UT-006 | `confirmOnlyExactly36HoursIsNot` | Announcements (area 2) | The confirm-only window: 36 hours by default |
| UT-007 | `confirmOnlyForShortNotice` | Announcements (area 2) | The confirm-only window: 36 hours by default |
| UT-008 | `confirmOnlyAndReminderMeetAtTheBoundary` | Announcements (area 2) | The confirm-only window: 36 hours by default |
| UT-009 | `confirmOnlyDefaultIs1Point5TimesRemind` | Announcements (area 2) | The confirm-only window: 36 hours by default |
| UT-010 | `confirmOnlyUsesAValidValue` | Announcements (area 2) | The confirm-only window: 36 hours by default |
| UT-011 | `confirmOnlyLessThanRemindUsesTheDefault` | Announcements (area 2) | The confirm-only window: 36 hours by default |
| UT-012 | `confirmOnlyNotANumberUsesTheDefault` | Announcements (area 2) | The confirm-only window: 36 hours by default |
| UT-013 | `remindHoursDefaultsAndLimits` | Announcements (area 2) | The confirm-only window: 36 hours by default |
| UT-014 | `holdLastsWaitMinutes` | Announcements (area 2) | The hold, the send window and the same day |
| UT-015 | `sendWindowIsStartInclusiveEndExclusive` | Announcements (area 2) | The hold, the send window and the same day |
| UT-016 | `onTheDayUsesLocalDate` | Announcements (area 2) | The hold, the send window and the same day |
| UT-017 | `dayRecordKeepsTheChannelsOfTheDay` | Announcements (area 2) | The hold, the send window and the same day |
| UT-018 | `newAppointmentByEmailGetsAConfirmation` | Announcements (area 2) | The messages decided for an appointment |
| UT-019 | `newAppointmentByTextGetsAConfirmation` | Announcements (area 2) | The messages decided for an appointment |
| UT-020 | `newAppointmentWithBothGetsEmailOnly` | Announcements (area 2) | The messages decided for an appointment |
| UT-021 | `newAppointmentWithNoChannelGetsNothing` | Announcements (area 2) | The messages decided for an appointment |
| UT-022 | `confirmOnlyFoldsTheReminderIntoTheConfirmation` | Announcements (area 2) | The messages decided for an appointment |
| UT-023 | `changedAppointmentGetsAnUpdateOnItsChannel` | Announcements (area 2) | The messages decided for an appointment |
| UT-024 | `unchangedSentAppointmentGetsNothing` | Announcements (area 2) | The messages decided for an appointment |
| UT-025 | `deletedSentAppointmentGetsACancellationOnItsChannel` | Announcements (area 2) | The messages decided for an appointment |
| UT-026 | `deletedUnsentAppointmentGetsNothing` | Announcements (area 2) | The messages decided for an appointment |
| UT-027 | `reminderOnceByTheChannelOfTheContact` | Announcements (area 2) | The messages decided for an appointment |
| UT-028 | `reminderIsNotSentTwice` | Announcements (area 2) | The messages decided for an appointment |
| UT-029 | `reminderNotDueIsNotSent` | Announcements (area 2) | The messages decided for an appointment |
| UT-030 | `removedOccurrenceIsCancelledOnItsChannel` | Announcements (area 2) | The messages decided for an appointment |
| UT-031 | `removedOccurrenceOnlyForARepeat` | Announcements (area 2) | The messages decided for an appointment |
| UT-032 | `onTheDayBothChannelsAreSentOnTheDay` | Announcements (area 2) | The on-the-day messages: both channels, once each, on the day |
| UT-033 | `onTheDayTextIsSentWhenEmailHasGone` | Announcements (area 2) | The on-the-day messages: both channels, once each, on the day |
| UT-034 | `onTheDayEmailIsSentWhenTextHasGone` | Announcements (area 2) | The on-the-day messages: both channels, once each, on the day |
| UT-035 | `onTheDayIsNotSentAgainOnceBothHaveGone` | Announcements (area 2) | The on-the-day messages: both channels, once each, on the day |
| UT-036 | `onTheDayIsSentForASingleChannel` | Announcements (area 2) | The on-the-day messages: both channels, once each, on the day |
| UT-037 | `onTheDayIsNotSentOnAnotherDay` | Announcements (area 2) | The on-the-day messages: both channels, once each, on the day |
| UT-038 | `sameDayBookingGivesItsEmailAndThenTheOnTheDayText` | Announcements (area 2) | The on-the-day messages: both channels, once each, on the day |
| UT-039 | `sameDayTextBookingCoversTheDaysText` | Announcements (area 2) | The on-the-day messages: both channels, once each, on the day |
| UT-040 | `sameDaySmsIsSentWhenBookedEarlierAndTheDayComes` | Announcements (area 2) | The on-the-day messages: both channels, once each, on the day |
| UT-041 | `caseSevenEmailIsTheConfirmationWhenWindowOpensLate` | Announcements (area 2) | The cases in the messaging rules document, decided one message at a time |
| UT-042 | `caseSevenTextIsTheOnTheDayTextWhenWindowOpensLate` | Announcements (area 2) | The cases in the messaging rules document, decided one message at a time |
| UT-043 | `caseSevenBothIsEmailThenTheOnTheDayText` | Announcements (area 2) | The cases in the messaging rules document, decided one message at a time |
| UT-044 | `caseEightNoReminderOnTheDay` | Announcements (area 2) | The cases in the messaging rules document, decided one message at a time |
| UT-045 | `reminderOnTheDayIsReplacedByTheOnTheDayText` | Announcements (area 2) | The cases in the messaging rules document, decided one message at a time |
| UT-046 | `channelSwitchedToTextSendsOnlyWhatIsPending` | Announcements (area 2) | The cases in the messaging rules document, decided one message at a time |
| UT-047 | `scenarioCreatedThirtySixHoursAheadBothChannels` | Announcements (area 2) | The timing and the messages together, at the 24 and 36 hour boundaries |
| UT-048 | `scenarioCreatedTwentyFiveHoursAheadBothChannels` | Announcements (area 2) | The timing and the messages together, at the 24 and 36 hour boundaries |
| UT-049 | `scenarioBookedOnTheDayBothChannels` | Announcements (area 2) | The timing and the messages together, at the 24 and 36 hour boundaries |
| UT-050 | `listedOccurrencesOfARepeat` | Calendar (area 3b) | Appointments: the occurrences the calendar lists |
| UT-051 | `removedOccurrenceIsNotListed` | Calendar (area 3b) | Appointments: the occurrences the calendar lists |
| UT-052 | `repeatWithEveryOccurrenceRemovedListsNothing` | Calendar (area 3b) | Appointments: the occurrences the calendar lists |
| UT-053 | `periodsTouchingAtAnEndDoNotClash` | Calendar (area 3b) | Appointments: the occurrences the calendar lists |
| UT-054 | `clashIgnoresARemovedOccurrence` | Calendar (area 3b) | Appointments: the occurrences the calendar lists |
| UT-055 | `clashWithAListedOccurrence` | Calendar (area 3b) | Appointments: the occurrences the calendar lists |
| UT-056 | `repeatWithNoOccurrenceLeftIsFound` | Calendar (area 3b) | Appointments: the occurrences the calendar lists |
| UT-057 | `clashIsNotReportedForAnEmptyRepeat` | Calendar (area 3b) | Appointments: the occurrences the calendar lists |
| UT-058 | `getDateGivesACopyThatStaysTheSame` | Calendar (area 3b) | Appointments: the occurrences the calendar lists |
| UT-059 | `sentStateDoesNotCountAsAnEdit` | Calendar (area 3b) | Appointments: the occurrences the calendar lists |
| UT-060 | `googleRecordCopyDoesNotRestartTheWait` | Calendar (area 3b) | Appointments: the occurrences the calendar lists |
| UT-061 | `contactLinkIsKeptWhenTheContactExists` | Calendar (area 3b) | Appointments: the occurrences the calendar lists |
| UT-062 | `contactLinkIsSetByAnExactNameMatch` | Calendar (area 3b) | Appointments: the occurrences the calendar lists |
| UT-063 | `contactLinkIsRemovedWhenTheNameMatchesMoreThanOne` | Calendar (area 3b) | Appointments: the occurrences the calendar lists |
| UT-064 | `contactLinkIsRemovedWhenNoNameMatches` | Calendar (area 3b) | Appointments: the occurrences the calendar lists |
| UT-065 | `singleAppointmentEndedBeforeTheEdgeIsExpired` | Calendar (area 3b) | Expired appointments and repeats that have passed the edge of the window (issue #96) |
| UT-066 | `repeatStartMovesToTheFirstOccurrenceInTheWindow` | Calendar (area 3b) | Expired appointments and repeats that have passed the edge of the window (issue #96) |
| UT-067 | `repeatTrimKeepsOnlyTheExceptionsStillToCome` | Calendar (area 3b) | Expired appointments and repeats that have passed the edge of the window (issue #96) |
| UT-068 | `repeatWithNoOccurrenceLeftIsExpired` | Calendar (area 3b) | Expired appointments and repeats that have passed the edge of the window (issue #96) |
| UT-069 | `trimKeepsTheMessageSignatureSoTheContactIsNotMessaged` | Calendar (area 3b) | Expired appointments and repeats that have passed the edge of the window (issue #96) |
| UT-070 | `focusGoesToTheNearestDate` | Calendar (area 3b) | Expired appointments and repeats that have passed the edge of the window (issue #96) |
| UT-071 | `twoDigitYearsUseTheCenturyOfThisYear` | Calendar (area 3b) | Expired appointments and repeats that have passed the edge of the window (issue #96) |
| UT-072 | `ratePacerIsOffWithoutRateLimits` | Sync (area 1) | Issue #94: pacing of the uploads after Google's rate limits |
| UT-073 | `ratePacerStartsAtFourSecondsAfterFiveLimitedOfAHundred` | Sync (area 1) | Issue #94: pacing of the uploads after Google's rate limits |
| UT-074 | `ratePacerHalvesEvery500DownToHalfASecond` | Sync (area 1) | Issue #94: pacing of the uploads after Google's rate limits |
| UT-075 | `ratePacerCountsOnlyTheLastHundred` | Sync (area 1) | Issue #94: pacing of the uploads after Google's rate limits |
| UT-076 | `numericDatesAreParsed` | Sync (area 1) | Issue #94: pacing of the uploads after Google's rate limits |
| UT-077 | `typedDatesAreParsed` | Sync (area 1) | Issue #94: pacing of the uploads after Google's rate limits |
| UT-078 | `loadKeepsTheSavedUpdatedDate` | Sync (area 1) | Issue #94: pacing of the uploads after Google's rate limits |
| UT-079 | `mergedSentStateDoesNotCountAsAnEdit` | Sync (area 1) | Issue #94: pacing of the uploads after Google's rate limits |
| UT-080 | `limitsStopMessagesOnceReached` | Announcements (area 2) | Corner cases from issue #110 |
| UT-081 | `limitsAreCheckedHourFirst` | Announcements (area 2) | Corner cases from issue #110 |
| UT-082 | `failedMessageWaitsBeforeItIsTriedAgain` | Announcements (area 2) | Corner cases from issue #110 |
| UT-083 | `failedMessageIsGivenUpAfterMaxRetries` | Announcements (area 2) | Corner cases from issue #110 |
| UT-084 | `messageWithNoFailureMayBeTried` | Announcements (area 2) | Corner cases from issue #110 |
| UT-085 | `channelNeedsItsOwnAddress` | Announcements (area 2) | Corner cases from issue #110 |
| UT-086 | `missingEmailAddressSendsNothingByEmailButTextStillGoes` | Announcements (area 2) | Corner cases from issue #110 |
| UT-087 | `noUsableChannelSendsNothing` | Announcements (area 2) | Corner cases from issue #110 |
| UT-088 | `onTheDayUsesTheReminderTemplateWhenNotSet` | Announcements (area 2) | Corner cases from issue #110 |
| UT-089 | `reminderIs24ElapsedHoursAcrossTheClockChange` | Announcements (area 2) | Corner cases from issue #110 |
| UT-090 | `onTheDayIsTheLocalDateAcrossTheClockChange` | Announcements (area 2) | Corner cases from issue #110 |
| UT-091 | `onTheDayAndReminderAcrossTheYearBoundary` | Announcements (area 2) | Corner cases from issue #110 |
| UT-092 | `onTheDayAndReminderAcrossTheLeapDay` | Announcements (area 2) | Corner cases from issue #110 |
| UT-093 | `backlogOfChangesGivesOneUpdate` | Announcements (area 2) | Corner cases from issue #110 |
| UT-094 | `deletedAndRecreatedInsideTheHoldSendsOneConfirmation` | Announcements (area 2) | Corner cases from issue #110 |
| UT-095 | `secondOccurrenceOfARepeatIsStillReminded` | Announcements (area 2) | Corner cases from issue #110 |
| UT-096 | `signatureChangesOnlyForWhatMessagesShow` | Announcements (area 2) | Corner cases from issue #110 |
| UT-097 | `editWithinConfirmOnlyReplacesTheReminder` | Announcements (area 2) | Corner cases from issue #110 |
| UT-098 | `secondCopyCannotTakeTheDataLock` | Announcements (area 2) | Corner cases from issue #110 |
| UT-099 | `smsKeyIsReportedInTheLastThirtyDays` | Announcements (area 2) | Corner cases from issue #110 |
| UT-100 | `onlyASentMessageIsRecorded` | Announcements (area 2) | Corner cases from issue #110 |
| UT-101 | `blankEmailSenderSendsNothingAndSaysWhy` | Announcements (area 2) | Message senders: from ContactManager.ini only, never from the template |
| UT-102 | `blankSmsSenderSendsNothingAndSaysWhy` | Announcements (area 2) | Message senders: from ContactManager.ini only, never from the template |
| UT-103 | `whitespaceOnlySenderCountsAsBlank` | Announcements (area 2) | Message senders: from ContactManager.ini only, never from the template |
| UT-104 | `emailIsSentWithTheIniSender` | Announcements (area 2) | Message senders: from ContactManager.ini only, never from the template |
| UT-105 | `smsIsSentWithTheIniSenderAndTheText` | Announcements (area 2) | Message senders: from ContactManager.ini only, never from the template |
| UT-106 | `transportFailureIsReturnedWithItsReason` | Announcements (area 2) | Message senders: from ContactManager.ini only, never from the template |
| UT-107 | `maybeSentIsReturnedAsMaybeSent` | Announcements (area 2) | Message senders: from ContactManager.ini only, never from the template |
| UT-108 | `templateFirstLineIsTheSubjectAndTheRestIsTheBody` | Announcements (area 2) | Templates: the first line is the subject (or the text), and there is no sender line |
| UT-109 | `smsTemplateIsOneLineOfText` | Announcements (area 2) | Templates: the first line is the subject (or the text), and there is no sender line |
| UT-110 | `templateWithNoSubjectIsRejected` | Announcements (area 2) | Templates: the first line is the subject (or the text), and there is no sender line |
| UT-111 | `shippedTemplatesHaveNoSenderLine` | Announcements (area 2) | Templates: the first line is the subject (or the text), and there is no sender line |
| UT-112 | `missingSenderWarningsNameEachBlankChannel` | Announcements (area 2) | Startup: a warning for each blank sender |
| UT-113 | `testCodeIsTheLeadingCode` | Announcements (area 2) | Startup: a warning for each blank sender |
| UT-114 | `testTagIsReplacedAndNothingElseChanges` | Announcements (area 2) | Startup: a warning for each blank sender |

The suite's source is `autotest/unit/tst_messages.cpp`, and it is run with `sh autotest/unit/run-messages-tests.sh <build folder>`, which builds it against the application's own objects (documents/headless-test.md). It runs headless, in an empty home folder.

---

## Appendix A. Traceability: specification section to tests

| Specification section | Tests |
|---|---|
| 1 Overview (keyboard operation, screen reader) | AC-001 to AC-018 (manual); AC-019 to AC-029 (automated) |
| 2.1 Files and folders | SS-001, SS-008, SS-011, SS-013, GS-012 |
| 2.2 The ini file | SS-012, AN-036, AN-037, AN-038, AN-039, AN-045 |
| 2.3 Settings (Setup) | DL-001 to DL-007 |
| 2.4 Messages and templates | AN-036 to AN-039, AN-044 |
| 2.5 Encryption and passwords | SS-002, SS-003, SS-014, MF-007, DL-025, DL-026, GS-012 |
| 2.6 Linking services | DL-007, GS-031, GS-041, AN-043 |
| 3.1 Opening and creating contacts | MF-001, MF-022, MF-035 |
| 3.2 Editing a contact | DL-014 to DL-018, GS-001, GS-002, GS-003, GS-005, GS-008, GS-010, GS-011, GS-012 |
| 3.3 Storage | GS-002, GS-008, GS-012, SS-009 |
| 3.4 Searching | MF-014 to MF-021 |
| 3.5 Tidy Records | MF-006, DL-028 |
| 3.6 Hiding and deleting | GS-008, GS-011, MF-037, AN-026, AN-027, GS-029 |
| 3.7 Import and export | MF-003, MF-004, MF-005, DL-030 |
| 3.8 Transactions | MF-040, MF-041, MF-042, DL-021 |
| 4.1 What is listed | MF-009, GS-023, GS-038 |
| 4.2 Moving around the list | MF-025, MF-045 to MF-049 |
| 4.3 Typing a date | DL-027, MF-045 |
| 4.4 Creating and changing | MF-028 to MF-033 |
| 4.5 The appointment form | DL-008 to DL-011 |
| 4.6 Repeats | GS-020, DL-012, DL-013, AN-015, AN-016 |
| 4.7 Clashes | DL-011, SS-005, GS-030 |
| 4.8 Storage, archive, deletion | GS-023, GS-017 |
| 4.9 Birthdays | MF-009 (birthdays list), GS-001 |
| 5.1 Messages | AN-001 to AN-006, AN-015 |
| 5.2 Who is messaged | AN-029, AN-030, AN-026, AN-027 |
| 5.3 By channel setting | AN-001 to AN-006, AN-028 |
| 5.4 The hold | AN-009 to AN-014 |
| 5.5 Send window | AN-017, AN-020, AN-021 |
| 5.6 Confirm-only | AN-008 |
| 5.7 Limits | AN-022, AN-023 |
| 5.8 Missed messages and backlogs | AN-031 to AN-034 |
| 5.9 Two copies | AN-035, GS-039 |
| 5.10 Failures | AN-040 to AN-042 |
| 5.11 Expiry and warnings | AN-043, SS-006, SS-007, AN-036, AN-037 |
| 5.12 What is not messaged | AN-014, AN-025 |
| 6 Menus and keys | MF-001 to MF-053 |
| 7.1 What is synchronised | GS-001, GS-002, GS-015, GS-025 |
| 7.2 Masters and conflicts | GS-005 to GS-010 |
| 7.3 The sync process | GS-001 to GS-003 |
| 7.4 The calendar window | GS-023, GS-038 |
| 7.5 Retries and rate limits | GS-033 |
| 7.6 Interrupted syncs | GS-035 |
| 7.7 Failures and recovery | GS-031, GS-033 |

---

## Appendix B. Requirements that have no test yet

| No. | Item | What is needed |
|---|---|---|
| 1 | Spelling of the test domain `sgc.contacmanagertest` | Confirm before the first live run (0.4). |
| 2 | The send-record write failure (issue #110, item 3) | A test that the message is retried and not sent twice (specification 5.10). |
| 3 | The stub transports and the accelerated harness are in a scratch folder, not the repository | Bring them into `autotest/` before area 2 is run again. |
| 4 | The Test Function menu item | Removed in ticket #119. Once that is done, check that the File menu has no Test Function item. |
| 5 | The download window read from the ini file | Tests GS-023 and GS-038, and the window scenarios, after ticket #118. |
| 6 | Google sync for a token refused on a later page of a listing (issue #110, item 19) | A test in area 1. |
| 7 | Read-back of archive and deleted-folder writes (issue #110, item 18) | Add read-back checks to the archive and deleted-folder tests. |
| 8 | Google-made appointments get no confirmation (issue #110, item 15) | Decide the rule first, then add a test in area 2. |
| 9 | Area 4 manual tests need a person and a screen reader | Schedule with the tester; the expected results are recorded by hand. The automated set-up step is `autotest/accessibility/setup_manual_data.py`. |
