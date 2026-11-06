# Imported tests

The tests imported from the archive bench (`autotest/archive/`, ticket #121). Each has its own ID, so every test can be tracked. The archive's own IDs are kept for reference, but they are not unique: `L05` and `L06` are each used for two different scenarios in `google_bench.py`, so the IDs in this register are the references.

The test results are in `documents/test-log.csv`, one row per ID.

## Google sync scenarios (IG, area 1)

The scenarios of `autotest/archive/google_bench.py`, except `U01` (the capacity test, CS-001 in section 5 of the specification) and `L01` (the runner of the unit suite, which is not a test). Two scenarios, `L05` and `L06`, are accessibility tests and are listed under IA.

| ID | Area | Archive ID | Line in `google_bench.py` | Title (from the archive) |
|---|---|---|---|---|
| IG-001 | Google sync (area 1) | C01 | 512 | Add a contact locally, no sync: Google unchanged |
| IG-002 | Google sync (area 1) | C02 | 519 | Contact made on Google: downloaded once across two syncs |
| IG-003 | Google sync (area 1) | C03 | 530 | Add then delete locally before any sync: nothing reaches Google |
| IG-004 | Google sync (area 1) | C04 | 539 | Contact deleted on Google between syncs: removed from the app |
| IG-005 | Google sync (area 1) | C05 | 548 | Contact deleted in the app after it synced: deleted on Google |
| IG-006 | Google sync (area 1) | C06 | 557 | Contact hidden in the app after it synced: Google agrees with the app |
| IG-007 | Google sync (area 1) | C07 | 570 | Same name on both sides before sync: both kept |
| IG-008 | Google sync (area 1) | C08 | 582 | Edit a downloaded contact before a second sync: change reaches Google |
| IG-009 | Google sync (area 1) | C09 | 593 | Edit on Google between syncs: app picks it up without duplicating |
| IG-010 | Google sync (area 1) | C10 | 602 | Out-of-sequence phones and emails: each value kept in some slot |
| IG-011 | Google sync (area 1) | C11 | 622 | Stray spaces and newlines in Google data: no spurious upload |
| IG-012 | Google sync (area 1) | C12 | 634 | Large upload: 40 local contacts reach Google once |
| IG-013 | Google sync (area 1) | C13 | 644 | Unicode and apostrophes in names round-trip exactly |
| IG-014 | Google sync (area 1) | C14 | 652 | Email-only contact (no name) is uploaded with its email |
| IG-015 | Google sync (area 1) | C15 | 659 | Repeated syncs with no changes do not write to Google |
| IG-016 | Google sync (area 1) | C16 | 669 | Edit keeps URLs the app does not hold (option 1 regression) |
| IG-017 | Google sync (area 1) | A01 | 687 | Add an appointment locally, no sync: Google unchanged |
| IG-018 | Google sync (area 1) | A02 | 695 | Event made on Google: downloaded once across two syncs |
| IG-019 | Google sync (area 1) | A03 | 704 | Identical events on Google (duplicates): both kept, no growth on re-sync |
| IG-020 | Google sync (area 1) | A04 | 716 | Same event added locally and on Google before sync: both kept |
| IG-021 | Google sync (area 1) | A05 | 724 | Edit a downloaded event before a second sync: change reaches Google |
| IG-022 | Google sync (area 1) | A06 | 735 | Event moved on Google between syncs: app picks it up |
| IG-023 | Google sync (area 1) | A07 | 745 | Event deleted in the app after it synced: deleted on Google |
| IG-024 | Google sync (area 1) | A08 | 752 | Event deleted on Google between syncs: app does not keep a live copy |
| IG-025 | Google sync (area 1) | A09 | 760 | Overlapping appointments added locally: both uploaded, neither merged |
| IG-026 | Google sync (area 1) | A10 | 770 | Large upload: 40 local appointments reach Google once |
| IG-027 | Google sync (area 1) | A11 | 781 | Read-only event (organised by someone else): not uploaded across syncs |
| IG-028 | Google sync (area 1) | A12 | 792 | Stray spaces and newlines in a location: no spurious upload |
| IG-029 | Google sync (area 1) | A13 | 803 | Local ID of a Google event is the same across syncs |
| IG-030 | Google sync (area 1) | M01 | 854 | ContactManager master adds a contact; Google master syncs it: no duplicate |
| IG-031 | Google sync (area 1) | M02 | 865 | Google master downloads; ContactManager master edits; Google master re-syncs: one copy |
| IG-032 | Google sync (area 1) | M03 | 881 | Deleted in ContactManager master: Google master does not bring it back |
| IG-033 | Google sync (area 1) | M04 | 894 | Deleted on Google in Google master: ContactManager master does not bring it back |
| IG-034 | Google sync (area 1) | M05 | 906 | Appointment: ContactManager master adds, Google master edits, ContactManager master picks it up |
| IG-035 | Google sync (area 1) | M06 | 929 | Repeated switching with edits: values and notes survive, no duplicates |
| IG-036 | Google sync (area 1) | M07 | 951 | Google master: a local-only edit is not sent, and Google's values stay |
| IG-037 | Google sync (area 1) | M08 | 965 | ContactManager master: a change made on Google is kept, not overwritten |
| IG-038 | Google sync (area 1) | X01 | 991 | Blank and whitespace-only name fields: no crash, no duplicate |
| IG-039 | Google sync (area 1) | X02 | 1002 | Very long comment (6000 characters) round-trips intact |
| IG-040 | Google sync (area 1) | X03 | 1016 | Quotes, backslashes, ampersands, angle brackets, emoji and right-to-left text round-trip |
| IG-041 | Google sync (area 1) | X04 | 1028 | Phone number formats (international, brackets, extensions) keep their digits |
| IG-042 | Google sync (area 1) | X05 | 1046 | Same email on two different contacts: both kept, not merged |
| IG-043 | Google sync (area 1) | X06 | 1054 | Six emails on Google: the app holds four, and the rest go to Comments |
| IG-044 | Google sync (area 1) | X07 | 1070 | Birthdays: leap day and no-year birthday are not changed by the sync |
| IG-045 | Google sync (area 1) | X08 | 1084 | Multi-line address round-trips with formatting ignored |
| IG-046 | Google sync (area 1) | X09 | 1096 | Hidden, then shown again: the contact is back on Google |
| IG-047 | Google sync (area 1) | X10 | 1109 | Edit then delete between syncs: the stale edit is not uploaded |
| IG-048 | Google sync (area 1) | X11 | 1121 | All-day event on Google: read without crashing, not rewritten |
| IG-049 | Google sync (area 1) | X12 | 1132 | Event crossing midnight with a timezone offset keeps the same instant |
| IG-050 | Google sync (area 1) | X13 | 1142 | Local appointment with no end time: uploaded or refused, but no crash and no duplicate |
| IG-051 | Google sync (area 1) | X14 | 1167 | Unicode summary and multi-line description round-trip |
| IG-052 | Google sync (area 1) | X15 | 1180 | Event outside the sync window is not duplicated across syncs |
| IG-053 | Google sync (area 1) | X16 | 1188 | Appointment deleted on both sides before sync: no error, nothing left |
| IG-054 | Google sync (area 1) | X17 | 1197 | Conflict: location changed in the app and on Google; ContactManager master, so the app's change wins (item 4) |
| IG-055 | Google sync (area 1) | X18 | 1211 | Five back-to-back syncs with no change: no writes after the first |
| IG-056 | Google sync (area 1) | X19 | 1220 | Twenty contacts made in ContactManager master, then edits on Google under Google master |
| IG-057 | Google sync (area 1) | LB01 | 1251 | Four emails from Google (home, other, work, Partner): each keeps its slot and label, no write |
| IG-058 | Google sync (area 1) | LB02 | 1272 | A label changed in the app is sent to Google as that email's type |
| IG-059 | Google sync (area 1) | LB03 | 1290 | Emails with no labels set are sent with the labels they always had |
| IG-060 | Google sync (area 1) | X20 | 1304 | An address with no type is held in the first address slot |
| IG-061 | Google sync (area 1) | P01 | 1325 | Four phones from Google (home, work, mobile, Pager): each keeps its slot and label, no write |
| IG-062 | Google sync (area 1) | P02 | 1346 | A phone label changed in the app is sent to Google as that number's type |
| IG-063 | Google sync (area 1) | P03 | 1365 | Phones with no labels set are sent with the labels they always had |
| IG-064 | Google sync (area 1) | P04 | 1379 | Seven phones on Google: the app holds six, the seventh goes to Comments |
| IG-065 | Google sync (area 1) | D01 | 1399 | Clearing an email and a phone in the app removes them from Google |
| IG-066 | Google sync (area 1) | ED01 | 1436 | Editor: a label typed after an address becomes its title, and Google gets it as that type |
| IG-067 | Google sync (area 1) | ED02 | 1453 | Editor: a save with a bad entry is refused, focus goes to the first, nothing is saved, and the typed text is kept |
| IG-068 | Google sync (area 1) | E06 | 1487 | Editor: an older bad entry that was never edited still stops a save, and is shown with focus |
| IG-069 | Google sync (area 1) | F01 | 1507 | More emails on Google than the app has slots: all of them are still on Google after an upload |
| IG-070 | Google sync (area 1) | F02 | 1527 | Google master: a bad entry downloaded from Google stops a save of the form, and nothing is uploaded |
| IG-071 | Google sync (area 1) | F03 | 1553 | More phones on Google than the app has slots: all of them are still on Google after an upload |
| IG-072 | Google sync (area 1) | F04 | 1577 | A save refused for a bad entry stops the next sync, so nothing is sent to Google |
| IG-073 | Google sync (area 1) | MG01 | 1597 | Tidy Contacts merges two entries for one person when they share an address and do not conflict |
| IG-074 | Google sync (area 1) | MG02 | 1616 | Tidy Contacts does not merge two entries that share an address but have different names |
| IG-075 | Google sync (area 1) | F05 | 1630 | #66: an unchanged bad entry does not stop switching to another contact |
| IG-076 | Google sync (area 1) | F06 | 1647 | #66: with a contact holding an unchanged bad entry open, a Google sync still runs |
| IG-077 | Google sync (area 1) | F07 | 1658 | #66: a contact kept with an unfixable entry is uploaded once, then not again on later syncs |
| IG-078 | Google sync (area 1) | F08 | 1676 | A contact with a clean address and no titles set: uploaded once, then not again on later syncs |
| IG-079 | Google sync (area 1) | F09 | 1689 | #66: moving the selection on, as Tidy Contacts does, does not check the open contact's entries |
| IG-080 | Google sync (area 1) | F10 | 1702 | #69: the report's steps: a bad entry from Google, Alt-E,D, Ctrl-S, Exit, relaunch, Tidy Contacts, Alt-E,D, Exit |
| IG-081 | Google sync (area 1) | F12 | 1748 | #68: quitting with edits asks Save, Discard or Cancel: Save refuses a bad entry, Discard keeps the stored contact |
| IG-082 | Google sync (area 1) | F13 | 1779 | #68: switching contact, Tidy Contacts and sync ask Save or Discard when the contact has edits |
| IG-083 | Google sync (area 1) | ED03 | 1820 | Editor: an older entry with its label in the address is tidied when opened, and saved with the next change |
| IG-084 | Google sync (area 1) | E04 | 1837 | Editor: a phone typed with (0) after the country code and a label after the number is saved tidily |
| IG-085 | Google sync (area 1) | E05 | 1856 | Editor: clearing the only detail of a contact saves the change |
| IG-086 | Google sync (area 1) | T01 | 1869 | Tidy Contacts puts right an older address label and a phone label; they go to Google; a second tidy changes nothing |
| IG-087 | Google sync (area 1) | T02 | 1896 | Tidy Contacts lists an entry it cannot fix, leaves it alone, and still fixes the other entry on the contact |
| IG-088 | Google sync (area 1) | T03 | 1914 | Tidy Contacts lists a title with a colon, and leaves it and its number as they are |
| IG-089 | Google sync (area 1) | U34 | 2027 | #34: double-clicking a calendar entry opens its contact, and keeps the calendar row and its focus |
| IG-090 | Google sync (area 1) | K24 | 2045 | #24: when the app opens, overlapping appointments are listed, and nothing is changed |
| IG-091 | Google sync (area 1) | P30 | 2075 | #30: a weekly appointment is listed on each week in the repeats window, and stored once |
| IG-092 | Google sync (area 1) | P31 | 2101 | #30: an edit of a repeat changes its appointment; a move to another day moves the appointment and the focus with it |
| IG-093 | Google sync (area 1) | D30 | 2134 | #30: the appointment form keeps a repeat's type and interval, and gives back none for an appointment that does not repeat |
| IG-094 | Google sync (area 1) | G40 | 2163 | #30: a repeating Google event is stored once, with its repeat, and listed on each week |
| IG-095 | Google sync (area 1) | T40 | 2177 | #75: a weekly repeat keeps its 14:00 London time across the clocks going back |
| IG-096 | Google sync (area 1) | B78 | 2205 | #78: a birthday saved on a contact shows in the calendar straight away, with birthdays shown |
| IG-097 | Google sync (area 1) | O79 | 2225 | #79: a repeat that started before today, with later dates, is in the overview's future appointments |
| IG-098 | Google sync (area 1) | X81D | 2257 | #81: deleting just one occurrence of a repeat keeps the others, and Google leaves that date out |
| IG-099 | Google sync (area 1) | X81E | 2283 | #81: changing one occurrence and all following splits the repeat, and Google gets both parts |
| IG-100 | Google sync (area 1) | X81C | 2307 | #81: cancelling the change of a repeat changes nothing |
| IG-101 | Google sync (area 1) | X81G | 2325 | #81: an occurrence deleted on Google is left out of the repeat in the app, when Google is the master |
| IG-102 | Google sync (area 1) | X81H | 2351 | #81: a split that moves the time keeps Google's deleted occurrence on the right time, in London time |
| IG-103 | Google sync (area 1) | G41 | 2383 | #30: an edit of a repeat in the app changes the Google event, and the event stays repeating |
| IG-104 | Google sync (area 1) | W30 | 2403 | #30: a repeat whose source is older than the window is listed, and the source is not |
| IG-105 | Google sync (area 1) | G42 | 2420 | #30: a repeating Google event whose first occurrence is older than the window still lists its repeats |
| IG-106 | Google sync (area 1) | E30 | 2433 | #30: a repeat ends after a number of times, or on a date, in the list and in the form |
| IG-107 | Google sync (area 1) | G43 | 2462 | #30: a Google repeat with a count is stored once and listed that many times, and an app edit keeps the count |
| IG-108 | Google sync (area 1) | T30 | 2482 | #30: Goto Today selects the first entry that has not ended, including a repeat, rather than the end of the list |
| IG-109 | Google sync (area 1) | G44 | 2497 | #30: an appointment on Google older than the one-year lookback is not downloaded |
| IG-110 | Google sync (area 1) | P33 | 2506 | #73: shortening a repeat from a later entry keeps the focus on the nearest entry of that repeat |
| IG-111 | Google sync (area 1) | Q34 | 2527 | #74: OK on the edit of a repeat that started 11 days ago closes the form, with no dialog |
| IG-112 | Google sync (area 1) | Q35 | 2542 | #74: a start three months back is accepted, and OK then closes the form, as with a start 11 days back |
| IG-113 | Google sync (area 1) | E01 | 2616 | #86: an ETag is stored from Google and sent with a change: a local edit reaches Google, with no conflict |
| IG-114 | Google sync (area 1) | E02 | 2635 | #86: Google's copy changed in its details since the last sync (non-matching ETag) and the app changed it too, ContactManager master: the app's version is sent, and the conflict is reported (item 4) |
| IG-115 | Google sync (area 1) | E03 | 2653 | #86: Google's ETag changed, but not its details (a field the app does not hold): the change is sent again with the new ETag, and reaches Google |
| IG-116 | Google sync (area 1) | C103 | 2675 | #103: the contact on the form stays loaded through a sync, in ContactManager master and in Google master, and the calendar entry does not move |
| IG-117 | Google sync (area 1) | N107 | 2712 | #107: an appointment with no end time, or one ending on 1 Jan 1970, is not uploaded; the sync completes and the others reach Google |
| IG-118 | Google sync (area 1) | N108 | 2734 | #108: with the last calendar sync back at the epoch (as after a failed sync), the next sync sends only the appointment that was edited, not the ones it downloaded |
| IG-119 | Google sync (area 1) | R01 | 2775 | #105 ContactManager master, edit in Google, repeat: the Google change reaches the app, one repeat on Google |
| IG-120 | Google sync (area 1) | R02 | 2792 | #105 Google master, edit in ContactManager, single: the next sync takes Google's location, and nothing is uploaded |
| IG-121 | Google sync (area 1) | R03 | 2807 | #105 Google master, edit in ContactManager, repeat: the next sync keeps the repeat and Google's location |
| IG-122 | Google sync (area 1) | R04 | 2825 | #105 Google master, edit in Google, single: the Google change is downloaded |
| IG-123 | Google sync (area 1) | R05 | 2838 | #105 Google master, edit in Google, repeat: the Google change is downloaded, and the repeat is kept |
| IG-124 | Google sync (area 1) | AR01 | 2867 | #105 Archive: an entry that the window leaves behind is archived; moved into the window on Google, it is restored and leaves the archive |
| IG-125 | Google sync (area 1) | U02 | 3030 | #105 Uploads: 40 appointments added in the app inside the window are all on Google after one sync (the 66 not uploaded) |
| IG-126 | Google sync (area 1) | L02 | 3163 | Local: an appointment and a contact made in ContactManager are saved, and are there after a restart (no Google) |
| IG-127 | Google sync (area 1) | L03 | 3176 | Local: an appointment file moved into the archive is not loaded at start-up, and start-up leaves it in the archive |
| IG-128 | Google sync (area 1) | L04 | 3197 | Local: a start with no to-do or history file opens and writes its data (#100) |
| IG-129 | Google sync (area 1) | L05 | 3254 | #105 Notes, plain text: a contact's to-do list and history are saved as plain files, and a restart reads them back |
| IG-130 | Google sync (area 1) | L06 | 3259 | #105 Notes, encrypted: a contact's to-do list and history are saved encrypted, with no plain text on disk, and a restart reads them back |
| IG-131 | Google sync (area 1) | W01 | 3267 | #105 Window: an entry that starts before the window and ends inside it is downloaded, once |
| IG-132 | Google sync (area 1) | W02 | 3279 | #105 Window: an entry that ends before the window is not downloaded, and stays on Google once |
| IG-133 | Google sync (area 1) | W03 | 3290 | #105 Window: an entry beyond the far edge of the window is not downloaded, and stays on Google once |
| IG-134 | Google sync (area 1) | W04 | 3301 | #105 Window: widening the window downloads the entries it now reaches, once |
| IG-135 | Google sync (area 1) | TK01 | 3315 | #105 Token: a sync token Google refuses is replaced by a full listing, and the sync completes |
| IG-136 | Google sync (area 1) | TK03 | 3330 | #105 Token: a sync token Google answers with 410 (gone), as with 400, is replaced by a full listing, and the sync completes |
| IG-137 | Google sync (area 1) | TK04 | 3349 | #105 Listing: after a full sync, the next sync lists only the changes since the last sync, and downloads no unchanged appointment |
| IG-138 | Google sync (area 1) | TK02 | 3361 | #105 Token: with no sync token, the listing is full and every entry in the window is downloaded |
| IG-139 | Google sync (area 1) | R06 | 3373 | #105 ContactManager master, repeat changed in both places since the last sync: the app's version wins, and the report says so (item 4) |
| IG-140 | Google sync (area 1) | R07 | 3396 | #105 ContactManager master, repeat moved on Google: the app shows the moved start, and one repeat |
| IG-141 | Google sync (area 1) | R08 | 3412 | #105 Repeat with no occurrence in the window: not downloaded, and it stays on Google once |
| IG-142 | Google sync (area 1) | R09 | 3423 | #105 Repeat with an exception: the excluded occurrence is not listed, and the other five are |
| IG-143 | Google sync (area 1) | TC01 | 3441 | #105 Two copies of ContactManager on one calendar: each sees the other's change, and nothing is duplicated |
| IG-144 | Google sync (area 1) | RS01 | 3457 | #105 A sync that stops part-way resumes without repeating or losing its uploads (#93) |
| IG-145 | Google sync (area 1) | AR02 | 3474 | #105 Archive: an archived entry changed on Google, still outside the window, has its archived copy brought up to date |
| IG-146 | Google sync (area 1) | AR03 | 3493 | #105 Archive: an archived entry cancelled on Google leaves the archive, and is kept in the deleted folder |
| IG-147 | Google sync (area 1) | E70 | 3511 | #104 A local entry dated 1 January 1970, or with no start, is neither stored nor uploaded; a normal entry alongside is unaffected |
| IG-148 | Google sync (area 1) | RL01 | 3574 | #94 A rate-limit answer (403 rateLimitExceeded) is retried with backoff, and the upload then completes |
| IG-149 | Google sync (area 1) | RL02 | 3586 | #94 A failure that is not a rate limit (400 badRequest) is not retried, and its reason is reported |
| IG-150 | Google sync (area 1) | RL03 | 3595 | #94 A rate limit that keeps coming back is given up after four retries, and the report gives the reason and a recovery step |
| IG-151 | Google sync (area 1) | RL04 | 3608 | #94 Repeated rate limits start the adaptive pacing, and the report says so |
| IG-152 | Google sync (area 1) | RL05 | 3619 | #94 A connection closed by Google part-way: the report says so and what to do, and a second sync finishes the uploads |
| IG-153 | Google sync (area 1) | C17 | 3640 | #105 Contact changed in both places since the last sync, ContactManager master: the app's change is kept, once |
| IG-154 | Google sync (area 1) | C18 | 3654 | #105 Contact changed in both places since the last sync, Google master: Google's change is kept, once |
| IG-155 | Google sync (area 1) | C19 | 3668 | #105 Contact deleted on Google and changed in the app, ContactManager master: created on Google again with the app's change (item 4) |
| IG-156 | Google sync (area 1) | C20 | 3682 | #105 Contact deleted in the app and changed on Google, ContactManager master: the app's delete wins, so Google deletes it too (item 4) |
| IG-157 | Google sync (area 1) | LF01 | 3695 | #105 Local files: after two syncs the calendar folder holds each appointment once, and no file is empty |
| IG-158 | Google sync (area 1) | LF02 | 3723 | #105 Local files: a deleted appointment moves to the deleted folder, and is not left in the calendar folder |
| IG-159 | Google sync (area 1) | A14 | 3739 | #105 Appointment deleted in the app and changed on Google, ContactManager master: the app's delete wins, so Google deletes it too (item 4) |
| IG-160 | Google sync (area 1) | A15 | 3751 | #105 Appointment deleted on Google and changed in the app, ContactManager master: created on Google again with the app's change (item 4) |
| IG-161 | Google sync (area 1) | R11 | 3813 | #105 Repeat with no occurrence left: every occurrence cancelled on Google, the app removes the repeat, and Google holds none |
| IG-162 | Google sync (area 1) | R10 | 3829 | #102 A changed occurrence of a repeat, changed on Google, is downloaded as its own entry; the other occurrences are unchanged |

## Accessibility scenarios (IA, area 4)

| ID | Area | Archive ID | Line in `google_bench.py` | Title (from the archive) |
|---|---|---|---|---|
| IA-001 | Accessibility (area 4) | L05 | 2577 | #101: Show Screen Reader Text: shows the name, role, value and description of the focused control, and says it is an approximation |
| IA-002 | Accessibility (area 4) | L06 | 2590 | #101: the accessibility debug checkbox in the screen reader text dialog turns console tracing on and off |

## Old-file test (OF, area 3)

| ID | Area | Archive file | Line | Title |
|---|---|---|---|---|
| OF-001 | Operation testing (area 3) | old_contact_test.cpp | - | Loads a contact file written before the email and phone labels existed, and checks that it keeps its meaning |

## Not yet identified

The estimate (`documents/test-duration-estimate.md`, section 1) counts 31 imported operation scenarios (SS 6, MF 13, DL 12). No archive file defines them, so they have no IDs here and are not in the test log. They need to be identified before they can be numbered.

