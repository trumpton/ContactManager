# #89 live messaging run, 1 to 3 November 2026 (run 2)

Run on the accelerated clock at 60 times real time, from 23:36 on Sunday 1 November, with the plan in `ops.json`.
The first attempt (on the real clock) stopped at 23:58 on 1 November; its contacts were never saved, so it is not used.
Run 2 saves the contacts after they are added. Its messages are in `box/`, one file each.

## Messages sent (12: 8 emails, 4 texts)

| Sent (plan time) | Channel | To | Message | Note |
|---|---|---|---|---|
| 23:47:30 Sun 1 Nov | email | Daniel | Confirmation, A (Wed 4 Nov 10:00) | Due 23:46 |
| 23:49:30 Sun 1 Nov | email | Jack | Confirmation, B (Tue 3 Nov 12:00) | Due 23:48 |
| 23:51:30 Sun 1 Nov | text | Sam | Confirmation, C (Mon 2 Nov 15:00) | Due 23:50 |
| 07:00:30 Mon 2 Nov | email | Daniel | Confirmation, Y (Thu 5 Nov 09:00) | Due 23:58 Sun 1 Nov. Sent after the window opened (accelerated clock boundary) |
| 07:00:30 Mon 2 Nov | text | Sam | Same-day text, C | Before C was deleted at 09:00 |
| 09:00:30 Mon 2 Nov | text | Sam | Cancellation, C | |
| 11:05:30 Mon 2 Nov | email | Daniel | Confirmation, G (Mon 2 Nov 17:00, booked 11:00) | |
| 11:06:30 Mon 2 Nov | text | Daniel | Same-day text, G | |
| 12:00:30 Mon 2 Nov | email | Jack | Reminder, B (Tue 3 Nov 12:00) | |
| 07:00:30 Tue 3 Nov | email | Jack | On-the-day email, B | |
| 07:00:30 Tue 3 Nov | email | Jack | Confirmation, H (Thu 5 Nov 14:00, booked 23:58 Mon) | Held to the window |
| 10:00:30 Tue 3 Nov | email | Daniel | Reminder, A (Wed 4 Nov 10:00) | |

Not reached: messages due after 23:05 on Tuesday 3 November.
X (booked and deleted within its hold) correctly sent nothing.

## Outcome

Accepted. The messages were checked per person (Daniel Jackson, Jack O'Neil, Sam Carter) and each looks correct.
The on-the-day reminders for Wednesday 4 November at 07:00 (email and text) were not reached: the run ends at 23:05 on Tuesday 3 November.
The totals (8 emails, 4 texts) were not reconciled against the 7 emails and 3 texts in the scheduled check, which predated this run's plan.
