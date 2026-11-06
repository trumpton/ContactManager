# Imported tests

The tests imported from the archive bench (`autotest/archive/`, ticket #121). Each has its own ID, so every test can be tracked. The archive's own IDs are kept for reference, but they are not unique: `L05` and `L06` are each used for two different scenarios in `google_bench.py`, so the IDs in this register are the references.

The test results are in `documents/test-log.csv`, one row per ID.

How the imports are split: a scenario is an operation test (`IO`) if its body calls neither Google nor the sync, as section 3 of the specification defines operation tests. Every other scenario is a Google sync test (`IG`), including those that call the sync to check a local result. `U01` is the capacity test (CS-001, section 5), and `L01` is the runner of the unit suite, not a test; both are left out.

## Google sync scenarios (IG, area 1)

| ID | Archive ID | Line in `google_bench.py` | Title (from the archive)  | Status |
|---|---|---|---|---|
| IG-001 | C01 | 512 | Add a contact locally, no sync: Google unchanged | To port |
| IG-002 | C02 | 519 | Contact made on Google: downloaded once across two syncs | Covered by GS-002 |
| IG-003 | C03 | 530 | Add then delete locally before any sync: nothing reaches Google | To port |
| IG-004 | C04 | 539 | Contact deleted on Google between syncs: removed from the app | Covered by GS-010 |
| IG-005 | C05 | 548 | Contact deleted in the app after it synced: deleted on Google | Covered by GS-008 |
| IG-006 | C06 | 557 | Contact hidden in the app after it synced: Google agrees with the app | Covered by GS-011 |
| IG-007 | C07 | 570 | Same name on both sides before sync: both kept | To port |
| IG-008 | C08 | 582 | Edit a downloaded contact before a second sync: change reaches Google | Covered by GS-003 |
| IG-009 | C09 | 593 | Edit on Google between syncs: app picks it up without duplicating | Covered by GS-003 |
| IG-010 | C10 | 602 | Out-of-sequence phones and emails: each value kept in some slot | To port |
| IG-011 | C11 | 622 | Stray spaces and newlines in Google data: no spurious upload | To port |
| IG-012 | C12 | 634 | Large upload: 40 local contacts reach Google once | To port |
| IG-013 | C13 | 644 | Unicode and apostrophes in names round-trip exactly | To port |
| IG-014 | C14 | 652 | Email-only contact (no name) is uploaded with its email | To port |
| IG-015 | C15 | 659 | Repeated syncs with no changes do not write to Google | To port |
| IG-016 | C16 | 669 | Edit keeps URLs the app does not hold (option 1 regression) | To port |
| IG-017 | A01 | 687 | Add an appointment locally, no sync: Google unchanged | To port |
| IG-018 | A02 | 695 | Event made on Google: downloaded once across two syncs | Covered by GS-016 |
| IG-019 | A03 | 704 | Identical events on Google (duplicates): both kept, no growth on re-sync | To port |
| IG-020 | A04 | 716 | Same event added locally and on Google before sync: both kept | To port |
| IG-021 | A05 | 724 | Edit a downloaded event before a second sync: change reaches Google | Covered by GS-017 |
| IG-022 | A06 | 735 | Event moved on Google between syncs: app picks it up | Covered by GS-017 |
| IG-023 | A07 | 745 | Event deleted in the app after it synced: deleted on Google | Covered by GS-017 |
| IG-024 | A08 | 752 | Event deleted on Google between syncs: app does not keep a live copy | Covered by GS-017 |
| IG-025 | A09 | 760 | Overlapping appointments added locally: both uploaded, neither merged | Covered by GS-030 |
| IG-026 | A10 | 770 | Large upload: 40 local appointments reach Google once | To port |
| IG-027 | A11 | 781 | Read-only event (organised by someone else): not uploaded across syncs | Covered by GS-025 |
| IG-028 | A12 | 792 | Stray spaces and newlines in a location: no spurious upload | To port |
| IG-029 | A13 | 803 | Local ID of a Google event is the same across syncs | To port |
| IG-030 | M01 | 854 | ContactManager master adds a contact; Google master syncs it: no duplicate | Covered by GS-040 |
| IG-031 | M02 | 865 | Google master downloads; ContactManager master edits; Google master re-syncs: one copy | Covered by GS-040 |
| IG-032 | M03 | 881 | Deleted in ContactManager master: Google master does not bring it back | Covered by GS-009 |
| IG-033 | M04 | 894 | Deleted on Google in Google master: ContactManager master does not bring it back | Covered by GS-010 |
| IG-034 | M05 | 906 | Appointment: ContactManager master adds, Google master edits, ContactManager master picks it up | Covered by GS-040 |
| IG-035 | M06 | 929 | Repeated switching with edits: values and notes survive, no duplicates | Covered by GS-040 |
| IG-036 | M07 | 951 | Google master: a local-only edit is not sent, and Google's values stay | Covered by GS-007 |
| IG-037 | M08 | 965 | ContactManager master: a change made on Google is kept, not overwritten | Covered by GS-005 |
| IG-038 | X01 | 991 | Blank and whitespace-only name fields: no crash, no duplicate | To port |
| IG-039 | X02 | 1002 | Very long comment (6000 characters) round-trips intact | To port |
| IG-040 | X03 | 1016 | Quotes, backslashes, ampersands, angle brackets, emoji and right-to-left text round-trip | To port |
| IG-041 | X04 | 1028 | Phone number formats (international, brackets, extensions) keep their digits | To port |
| IG-042 | X05 | 1046 | Same email on two different contacts: both kept, not merged | To port |
| IG-043 | X06 | 1054 | Six emails on Google: the app holds four, and the rest go to Comments | To port |
| IG-044 | X07 | 1070 | Birthdays: leap day and no-year birthday are not changed by the sync | To port |
| IG-045 | X08 | 1084 | Multi-line address round-trips with formatting ignored | To port |
| IG-046 | X09 | 1096 | Hidden, then shown again: the contact is back on Google | To port |
| IG-047 | X10 | 1109 | Edit then delete between syncs: the stale edit is not uploaded | To port |
| IG-048 | X11 | 1121 | All-day event on Google: read without crashing, not rewritten | To port |
| IG-049 | X12 | 1132 | Event crossing midnight with a timezone offset keeps the same instant | To port |
| IG-050 | X13 | 1142 | Local appointment with no end time: uploaded or refused, but no crash and no duplicate | Covered by GS-025 |
| IG-051 | X14 | 1167 | Unicode summary and multi-line description round-trip | To port |
| IG-052 | X15 | 1180 | Event outside the sync window is not duplicated across syncs | Covered by GS-023 |
| IG-053 | X16 | 1188 | Appointment deleted on both sides before sync: no error, nothing left | To port |
| IG-054 | X17 | 1197 | Conflict: location changed in the app and on Google; ContactManager master, so the app's change wins (item 4) | Covered by GS-005 |
| IG-055 | X18 | 1211 | Five back-to-back syncs with no change: no writes after the first | To port |
| IG-056 | X19 | 1220 | Twenty contacts made in ContactManager master, then edits on Google under Google master | To port |
| IG-057 | LB01 | 1251 | Four emails from Google (home, other, work, Partner): each keeps its slot and label, no write | To port |
| IG-058 | LB02 | 1272 | A label changed in the app is sent to Google as that email's type | To port |
| IG-059 | LB03 | 1290 | Emails with no labels set are sent with the labels they always had | To port |
| IG-060 | X20 | 1304 | An address with no type is held in the first address slot | To port |
| IG-061 | P01 | 1325 | Four phones from Google (home, work, mobile, Pager): each keeps its slot and label, no write | To port |
| IG-062 | P02 | 1346 | A phone label changed in the app is sent to Google as that number's type | To port |
| IG-063 | P03 | 1365 | Phones with no labels set are sent with the labels they always had | To port |
| IG-064 | P04 | 1379 | Seven phones on Google: the app holds six, the seventh goes to Comments | To port |
| IG-065 | D01 | 1399 | Clearing an email and a phone in the app removes them from Google | To port |
| IG-066 | ED01 | 1436 | Editor: a label typed after an address becomes its title, and Google gets it as that type | Covered by DL-015 |
| IG-067 | ED02 | 1453 | Editor: a save with a bad entry is refused, focus goes to the first, nothing is saved, and the typed text is kept | Covered by DL-014 |
| IG-068 | F01 | 1507 | More emails on Google than the app has slots: all of them are still on Google after an upload | To port |
| IG-069 | F02 | 1527 | Google master: a bad entry downloaded from Google stops a save of the form, and nothing is uploaded | To port |
| IG-070 | F03 | 1553 | More phones on Google than the app has slots: all of them are still on Google after an upload | To port |
| IG-071 | F04 | 1577 | A save refused for a bad entry stops the next sync, so nothing is sent to Google | To port |
| IG-072 | F05 | 1630 | #66: an unchanged bad entry does not stop switching to another contact | To port |
| IG-073 | F06 | 1647 | #66: with a contact holding an unchanged bad entry open, a Google sync still runs | To port |
| IG-074 | F07 | 1658 | #66: a contact kept with an unfixable entry is uploaded once, then not again on later syncs | To port |
| IG-075 | F08 | 1676 | A contact with a clean address and no titles set: uploaded once, then not again on later syncs | To port |
| IG-076 | F09 | 1689 | #66: moving the selection on, as Tidy Contacts does, does not check the open contact's entries | To port |
| IG-077 | F10 | 1702 | #69: the report's steps: a bad entry from Google, Alt-E,D, Ctrl-S, Exit, relaunch, Tidy Contacts, Alt-E,D, Exit | To port |
| IG-078 | F13 | 1779 | #68: switching contact, Tidy Contacts and sync ask Save or Discard when the contact has edits | Covered by DL-019 |
| IG-079 | ED03 | 1820 | Editor: an older entry with its label in the address is tidied when opened, and saved with the next change | To port |
| IG-080 | E04 | 1837 | Editor: a phone typed with (0) after the country code and a label after the number is saved tidily | Covered by DL-016 |
| IG-081 | T01 | 1869 | Tidy Contacts puts right an older address label and a phone label; they go to Google; a second tidy changes nothing | To port |
| IG-082 | T03 | 1914 | Tidy Contacts lists a title with a colon, and leaves it and its number as they are | To port |
| IG-083 | D30 | 2134 | #30: the appointment form keeps a repeat's type and interval, and gives back none for an appointment that does not repeat | Covered by DL-012 |
| IG-084 | G40 | 2163 | #30: a repeating Google event is stored once, with its repeat, and listed on each week | To port |
| IG-085 | T40 | 2177 | #75: a weekly repeat keeps its 14:00 London time across the clocks going back | Covered by GS-028 |
| IG-086 | O79 | 2225 | #79: a repeat that started before today, with later dates, is in the overview's future appointments | To port |
| IG-087 | X81D | 2257 | #81: deleting just one occurrence of a repeat keeps the others, and Google leaves that date out | Covered by GS-021 |
| IG-088 | X81E | 2283 | #81: changing one occurrence and all following splits the repeat, and Google gets both parts | Covered by GS-022 |
| IG-089 | X81C | 2307 | #81: cancelling the change of a repeat changes nothing | To port |
| IG-090 | X81G | 2325 | #81: an occurrence deleted on Google is left out of the repeat in the app, when Google is the master | To port |
| IG-091 | X81H | 2351 | #81: a split that moves the time keeps Google's deleted occurrence on the right time, in London time | To port |
| IG-092 | G41 | 2383 | #30: an edit of a repeat in the app changes the Google event, and the event stays repeating | To port |
| IG-093 | G42 | 2420 | #30: a repeating Google event whose first occurrence is older than the window still lists its repeats | To port |
| IG-094 | G43 | 2462 | #30: a Google repeat with a count is stored once and listed that many times, and an app edit keeps the count | To port |
| IG-095 | G44 | 2497 | #30: an appointment on Google older than the one-year lookback is not downloaded | To port |
| IG-096 | Q35 | 2542 | #74: a start three months back is accepted, and OK then closes the form, as with a start 11 days back | To port |
| IG-097 | E01 | 2616 | #86: an ETag is stored from Google and sent with a change: a local edit reaches Google, with no conflict | To port |
| IG-098 | E02 | 2635 | #86: Google's copy changed in its details since the last sync (non-matching ETag) and the app changed it too, ContactManager master: the app's version is sent, and the conflict is reported (item 4) | To port |
| IG-099 | E03 | 2653 | #86: Google's ETag changed, but not its details (a field the app does not hold): the change is sent again with the new ETag, and reaches Google | To port |
| IG-100 | C103 | 2675 | #103: the contact on the form stays loaded through a sync, in ContactManager master and in Google master, and the calendar entry does not move | To port |
| IG-101 | N107 | 2712 | #107: an appointment with no end time, or one ending on 1 Jan 1970, is not uploaded; the sync completes and the others reach Google | Covered by GS-025 |
| IG-102 | N108 | 2734 | #108: with the last calendar sync back at the epoch (as after a failed sync), the next sync sends only the appointment that was edited, not the ones it downloaded | To port |
| IG-103 | R01 | 2775 | #105 ContactManager master, edit in Google, repeat: the Google change reaches the app, one repeat on Google | To port |
| IG-104 | R02 | 2792 | #105 Google master, edit in ContactManager, single: the next sync takes Google's location, and nothing is uploaded | To port |
| IG-105 | R03 | 2807 | #105 Google master, edit in ContactManager, repeat: the next sync keeps the repeat and Google's location | To port |
| IG-106 | R04 | 2825 | #105 Google master, edit in Google, single: the Google change is downloaded | To port |
| IG-107 | R05 | 2838 | #105 Google master, edit in Google, repeat: the Google change is downloaded, and the repeat is kept | To port |
| IG-108 | AR01 | 2867 | #105 Archive: an entry that the window leaves behind is archived; moved into the window on Google, it is restored and leaves the archive | Covered by GS-023 |
| IG-109 | U02 | 3030 | #105 Uploads: 40 appointments added in the app inside the window are all on Google after one sync (the 66 not uploaded) | To port |
| IG-110 | L06 | 3259 | #105 Notes, encrypted: a contact's to-do list and history are saved encrypted, with no plain text on disk, and a restart reads them back | Covered by GS-013 |
| IG-111 | W01 | 3267 | #105 Window: an entry that starts before the window and ends inside it is downloaded, once | To port |
| IG-112 | W02 | 3279 | #105 Window: an entry that ends before the window is not downloaded, and stays on Google once | Covered by GS-023 |
| IG-113 | W03 | 3290 | #105 Window: an entry beyond the far edge of the window is not downloaded, and stays on Google once | Covered by GS-038 |
| IG-114 | W04 | 3301 | #105 Window: widening the window downloads the entries it now reaches, once | To port |
| IG-115 | TK01 | 3315 | #105 Token: a sync token Google refuses is replaced by a full listing, and the sync completes | To port |
| IG-116 | TK03 | 3330 | #105 Token: a sync token Google answers with 410 (gone), as with 400, is replaced by a full listing, and the sync completes | To port |
| IG-117 | TK04 | 3349 | #105 Listing: after a full sync, the next sync lists only the changes since the last sync, and downloads no unchanged appointment | To port |
| IG-118 | TK02 | 3361 | #105 Token: with no sync token, the listing is full and every entry in the window is downloaded | To port |
| IG-119 | R06 | 3373 | #105 ContactManager master, repeat changed in both places since the last sync: the app's version wins, and the report says so (item 4) | To port |
| IG-120 | R07 | 3396 | #105 ContactManager master, repeat moved on Google: the app shows the moved start, and one repeat | To port |
| IG-121 | R08 | 3412 | #105 Repeat with no occurrence in the window: not downloaded, and it stays on Google once | To port |
| IG-122 | R09 | 3423 | #105 Repeat with an exception: the excluded occurrence is not listed, and the other five are | To port |
| IG-123 | TC01 | 3441 | #105 Two copies of ContactManager on one calendar: each sees the other's change, and nothing is duplicated | Covered by GS-039 |
| IG-124 | RS01 | 3457 | #105 A sync that stops part-way resumes without repeating or losing its uploads (#93) | Covered by GS-035 |
| IG-125 | AR02 | 3474 | #105 Archive: an archived entry changed on Google, still outside the window, has its archived copy brought up to date | To port |
| IG-126 | AR03 | 3493 | #105 Archive: an archived entry cancelled on Google leaves the archive, and is kept in the deleted folder | To port |
| IG-127 | E70 | 3511 | #104 A local entry dated 1 January 1970, or with no start, is neither stored nor uploaded; a normal entry alongside is unaffected | To port |
| IG-128 | RL01 | 3574 | #94 A rate-limit answer (403 rateLimitExceeded) is retried with backoff, and the upload then completes | Covered by GS-033 |
| IG-129 | RL02 | 3586 | #94 A failure that is not a rate limit (400 badRequest) is not retried, and its reason is reported | To port |
| IG-130 | RL03 | 3595 | #94 A rate limit that keeps coming back is given up after four retries, and the report gives the reason and a recovery step | To port |
| IG-131 | RL04 | 3608 | #94 Repeated rate limits start the adaptive pacing, and the report says so | To port |
| IG-132 | RL05 | 3619 | #94 A connection closed by Google part-way: the report says so and what to do, and a second sync finishes the uploads | To port |
| IG-133 | C17 | 3640 | #105 Contact changed in both places since the last sync, ContactManager master: the app's change is kept, once | Covered by GS-005 |
| IG-134 | C18 | 3654 | #105 Contact changed in both places since the last sync, Google master: Google's change is kept, once | Covered by GS-005 |
| IG-135 | C19 | 3668 | #105 Contact deleted on Google and changed in the app, ContactManager master: created on Google again with the app's change (item 4) | To port |
| IG-136 | C20 | 3682 | #105 Contact deleted in the app and changed on Google, ContactManager master: the app's delete wins, so Google deletes it too (item 4) | To port |
| IG-137 | LF01 | 3695 | #105 Local files: after two syncs the calendar folder holds each appointment once, and no file is empty | To port |
| IG-138 | LF02 | 3723 | #105 Local files: a deleted appointment moves to the deleted folder, and is not left in the calendar folder | To port |
| IG-139 | A14 | 3739 | #105 Appointment deleted in the app and changed on Google, ContactManager master: the app's delete wins, so Google deletes it too (item 4) | To port |
| IG-140 | A15 | 3751 | #105 Appointment deleted on Google and changed in the app, ContactManager master: created on Google again with the app's change (item 4) | To port |
| IG-141 | R11 | 3813 | #105 Repeat with no occurrence left: every occurrence cancelled on Google, the app removes the repeat, and Google holds none | To port |
| IG-142 | R10 | 3829 | #102 A changed occurrence of a repeat, changed on Google, is downloaded as its own entry; the other occurrences are unchanged | To port |

## Operation scenarios (IO, area 3)

| ID | Archive ID | Line in `google_bench.py` | Title (from the archive)  | Status |
|---|---|---|---|---|
| IO-001 | E06 | 1487 | Editor: an older bad entry that was never edited still stops a save, and is shown with focus | Covered by DL-014 |
| IO-002 | MG01 | 1597 | Tidy Contacts merges two entries for one person when they share an address and do not conflict | Ported to the operation bench, not run |
| IO-003 | MG02 | 1616 | Tidy Contacts does not merge two entries that share an address but have different names | Ported to the operation bench, not run |
| IO-004 | F12 | 1748 | #68: quitting with edits asks Save, Discard or Cancel: Save refuses a bad entry, Discard keeps the stored contact | Ported to the operation bench, not run |
| IO-005 | E05 | 1856 | Editor: clearing the only detail of a contact saves the change | Covered by DL-018 |
| IO-006 | T02 | 1896 | Tidy Contacts lists an entry it cannot fix, leaves it alone, and still fixes the other entry on the contact | Ported to the operation bench, not run |
| IO-007 | U34 | 2027 | #34: double-clicking a calendar entry opens its contact, and keeps the calendar row and its focus | Ported to the operation bench, not run |
| IO-008 | K24 | 2045 | #24: when the app opens, overlapping appointments are listed, and nothing is changed | Covered by SS-005 |
| IO-009 | P30 | 2075 | #30: a weekly appointment is listed on each week in the repeats window, and stored once | Ported to the operation bench, not run |
| IO-010 | P31 | 2101 | #30: an edit of a repeat changes its appointment; a move to another day moves the appointment and the focus with it | Ported to the operation bench, not run |
| IO-011 | B78 | 2205 | #78: a birthday saved on a contact shows in the calendar straight away, with birthdays shown | Ported to the operation bench, not run |
| IO-012 | W30 | 2403 | #30: a repeat whose source is older than the window is listed, and the source is not | Ported to the operation bench, not run |
| IO-013 | E30 | 2433 | #30: a repeat ends after a number of times, or on a date, in the list and in the form | Covered by DL-013 |
| IO-014 | T30 | 2482 | #30: Goto Today selects the first entry that has not ended, including a repeat, rather than the end of the list | Ported to the operation bench, not run |
| IO-015 | P33 | 2506 | #73: shortening a repeat from a later entry keeps the focus on the nearest entry of that repeat | Ported to the operation bench, not run |
| IO-016 | Q34 | 2527 | #74: OK on the edit of a repeat that started 11 days ago closes the form, with no dialog | Ported to the operation bench, not run |
| IO-017 | L02 | 3163 | Local: an appointment and a contact made in ContactManager are saved, and are there after a restart (no Google) | Covered by SS-009 |
| IO-018 | L03 | 3176 | Local: an appointment file moved into the archive is not loaded at start-up, and start-up leaves it in the archive | Ported to the operation bench, not run |
| IO-019 | L04 | 3197 | Local: a start with no to-do or history file opens and writes its data (#100) | Ported to the operation bench, not run |
| IO-020 | L05 | 3254 | #105 Notes, plain text: a contact's to-do list and history are saved as plain files, and a restart reads them back | Ported to the operation bench, not run |

## Accessibility scenarios (IA, area 4)

| ID | Archive ID | Line in `google_bench.py` | Title (from the archive)  | Status |
|---|---|---|---|---|
| IA-001 | L05 | 2577 | #101: Show Screen Reader Text: shows the name, role, value and description of the focused control, and says it is an approximation | Covered by AC-004 |
| IA-002 | L06 | 2590 | #101: the accessibility debug checkbox in the screen reader text dialog turns console tracing on and off | To port |

## Old-file test (OF, area 3)

| ID | Archive file | Line | Title | Status |
|---|---|---|---|---|---|
| OF-001 | old_contact_test.cpp | - | Loads a contact file written before the email and phone labels existed, and checks that it keeps its meaning | To port |
