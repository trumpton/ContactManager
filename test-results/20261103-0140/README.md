# Messaging test, accelerated run 2 (issue #89): Today Reminder templates

Run date: 2 to 5 November 2026 (fake clock). Results filed 3 November 2026, 01:40.

## What changed from run 1

Run 1 (`test-results/20261102-2313/`) sent the on-the-day email and text with the reminder templates. This run uses the
Today Reminder templates for them: `emailtoday.msg` (subject "Appointment Today at HH:MM on DOW") and `smstoday.msg`
("Reminder: You have an appointment today at HH:MM"). The application and the sequence are otherwise the same, at commit
79cafc3.

## How it was run

As run 1: the real application with messages intercepted, a clock running at 60 times real time, and the same 25
scenarios (`sequence-1-accelerated.csv`). The run was stopped at fake 12:02 on 3 November, after operation 77, when the
worker process restarted. It was resumed from that operation, so no operation was repeated, and it finished at 22:17 on
5 November.

## Results

- **60 of 60 expected messages were sent and matched**: no missing, no unexpected, and every message quoted the right
  appointment time. Timing: all 60 within one check of the expected minute.
- **The restart did not change the outcome.** No message was duplicated or lost across the stop and resume.
- **The on-the-day messages used the Today Reminder templates**: 22 of 22 (9 emails, 13 texts), each with the Today
  wording. No on-the-day message used the reminder wording.
- Single-channel contacts received their own on-the-day message by their own channel (for example Jack and Tealc by
  email, George and Sam by text).

Full per-message results: `run-results.csv`. Comparison report: `report.md`. The expected messages are in `expected.json`.

## Scenarios that sent a confirmation or change in place of an on-the-day message

On the appointment day, a confirmation or update by a channel gives that channel's on-the-day message, so no separate
on-the-day message was sent on that channel. These are the cases (all as intended):

| Scenario | Contact | Channel | Sent instead | Time sent | Appointment |
|---|---|---|---|---|---|
| A03 | Sam | text | confirmation | Tue 3 Nov 07:02 | Tue 3 Nov 15:00 |
| B01 | Daniel | email | update | Tue 3 Nov 07:00 | Tue 3 Nov 18:01 (moved at 06:30) |
| B05 | Tealc | email | confirmation | Tue 3 Nov 10:05 | Tue 3 Nov 16:00 |
| B06 | Vala | email | confirmation | Wed 4 Nov 07:00 | Wed 4 Nov 12:00 (booked after the window closed on 3 Nov) |
| B07 | George | text | confirmation | Wed 4 Nov 21:05 | Wed 4 Nov 23:30 |
| B09 | Daniel | email | confirmation | Thu 5 Nov 07:00 | Thu 5 Nov 08:30 (booked after the window closed on 4 Nov) |
| B10 | Jack | email | confirmation | Wed 4 Nov 07:00 | Wed 4 Nov 17:00 (hold ended before 07:00) |

Scenario B01 also sent the text on-the-day message at 07:01, because the update covered only the email. Scenario B06 and
B09 also sent their text on-the-day messages at 07:01 on their day.
