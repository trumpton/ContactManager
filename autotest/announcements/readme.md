# Announcement tests (area 2)

The bench for section 2 of `documents/test-specification.md`: AN-001 to AN-048, on the accelerated clock with stub transports.

**Status: written, not run.** The plan is checked (`--plan`), the C++ parts are compile-checked against the application's
headers, and the driver and the clock are not yet built or run. The first run is the first real test of the bench.

## Files

| File | What it is |
|---|---|
| `model.py` | The plan: every test's events, its expected messages (from the rules in specification section 5), and the runs and their segments. |
| `announcements_tests.py` | The runner: builds each run's home, runs its segments in parallel, and checks each test's messages. `--plan` checks the plan and prints it, and starts nothing. |
| `clock/fakeclock.c` | The clock. Preloaded into the program: its wall clock runs at 360 times real time from a start time it is given. |
| `stubs/email_stub.cpp`, `stubs/sms_stub.cpp` | Test substitutes for `Email` and `SMS`. They write each send to the run's outbox and nothing is sent. |
| `driver/driver.cpp`, `driver/build_driver.sh` | The headless driver (a copy of the Google bench's driver) and the build script for the driver, the stubs and the clock. |
| `templates/` | The message templates for the bench. Each carries `$TESTTAG`, so each message shows its test code. |

Nothing here imports from or reads the archive folder. The clock and the stubs are new code; the driver is a copy of the Google bench's.

## Set-up

1. Build the application (`documents/headless-test.md`, section 1).
2. Build the driver, the stubs and the clock:

       sh autotest/announcements/driver/build_driver.sh ~/build-contactmanager

   This writes `~/build-contactmanager/driver` and `~/build-contactmanager/fakeclock.so`. The stub objects replace `email.o` and
   `sms.o` in the link. The application's sources are not changed.
3. Check the plan (starts nothing):

       python3 autotest/announcements/announcements_tests.py --plan

## Running

    python3 autotest/announcements/announcements_tests.py                 # every run, in parallel
    python3 autotest/announcements/announcements_tests.py --runs A,B      # some runs (N needs A, so A is added)

The runs run in parallel, as section 2.0 says, and do not touch Google. The main set takes about 16 minutes (the longest run
is four virtual days). Run W takes about 66 minutes (sixteen virtual days, see below). The evidence goes to `--out`
(default: a timestamped folder in your home folder). `report.md` and `summary.json` are at the top; each run has a folder
with its home, its outbox (every message as a file, sent or refused), and each segment's ops, driver log, dumps and stdout.

## How the clock works

`fakeclock.so` changes the realtime clock (`clock_gettime(CLOCK_REALTIME)`, `gettimeofday`, `time`). The program starts at
`FAKE_START` (seconds since 1970) and runs at `FAKE_SPEED` (360). The monotonic clock is left at real speed, so the program's
one-second check runs once per real second, which is once per six fake minutes. A message is therefore on time when it is
sent between its due time and one check later (specification 0.7). A restart is a new process whose `FAKE_START` is the
time of the restart; its clock continues from there.

## How the messages are read

Each send is a file in the run's outbox: `result` (sent or refused), `channel`, `to` (the address or the number), `subject`
(the subject, or the text of an SMS), `sent` (the program's own time), and the body. The checker takes:

- the test code from the subject (`CM_TEST_MESSAGE_TAGS` makes `$TESTTAG` the test code);
- the kind from the second word of the subject (`Confirmation`, `Updated`, `Cancelled`, `Reminder`, `Today`);
- the appointment time quoted in the subject (`10:00 05 November`).

A message matches an expectation when the code, channel, recipient, kind and quoted time all match. Each expected message
must be sent once, on time. Any other message with a test's code is unexpected, and fails that test. A run's shared outbox
also holds the second copy's sends, so the second copy is checked too.

Why stubs at the class level: the program builds its transport inline (`RealMessageTransport` in
`mainwindow_timer_alert.cpp`), so there is no point where a test can inject one. Replacing `Email` and `SMS` at link time
keeps the program's code unchanged, as the earlier accelerated harness did.

## The runs

Each run is one home folder, one outbox and one clock. A run's tests are listed in `model.py`. Two tests share a run only
when neither changes anything the other relies on (specification 2.0). Where the specification's table puts tests together
that its own rule keeps apart, the bench splits them (see the list below).

| Run | Tests | Length (virtual) | Notes |
|---|---|---|---|
| A | AN-001 to AN-015, AN-017 to AN-021, AN-024, AN-025, AN-047 | 3 days | Plain tests, limits raised. AN-047 checks the run as a whole. |
| B | AN-026 to AN-030 | 3 days | Contact state. Each test has its own contact. |
| C1 | AN-031 | 3 days | Shutdown at 10:02 on day 0, restart at 10:20. |
| C2 | AN-032 | 1 day | Shutdown 11:00 on day 0 to 06:00 on day 1. |
| C3 | AN-033 | 1 day | Restart at 12:00 on day 1. |
| C4 | AN-034 | 4 days | Shutdown on day 0, three days off, then the edits. |
| C5 | AN-043 | minutes | Two starts: the key expires in 29 days (warning), then in 31 days (none). |
| L1 | AN-022 | 2 days | Default limits (5 an hour, 10 a day). |
| L2 | AN-023 | 2 days | Default limits, restart at 12:00 on day 1. |
| D1 | AN-045 | 2 days | Starts 27 March 2027, across the clock change on 28 March. |
| D2 | AN-046 | 2 days | Starts 30 December 2026, across New Year. |
| G1a / G1b | AN-036 / AN-037 | 12 hours | A blank sender each (email / text). |
| G2a / G2b / G2c | AN-038 / AN-039 / AN-044 | 1 to 3 days | A template change each (emailtoday blank / missing file / two-line text). |
| G3a / G3b / G3c | AN-040 / AN-041 / AN-042 | 3 days | A stub failure each (two refusals / always refused / one text refused with a reason). |
| G4 | AN-048 | 4 days | Reminders off at the start, on at day 3. |
| N | AN-035 | 1 hour | A second copy on run A's home, from 09:00 to 10:00 on day 1. |
| W | AN-016 | 16 days, 9 hours | Weekly repeat, three occurrences. Not in the sixteen-minute set. |

## Where the specification and the rules disagree

These are recorded in each test's note in `model.py`, and in the report. The expectations follow the rules in section 5.

1. **AN-017.** The row says the confirmation is sent at 07:00. Created at 21:30, with the five-minute hold, it is due at 21:35,
   inside the send window (5.5), so the rules send it then. The row needs correcting.
2. **AN-018.** The row says a reminder is sent at 07:00. The reminder is due at 23:00 on day 1, but the appointment is on day
   2, and a reminder is not sent on the day (5.1). So the message at 07:00 is the on-the-day message.
3. **AN-003.** The row says "no text". A contact with both channels gets the on-the-day text as well (5.2, 5.3), so that
   text is expected. The confirmation itself is by email only.
4. **AN-016.** The row says "over three weeks", but it is in run A (three days). It is in its own run W, which is sixteen
   days long. This makes the bench's total time much longer than sixteen minutes; the spec should choose a shorter procedure
   or accept the run.
5. **Runs that the rule keeps apart.** The specification's table puts several tests in one run, but its own rule (2.0)
   says a change to the ini file, the templates, the stub's failures, a restart, or a date keeps tests apart. The bench
   therefore has one run each for the blank senders (G1a, G1b), the templates (G2a, G2b, G2c), the failures (G3a, G3b, G3c),
   the restarts (C1 to C5), the limits (L1, L2) and the dates (D1, D2). Run A keeps the plain tests together, and run B the
   contact-state tests.

## Choices and assumptions to confirm

- **AN-034, "while off".** The program cannot be edited while it is shut down, and the driver is the only thing that edits it.
  The bench makes the 200 edits in the silent period before 07:00 on day 3, when no message can be sent. That gives the same
  result as edits made while the program is off (5.8): one message per appointment.
- **The second copy (AN-035).** Run N starts when run A's clock reaches 09:00 on day 1, which the runner works out from A's
  real start and speed. The check is on the warning and on run A's outbox in that hour.
- **History and log wording.** The history entries are `Sending confirmation email to ...` (or `... SMS to ...`), which is what
  the checks count. The log lines are `Error ... (try N of 10, next try in 5 minutes)` and `Given up on ... after 10 tries`,
  which are what the checks look for. If the program's wording changes, these checks fail, not the messages.
- **The quoted time.** Each template's subject carries `$TIME $DATE $MONTH` (for example `10:00 05 November`). The checker
  matches that text. The reminder's expected time is 24 elapsed hours before the appointment, not 24 wall-clock hours
  (5.1), which matters only across the clock change (AN-045).
- **Matching tolerance.** A message is on time if sent between its expected time and six minutes later. Groups of messages
  (AN-022, AN-023) are checked by count in their windows, because the order of a batch cannot be known in advance.

## Not run yet, and what to check first

- The driver, the stubs and the clock have been compile-checked (the stubs and the driver against the application's
  headers), but not built, linked or run.
- The program's first start (CM_TEST_SETKEY) and the key and login dialogs have not been exercised with the stubs.
- Twenty-two processes run at once. Check the machine can take that load before the full set.
- The checks on the wording of the program's log and history (above) are the most likely to need a change after the first run.
