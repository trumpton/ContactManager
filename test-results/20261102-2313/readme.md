# Messaging test, accelerated run (issue #89)

Run date: 2 to 5 November 2026 (fake clock). Results filed 2 November 2026, 23:13.

## What was tested

The reminder and confirmation rules in ContactManager: when confirmations, updates, cancellations, reminders and
on-the-day messages are sent. The focus is the send window (07:00 to 22:00), the 5-minute hold, the 24-hour reminder and
the 36-hour confirm-only period.

The test ran the real application with its messages intercepted. Nothing was sent to any address or number. The
outbox files were compared with the messages predicted by the rules.

## How it was run

- **Application:** the `messaging-tests` branch at commit 376cce5, with no changes to the application source.
- **Test driver:** `autotest/bench/sync_driver.cpp` (now `autotest/archive/bench/sync_driver.cpp`) with uncommitted changes (`save`, `waituntil` and an exemption for
  the main window in the dialog watchdog). A copy with a 50 ms `waituntil` loop was built for this run.
- **Email and SMS:** the send functions were replaced by copies that write each message to an outbox folder.
- **Clock:** the application's wall clock ran at 60 times real time, using an LD_PRELOAD shim. The fake clock started at
  20:00 on 2 November, so the six contacts could be added before the first appointment at 20:40. The run stopped at
  22:17 on 5 November, two minutes after the last appointment started at 22:15. It took 75 minutes of real time.
- **Scenarios:** 25 scenarios on six contacts (Daniel and Vala both channels, Jack and Tealc email, Sam and George text),
  with 60 expected messages. Each scenario is described in the sequence file.
- **Expected messages:** produced by a model of the rules, which follows `messagedecision.cpp` and the send loop, checked
  once a fake minute. The model is in the test tooling, not the repository (see below).

## Results

**60 of 60 expected messages were sent, and 60 of 60 matched.**

- No expected message was missing, and no unexpected message was sent.
- Every message quoted the right appointment time.
- Timing against the expected minute: 40 on time, 14 one minute late, 6 one minute early. The clock checks once a fake
  minute, so one minute is the resolution of this test. Boundary cases are the ones where the expected minute is itself
  on the boundary, such as a reminder exactly 24 hours before a start.

Full per-message results: `run-results.csv`. Comparison report: `report-final.md`.

## Corner cases covered

| Scenario | Corner case | Result |
|---|---|---|
| A01 | Created 5 minutes before the window closes: confirmation inside the window, on-the-day messages at 07:00 | Pass |
| A02 | Hold ends at 22:02, after the window closes: confirmation waits for 07:00 | Pass |
| A03 | Created before 07:00: confirmation at 07:03; same-day text booking gets no on-the-day text | Pass |
| A04 | Reminder 24 hours before a start 40 hours away: sent at the next check | Pass |
| A05 | Confirmed 36 h 15 min before start (outside 36 h): reminder still due | Pass |
| A06 | Confirmed 35 h 25 min before start (inside 36 h): no reminder | Pass |
| A07 | Reminder due at 22:00, outside the window: held to 07:00 and replaced by the on-the-day message | Pass |
| A08 | Reminder due exactly at the window opening (07:00) | Pass |
| A09 | Reminder due at 06:30, before the window: sent at 07:00 | Pass |
| A10 | Start at 07:30: on-the-day message at 07:00 | Pass |
| A11 | Reminder due at 22:15 (outside the window), replaced by the on-the-day message at 07:00 on the start day | Pass |
| B01 | Moved at 06:30, outside the window: update at 07:00, then the text on-the-day message at 07:01 | Pass |
| B02 | Moved within 36 h, after the window closes: update takes the reminder's place | Pass |
| B03 | Reminder at 24 hours, then a move: update counts as the reminder for the new time | Pass |
| B04 | Four edits overnight: one confirmation at 07:00 with the final times | Pass |
| B05 | Same-day email booking, then cancelled: cancellation sent at once inside the window | Pass |
| B06 | Booked after the window closes for a start later the same day: email confirmation at 07:00, text on-the-day at 07:01 | Pass |
| B07 | Same-day text booking in the evening: confirmation only | Pass |
| B08 | Deleted inside the hold: nothing sent | Pass |
| B09 | Booked at 21:57 for a start at 08:30 the next day: confirmation at 07:00, then text on-the-day | Pass |
| B10 | Hold ends at 06:35, before the window: confirmation at 07:00, same-day email booking | Pass |
| B11 | Deleted before its reminder is due: no reminder, cancellation at 07:00 | Pass |
| B12 | Reminder due at 22:45 on day 3, held to 07:00 on day 4: replaced by the on-the-day text | Pass |
| B13 | Confirmed, then on-the-day messages at 07:00, then cancelled at 09:00 | Pass |
| B14 | Reminder at 24 hours, then a move: update inside 36 h, no second reminder | Pass |

## Not covered

- Title and location changes (only start, end and repeat change an update).
- Per-contact message limits (set to 20000 for this run).
- Same-day bookings for single-channel contacts: the test follows the current rule, that a confirmation covers the day.
  This is to be confirmed.
- The wording of the messages. The templates are the sample templates with a `[TEST]` prefix.

## Files

- `sequence-1-accelerated.csv`: the test sequence in time order, with the corner case each step tests.
- `run-results.csv`: every expected message, the time it was sent, the time it quotes, and PASS or FAIL.
- `report-final.md`: the comparison report from the last run.
- `expected.json`: the expected messages, as the model produced them.
- `documents/messaging-rules.md` (in the repository's `documents` folder): the rules these tests check.

The scripts that build and run the test (the model, the clock shim, the supervisor, and the comparison) are not in the
repository. They are in the session's scratch folder, and can be added if you want the test to be repeatable.
