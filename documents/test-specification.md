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
4. Accessibility
5. Capacity stress test (not part of the normal cycle)
- Appendix A. Tests imported from the archive, with their titles
- Appendix B. Traceability: specification section to tests
- Appendix C. Requirements that have no test yet

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

Numbers 001 to 099 are new tests written for this specification. Numbers from 101 are tests **imported** from the archive (`autotest/archive/`). Their original names are kept in Appendix A, so each can be traced to the code that runs it.

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
- The domain is spelled as specified: `sgc.contacmanagertest`. Appendix C, item 1, asks for the spelling to be confirmed before the first live run, because the clean-up utility finds test records by this domain.
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

At 360 times, one real second is 6 minutes of program time. The program checks for due messages once per real second, so a message can be up to one check (6 minutes) late in program time. Area 2 expectations allow this: a message is **on time** when it is sent at the expected minute, or at the next check (within 6 minutes), and not later. Any later is a failure.

The clock is set by the test harness, through a shim that changes the time the program reads (the shim is in the scratch folder, with the harness: Appendix C, item 3). The program's own timers and time reads all follow it.

### 0.8 Stub transports

Area 2 does not send real email or text. The send functions are replaced by stubs that record each message (recipient, subject or text, body, time) and return a chosen result (sent, refused, timed out). The unit tests already use this approach (`StubTransport` in `autotest/archive/messages/tst_messages.cpp`). The accelerated harness used in earlier runs keeps its stub in a scratch folder: bringing that stub into the repository is a prerequisite for area 2 (Appendix C, item 3).

### 0.9 Run order and locations

Read `headless-test.md` (how to run the bench and the accelerated harness) and `gotchas.md` (what has gone wrong before) before running any area.

- Area 1 needs the Google account and the network. It runs last in any cycle, because it changes the account.
- Area 2 needs no network, and runs in the accelerated harness.
- Area 3 needs the program only. Imported tests in 3a to 3c run from `autotest/archive/`.
- Area 4 needs the program and a screen reader (NVDA, or the accessibility inspection tools of the platform), and a person to listen.

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

So there are 13 merged tests, which together contain the checks of 31 tests from an earlier draft, and 11 single tests: 42 in all. Every check is kept.

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
| D (dates) | AN-045, AN-046 | Each starts the clock on a date of its own (a clock change; New Year). | up to 2 virtual days |
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
| AN-016 | Each occurrence of a repeat is reminded once | Weekly repeat for contact 5 over three weeks. Expect one reminder per occurrence, and none twice. | Accelerated, run A |

### 2.2 Send window, silent period and backlogs

| ID | Title | Procedure and expected result | Run |
|---|---|---|---|
| AN-017 | An appointment made at 21:30 is confirmed at 07:00, when the window opens | Create at 21:30 for the next day. Expect the confirmation at 07:00, not before. | Accelerated, run A |
| AN-018 | A reminder due in the silent period is sent once, at 07:00 | Appointment whose reminder is due at 23:00. Expect the reminder at 07:00, once. | Accelerated, run A |
| AN-019 | Edits made overnight send one update at 07:00, with the final times | Confirm at 20:00; edit at 21:00, 22:30 and 23:30. Expect one update at 07:00 with the final times. | Accelerated, run A |
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
| AN-039 | A template file that cannot be read sends nothing, and the log says why | Point `emailconfirmation` at a missing file. Expect no email, and the log entry. | Accelerated, run G2 |
| AN-040 | A send refused by the transport is retried after 5 minutes, and recorded once when it succeeds | Stub refuses the first two sends. Expect retries at +5 and +10 minutes, success on the third try, and one history entry. | Accelerated, run G3 |
| AN-041 | A send that always fails is given up after 10 tries, and the log says so | Stub refuses every send. Expect ten tries, then no more, and a log entry giving up. | Accelerated, run G3 |
| AN-042 | A text refused by the transport is not recorded as sent, and the reason is logged | Stub refuses one text with a reason. Expect the reason in the log, no history entry, and a retry. | Accelerated, run G3 |
| AN-043 | The SMS key expiry warning appears at start-up within 30 days of expiry, and not before | Set expiry 29 days ahead, start; then 31 days ahead, start. Expect a warning only for the first. | Accelerated, run C |
| AN-044 | A multi-line text template sends only its first line | Text template with two lines. Expect only the first line sent. | Accelerated, run G2 |
| AN-045 | A clock change in March keeps the reminder at 24 elapsed hours, and sends in local time | Reminder across 28 March 2027. Expect the reminder 24 hours before, and the window in local time. | Accelerated, run D |
| AN-046 | A reminder and on-the-day message across New Year are sent on the right dates | Appointment for 1 January with reminder on 31 December. Expect both on the right days. | Accelerated, run D |
| AN-047 | Run A, as a whole: every expected message of the run is sent once, on time, and nothing unexpected is sent | Run A runs with its selected tests. Compare the messages sent with the expected list for those tests. Expect every expected message on time, once, and no extra. | Accelerated, run A |
| AN-048 | Setup's "Enable SMS and Email Reminders" off stops all automatic messages | Untick it. Create an appointment for contact 1 and run three days. Expect no message of any kind. Tick it again, create an appointment, and expect its confirmation at the hold's end. | Accelerated, run G4 |

---

## 3. Operation testing

Operation tests check the program without Google, sync or messages, unless a test says otherwise. **All of the tests in this section are automated.** Each one is run by the operation runner (`autotest/operation/operation_tests.py`), which starts the program headless through the test driver, presses the keys or triggers the menu item, and checks the result. The driver answers each dialog with its default button and writes the dialog's title to its log, so a dialog is checked by its title. A test passes when every expected result in its row is observed. A test that cannot run in the test environment is reported as blocked, with the reason. No test in this section needs a person to look at the screen or listen. The screen-reader checks are in section 4, and are the only manual tests.

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

---

## 4. Accessibility

Accessibility tests are carried out with a screen reader (NVDA or the platform's inspector) and with keyboard only. Each test states what a person should hear or see, and the expected result is recorded by the person running it.

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

---

## 5. Capacity stress test

This is not part of the normal test cycle. It checks how the program and Google behave with a very large data set: 250 contacts and 1,000 appointments. It is run on the scratch account only, when the capacity of the sync is in question. It is not expected in routine runs, and the specification does not expect thousands of records in any other test.

| ID | Title | Procedure and expected result | How run |
|---|---|---|---|
| CS-001 | Purge the account, then upload 250 contacts and 1,000 appointments, half by the Google API and half by ContactManager, and leave them in place | Imported from the archive as `U01` (`autotest/archive/google_bench.py`). The purge removes everything of each kind on the account, so run it only on the scratch account. Expect every contact and appointment on Google once, and the sync to finish within its time. Records are left in place. | Automated (bench), capacity only |

## Appendix A. Tests imported from the archive

These tests were written earlier and exist in `autotest/archive/`. They are kept, with their titles as they were written. Each is given a new ID in its area, with its original code. Those that use the test bench's own test data (the `Bench` contacts and `BENCH ` appointments) will be re-pointed to the six fixtures (0.4) when the benches are rewritten.

Sources: `autotest/archive/google_bench.py` (bench scenarios), `autotest/archive/messages/tst_messages.cpp` (unit tests), `autotest/archive/oldfile/old_contact_test.cpp` (old-file test).

Area assignment is by the test's content: a test that calls the sync, or whose title is about Google or sync, is in area 1; the other tests are placed in the area of the feature they check. The code letters are not reused between tests: two bench scenarios share the code L05 and L06 (the screen-reader tests of #101 and the notes tests of #105). Their new IDs keep them apart.

Counts: Google sync 143, Announcements 85, Start-up and shutdown 7, By menu and function 29, By dialog 12, Accessibility 2. Total 278 (U01 is in section 5, a capacity stress test).

### A.1 Google sync (area 1)

| New ID | Archive code or name | Title, as written in the archive |
|---|---|---|
| GS-101 | `C01` | Add a contact locally, no sync: Google unchanged |
| GS-102 | `C02` | Contact made on Google: downloaded once across two syncs |
| GS-103 | `C03` | Add then delete locally before any sync: nothing reaches Google |
| GS-104 | `C04` | Contact deleted on Google between syncs: removed from the app |
| GS-105 | `C05` | Contact deleted in the app after it synced: deleted on Google |
| GS-106 | `C06` | Contact hidden in the app after it synced: Google agrees with the app |
| GS-107 | `C07` | Same name on both sides before sync: both kept |
| GS-108 | `C08` | Edit a downloaded contact before a second sync: change reaches Google |
| GS-109 | `C09` | Edit on Google between syncs: app picks it up without duplicating |
| GS-110 | `C10` | Out-of-sequence phones and emails: each value kept in some slot |
| GS-111 | `C11` | Stray spaces and newlines in Google data: no spurious upload |
| GS-112 | `C12` | Large upload: 40 local contacts reach Google once |
| GS-113 | `C13` | Unicode and apostrophes in names round-trip exactly |
| GS-114 | `C14` | Email-only contact (no name) is uploaded with its email |
| GS-115 | `C15` | Merged into X18 (above): repeated syncs with no changes do not write to Google |
| GS-116 | `C16` | Edit keeps URLs the app does not hold (option 1 regression) |
| GS-117 | `A01` | Add an appointment locally, no sync: Google unchanged |
| GS-118 | `A02` | Event made on Google: downloaded once across two syncs |
| GS-119 | `A03` | Identical events on Google (duplicates): both kept, no growth on re-sync |
| GS-120 | `A04` | Same event added locally and on Google before sync: both kept |
| GS-121 | `A05` | Edit a downloaded event before a second sync: change reaches Google |
| GS-122 | `A06` | Event moved on Google between syncs: app picks it up |
| GS-123 | `A07` | Event deleted in the app after it synced: deleted on Google |
| GS-124 | `A08` | Event deleted on Google between syncs: app does not keep a live copy |
| GS-125 | `A09` | Overlapping appointments added locally: both uploaded, neither merged |
| GS-126 | `A10` | Large upload: 40 local appointments reach Google once |
| GS-127 | `A11` | Read-only event (organised by someone else): not uploaded across syncs |
| GS-128 | `A12` | Stray spaces and newlines in a location: no spurious upload |
| GS-129 | `A13` | Local ID of a Google event is the same across syncs |
| GS-130 | `M01` | ContactManager master adds a contact; Google master syncs it: no duplicate |
| GS-131 | `M02` | Google master downloads; ContactManager master edits; Google master re-syncs: one copy |
| GS-132 | `M03` | Deleted in ContactManager master: Google master does not bring it back |
| GS-133 | `M04` | Deleted on Google in Google master: ContactManager master does not bring it back |
| GS-134 | `M05` | Appointment: ContactManager master adds, Google master edits, ContactManager master picks it up |
| GS-135 | `M06` | Repeated switching with edits: values and notes survive, no duplicates |
| GS-136 | `M07` | Google master: a local-only edit is not sent, and Google's values stay |
| GS-137 | `M08` | ContactManager master: a change made on Google is kept, not overwritten |
| GS-138 | `X01` | Blank and whitespace-only name fields: no crash, no duplicate |
| GS-139 | `X02` | Very long comment (6000 characters) round-trips intact |
| GS-140 | `X03` | Quotes, backslashes, ampersands, angle brackets, emoji and right-to-left text round-trip |
| GS-141 | `X04` | Phone number formats (international, brackets, extensions) keep their digits |
| GS-142 | `X05` | Same email on two different contacts: both kept, not merged |
| GS-143 | `X06` | Six emails on Google: the app holds four, and the rest go to Comments |
| GS-144 | `X07` | Birthdays: leap day and no-year birthday are not changed by the sync |
| GS-145 | `X08` | Multi-line address round-trips with formatting ignored |
| GS-146 | `X09` | Hidden, then shown again: the contact is back on Google |
| GS-147 | `X10` | Edit then delete between syncs: the stale edit is not uploaded |
| GS-148 | `X11` | All-day event on Google: read without crashing, not rewritten |
| GS-149 | `X12` | Event crossing midnight with a timezone offset keeps the same instant |
| GS-150 | `X13` | Local appointment with no end time: uploaded or refused, but no crash and no duplicate |
| GS-151 | `X14` | Unicode summary and multi-line description round-trip |
| GS-152 | `X15` | Event outside the sync window is not duplicated across syncs |
| GS-153 | `X16` | Appointment deleted on both sides before sync: no error, nothing left |
| GS-154 | `X17` | Conflict: location changed in the app and on Google; ContactManager master, so the app's change wins (item 4) |
| GS-155 | `X18` | Two back-to-back syncs with no change: no writes after the first (shortened from five; merges C15, see its entry) |
| GS-156 | `X19` | Twenty contacts made in ContactManager master, then edits on Google under Google master |
| GS-157 | `LB01` | Four emails from Google (home, other, work, Partner): each keeps its slot and label, no write |
| GS-158 | `LB02` | A label changed in the app is sent to Google as that email's type |
| GS-159 | `LB03` | Emails with no labels set are sent with the labels they always had |
| GS-160 | `X20` | An address with no type is held in the first address slot |
| GS-161 | `P01` | Four phones from Google (home, work, mobile, Pager): each keeps its slot and label, no write |
| GS-162 | `P02` | A phone label changed in the app is sent to Google as that number's type |
| GS-163 | `P03` | Phones with no labels set are sent with the labels they always had |
| GS-164 | `P04` | Seven phones on Google: the app holds six, the seventh goes to Comments |
| GS-165 | `D01` | Clearing an email and a phone in the app removes them from Google |
| GS-166 | `ED01` | Editor: a label typed after an address becomes its title, and Google gets it as that type |
| GS-167 | `F01` | More emails on Google than the app has slots: all of them are still on Google after an upload |
| GS-168 | `F02` | Google master: a bad entry downloaded from Google stops a save of the form, and nothing is uploaded |
| GS-169 | `F03` | More phones on Google than the app has slots: all of them are still on Google after an upload |
| GS-170 | `F04` | A save refused for a bad entry stops the next sync, so nothing is sent to Google |
| GS-171 | `F06` | #66: with a contact holding an unchanged bad entry open, a Google sync still runs |
| GS-172 | `F07` | #66: a contact kept with an unfixable entry is uploaded once, then not again on later syncs |
| GS-173 | `F08` | A contact with a clean address and no titles set: uploaded once, then not again on later syncs |
| GS-174 | `F10` | #69: the report's steps: a bad entry from Google, Alt-E,D, Ctrl-S, Exit, relaunch, Tidy Contacts, Alt-E,D, Exit |
| GS-175 | `F13` | #68: switching contact, Tidy Contacts and sync ask Save or Discard when the contact has edits |
| GS-176 | `T01` | Tidy Contacts puts right an older address label and a phone label; they go to Google; a second tidy changes nothing |
| GS-177 | `G40` | #30: a repeating Google event is stored once, with its repeat, and listed on each week |
| GS-178 | `X81D` | #81: deleting just one occurrence of a repeat keeps the others, and Google leaves that date out |
| GS-179 | `X81E` | #81: changing one occurrence and all following splits the repeat, and Google gets both parts |
| GS-180 | `X81C` | #81: cancelling the change of a repeat changes nothing |
| GS-181 | `X81G` | #81: an occurrence deleted on Google is left out of the repeat in the app, when Google is the master |
| GS-182 | `X81H` | #81: a split that moves the time keeps Google's deleted occurrence on the right time, in London time |
| GS-183 | `G41` | #30: an edit of a repeat in the app changes the Google event, and the event stays repeating |
| GS-184 | `G42` | #30: a repeating Google event whose first occurrence is older than the window still lists its repeats |
| GS-185 | `G43` | #30: a Google repeat with a count is stored once and listed that many times, and an app edit keeps the count |
| GS-186 | `G44` | #30: an appointment on Google older than the one-year lookback is not downloaded |
| GS-187 | `E01` | #86: an ETag is stored from Google and sent with a change: a local edit reaches Google, with no conflict |
| GS-188 | `E02` | #86: Google's copy changed in its details since the last sync (non-matching ETag) and the app changed it too, ContactManager master: the app's version is sent, and the conflict is reported (item 4) |
| GS-189 | `E03` | #86: Google's ETag changed, but not its details (a field the app does not hold): the change is sent again with the new ETag, and reaches Google |
| GS-190 | `C103` | #103: the contact on the form stays loaded through a sync, in ContactManager master and in Google master, and the calendar entry does not move |
| GS-191 | `N107` | #107: an appointment with no end time, or one ending on 1 Jan 1970, is not uploaded; the sync completes and the others reach Google |
| GS-192 | `N108` | #108: with the last calendar sync back at the epoch (as after a failed sync), the next sync sends only the appointment that was edited, not the ones it downloaded |
| GS-193 | `R01` | #105 ContactManager master, edit in Google, repeat: the Google change reaches the app, one repeat on Google |
| GS-194 | `R02` | #105 Google master, edit in ContactManager, single: the next sync takes Google's location, and nothing is uploaded |
| GS-195 | `R03` | #105 Google master, edit in ContactManager, repeat: the next sync keeps the repeat and Google's location |
| GS-196 | `R04` | #105 Google master, edit in Google, single: the Google change is downloaded |
| GS-197 | `R05` | #105 Google master, edit in Google, repeat: the Google change is downloaded, and the repeat is kept |
| GS-198 | `AR01` | #105 Archive: an entry that the window leaves behind is archived; moved into the window on Google, it is restored and leaves the archive |
| GS-199 | `U02` | #105 Uploads: 40 appointments added in the app inside the window are all on Google after one sync (the 66 not uploaded) |
| GS-201 | `W01` | #105 Window: an entry that starts before the window and ends inside it is downloaded, once |
| GS-202 | `W02` | #105 Window: an entry that ends before the window is not downloaded, and stays on Google once |
| GS-203 | `W03` | #105 Window: an entry beyond the far edge of the window is not downloaded, and stays on Google once |
| GS-204 | `W04` | #105 Window: widening the window downloads the entries it now reaches, once |
| GS-205 | `TK01` | #105 Token: a sync token Google refuses is replaced by a full listing, and the sync completes |
| GS-206 | `TK03` | #105 Token: a sync token Google answers with 410 (gone), as with 400, is replaced by a full listing, and the sync completes |
| GS-207 | `TK04` | #105 Listing: after a full sync, the next sync lists only the changes since the last sync, and downloads no unchanged appointment |
| GS-208 | `TK02` | #105 Token: with no sync token, the listing is full and every entry in the window is downloaded |
| GS-209 | `R06` | #105 ContactManager master, repeat changed in both places since the last sync: the app's version wins, and the report says so (item 4) |
| GS-210 | `R07` | #105 ContactManager master, repeat moved on Google: the app shows the moved start, and one repeat |
| GS-211 | `R08` | #105 Repeat with no occurrence in the window: not downloaded, and it stays on Google once |
| GS-212 | `R09` | #105 Repeat with an exception: the excluded occurrence is not listed, and the other five are |
| GS-213 | `TC01` | #105 Two copies of ContactManager on one calendar: each sees the other's change, and nothing is duplicated |
| GS-214 | `RS01` | #105 A sync that stops part-way resumes without repeating or losing its uploads (#93) |
| GS-215 | `AR02` | #105 Archive: an archived entry changed on Google, still outside the window, has its archived copy brought up to date |
| GS-216 | `AR03` | #105 Archive: an archived entry cancelled on Google leaves the archive, and is kept in the deleted folder |
| GS-217 | `E70` | #104 A local entry dated 1 January 1970, or with no start, is neither stored nor uploaded; a normal entry alongside is unaffected |
| GS-218 | `RL01` | #94 A rate-limit answer (403 rateLimitExceeded) is retried with backoff, and the upload then completes |
| GS-219 | `RL02` | #94 A failure that is not a rate limit (400 badRequest) is not retried, and its reason is reported |
| GS-220 | `RL03` | #94 A rate limit that keeps coming back is given up after four retries, and the report gives the reason and a recovery step |
| GS-221 | `RL04` | #94 Repeated rate limits start the adaptive pacing, and the report says so |
| GS-222 | `RL05` | #94 A connection closed by Google part-way: the report says so and what to do, and a second sync finishes the uploads |
| GS-223 | `C17` | #105 Contact changed in both places since the last sync, ContactManager master: the app's change is kept, once |
| GS-224 | `C18` | #105 Contact changed in both places since the last sync, Google master: Google's change is kept, once |
| GS-225 | `C19` | #105 Contact deleted on Google and changed in the app, ContactManager master: created on Google again with the app's change (item 4) |
| GS-226 | `C20` | #105 Contact deleted in the app and changed on Google, ContactManager master: the app's delete wins, so Google deletes it too (item 4) |
| GS-227 | `LF01` | #105 Local files: after two syncs the calendar folder holds each appointment once, and no file is empty |
| GS-228 | `LF02` | #105 Local files: a deleted appointment moves to the deleted folder, and is not left in the calendar folder |
| GS-229 | `A14` | #105 Appointment deleted in the app and changed on Google, ContactManager master: the app's delete wins, so Google deletes it too (item 4) |
| GS-230 | `A15` | #105 Appointment deleted on Google and changed in the app, ContactManager master: created on Google again with the app's change (item 4) |
| GS-231 | `R11` | #105 Repeat with no occurrence left: every occurrence cancelled on Google, the app removes the repeat, and Google holds none |
| GS-232 | `R10` | #102 A changed occurrence of a repeat, changed on Google, is downloaded as its own entry; the other occurrences are unchanged |
| GS-233 | `sentStateDoesNotCountAsAnEdit` | Sent State Does Not Count As An Edit |
| GS-234 | `googleRecordCopyDoesNotRestartTheWait` | Google Record Copy Does Not Restart The Wait |
| GS-235 | `contactLinkIsKeptWhenTheContactExists` | Contact Link Is Kept When The Contact Exists |
| GS-236 | `contactLinkIsSetByAnExactNameMatch` | Contact Link Is Set By An Exact Name Match |
| GS-237 | `contactLinkIsRemovedWhenTheNameMatchesMoreThanOne` | Contact Link Is Removed When The Name Matches More Than One |
| GS-238 | `contactLinkIsRemovedWhenNoNameMatches` | Contact Link Is Removed When No Name Matches |
| GS-239 | `ratePacerIsOffWithoutRateLimits` | Rate Pacer Is Off Without Rate Limits |
| GS-240 | `ratePacerStartsAtFourSecondsAfterFiveLimitedOfAHundred` | Rate Pacer Starts At Four Seconds After Five Limited Of AHundred |
| GS-241 | `ratePacerHalvesEvery500DownToHalfASecond` | Rate Pacer Halves Every500 Down To Half ASecond |
| GS-242 | `ratePacerCountsOnlyTheLastHundred` | Rate Pacer Counts Only The Last Hundred |
| GS-243 | `loadKeepsTheSavedUpdatedDate` | Load Keeps The Saved Updated Date |
| GS-244 | `mergedSentStateDoesNotCountAsAnEdit` | Merged Sent State Does Not Count As An Edit |

### A.2 Announcements (area 2)

| New ID | Archive code or name | Title, as written in the archive |
|---|---|---|
| AN-101 | `L01` | Local: the message and appointment unit tests (autotest/archive/messages, QtTest), with no Google |
| AN-102 | `reminderDueWithinTheReminderHours` | Reminder Due Within The Reminder Hours |
| AN-103 | `reminderDueAtTheStartOfTheDayBefore` | Reminder Due At The Start Of The Day Before |
| AN-104 | `reminderDueForAnAppointmentThatHasStarted` | Reminder Due For An Appointment That Has Started |
| AN-105 | `reminderFollowsRemindHours` | Reminder Follows Remind Hours |
| AN-106 | `confirmOnlyJustInside36Hours` | Confirm Only Just Inside36 Hours |
| AN-107 | `confirmOnlyExactly36HoursIsNot` | Confirm Only Exactly36 Hours Is Not |
| AN-108 | `confirmOnlyForShortNotice` | Confirm Only For Short Notice |
| AN-109 | `confirmOnlyAndReminderMeetAtTheBoundary` | Confirm Only And Reminder Meet At The Boundary |
| AN-110 | `confirmOnlyDefaultIs1Point5TimesRemind` | Confirm Only Default Is1 Point5 Times Remind |
| AN-111 | `confirmOnlyUsesAValidValue` | Confirm Only Uses AValid Value |
| AN-112 | `confirmOnlyLessThanRemindUsesTheDefault` | Confirm Only Less Than Remind Uses The Default |
| AN-113 | `confirmOnlyNotANumberUsesTheDefault` | Confirm Only Not ANumber Uses The Default |
| AN-114 | `remindHoursDefaultsAndLimits` | Remind Hours Defaults And Limits |
| AN-115 | `holdLastsWaitMinutes` | Hold Lasts Wait Minutes |
| AN-116 | `sendWindowIsStartInclusiveEndExclusive` | Send Window Is Start Inclusive End Exclusive |
| AN-117 | `onTheDayUsesLocalDate` | On The Day Uses Local Date |
| AN-118 | `dayRecordKeepsTheChannelsOfTheDay` | Day Record Keeps The Channels Of The Day |
| AN-119 | `newAppointmentByEmailGetsAConfirmation` | New Appointment By Email Gets AConfirmation |
| AN-120 | `newAppointmentByTextGetsAConfirmation` | New Appointment By Text Gets AConfirmation |
| AN-121 | `newAppointmentWithBothGetsEmailOnly` | New Appointment With Both Gets Email Only |
| AN-122 | `newAppointmentWithNoChannelGetsNothing` | New Appointment With No Channel Gets Nothing |
| AN-123 | `confirmOnlyFoldsTheReminderIntoTheConfirmation` | Confirm Only Folds The Reminder Into The Confirmation |
| AN-124 | `changedAppointmentGetsAnUpdateOnItsChannel` | Changed Appointment Gets An Update On Its Channel |
| AN-125 | `unchangedSentAppointmentGetsNothing` | Unchanged Sent Appointment Gets Nothing |
| AN-126 | `deletedSentAppointmentGetsACancellationOnItsChannel` | Deleted Sent Appointment Gets ACancellation On Its Channel |
| AN-127 | `deletedUnsentAppointmentGetsNothing` | Deleted Unsent Appointment Gets Nothing |
| AN-128 | `reminderOnceByTheChannelOfTheContact` | Reminder Once By The Channel Of The Contact |
| AN-129 | `reminderIsNotSentTwice` | Reminder Is Not Sent Twice |
| AN-130 | `reminderNotDueIsNotSent` | Reminder Not Due Is Not Sent |
| AN-131 | `removedOccurrenceIsCancelledOnItsChannel` | Removed Occurrence Is Cancelled On Its Channel |
| AN-132 | `onTheDayBothChannelsAreSentOnTheDay` | On The Day Both Channels Are Sent On The Day |
| AN-133 | `onTheDayTextIsSentWhenEmailHasGone` | On The Day Text Is Sent When Email Has Gone |
| AN-134 | `onTheDayEmailIsSentWhenTextHasGone` | On The Day Email Is Sent When Text Has Gone |
| AN-135 | `onTheDayIsNotSentAgainOnceBothHaveGone` | On The Day Is Not Sent Again Once Both Have Gone |
| AN-136 | `onTheDayIsSentForASingleChannel` | On The Day Is Sent For ASingle Channel |
| AN-137 | `onTheDayIsNotSentOnAnotherDay` | On The Day Is Not Sent On Another Day |
| AN-138 | `sameDayBookingGivesItsEmailAndThenTheOnTheDayText` | Same Day Booking Gives Its Email And Then The On The Day Text |
| AN-139 | `sameDayTextBookingCoversTheDaysText` | Same Day Text Booking Covers The Days Text |
| AN-140 | `sameDaySmsIsSentWhenBookedEarlierAndTheDayComes` | Same Day Sms Is Sent When Booked Earlier And The Day Comes |
| AN-141 | `caseSevenEmailIsTheConfirmationWhenWindowOpensLate` | Case Seven Email Is The Confirmation When Window Opens Late |
| AN-142 | `caseSevenTextIsTheOnTheDayTextWhenWindowOpensLate` | Case Seven Text Is The On The Day Text When Window Opens Late |
| AN-143 | `caseSevenBothIsEmailThenTheOnTheDayText` | Case Seven Both Is Email Then The On The Day Text |
| AN-144 | `caseEightNoReminderOnTheDay` | Case Eight No Reminder On The Day |
| AN-145 | `reminderOnTheDayIsReplacedByTheOnTheDayText` | Reminder On The Day Is Replaced By The On The Day Text |
| AN-146 | `channelSwitchedToTextSendsOnlyWhatIsPending` | Channel Switched To Text Sends Only What Is Pending |
| AN-147 | `scenarioCreatedThirtySixHoursAheadBothChannels` | Scenario Created Thirty Six Hours Ahead Both Channels |
| AN-148 | `scenarioCreatedTwentyFiveHoursAheadBothChannels` | Scenario Created Twenty Five Hours Ahead Both Channels |
| AN-149 | `scenarioBookedOnTheDayBothChannels` | Scenario Booked On The Day Both Channels |
| AN-150 | `singleAppointmentEndedBeforeTheEdgeIsExpired` | Single Appointment Ended Before The Edge Is Expired |
| AN-151 | `repeatTrimKeepsOnlyTheExceptionsStillToCome` | Repeat Trim Keeps Only The Exceptions Still To Come |
| AN-152 | `trimKeepsTheMessageSignatureSoTheContactIsNotMessaged` | Trim Keeps The Message Signature So The Contact Is Not Messaged |
| AN-153 | `limitsStopMessagesOnceReached` | Limits Stop Messages Once Reached |
| AN-154 | `limitsAreCheckedHourFirst` | Limits Are Checked Hour First |
| AN-155 | `failedMessageWaitsBeforeItIsTriedAgain` | Failed Message Waits Before It Is Tried Again |
| AN-156 | `failedMessageIsGivenUpAfterMaxRetries` | Failed Message Is Given Up After Max Retries |
| AN-157 | `messageWithNoFailureMayBeTried` | Message With No Failure May Be Tried |
| AN-158 | `channelNeedsItsOwnAddress` | Channel Needs Its Own Address |
| AN-159 | `missingEmailAddressSendsNothingByEmailButTextStillGoes` | Missing Email Address Sends Nothing By Email But Text Still Goes |
| AN-160 | `noUsableChannelSendsNothing` | No Usable Channel Sends Nothing |
| AN-161 | `onTheDayUsesTheReminderTemplateWhenNotSet` | On The Day Uses The Reminder Template When Not Set |
| AN-162 | `reminderIs24ElapsedHoursAcrossTheClockChange` | Reminder Is24 Elapsed Hours Across The Clock Change |
| AN-163 | `onTheDayIsTheLocalDateAcrossTheClockChange` | On The Day Is The Local Date Across The Clock Change |
| AN-164 | `onTheDayAndReminderAcrossTheYearBoundary` | On The Day And Reminder Across The Year Boundary |
| AN-165 | `onTheDayAndReminderAcrossTheLeapDay` | On The Day And Reminder Across The Leap Day |
| AN-166 | `backlogOfChangesGivesOneUpdate` | Backlog Of Changes Gives One Update |
| AN-167 | `deletedAndRecreatedInsideTheHoldSendsOneConfirmation` | Deleted And Recreated Inside The Hold Sends One Confirmation |
| AN-168 | `secondOccurrenceOfARepeatIsStillReminded` | Second Occurrence Of ARepeat Is Still Reminded |
| AN-169 | `signatureChangesOnlyForWhatMessagesShow` | Signature Changes Only For What Messages Show |
| AN-170 | `editWithinConfirmOnlyReplacesTheReminder` | Edit Within Confirm Only Replaces The Reminder |
| AN-171 | `secondCopyCannotTakeTheDataLock` | Second Copy Cannot Take The Data Lock |
| AN-172 | `smsKeyIsReportedInTheLastThirtyDays` | Sms Key Is Reported In The Last Thirty Days |
| AN-173 | `onlyASentMessageIsRecorded` | Only ASent Message Is Recorded |
| AN-174 | `blankEmailSenderSendsNothingAndSaysWhy` | Blank Email Sender Sends Nothing And Says Why |
| AN-175 | `blankSmsSenderSendsNothingAndSaysWhy` | Blank Sms Sender Sends Nothing And Says Why |
| AN-176 | `whitespaceOnlySenderCountsAsBlank` | Whitespace Only Sender Counts As Blank |
| AN-177 | `emailIsSentWithTheIniSender` | Email Is Sent With The Ini Sender |
| AN-178 | `smsIsSentWithTheIniSenderAndTheText` | Sms Is Sent With The Ini Sender And The Text |
| AN-179 | `transportFailureIsReturnedWithItsReason` | Transport Failure Is Returned With Its Reason |
| AN-180 | `maybeSentIsReturnedAsMaybeSent` | Maybe Sent Is Returned As Maybe Sent |
| AN-181 | `templateFirstLineIsTheSubjectAndTheRestIsTheBody` | Template First Line Is The Subject And The Rest Is The Body |
| AN-182 | `smsTemplateIsOneLineOfText` | Sms Template Is One Line Of Text |
| AN-183 | `templateWithNoSubjectIsRejected` | Template With No Subject Is Rejected |
| AN-184 | `shippedTemplatesHaveNoSenderLine` | Shipped Templates Have No Sender Line |
| AN-185 | `missingSenderWarningsNameEachBlankChannel` | Missing Sender Warnings Name Each Blank Channel |

### A.3 Start-up and shutdown (3a)

| New ID | Archive code or name | Title, as written in the archive |
|---|---|---|
| SS-101 | `K24` | #24: when the app opens, overlapping appointments are listed, and nothing is changed |
| SS-102 | `L02` | Local: an appointment and a contact made in ContactManager are saved, and are there after a restart (no Google) |
| SS-103 | `L03` | Local: an appointment file moved into the archive is not loaded at start-up, and start-up leaves it in the archive |
| SS-104 | `L04` | Local: a start with no to-do or history file opens and writes its data (#100) |
| SS-105 | `L05` | #105 Notes, plain text: a contact's to-do list and history are saved as plain files, and a restart reads them back |
| SS-106 | `L06` | #105 Notes, encrypted: a contact's to-do list and history are saved encrypted, with no plain text on disk, and a restart reads them back |
| SS-107 | `old_contact_test` | Loads a contact file written before the email and phone labels existed, and checks that it keeps its meaning. |

### A.4 By menu and function (3b)

| New ID | Archive code or name | Title, as written in the archive |
|---|---|---|
| MF-101 | `MG01` | Tidy Contacts merges two entries for one person when they share an address and do not conflict |
| MF-102 | `MG02` | Tidy Contacts does not merge two entries that share an address but have different names |
| MF-103 | `T02` | Tidy Contacts lists an entry it cannot fix, leaves it alone, and still fixes the other entry on the contact |
| MF-104 | `T03` | Tidy Contacts lists a title with a colon, and leaves it and its number as they are |
| MF-105 | `U34` | #34: double-clicking a calendar entry opens its contact, and keeps the calendar row and its focus |
| MF-106 | `P30` | #30: a weekly appointment is listed on each week in the repeats window, and stored once |
| MF-107 | `P31` | #30: an edit of a repeat changes its appointment; a move to another day moves the appointment and the focus with it |
| MF-108 | `T40` | #75: a weekly repeat keeps its 14:00 London time across the clocks going back |
| MF-109 | `B78` | #78: a birthday saved on a contact shows in the calendar straight away, with birthdays shown |
| MF-110 | `O79` | #79: a repeat that started before today, with later dates, is in the overview's future appointments |
| MF-111 | `W30` | #30: a repeat whose source is older than the window is listed, and the source is not |
| MF-112 | `T30` | #30: Goto Today selects the first entry that has not ended, including a repeat, rather than the end of the list |
| MF-113 | `P33` | #73: shortening a repeat from a later entry keeps the focus on the nearest entry of that repeat |
| MF-114 | `removedOccurrenceOnlyForARepeat` | Removed Occurrence Only For ARepeat |
| MF-115 | `listedOccurrencesOfARepeat` | Listed Occurrences Of ARepeat |
| MF-116 | `removedOccurrenceIsNotListed` | Removed Occurrence Is Not Listed |
| MF-117 | `repeatWithEveryOccurrenceRemovedListsNothing` | Repeat With Every Occurrence Removed Lists Nothing |
| MF-118 | `periodsTouchingAtAnEndDoNotClash` | Periods Touching At An End Do Not Clash |
| MF-119 | `clashIgnoresARemovedOccurrence` | Clash Ignores ARemoved Occurrence |
| MF-120 | `clashWithAListedOccurrence` | Clash With AListed Occurrence |
| MF-121 | `repeatWithNoOccurrenceLeftIsFound` | Repeat With No Occurrence Left Is Found |
| MF-122 | `clashIsNotReportedForAnEmptyRepeat` | Clash Is Not Reported For An Empty Repeat |
| MF-123 | `getDateGivesACopyThatStaysTheSame` | Get Date Gives ACopy That Stays The Same |
| MF-124 | `repeatStartMovesToTheFirstOccurrenceInTheWindow` | Repeat Start Moves To The First Occurrence In The Window |
| MF-125 | `repeatWithNoOccurrenceLeftIsExpired` | Repeat With No Occurrence Left Is Expired |
| MF-126 | `focusGoesToTheNearestDate` | Focus Goes To The Nearest Date |
| MF-127 | `twoDigitYearsUseTheCenturyOfThisYear` | Two Digit Years Use The Century Of This Year |
| MF-128 | `numericDatesAreParsed` | Numeric Dates Are Parsed |
| MF-129 | `typedDatesAreParsed` | Typed Dates Are Parsed |

### A.5 By dialog (3c)

| New ID | Archive code or name | Title, as written in the archive |
|---|---|---|
| DL-101 | `ED02` | Editor: a save with a bad entry is refused, focus goes to the first, nothing is saved, and the typed text is kept |
| DL-102 | `E06` | Editor: an older bad entry that was never edited still stops a save, and is shown with focus |
| DL-103 | `F05` | #66: an unchanged bad entry does not stop switching to another contact |
| DL-104 | `F09` | #66: moving the selection on, as Tidy Contacts does, does not check the open contact's entries |
| DL-105 | `F12` | #68: quitting with edits asks Save, Discard or Cancel: Save refuses a bad entry, Discard keeps the stored contact |
| DL-106 | `ED03` | Editor: an older entry with its label in the address is tidied when opened, and saved with the next change |
| DL-107 | `E04` | Editor: a phone typed with (0) after the country code and a label after the number is saved tidily |
| DL-108 | `E05` | Editor: clearing the only detail of a contact saves the change |
| DL-109 | `D30` | #30: the appointment form keeps a repeat's type and interval, and gives back none for an appointment that does not repeat |
| DL-110 | `E30` | #30: a repeat ends after a number of times, or on a date, in the list and in the form |
| DL-111 | `Q34` | #74: OK on the edit of a repeat that started 11 days ago closes the form, with no dialog |
| DL-112 | `Q35` | #74: a start three months back is accepted, and OK then closes the form, as with a start 11 days back |

### A.6 Accessibility (area 4)

| New ID | Archive code or name | Title, as written in the archive |
|---|---|---|
| AC-101 | `L05` | #101: Show Screen Reader Text: shows the name, role, value and description of the focused control, and says it is an approximation |
| AC-102 | `L06` | #101: the accessibility debug checkbox in the screen reader text dialog turns console tracing on and off |

---

## Appendix B. Traceability: specification section to tests

| Specification section | Tests |
|---|---|
| 1 Overview (keyboard operation, screen reader) | AC-001 to AC-018 |
| 2.1 Files and folders | SS-001, SS-008, SS-011, SS-013, GS-012 |
| 2.2 The ini file | SS-012, AN-036, AN-037, AN-038, AN-039, AN-045 |
| 2.3 Settings (Setup) | DL-001 to DL-007 |
| 2.4 Messages and templates | AN-036 to AN-039, AN-044 |
| 2.5 Encryption and passwords | SS-002, SS-003, SS-014, MF-007, DL-025, DL-026, GS-012 |
| 2.6 Linking services | DL-007, GS-031, GS-041, AN-043 |
| 3.1 Opening and creating contacts | MF-001, MF-022, MF-035 |
| 3.2 Editing a contact | DL-014 to DL-018, GS-001 to GS-014 |
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

## Appendix C. Requirements that have no test yet

| No. | Item | What is needed |
|---|---|---|
| 1 | Spelling of the test domain `sgc.contacmanagertest` | Confirm before the first live run (0.4). |
| 2 | The send-record write failure (issue #110, item 3) | A test that the message is retried and not sent twice (Appendix A and specification 5.10). |
| 3 | The stub transports and the accelerated harness are in a scratch folder, not the repository | Bring them into `autotest/` before area 2 is run again. |
| 4 | The Test Function menu item | Removed in ticket #119. Once that is done, check that the File menu has no Test Function item. |
| 5 | The download window read from the ini file | Tests GS-023 and GS-038, and the window scenarios, after ticket #118. |
| 6 | Google sync for a token refused on a later page of a listing (issue #110, item 19) | A test in area 1. |
| 7 | Read-back of archive and deleted-folder writes (issue #110, item 18) | Extend AR01 to AR03 (Appendix A) to read the files back. |
| 8 | Google-made appointments get no confirmation (issue #110, item 15) | Decide the rule first, then add a test in area 2. |
| 9 | Re-pointing imported bench tests to the six fixtures | Done when the benches are rewritten (planned, issue #105). |
| 10 | Area 4 tests need a person and a screen reader | Schedule with the tester; the expected results are recorded by hand. |
