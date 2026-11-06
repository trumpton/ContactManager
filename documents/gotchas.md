# Gotchas

Things that were found while testing ContactManager, and that will catch the next person who runs the tests or changes the sync. Each entry says what was seen, why it matters, and where it stands. The status is one of:

- **Fixed**: the change is in the code, with its commit.
- **Open**: nothing has changed yet. The ticket is named where there is one.
- **Process**: a rule for running the tests, which stays in force.
- **Accepted**: known, and left as it is, with the reason.

How to run the tests is in `headless-test.md`. The test plan is in `test-specification.md`.

---

## 1. Google contacts

### 1.1 An upload can remove data that the app does not hold

**Seen.** A contact with a URL, a nickname and a biography. After a name edit and a sync, the name was updated and the nickname and biography were kept, but the URL was removed.

**Why.** On upload, the app asks Google to overwrite each person field group in `WRITEPERSONFIELDS` (`googleaccess.h`). Within a group, Google keeps only what is sent. The app holds some data in each group, so an entry it does not hold is removed from Google.

**Status: Fixed (commit 83b3e47), with the open items below.** The upload now sends only the groups whose value differs from Google's (`changedPersonFields`, `googleupdatedialog_contacts.cpp`). `userDefined` is always sent, because it holds the app's ID and upload marker. Groups that are not sent keep their Google value.

The alternative, not chosen: fetch the Google contact, copy the app's fields into it, and send the whole result. That needs one more request per upload. Switch to it if a group the app holds only in part (addresses, phone numbers, more than four emails) loses data.

### 1.2 What the fix still does to a changed group

- **A changed group is replaced whole.** Clearing the website sends an empty `urls` group, which removes every URL on Google, not only the one the app holds. A contact with two URLs lost both when its website was cleared.
- **An unchanged group is left alone.** Renaming a contact with two URLs kept both. This is the case the fix is for (bench scenario C16).
- **Emails beyond four** are kept in Comments when Google is downloaded. They are not sent back as addresses, so an upload that changes the email group drops them from Google.
- **Phone numbers (six), addresses (two) and organisations (one)** are held in fixed slots. Values with no slot are kept in Comments, and an upload that changes that group drops them from Google.

### 1.3 Which contacts are uploaded

- **Local master (the default).** Only the contacts changed in ContactManager since the last sync are uploaded, plus those that are merged or deleted. Everything else is downloaded, so a change made on Google is kept. Where both sides changed, the local copy is sent. (Commit e24d1e0. Before that commit, every local contact was marked for upload each sync.)
- **Google master.** Nothing is uploaded unless it is marked (merged, or deleted locally).
- A contact that is not uploaded is left untouched on Google.

### 1.4 A contact made on Google has an ID that is not stored on Google

A contact downloaded without an app ID gets one derived from its Google resource name (`parseContact`, `googleaccess_parsecontact.cpp`, issue #49). The first sync therefore does not upload it. An upload would rewrite fields the app does not hold, such as URLs.

**Status: Implemented** (issue #49, branch `fix/49-contact-upload-on-download`). The issue's state on GitHub is not checked here.

### 1.5 Open: clearing a website did not upload

**Seen.** In a test, the website was cleared in ContactManager, but the next sync neither uploaded the contact nor reported it. Google kept its URLs, so nothing was lost, but the clear did not reach Google.

**Status: Open.** The cause is not known, and it is not known whether it predates the fix in 1.1. No bench scenario clears a website: C16 only covers a rename. Add one before closing this.

### 1.6 Open: a URL already lost is not restored

A URL removed by the earlier code (before 1.1) is not restored by the fix. **Status: Open, accepted.**

### 1.7 Open: contacts downloaded by the earlier code

The earlier code uploaded such contacts on their first sync, so they should already hold the app ID. That is reasoned, not tested. **Status: Open, untested.**

---

## 2. Google calendar and sync

### 2.1 Links must be saved as they are made

**Seen.** In one run, 137 contacts reached Google, but their links were not saved locally, so the next sync would have made copies. The links were saved again by name and email before the download.

**Why.** An interrupted sync that has not saved its links leaves Google records with no local link, and the next sync treats them as new.

**Status: Fixed** (commit e928e02). A Google ID is saved as soon as Google replies, even if the reply does not match what was sent, so a retry does not make a second copy.

### 2.2 Uploads resume from a progress file, keyed to the last sync time

The upload progress is kept in `sync-<phase>.progress` in the data folder. The file records the last-sync time it was written for. If that time has changed, the file is ignored and uploads start again. It is deleted when its phase completes (`loadUploadProgress`, `googleupdatedialog.cpp`).

**Status: Fixed** (commit e928e02, issue #93). Resuming an interrupted sync works only while the last-sync time is unchanged.

### 2.3 Rate limits: retry only what Google says will clear

Retry on `rateLimitExceeded`, `userRateLimitExceeded` and HTTP 429 only, and on 5xx server errors. Other failures are reported at once. A retry waits 0.5 s, doubling up to 4 s, with a random addition, and gives up after four retries. When 5 of the last 100 transactions needed a retry, the pacing starts at 4 s and halves every 500 transactions, down to 0.5 s.

**Seen.** A burst gave 46 HTTP 403 responses mixed with successes, and the body was not logged, so the cause was not known.

**Status: Fixed** (commit 8a40308, issue #94). The reason and the recovery step are reported. **Process:** when a test shows a 403, read the reason in its log before deciding it is a rate limit.

### 2.4 A closed connection stops every later request

**Seen.** Two runs ended with "Connection closed" or GOAWAY. After that, every request failed in about 1 ms, and the process ended with no message.

**Status: Fixed** (issue #94). The report now says "Connection closed by Google" with its recovery step (sync again, which resumes from the progress file, 2.2).

### 2.5 Server errors are retried like rate limits

**Seen.** A change the bench made on Google got HTTP 502 and was not retried. The sync then had nothing to download, and bench scenario X19 failed two runs in three.

**Status: Fixed** (commit afaba97). 5xx answers are retried in the same way as 429.

### 2.6 An appointment with no contact is "Unnamed Contact" on Google

**Seen.** Google sends back "Unnamed Contact" in the For field of an appointment with no contact, and writes `repeatzone` as well. An upload with an empty For field then failed to match its reply.

**Status: Fixed** (commit d0127e0, issue #95). The summary is sent without empty braces. **Open:** four older test appointments still mismatch on For, and are listed in the report for issue #94.

### 2.7 A date in 1970 is no date

**Seen.** An entry with no real start or end was given the date 1 January 1970, and was uploaded. An earlier bench scenario (E70) failed on it.

**Why.** An appointment with no start or end must never be stored, downloaded over, or uploaded (issue #104).

**Status: Fixed** (commits 02428c6, a84ab81, c9b083a). A time on 1 January 1970 counts as no start or end. Bench scenarios N107 and E70 check it.

### 2.8 Listing: a full listing first, then only changes

The first sync of a run lists every event in the window. Later syncs list only the changes, while the sync token is kept. A token that Google refuses (400 or 410) causes a full listing (commit 48acc91, issue #96).

**Why it matters.** An incremental listing leaves out an entry that Google has not changed. A sync that took that as "missing on Google" never sent a deletion (scenario A07). The fix is the `DeletePending` record, which keeps a deletion until Google confirms it (commit 2e9e452).

**Status: Fixed.**

### 2.9 The window's far edge depends on the time of day

The calendar window runs from now less the "before" days, plus one hour, to now plus the "after" days, less one hour (`googleupdatedialog_appointments.cpp`). Whether an entry on the far day is inside depends on the time of day when the sync runs.

**Seen.** The expected repeat counts in G40 and P30 changed with the time of day, so those tests failed at some times.

**Why it matters.** A test that expects a count, or an entry on the far day, must compute the edge from the time of the run. The bench's older model (`in_scale_window`) used "one day inside" as a rough approximation; `repeat_weeks_in_window` uses the exact edge.

**Status: Fixed** (commits c9b083a, 750fcce, 3875e6e).

### 2.10 Repeats are downloaded with their first occurrence

A repeat is downloaded with its first occurrence. A repeat whose first occurrence is older than the window is still listed on its dates inside the window (G42).

**Status: Working as designed.**

### 2.11 Time zones: run London-time tests in London time

The London daylight-saving scenarios (T40, and the repeat scenarios that cross the clocks) must run with `TZ=Europe/London`. The driver sets it for each scenario that asks for it (commit e583c67). The message unit tests need it too, and `run-messages-tests.sh` sets it.

**Why it matters.** On a machine set to UTC, a London-time expectation is out by an hour.

**Status: Fixed** for the bench and the unit tests.

---

## 3. Reading the accelerated-clock results

### 3.1 The clock and its checks

The fake clock (`accel/clock/fakeclock.c` in the scratch folder) sets its start from `FAKE_START` when the process starts, and runs at `FAKE_SPEED` times real time. Monotonic timers are left at real speed. So the application's one-second check runs once per real second, which is once per fake minute at 60 times, and once per fake six minutes at 360 times.

**Why it matters.** A message can therefore be up to one check late. A test that expects a message to the minute must allow one check (accepted: the tolerance is written into `test-specification.md`, section 0.7). **Status: Accepted.**

**Restarts.** A process started again needs `FAKE_START` set to the fake time at the restart, or its clock starts again from the original start time. **Process.**

### 3.2 Speed arithmetic is not a fault

At 60 times, one real minute is one hour of plan time, so 47 plan hours take about 47 real minutes. A run that looks stalled may be running at the right speed. Compare the plan time with the real time before concluding that a timer has stopped.

**Seen.** A live run looked silent after a restart. The clock was fine. The real fault was in 3.3 below.

**Status: Resolved.**

### 3.3 Expected times must respect the send window

**Seen.** A confirmation due at 23:58 was expected then, but it was sent at 07:00. The window (07:00 to 22:00) was closed at 23:58, so 07:00 was correct. The expectation was wrong, not the application.

**Status: Accepted** (recorded in the run's readme, `test-results/20261104-0900/readme.md`). **Process:** every expected time must apply the window.

### 3.4 A late message is reported as missing and unexpected

**Seen.** A retried message was sent two minutes late. The comparison reported it as missing (at its expected time) and as unexpected (at its actual time). That is one late message.

**Status: Open, ticket #114** (low priority, accepted for now). The late message itself is not explained: the times of its failed tries were not recorded, so it is not known whether the delay came before the second failure or after it.

**Process:** when reading a comparison report, look for a pair of "missing" and "unexpected" entries for the same message before treating them as two faults.

### 3.5 Expected messages and the plan must come from one version

**Seen.** In one run, the totals (8 emails and 4 texts) did not match the scheduled check (7 and 3), because the check had been written for an earlier version of the plan.

**Status: Process.** Generate the expected messages from the same plan the run uses, and check the totals against that plan only.

### 3.6 Results are repeatable only with the scratch scripts

The accelerated runs in `test-results/` used a model, a clock shim, a supervisor and a comparison that live in the scratch folder, not in the repository. Their results cannot be reproduced from the repository alone.

**Status: Open.** The harness must be brought into the repository before area 2 of the test specification is run (`test-specification.md`, Appendix C, item 3).

---

## 4. Running the tests

### 4.1 Save contacts before the process ends

**Seen.** In the first live messaging run, contacts were added but never saved. The process ended, and after the resume there were no contacts, so no messages were sent.

**Status: Fixed in the procedure** (a `save` after the contacts are added, in the second run). **Process:** every test that adds data must save it before any restart.

### 4.2 A second copy on a data folder sends no messages

Only the copy that holds `ContactManager.lock` sends messages (`mainwindow.cpp`). A copy started while another has the lock shows "Messages Not Sent", and sends nothing. A copy that is still running from an earlier test is the usual cause of "no messages".

**Status: Process.** Stop any earlier run, and check that no copy of the application is running, before starting an accelerated test.

### 4.3 Upload-group contacts break the Tidy Records scenarios

**Seen.** The 250 contacts of the upload group made many merge candidates, so the Tidy Records scenarios (F10, F13, LF02, T01 to T03) stopped at the driver's 40-click limit.

**Status: Fixed** (commit 220a509). The upload group's contacts are cleared before the Tidy scenarios. F10 is not yet confirmed as passing (`test-specification.md`, Appendix B).

### 4.4 A fault plan that is mistyped does nothing

`CM_TEST_FAULTS` makes numbered requests fail as if Google had sent the answer (`googleaccess_network.cpp`). Each entry is `w<n>` (writes) or `r<n>` (reads), then `=`, then `status:reason`, for example:

    CM_TEST_FAULTS="w3=403:rateLimitExceeded;w5-6=429:rateLimitExceeded;r2=400:badRequest;w9=closed"

An entry without the `status:reason` pair is skipped without a message, so the fault never fires. `closed` is the only form with no reason.

**Status: Process.** After a fault run, check that the fault shows in the run's log.

### 4.5 Test hooks are in product code

`CM_TEST_FAULTS` is read by the application (`googleaccess_network.cpp`). It has no effect unless it is set, but it is compiled into every build. `CM_TEST_FAIL_EMAIL` and `CM_TEST_OUTBOX` are in the email test substitute (`test-results/20261105-limits-retries/email_test.cpp`), which is a test build only.

**Status: Accepted.** Do not build the substitute into a release.

### 4.6 Background runs end with the container

A foreground command stops after two minutes, and the bench stops a scenario after 900 s (`Run.execute`). A container restart ends a background run. After a restart, resume from the scenario after the last one that finished, not from the start, so no upload runs twice.

**Status: Process.** The resume script in the scratch folder does this for the messaging runs.

### 4.7 `pkill -f` kills the shell that runs it

`pkill -f <pattern>` matches the command line of the shell running it, so it can kill that shell (exit 144). Use `pgrep -x <name>` or an exact process name.

**Status: Process.**

### 4.8 A background build can leave an empty object file

**Seen.** A driver built in the background produced an empty `.o` file, which then failed to link.

**Status: Process.** Build the driver in the foreground, and check that each object has content.

### 4.9 `#define private public` must come after the Qt and standard headers

Test builds reach a window's private members with `#define private public`, as the bench drivers do. Defining it before the standard library headers breaks them.

**Status: Process.** Include the Qt and standard headers first, then define `private`, then include the application's header (`headless-test.md`, section 5a).

### 4.10 The data path comes from the settings, not the working directory

The data folder is `[database] path` in `ContactManager.ini`. If the file is missing, the path is empty and the default is `~/TrumptonApps/ContactManager`. An earlier version of `Lib` returned the text `ERROR` for a missing setting, which made the path `ERROR/` and the dialling codes `error202020202`. That was fixed in `Lib` (commit `ad2c0d3`).

**Status: Fixed.** The bench gives each run its own HOME folder (`prepare_home`, `google_bench.py`), so one run cannot load another's contacts.

### 4.11 Test contacts from the first experiment

Contacts made in the first option-1 experiment were named `Email49 ...`. The bench now names its contacts `Bench ...` and its appointments `BENCH ...`, and removes them before and after each scenario (`headless-test.md`, section 6). Records from an old run are removed with the purge command.

**Status: Fixed** (superseded).

---

## 5. Open items

| Item | Where | Status |
|---|---|---|
| Clearing a website did not upload | 1.5 | Open. Needs a bench scenario first. |
| A URL already lost is not restored | 1.6 | Open, accepted. |
| Contacts downloaded by the earlier code | 1.7 | Open, untested. |
| Four older appointments mismatch on For | 2.6 | Open (issue #94). |
| The late retry and its missing failure times | 3.4 | Open, low priority (ticket #114). |
| The harness is in the scratch folder | 3.6 | Open. Move it into the repository (test specification, Appendix B). |
| F10 is not confirmed as passing | 4.3 | Open (test specification, Appendix B). |
| Focus after an edit calls `find()` with a hint | 4.3 (issue #109) | Open. |
