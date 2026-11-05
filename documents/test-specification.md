# Test specification

This specification describes every test run for the messaging rules (#89) and the Google sync test bench (#105): what each test
checks, how it is run, the result it gives, and where the evidence is kept. The generated appendices list each bench scenario and
each unit test by name. Corner cases that are not yet tested are listed in issue #110: https://github.com/trumpton/ContactManager/issues/110

Results are as at 4 November 2026. Commit references are to the `messaging-tests` branch.

## 1. Overview

| Area | What it tests | Count | Last result |
|---|---|---|---|
| Messaging unit tests | The message rules: reminders, confirm-only, send window, hold, same-day, on-the-day, repeats, contact links, pacing | 81 | 81 of 81 pass (`autotest/archive/messages`) |
| Accelerated messaging test (#89) | The rules end to end, on a scaled clock, with a real outbox | 25 scenarios, 60 messages | 60 of 60 sent and matched, in two runs (`test-results/20261102-2313`, `test-results/20261103-0140`) |
| Live messaging run (#89) | The rules on the real clock with a fixed plan of 32 operations | 32 operations | Stopped at operation 18 (1 November, 23:58). Resumed on the accelerated clock at 60 times real speed. See section 4 |
| Google sync bench (#105) | Every way data moves between ContactManager and Google, and the local files | 164 scenarios: 1 upload, 133 update, 30 local | Local 30 of 30. Updates: 64 of 67 in the second half and 66 of 66 in the first half before the 1970 fix. Re-run after the fix: E70, N107, G40, P30, W01 to W04, AR01, R08, X15, E01, LF01, LF02 pass. F10 fails (section 5) |
| Upload group (U01) | 250 contacts and 1,000 appointments uploaded in one sync | 1 | Pass, 1,062 seconds, all 1,000 on Google (`scratchpad` run U01-final3) |

## 2. Messaging rules

The rules are in `documents/messaging-rules.md`. In summary:

- **Send window:** messages go only between 07:00 and 22:00 local time. Anything due outside it waits for 07:00.
- **Hold:** a new or changed appointment waits 5 minutes after its last change. Each change restarts the hold. A change that
  returns to the sent details sends nothing.
- **Confirmation:** once per new appointment, by email if the contact has an address, otherwise by text. Both channels ticked means email.
- **Update:** once per settled change to the start, end or repeat. A title or location change sends nothing.
- **Cancellation:** not held. Sent once, inside the window.
- **Reminder:** 24 hours before the start. Not sent if the appointment was confirmed or changed less than 36 hours before the start,
  and not for an appointment on its own day.
- **On the day:** each channel the contact has gets one on-the-day message, at the first check of the day inside the window.
- **Limits:** 10 messages per contact per day and 5 per hour. Messages over a limit wait.

## 3. Messaging tests

### 3.1 Unit tests (`autotest/archive/messages`, QtTest, no Google)

Run with `sh autotest/archive/messages/run-messages-tests.sh <build folder>`. Result: 81 of 81.

Tests, by name:

reminderDueWithinTheReminderHours, reminderDueAtTheStartOfTheDayBefore, reminderDueForAnAppointmentThatHasStarted, reminderFollowsRemindHours, confirmOnlyJustInside36Hours, confirmOnlyExactly36HoursIsNot, confirmOnlyForShortNotice, confirmOnlyAndReminderMeetAtTheBoundary, confirmOnlyDefaultIs1Point5TimesRemind, confirmOnlyUsesAValidValue, confirmOnlyLessThanRemindUsesTheDefault, confirmOnlyNotANumberUsesTheDefault, remindHoursDefaultsAndLimits, holdLastsWaitMinutes, sendWindowIsStartInclusiveEndExclusive, onTheDayUsesLocalDate, dayRecordKeepsTheChannelsOfTheDay, newAppointmentByEmailGetsAConfirmation, newAppointmentByTextGetsAConfirmation, newAppointmentWithBothGetsEmailOnly, newAppointmentWithNoChannelGetsNothing, confirmOnlyFoldsTheReminderIntoTheConfirmation, changedAppointmentGetsAnUpdateOnItsChannel, unchangedSentAppointmentGetsNothing, deletedSentAppointmentGetsACancellationOnItsChannel, deletedUnsentAppointmentGetsNothing, reminderOnceByTheChannelOfTheContact, reminderIsNotSentTwice, reminderNotDueIsNotSent, removedOccurrenceIsCancelledOnItsChannel, removedOccurrenceOnlyForARepeat, onTheDayBothChannelsAreSentOnTheDay, onTheDayTextIsSentWhenEmailHasGone, onTheDayEmailIsSentWhenTextHasGone, onTheDayIsNotSentAgainOnceBothHaveGone, onTheDayIsSentForASingleChannel, onTheDayIsNotSentOnAnotherDay, sameDayBookingGivesItsEmailAndThenTheOnTheDayText, sameDayTextBookingCoversTheDaysText, sameDaySmsIsSentWhenBookedEarlierAndTheDayComes, caseSevenEmailIsTheConfirmationWhenWindowOpensLate, caseSevenTextIsTheOnTheDayTextWhenWindowOpensLate, caseSevenBothIsEmailThenTheOnTheDayText, caseEightNoReminderOnTheDay, reminderOnTheDayIsReplacedByTheOnTheDayText, channelSwitchedToTextSendsOnlyWhatIsPending, scenarioCreatedThirtySixHoursAheadBothChannels, scenarioCreatedTwentyFiveHoursAheadBothChannels, scenarioBookedOnTheDayBothChannels, listedOccurrencesOfARepeat, removedOccurrenceIsNotListed, repeatWithEveryOccurrenceRemovedListsNothing, periodsTouchingAtAnEndDoNotClash, clashIgnoresARemovedOccurrence, clashWithAListedOccurrence, repeatWithNoOccurrenceLeftIsFound, clashIsNotReportedForAnEmptyRepeat, getDateGivesACopyThatStaysTheSame, sentStateDoesNotCountAsAnEdit, googleRecordCopyDoesNotRestartTheWait, contactLinkIsKeptWhenTheContactExists, contactLinkIsSetByAnExactNameMatch, contactLinkIsRemovedWhenTheNameMatchesMoreThanOne, contactLinkIsRemovedWhenNoNameMatches, singleAppointmentEndedBeforeTheEdgeIsExpired, repeatStartMovesToTheFirstOccurrenceInTheWindow, repeatTrimKeepsOnlyTheExceptionsStillToCome, repeatWithNoOccurrenceLeftIsExpired, trimKeepsTheMessageSignatureSoTheContactIsNotMessaged, focusGoesToTheNearestDate, twoDigitYearsUseTheCenturyOfThisYear, ratePacerIsOffWithoutRateLimits, ratePacerStartsAtFourSecondsAfterFiveLimitedOfAHundred, ratePacerHalvesEvery500DownToHalfASecond, ratePacerCountsOnlyTheLastHundred, numericDatesAreParsed, typedDatesAreParsed, loadKeepsTheSavedUpdatedDate, mergedSentStateDoesNotCountAsAnEdit

### 3.2 Accelerated messaging test (#89)

Run on a fake clock that advances 60 times faster than real time, with a fixed plan of appointments, edits, deletions and
restarts. Each message is matched against an expected list, with its time and the appointment time it quotes.

Settings: send window 07:00 to 22:00, hold 5 minutes, reminder 24 hours, confirm-only 36 hours, limits raised to 20,000 (so
limits are not exercised; see the ticket).

Scenarios (from `scratchpad/accel/model.py`):

| ID | Who | Booked | Start | What it checks |
|---|---|---|---|---|
| A01 | Daniel | Mon 21:50 | Tue 03 Nov 12:00 | Created 5 min before the window closes: the confirmation is sent inside the window; confirm-only (under 36 h), so no reminder; both channels get the on-the-day message at the window opening |
| A02 | Jack | Mon 21:57 | Wed 04 Nov 10:00 | Hold ends at 22:02, after latestsend: the confirmation waits for the window opening (07:00) |
| A03 | Sam | Tue 06:57 | Tue 03 Nov 15:00 | Created before earliestsend: hold ends at 07:02, inside the window. Same-day text booking: the confirmation gives the day, so no on-the-day text |
| A04 | Tealc | Mon 21:30 | Wed 04 Nov 14:00 | Over 36 h away when confirmed: reminder due 24 h before, 14:00 tomorrow, is sent at the next check after it |
| A05 | Vala | Mon 20:40 | Wed 04 Nov 09:00 | Confirmed 36 h 15 min before the start (just outside 36 h): not confirm-only, so the reminder is still due |
| A06 | George | Mon 21:00 | Wed 04 Nov 08:30 | Confirmed 35 h 25 min before the start (inside 36 h): confirm-only, so there is no reminder |
| A07 | Daniel | Mon 20:40 | Wed 04 Nov 22:00 | Reminder due at 22:00 falls outside the window: it is held to 07:00 on the day of the appointment, where the on-the-day message replaces it |
| A08 | Jack | Mon 20:40 | Thu 05 Nov 07:00 | Reminder due exactly at the window opening (24 h before 07:00 on day 5) |
| A09 | Sam | Mon 20:40 | Thu 05 Nov 06:30 | Reminder due at 06:30 on day 4, before the window: it is sent at 07:00, the first check in the window |
| A10 | George | Mon 20:40 | Thu 05 Nov 07:30 | Start at 07:30 on day 5: on-the-day message at 07:00, the first check, before the start |
| A11 | Vala | Mon 20:40 | Thu 05 Nov 22:15 | Reminder due at 22:15 on day 4 (outside the window), so held to 07:00 on day 5 where the on-the-day message replaces it. Last appointment of the run |
| B01 | Daniel | Mon 21:00 | Tue 03 Nov 18:00 | Moved at 06:30, outside the window: the hold ends at 06:35, so the update waits for 07:00. The update covers the email on the day, so the text on-the-day message follows at 07:01 |
| B02 | Jack | Mon 21:00 | Wed 04 Nov 15:00 | Reminder would be due at 15:00 on day 3; the move at 21:58 is held past the window to 07:00 on day 3, when the start is inside 36 h, so the update takes the reminder's place |
| B03 | Sam | Mon 21:00 | Thu 05 Nov 10:00 | Reminder sent 10:01 on day 4 (exactly 24 h before the start, one minute after). A move at 10:05 is sent at 10:10 as an update, and it counts as the reminder for the new time |
| B04 | George | Tue 23:00 | Thu 05 Nov 12:00 | Covers the rules for this booking; see the model |
| B05 | Tealc | Tue 10:00 | Tue 03 Nov 16:00 | Same-day email booking: confirmation at 10:05, then cancelled at 10:30 (a cancellation is not held) |
| B06 | Vala | Tue 22:30 | Wed 04 Nov 12:00 | Booked after the window closes, for a start later on day 4: confirmation waits for 07:00, and the on-the-day text follows at 07:01 (the email already covered the day) |
| B07 | George | Wed 21:00 | Wed 04 Nov 23:30 | Same-day booking in the evening, after the on-the-day check would have run: confirmation text only |
| B08 | Sam | Tue 12:00 | Wed 04 Nov 13:00 | Deleted inside the hold, before anything was confirmed: nothing is sent |
| B09 | Daniel | Wed 21:57 | Thu 05 Nov 08:30 | Booked at 21:57 for a start at 08:30 on day 5: confirmation at 07:00, and the text on-the-day message at 07:01 |
| B10 | Jack | Wed 06:30 | Wed 04 Nov 17:00 | Hold ends at 06:35, before the window: confirmation at 07:00, same-day email booking, so no on-the-day email |
| B11 | Tealc | Mon 21:00 | Thu 05 Nov 14:00 | Deleted at 22:30 on day 3, before its reminder is due at 14:00 on day 4: no reminder. The cancellation waits for 07:00 |
| B12 | George | Mon 21:00 | Wed 04 Nov 22:45 | Reminder due at 22:45 on day 3 is held to 07:00 on day 4, when the start is on that day: on-the-day text replaces it |
| B13 | Vala | Tue 19:00 | Wed 04 Nov 11:00 | Confirmed on day 3, then the on-the-day email and text at 07:00 on day 4, and cancelled at 09:00 (not held) |
| B14 | Jack | Mon 20:40 | Wed 04 Nov 20:40 | Reminder sent 20:41 on day 3 (exactly 24 h before). A move at 20:50 is sent as an update at 20:55, inside 36 h, and counts as the reminder for the new time, so no second reminder |

Results: run 1 (`test-results/20261102-2313`) and run 2 (`test-results/20261103-0140`, with the Today Reminder templates) both
sent and matched all 60 messages. Run 2 was stopped and resumed mid-run; no message was duplicated or lost. All 22 on-the-day
messages used the Today wording. Timing against the expected minute: 40 on time, 14 one minute late, 6 one minute early.

### 3.3 Live messaging run (#89, live check)

A plan of 32 operations on the real clock, starting 1 November 23:36. It adds three contacts (Daniel: both channels; Jack:
email only; Sam: text only) and then:

| Op | Clock | Action | Expected |
|---|---|---|---|
| 5 | 23:41 | A: Daniel, Wed 4 Nov 10:00 | Confirmation email at 23:46 |
| 7 | 23:43 | B: Jack, Tue 3 Nov 12:00 | Confirmation email at 23:48 |
| 9 | 23:45 | C: Sam, Mon 2 Nov 15:00 | Confirmation text at 23:50 |
| 11 | 23:47 | X: Daniel, Thu 5 Nov 09:00 | Nothing (deleted at 23:49, inside the hold) |
| 13 | 23:49 | Delete X | - |
| 15 | 23:51 | Y: Daniel, Thu 5 Nov 09:00 | Confirmation email at 23:58 (the edit at 23:53 restarts the hold) |
| 17 | 23:53 | Edit Y (location) | - |
| 19 | Mon 08:00 | Edit B (location) | Nothing (location change) |
| 21 | Mon 09:00 | Delete C | Cancellation text, inside the window |
| 23 | Mon 11:00 | G: Daniel, Mon 17:00 (same day) | Confirmation email, then the on-the-day text |
| 27 | Mon 23:58 | H: Jack, Thu 5 Nov 14:00 | Confirmation email about 00:03 Tuesday |
| 25, 29, 31 | Mon 12:30, Tue 12:30, Tue 23:05 | Dumps | - |

Result so far: the four messages due before 23:58 on 1 November were sent at the expected times (23:46, 23:48, 23:50, 23:58), and
X was correctly silent. The run then stopped at operation 18. It was resumed from operation 18 on the accelerated clock, at 60
times real speed, starting from 23:53 on 1 November. At the time of writing the resumed run is at 3 November 12:30 and has
not yet sent any message after 23:58 on 1 November. This is being investigated (section 4).

## 4. Status of the live run

The live run is not complete, and it is not marked complete. The resumed run has been running for about an hour. It has not sent
the messages due after 23:58 on 1 November (the reminder for B, the cancellation for C, the same-day confirmation for G and the
confirmation for H), so its result will show those as missing. The cause is not known yet: it may be the accelerated clock
(timers in the application may not follow the scaled clock), or a fault in the application's sending after a restart.

## 5. Google sync bench (#105)

Run with `python3 autotest/archive/google_bench.py run <codes or selections>`. Each scenario starts from a known state on the scratch
account, checks the local files and the Google account, and writes its evidence to `runs/<scenario>/` (the report, the dump, the
driver log, the Google state). Selections: `full`, `quick`, `google`, `google-quick`, `local`.

The scenarios in each group are listed in the appendix. The principal results:

- **Local group:** 30 of 30 pass (run after the prefix change).
- **Updates group:** the first half passed 66 of 66. The second half passed 64 of 67 before the 1970 fix. Re-run after the fix:
  E70, N107, W01 to W04, AR01, R08, X15, E01, LF01 and LF02 pass; G40 and P30 pass with the corrected far edge.
- **F10 (#69):** fails. The driver is stuck in a "Tidy Records" prompt loop and gives up after 40 clicks. The account's contacts
  are the likely cause, but this is not confirmed, and it is not known whether F10 passed before this session's changes.
- **Upload group (U01):** passes, 1,062 seconds, with all 1,000 appointment masters on Google. The records are kept on Google
  (no cleanup runs before or after the upload group).

Conventions the tests rely on:

- Every record a test creates carries the test prefix: an appointment's summary starts `BENCH `, and a contact is named `Bench`
  or has a `test.invalid` address. The helpers and the runner refuse anything else.
- The upload group purges the account before it uploads. This is for the scratch account only.
- Google-side changes made by the bench go through `gchange`, which refuses any record without the prefix.

### 5.1 Bench scenarios


### Uploads (purge, then upload; left in place)

| Code | Test |
|---|---|
| U01 | #105 Uploads: purge the account, then upload 250 contacts and 1,000 appointments (half by the Google API, half by ContactManager), left in place |

### Updates (every other sync test)

| Code | Test |
|---|---|
| A01 | Add an appointment locally, no sync: Google unchanged |
| A02 | Event made on Google: downloaded once across two syncs |
| A03 | Identical events on Google (duplicates): both kept, no growth on re-sync |
| A04 | Same event added locally and on Google before sync: both kept |
| A05 | Edit a downloaded event before a second sync: change reaches Google |
| A06 | Event moved on Google between syncs: app picks it up |
| A07 | Event deleted in the app after it synced: deleted on Google |
| A08 | Event deleted on Google between syncs: app does not keep a live copy |
| A09 | Overlapping appointments added locally: both uploaded, neither merged |
| A10 | Large upload: 40 local appointments reach Google once |
| A11 | Read-only event (organised by someone else): not uploaded across syncs |
| A12 | Stray spaces and newlines in a location: no spurious upload |
| A13 | Local ID of a Google event is the same across syncs |
| A14 | #105 Appointment deleted in the app and changed on Google, ContactManager master: the app's delete wins, so Google deletes it too (item 4) |
| A15 | #105 Appointment deleted on Google and changed in the app, ContactManager master: created on Google again with the app's change (item 4) |
| AR01 | #105 Archive: an entry that the window leaves behind is archived; moved into the window on Google, it is restored and leaves the archive |
| AR02 | #105 Archive: an archived entry changed on Google, still outside the window, has its archived copy brought up to date |
| AR03 | #105 Archive: an archived entry cancelled on Google leaves the archive, and is kept in the deleted folder |
| C01 | Add a contact locally, no sync: Google unchanged |
| C02 | Contact made on Google: downloaded once across two syncs |
| C03 | Add then delete locally before any sync: nothing reaches Google |
| C04 | Contact deleted on Google between syncs: removed from the app |
| C05 | Contact deleted in the app after it synced: deleted on Google |
| C06 | Contact hidden in the app after it synced: Google agrees with the app |
| C07 | Same name on both sides before sync: both kept |
| C08 | Edit a downloaded contact before a second sync: change reaches Google |
| C09 | Edit on Google between syncs: app picks it up without duplicating |
| C10 | Out-of-sequence phones and emails: each value kept in some slot |
| C103 | #103: the contact on the form stays loaded through a sync, in ContactManager master and in Google master, and the calendar entry does not move |
| C11 | Stray spaces and newlines in Google data: no spurious upload |
| C12 | Large upload: 40 local contacts reach Google once |
| C13 | Unicode and apostrophes in names round-trip exactly |
| C14 | Email-only contact (no name) is uploaded with its email |
| C15 | Repeated syncs with no changes do not write to Google |
| C16 | Edit keeps URLs the app does not hold (option 1 regression) |
| C17 | #105 Contact changed in both places since the last sync, ContactManager master: the app's change is kept, once |
| C18 | #105 Contact changed in both places since the last sync, Google master: Google's change is kept, once |
| C19 | #105 Contact deleted on Google and changed in the app, ContactManager master: created on Google again with the app's change (item 4) |
| C20 | #105 Contact deleted in the app and changed on Google, ContactManager master: the app's delete wins, so Google deletes it too (item 4) |
| D01 | Clearing an email and a phone in the app removes them from Google |
| E01 | #86: an ETag is stored from Google and sent with a change: a local edit reaches Google, with no conflict |
| E02 | #86: Google's copy changed in its details since the last sync (non-matching ETag) and the app changed it too, ContactManager master: the app's version is sent, and the conflict is reported (item 4) |
| E03 | #86: Google's ETag changed, but not its details (a field the app does not hold): the change is sent again with the new ETag, and reaches Google |
| E04 | Editor: a phone typed with (0) after the country code and a label after the number is saved tidily |
| E70 | #104 A local entry dated 1 January 1970, or with no start, is neither stored nor uploaded; a normal entry alongside is unaffected |
| ED01 | Editor: a label typed after an address becomes its title, and Google gets it as that type |
| ED02 | Editor: a save with a bad entry is refused, focus goes to the first, nothing is saved, and the typed text is kept |
| ED03 | Editor: an older entry with its label in the address is tidied when opened, and saved with the next change |
| F01 | More emails on Google than the app has slots: all of them are still on Google after an upload |
| F02 | Google master: a bad entry downloaded from Google stops a save of the form, and nothing is uploaded |
| F03 | More phones on Google than the app has slots: all of them are still on Google after an upload |
| F04 | A save refused for a bad entry stops the next sync, so nothing is sent to Google |
| F05 | #66: an unchanged bad entry does not stop switching to another contact |
| F06 | #66: with a contact holding an unchanged bad entry open, a Google sync still runs |
| F07 | #66: a contact kept with an unfixable entry is uploaded once, then not again on later syncs |
| F08 | A contact with a clean address and no titles set: uploaded once, then not again on later syncs |
| F09 | #66: moving the selection on, as Tidy Contacts does, does not check the open contact's entries |
| F10 | #69: the report's steps: a bad entry from Google, Alt-E,D, Ctrl-S, Exit, relaunch, Tidy Contacts, Alt-E,D, Exit |
| F13 | #68: switching contact, Tidy Contacts and sync ask Save or Discard when the contact has edits |
| G40 | #30: a repeating Google event is stored once, with its repeat, and listed on each week |
| G41 | #30: an edit of a repeat in the app changes the Google event, and the event stays repeating |
| G42 | #30: a repeating Google event whose first occurrence is older than the window still lists its repeats |
| G43 | #30: a Google repeat with a count is stored once and listed that many times, and an app edit keeps the count |
| G44 | #30: an appointment on Google older than the one-year lookback is not downloaded |
| LB01 | Four emails from Google (home, other, work, Partner): each keeps its slot and label, no write |
| LB02 | A label changed in the app is sent to Google as that email's type |
| LB03 | Emails with no labels set are sent with the labels they always had |
| LF01 | #105 Local files: after two syncs the calendar folder holds each appointment once, and no file is empty |
| LF02 | #105 Local files: a deleted appointment moves to the deleted folder, and is not left in the calendar folder |
| M01 | ContactManager master adds a contact; Google master syncs it: no duplicate |
| M02 | Google master downloads; ContactManager master edits; Google master re-syncs: one copy |
| M03 | Deleted in ContactManager master: Google master does not bring it back |
| M04 | Deleted on Google in Google master: ContactManager master does not bring it back |
| M05 | Appointment: ContactManager master adds, Google master edits, ContactManager master picks it up |
| M06 | Repeated switching with edits: values and notes survive, no duplicates |
| M07 | Google master: a local-only edit is not sent, and Google's values stay |
| M08 | ContactManager master: a change made on Google is kept, not overwritten |
| N107 | #107: an appointment with no end time, or one ending on 1 Jan 1970, is not uploaded; the sync completes and the others reach Google |
| N108 | #108: with the last calendar sync back at the epoch (as after a failed sync), the next sync sends only the appointment that was edited, not the ones it downloaded |
| P01 | Four phones from Google (home, work, mobile, Pager): each keeps its slot and label, no write |
| P02 | A phone label changed in the app is sent to Google as that number's type |
| P03 | Phones with no labels set are sent with the labels they always had |
| P04 | Seven phones on Google: the app holds six, the seventh goes to Comments |
| R01 | #105 ContactManager master, edit in Google, repeat: the Google change reaches the app, one repeat on Google |
| R02 | #105 Google master, edit in ContactManager, single: the next sync takes Google's location, and nothing is uploaded |
| R03 | #105 Google master, edit in ContactManager, repeat: the next sync keeps the repeat and Google's location |
| R04 | #105 Google master, edit in Google, single: the Google change is downloaded |
| R05 | #105 Google master, edit in Google, repeat: the Google change is downloaded, and the repeat is kept |
| R06 | #105 ContactManager master, repeat changed in both places since the last sync: the app's version wins, and the report says so (item 4) |
| R07 | #105 ContactManager master, repeat moved on Google: the app shows the moved start, and one repeat |
| R08 | #105 Repeat with no occurrence in the window: not downloaded, and it stays on Google once |
| R09 | #105 Repeat with an exception: the excluded occurrence is not listed, and the other five are |
| R10 | #102 A changed occurrence of a repeat, changed on Google, is downloaded as its own entry; the other occurrences are unchanged |
| R11 | #105 Repeat with no occurrence left: every occurrence cancelled on Google, the app removes the repeat, and Google holds none |
| RL05 | #94 A connection closed by Google part-way: the report says so and what to do, and a second sync finishes the uploads |
| RS01 | #105 A sync that stops part-way resumes without repeating or losing its uploads (#93) |
| T01 | Tidy Contacts puts right an older address label and a phone label; they go to Google; a second tidy changes nothing |
| T40 | #75: a weekly repeat keeps its 14:00 London time across the clocks going back |
| TC01 | #105 Two copies of ContactManager on one calendar: each sees the other's change, and nothing is duplicated |
| TK01 | #105 Token: a sync token Google refuses is replaced by a full listing, and the sync completes |
| TK02 | #105 Token: with no sync token, the listing is full and every entry in the window is downloaded |
| TK03 | #105 Token: a sync token Google answers with 410 (gone), as with 400, is replaced by a full listing, and the sync completes |
| TK04 | #105 Listing: after a full sync, the next sync lists only the changes since the last sync, and downloads no unchanged appointment |
| U02 | #105 Uploads: 40 appointments added in the app inside the window are all on Google after one sync (the 66 not uploaded) |
| W01 | #105 Window: an entry that starts before the window and ends inside it is downloaded, once |
| W02 | #105 Window: an entry that ends before the window is not downloaded, and stays on Google once |
| W03 | #105 Window: an entry beyond the far edge of the window is not downloaded, and stays on Google once |
| W04 | #105 Window: widening the window downloads the entries it now reaches, once |
| X01 | Blank and whitespace-only name fields: no crash, no duplicate |
| X02 | Very long comment (6000 characters) round-trips intact |
| X03 | Quotes, backslashes, ampersands, angle brackets, emoji and right-to-left text round-trip |
| X04 | Phone number formats (international, brackets, extensions) keep their digits |
| X05 | Same email on two different contacts: both kept, not merged |
| X06 | Six emails on Google: the app holds four, and the rest go to Comments |
| X07 | Birthdays: leap day and no-year birthday are not changed by the sync |
| X08 | Multi-line address round-trips with formatting ignored |
| X09 | Hidden, then shown again: the contact is back on Google |
| X10 | Edit then delete between syncs: the stale edit is not uploaded |
| X11 | All-day event on Google: read without crashing, not rewritten |
| X12 | Event crossing midnight with a timezone offset keeps the same instant |
| X13 | Local appointment with no end time: uploaded or refused, but no crash and no duplicate |
| X14 | Unicode summary and multi-line description round-trip |
| X15 | Event outside the sync window is not duplicated across syncs |
| X16 | Appointment deleted on both sides before sync: no error, nothing left |
| X17 | Conflict: location changed in the app and on Google; ContactManager master, so the app's change wins (item 4) |
| X18 | Five back-to-back syncs with no change: no writes after the first |
| X19 | Twenty contacts made in ContactManager master, then edits on Google under Google master |
| X20 | An address with no type is held in the first address slot |
| X81C | #81: cancelling the change of a repeat changes nothing |
| X81D | #81: deleting just one occurrence of a repeat keeps the others, and Google leaves that date out |
| X81E | #81: changing one occurrence and all following splits the repeat, and Google gets both parts |
| X81G | #81: an occurrence deleted on Google is left out of the repeat in the app, when Google is the master |
| X81H | #81: a split that moves the time keeps Google's deleted occurrence on the right time, in London time |

### Local (no Google)

| Code | Test |
|---|---|
| B78 | #78: a birthday saved on a contact shows in the calendar straight away, with birthdays shown |
| D30 | #30: the appointment form keeps a repeat's type and interval, and gives back none for an appointment that does not repeat |
| E05 | Editor: clearing the only detail of a contact saves the change |
| E06 | Editor: an older bad entry that was never edited still stops a save, and is shown with focus |
| E30 | #30: a repeat ends after a number of times, or on a date, in the list and in the form |
| F12 | #68: quitting with edits asks Save, Discard or Cancel: Save refuses a bad entry, Discard keeps the stored contact |
| K24 | #24: when the app opens, overlapping appointments are listed, and nothing is changed |
| L01 | Local: the message and appointment unit tests (autotest/archive/messages, QtTest), with no Google |
| L02 | Local: an appointment and a contact made in ContactManager are saved, and are there after a restart (no Google) |
| L03 | Local: an appointment file moved into the archive is not loaded at start-up, and start-up leaves it in the archive |
| L04 | Local: a start with no to-do or history file opens and writes its data (#100) |
| L05 | #105 Notes, plain text: a contact's to-do list and history are saved as plain files, and a restart reads them back |
| L06 | #105 Notes, encrypted: a contact's to-do list and history are saved encrypted, with no plain text on disk, and a restart reads them back |
| MG01 | Tidy Contacts merges two entries for one person when they share an address and do not conflict |
| MG02 | Tidy Contacts does not merge two entries that share an address but have different names |
| O79 | #79: a repeat that started before today, with later dates, is in the overview's future appointments |
| P30 | #30: a weekly appointment is listed on each week in the repeats window, and stored once |
| P31 | #30: an edit of a repeat changes its appointment; a move to another day moves the appointment and the focus with it |
| P33 | #73: shortening a repeat from a later entry keeps the focus on the nearest entry of that repeat |
| Q34 | #74: OK on the edit of a repeat that started 11 days ago closes the form, with no dialog |
| Q35 | #74: a start three months back is accepted, and OK then closes the form, as with a start 11 days back |
| RL01 | #94 A rate-limit answer (403 rateLimitExceeded) is retried with backoff, and the upload then completes |
| RL02 | #94 A failure that is not a rate limit (400 badRequest) is not retried, and its reason is reported |
| RL03 | #94 A rate limit that keeps coming back is given up after four retries, and the report gives the reason and a recovery step |
| RL04 | #94 Repeated rate limits start the adaptive pacing, and the report says so |
| T02 | Tidy Contacts lists an entry it cannot fix, leaves it alone, and still fixes the other entry on the contact |
| T03 | Tidy Contacts lists a title with a colon, and leaves it and its number as they are |
| T30 | #30: Goto Today selects the first entry that has not ended, including a repeat, rather than the end of the list |
| U34 | #34: double-clicking a calendar entry opens its contact, and keeps the calendar row and its focus |
| W30 | #30: a repeat whose source is older than the window is listed, and the source is not |

## 6. Test hooks and infrastructure

- **Fault injection** (`CM_TEST_FAULTS`, read in `googleaccess_network.cpp`): `r<n>=<status>:<reason>` answers the nth read
  request, and `w<n>=...` answers the nth write request, without sending it to Google. `closed` gives a closed connection.
- **Sync driver** (`autotest/archive/bench/sync_driver.cpp`): the operations used by the scenarios (adding, editing and deleting
  appointments and contacts; syncs; dumps; settings; folder listings; the calendar list; repeat operations; contact notes).
- **Scaled clock** (`scratchpad/accel/clock/fakeclock.c`): a preloaded shim that scales the real-time clock by `FAKE_SPEED`, from
  `FAKE_START`. Used by the accelerated messaging test.

## 7. Evidence

- Accelerated messaging run: `test-results/20261102-2313/`, `test-results/20261103-0140/` (run-results.csv, report-final.md, expected.json).
- Rules and decisions: `documents/messaging-rules.md`.
- Bench runs: `scratchpad/bench/runs/`, one folder per scenario, with `report1.txt`, `dump.json`, `driver.log`, `stdout.log`.
- Guide: `autotest/HEADLESS-TEST.md`.
