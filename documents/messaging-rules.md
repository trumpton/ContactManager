# ContactManager messaging rules (issue #89)

Times are UK local time. Settings are from the `[reminders]` section of ContactManager.ini, with the defaults shown. The rules are taken from `messagedecision.cpp` and `mainwindow_timer_alert.cpp`, and the test run uses them as they are built.

## 1. Settings

| Setting | Default | Six-person test | What it does |
|---|---|---|---|
| `waitbeforesendmins` | 5 | 5 | Hold: a new or changed appointment waits this long after its last change before a message is sent. Zero is allowed. |
| `earliestsend` | 07:00 | 07:00 | Messages are sent from this time... |
| `latestsend` | 22:00 | 22:00 | ...up to, but not including, this time. Messages due outside the window wait until the next start. |
| `remindhoursbefore` | 24 | 24 | A reminder is due this many hours before the appointment. |
| `confirmonlyhours` | 36 (24 × 1.5) | 36 | Inside this many hours before the start, a confirmation or update takes the place of the reminder. |
| `limitperhour` | 5 | 20000 | Most messages to one contact in any hour. Further messages wait. |
| `limitperday` | 10 | 20000 | Most messages to one contact in any 24 hours. Further messages wait. |

## 2. When each message is sent

| Message | Sent when | Channel | Timing | Sent once? |
|---|---|---|---|---|
| Confirmation | Appointment is new, in the future, and not yet messaged. Contact has email or text. | Email if the contact has an email address; otherwise text | After the hold, at the first check inside the window | Once per appointment |
| Update | Already confirmed, and the start, end, or repeat differs from what was last sent | Same as confirmation | After the hold, at the first check inside the window. Carries the final details. | Once per settled change |
| Cancellation | Already confirmed, then deleted | Same as confirmation | Not held. Sent at the first check inside the window. | Once |
| Reminder | Not confirm-only; occurrence is within 24 hours; the occurrence is not today; no reminder sent yet | Same as confirmation | At the first check inside the window once due | Once per occurrence |
| On-the-day (Today Reminder) | The occurrence is today | Each channel the contact has: email and/or text. Uses the Today Reminder template (`emailtoday` / `smstoday` in `[message]`). | At the first check inside the window on the day, after any confirmation or update due at that check | Once per channel per day |
| Removed repeat occurrence | A repeat occurrence is removed | Email | At the first check inside the window | Once per occurrence |

Nothing is sent for a past occurrence, or for an appointment deleted before it was confirmed.

## 3. Who gets what, by channel setting

| Contact is set to | Confirmation, update, cancellation, reminder | On-the-day |
|---|---|---|
| Email only | Email | Email |
| Text only | Text | Text |
| Both | Email only (the email address is used for these) | Email **and** text, each once |

On the day, a confirmation or update sent by a channel that day counts as that channel's on-the-day message. The other channel still sends its own.

| Booking on the day | Result for each contact type |
|---|---|
| Both | Confirmation by email, then on-the-day text (no second email) |
| Email only | Confirmation by email only. The confirmation covers the day. |
| Text only | Confirmation by text only. The confirmation covers the day. |

Confirmed: for a same-day booking, a single-channel contact gets no separate on-the-day message. The confirmation gives the time on the day, so it counts as that channel's on-the-day message.

## 4. Corrections inside the hold

The hold runs for `waitbeforesendmins` (default 5) after the last change. Each new change restarts it. Example: an appointment is created at 10:00 for 09:00 tomorrow.

| What happens | Message sent |
|---|---|
| Created 10:00, edited 10:02 and 10:04 | One confirmation at 10:09, with the final times. The two intermediate versions are never sent. |
| Created 10:00, changed 10:02, changed back 10:04 | One confirmation at 10:09 with the original times. Nothing says the time changed. |
| Created 10:00, deleted 10:01 | Nothing. It was never confirmed. |
| Confirmed 10:09. Moved 10:20, moved back 10:23. | At 10:28 the details match what was last sent, so no update. |
| Confirmed 10:09. Moved 10:20, moved again 10:23. | One update at 10:28 with the final times. |
| Confirmed 10:09. Moved 10:20, cancelled 10:22. | Cancellation sent at once (not held). No update. |
| Confirmed 10:09. Title or location changed only. | No update. The change check covers start, end, and repeat only. (To confirm: is this intended?) |
| A reminder is due while an appointment is inside its hold | The reminder waits. The log says it is held until the appointment has been unchanged for the hold period. |

## 5. Backlogs and bursts

The send loop looks at each appointment's current state at every check. It does not replay a list of events, so a backlog gives one message per appointment, not one per missed change.

| Situation | Result |
|---|---|
| Edits made overnight, outside 07:00–22:00 | One update per appointment at the first check after 07:00, with the final times |
| Several appointments created while ContactManager was off | One confirmation each. Their hold has passed, so they go at 07:00. |
| Reminders due while off | One reminder per occurrence, sent once each. Occurrences further than 24 hours away wait for their own reminder time. |
| More messages to one contact than the limits allow | Messages wait, and are not dropped. Limits apply to each contact. Waiting messages are still sent later, as the limit clears. |
| A send fails | Retried after 5 minutes, up to 10 tries, then given up. A failed send is not repeated after a restart if it was recorded as sent. |
| Restart in the middle of a day | On-the-day messages are recorded per channel, so they are not sent twice. |

**Limit example (defaults).** A contact has 12 appointments created overnight, so all 12 are due at 07:00:
- The first 5 go at 07:00 (the hourly limit).
- The next 5 go at 08:00 (the hourly limit again).
- The 11th and 12th wait until 24 hours after the first message, because the daily limit of 10 has been reached.

With the test limits of 20000, none of this applies.

## 6. Evidence

- **Accelerated test** (`test-results/20261102-2313/`): 25 scenarios on six contacts covering the send window, the 5-minute hold, the 24-hour reminder and the 36-hour confirm-only period. 60 expected messages; all 60 were sent and matched, each with the right appointment time, within one check of the expected minute. The full list of scenarios and the corner case each tests is in `README.md`.
- **Unit tests** (`autotest/archive/messages`): 77 of 77 pass. They cover same-day bookings with both channels, the hold, edits that return to the sent details, and the date parsing.

## 7. Confirmed decisions

1. Single-channel contacts get an on-the-day message by their own channel, using the Today Reminder templates (sections 2 and 3).
2. Same-day bookings for single-channel contacts get the confirmation only, because it gives the time on the day (section 3).
3. Updates are not sent for title or location changes, only for start, end and repeat (section 4).
