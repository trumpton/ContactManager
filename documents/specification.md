# ContactManager Specification

Version: as at 5 November 2026 (branch `master`, after PR #116).
Companion documents: `documents/test-specification.md` (tests), `documents/test-duration-estimate.md` (how long the tests take), `documents/messaging-rules.md` (the message rules in full, with worked evidence).

This specification describes what the program should do.

---

## Contents

1. Overview
2. Configuration and set-up
3. Contact management
4. Calendar
5. Announcements and alerts
6. Menus and keys
7. Synchronisation with Google
- Appendix B. Glossary

---

## 1. Overview

ContactManager is a keyboard-operated desktop program for keeping contacts and appointments, written for people who use screen readers. Every function is reached from the keyboard, the menus and the tabs are announced to screen readers, and the program reports its state in words rather than by colour or layout.

It does five jobs:

| Job | What it does |
|---|---|
| Contacts | Holds people and organisations: names, four email addresses, six phone numbers, two addresses, a web site, a birthday, a comment, and notes (a to-do list and a history) for each. |
| Calendar | Holds appointments for contacts (and for the user), with repeats, and lists them in date order. |
| Messages | Sends confirmations, updates, cancellations and reminders to a contact by email or text, and on the day of the appointment. Sending is automatic, within rules (section 5). |
| Google sync | Keeps contacts and appointments in step with a Google account, which phones and other programs also use (section 7). |
| Records | Keeps every record as a file, optionally encrypted, with a daily backup, and a log of what happened. |

### 1.1 Who it is for

One user, on one computer, with one data folder. Sending messages and syncing with Google are optional: the program runs on its own without them.

### 1.2 Where the data is

All data lives in one database folder (section 2.1). The program does not use a server. Two copies of the program on one folder are allowed, but only one of them sends messages (section 5.9).

### 1.3 Requirements the program meets

- Every function can be used from the keyboard, and screen readers can read every dialog (section 6 and the accessibility tests).
- A contact's message settings can stop messages to that contact, either at once (Hidden, Deleted) or per channel (Email, Text).
- Google is never given anything the program has not checked: a bad entry (for example an email address that does not look right) stops the save and is shown, rather than sent.
- Nothing is sent to a contact unless the contact has the matching channel ticked and an address for it.
- A failure to send is shown in the log, with the reason, and is retried a limited number of times.

### 1.4 Accessible text

The text a screen reader speaks, or a braille display shows, is part of the design. Two rules apply:

- **Spoken text is short and succinct.** Say what the item is and its value or state, then stop. Put the most important words first, so a listener who stops early still has the essentials. Do not repeat what the screen reader already announces, such as the role of a control, the window title or the program's path.
- **Text on a braille display is compact.** A braille line holds few characters. Use the fewest words that keep the meaning, and leave out padding and repeated separators. Put the most important words first, so they stay on the line when the display is scrolled.

The same item gives the same text wherever it appears (the list, a dialog, or Show Screen Reader Text), so a user learns it once.


---

## 2. Configuration and set-up

Configuration is in two places:

- **The ini file** (`ContactManager.ini`): where the data is, how messages are sent and how Google is called. Read at start-up. Sections are described in 2.2.
- **Settings** (File > Setup): the options a user changes while running the program, stored in the operating system's settings store (the registry on Windows). Described in 2.3.

### 2.1 Files and folders

**The ini file** is loaded from the first of these places that holds it: `$HOME/ContactManager`, `$HOME/TrumptonApps`, or the folder that holds the program's executable.

**The database folder** is set by `[database] path` in the ini file. Its default is `$CONFIG/ContactManager`, where `$CONFIG` is `$HOME/TrumptonApps`. The folder holds:

| Item | Where | What it is |
|---|---|---|
| `ContactManager.lock` | database folder | Held by the copy that is allowed to send messages (5.9). |
| `contact/` | database folder | One file per contact: `<ID>.contact`, with `<ID>.todolist` and `<ID>.history` beside it. Encrypted: `.zcontact`, `.ztodolist`, `.zhistory`. |
| `contact/deleted/` | under `contact/` | Deleted contacts, with their history and to-do files. |
| `calendar/` | database folder | One file per appointment: `<ID>.appointment`. |
| `calendar/archive/` | under `calendar/` | Old appointments that have been archived (section 4.8). Not read at start-up. |
| `backup/` | database folder | Daily backups, as zip files named `<sequence>-<yyyymmdd>-backup.zip`. The two newest are kept. |
| `sync-<phase>.progress` | database folder | The record of a sync in progress, so an interrupted sync can resume (section 7.6). Removed when the phase completes. |

**Logs.** Every action that matters is written to the log, which is shown in the Logs window (File > Show > Logs). The window is not modal, and it updates as entries are added. The log is not kept in a file.

### 2.2 The ini file

Each section, with its keys. A key that is not given takes the default shown.

**`[general]`**
- `codec` — text codec, `UTF-8`.

**`[database]`**
- `path` — the database folder (see 2.1).
- `encrypted` — `yes` or `true` to save contacts, notes and to-do lists encrypted; anything else saves them as plain files. Default: not encrypted.

**`[journal]`**
- `path` — the folder of the journal files for each contact (the Easy Notepad journal).
- `easynotepadexe` — the Easy Notepad program that opens a journal (Ctrl+J).
- `filename` — the file name used for the journal (read by the program; its pattern is not documented in the help).

**`[google]`**
- `debug` — `yes` to write debug output for support. Default `no`.
- `downloaddaysbefore` — the download window starts this many days before today. Default 90.
- `downloaddaysafter` — the download window ends this many days after today. Default 730.

**`[phone]`**
- `localcode` — the local dialling code, used when formatting numbers. Example `01234`.
- `countrycode` — the country code, for example `0044`.

**`[sms-reminder]`** — text messages
- `agent` — the SMS agent: one of the supported agents, `Voodoo` or `Esendex`.
- `username` — the account name the agent needs: for Voodoo, the HTTP API username; for Esendex, the account reference. The SMS password (or API key) is set in Settings, whichever agent is used.
- `sender` — the name shown to the recipient. For Esendex: letters and digits, a name registered with Esendex. **Required**: if blank, no text is sent, and a warning is logged at start-up.
- `debug` — `yes` for debug output for support.

**`[email-reminder]`** — email messages
- `server` — the SMTP server, for example `smtp.gmail.com`.
- `username` — the SMTP account.
- `port` — `465` for SSL, as the default server needs.
- `sender` — the From address, written `Name <address>`. **Required**: if blank, no email is sent, and a warning is logged at start-up.
- `debug` — `yes` for debug output for support.

The email password is set in Settings, not in the ini file.

**`[reminders]`** — when messages are sent. Rules are in section 5.

| Key | Default | Meaning |
|---|---|---|
| `remindhoursbefore` | 24 | The reminder is sent this many hours before the appointment. |
| `confirmonlyhours` | 36 (1.5 × `remindhoursbefore`) | An appointment made or changed this close to its start gets its confirmation or update only. If missing, not a number, or less than `remindhoursbefore`, the default is used. |
| `earliestsend` | 07:00 | Messages are sent from this time, local time. |
| `latestsend` | 22:00 | Messages are not sent from this time. |
| `waitbeforesendmins` | 5 | A new or changed appointment waits this long after its last change. 0 is allowed. |
| `limitperday` | 10 | Most messages to one contact in any 24 hours. |
| `limitperhour` | 5 | Most messages to one contact in any hour. |
| `attachics` | no | `yes` attaches a calendar (.ics) entry to each email. |

**`[email]`**
- `command` — the command that opens the user's own mail program with an email ready (Ctrl+M), using `$TO` and `$NAME`. Example: the Thunderbird compose command.

**`[message]`** — the message template files (2.4).
- `emailconfirmation`, `emailupdate`, `emaildelete`, `emailreminder`, `emailtoday`
- `smsconfirmation`, `smsupdate`, `smsdelete`, `smsreminder`, `smstoday`

**`[var]`** — named values the other entries can use. For example `THUNDER="C:\Program Files\...\thunderbird.exe"`, then `$THUNDER` in another key.

**Variables** that any ini value can use:

| Variable | Means |
|---|---|
| `$HOME` | the user's home folder |
| `$CONFIG` | `$HOME/TrumptonApps` |
| `$INI` | the folder the ini file was loaded from |
| `$DIRAPPLICATION` | the folder the program is in |
| `$TO`, `$NAME` | the recipient's address and name, for the Ctrl+M mail command |
| any name in `[var]` | its value |

A complete example, with the defaults, is in the program's help (File > Manual, section 8.1).

### 2.3 Settings (File > Setup)

The Setup dialog is titled "Contact Manager - Setup". It is written to the settings store, and takes effect when the dialog is closed with OK.

| Area | Control | What it does |
|---|---|---|
| Identify Me | My contact | The contact that is the user. Goto Me (Ctrl+Shift+O) opens it. |
| Google Account | Change, Reset | Chooses or removes the Google account (2.6). Shows the last contact and calendar sync times. |
| SMS Account | SMS Password | The SMS password, or API key, for the agent in use. |
| | SMS API key expires (optional) | The expiry date of the key, day/month/year, for example 25/01/2035. A warning is shown at start-up from 30 days before it. Blank if the key does not expire. |
| Email Account | Email Password | The SMTP password. |
| Synchronisation Selection | Calendar: Contact Manager is the master / Google is the master | Which side wins conflicts for appointments (7.2). |
| | Contacts: Contact Manager is the master / Google is the master | Which side wins conflicts for contacts (7.2). |
| Database Management | (group; see the dialog) | Maintenance settings for the database folder. |
| Options | Enable SMS and Email Reminders | Turns automatic messages on or off for this copy. |
| | Show Birthdays in Calendar List | Shows contacts' birthdays as calendar entries. |
| | Show appointments from ___ days before today | The start of the listing window (the appointments shown in the calendar list). Default 90. |
| | ___ days after today | The end of the listing window. Default 730. |

The listing window is the range of appointments shown in the calendar list (section 4.1). The download window, which decides which Google appointments are downloaded, is set in the ini file (`[google]`, section 2.2 and section 7.4).

### 2.4 Messages and templates

Each message is a text file. Its first line is the subject (email) or the message (text). The rest of an email file is the body. The sender is not in the file: it comes from `[email-reminder] sender` or `[sms-reminder] sender`.

| Message | Ini key | Sent for |
|---|---|---|
| Email confirmation | `emailconfirmation` | A new appointment, by email. |
| Email update | `emailupdate` | A changed appointment, by email. |
| Email cancellation | `emaildelete` | A deleted appointment, by email. |
| Email reminder | `emailreminder` | A reminder, by email. |
| Email on the day | `emailtoday` | The day's message, by email. The reminder file is used if this key is not set. |
| Text confirmation | `smsconfirmation` | A new appointment, by text. |
| Text update | `smsupdate` | A changed appointment, by text. |
| Text cancellation | `smsdelete` | A deleted appointment, by text. |
| Text reminder | `smsreminder` | A reminder, by text. |
| Text on the day | `smstoday` | The day's message, by text. The reminder file is used if this key is not set. |

**Rules for the files:**
- An SMS file should be one line. Only its first line is sent.
- An email file may have many lines; lines from the second on are the body.
- A template that cannot be read means that message is not sent, and the log says so.
- A channel with a blank sender sends nothing, and the log says "sender missing".

**Variables** that a template can use:

| Variable | Replaced with |
|---|---|
| `$NAME` | the contact's first names |
| `$TIME` | the appointment's start time |
| `$DOW` | the day of the week |
| `$DATE` | the day of the month |
| `$MONTH` | the month |
| `$YEAR` | the year |
| `$FDATE` | the full date, dd-MMM-yyyy |
| `$APPOINTMENTS` | the contact's next appointments, up to ten, soonest first. In an email body, one per line, as "10:00 on Friday 30 October". In a subject or a text, on one line, separated by "; ". |

### 2.5 Encryption: the file encryption key and the login password

Contact files, to-do lists and histories can be saved encrypted. Two secrets are involved. This specification uses these names for them throughout:

- **The file encryption key** encrypts the files. It is created once and is never changed. It is typed twice when it is created.
- **The login password** is typed each time the program starts. It decrypts the file encryption key, so without it no encrypted file can be read. It can be changed, and the file encryption key is then protected with the new password.

File > Key and Password has three items:
- **Set File Encryption Key** — creates the file encryption key and the login password. Available only when not logged in.
- **Change Login Password** — changes the login password, after the old one is typed. Available only when not logged in.
- **Logout** — logs out, so the next action asks for the login password. Available only when logged in.

**At start-up**, the program asks for the login password up to three times. If it still fails, it asks whether to set or reset the file encryption key, or to close.

**Losing the file encryption key is serious.** Without it, encrypted files cannot be read. If the login password is forgotten, the same file encryption key can be installed again with a new login password. The file encryption key cannot be recovered from the program.

**Multiple copies.** Copies of the program on the same data folder share the same file encryption key, so only the first one needs to log in.

**Plain files.** When encryption is off, the same files are saved as plain text: `.contact`, `.todolist`, `.history`. Changing the encryption setting does not convert files that already exist.

### 2.6 Setting up and linking online services

#### Google (contacts and calendar)

1. Set up a Google project with the Google Calendar API and the Google People API enabled, and an OAuth client ID for an application that can run on a device. The client ID and secret are placed in the program's build (`account-googleaccess.h`, from `account-googleaccess.h.example`). They are not kept in the repository.
2. In File > Setup, choose **Change** next to Google Account. The program shows a short code and a Google address. Open the address, type the code, and sign in to the account to use.
3. Google asks for the access requested by the scopes: calendar, contacts, and the account's name and email. Allow them.
4. The program stores the refresh token and the account's email address. **Reset** removes them, and the next sync asks for the account again.
5. Choose the master for calendar and for contacts (2.3).
6. Press F9 (Google Sync) to sync.

The program reaches Google at `accounts.google.com`, `www.googleapis.com` and `people.googleapis.com`. A network that blocks these hosts cannot sync.

#### SMS (text messages)

1. Choose the agent in `[sms-reminder] agent` (one of the supported agents: Voodoo or Esendex), and the username.
2. Set `[sms-reminder] sender`. Without it, texts are not sent.
3. In Setup, enter the SMS password (or API key). Enter the key's expiry date if the key expires.
4. A warning appears at start-up from 30 days before the expiry date.

#### Email (automatic messages)

1. Set `[email-reminder]` server, port, username and sender. Without the sender, no email is sent.
2. In Setup, enter the email password.

#### Manual email and journals

- **Email to a contact (Ctrl+M)** opens the user's mail program, using the command in `[email] command`. It needs no account settings in ContactManager.
- **Journal (Ctrl+J)** opens the contact's journal in Easy Notepad, from `[journal] easynotepadexe` and `[journal] path`.

---

## 3. Contact management

### 3.1 Opening and creating contacts

There are four ways to open a contact:

| Way | How |
|---|---|
| File > Open | Ctrl+O. Use the arrow keys to move through the list. Press the first letter of a surname to jump to it. |
| Global Find | Ctrl+G. Type part of a name (case and accents do not matter) and press Enter. |
| From the calendar | Select an appointment and press Enter. |
| Goto Me | Ctrl+Shift+O, for the contact set as "Identify Me". |

**Create a contact:** in File > Open, go to the top of the list ("-- create contact --") or press minus. Enter opens a blank Contact Details tab.

**Opening an existing contact** disables the Contact Details tab, so an existing contact cannot be changed by accident. Enable editing with Alt+E, D (Edit > Contact Details). This enables the tab and places the cursor in the first field.

### 3.2 Editing a contact

The Contact Details tab is a form grouped into sections. The sections are reached with Alt+N (names), Alt+M (email), Alt+A (address), Alt+P (phone); Tab moves between boxes within a section.

**Fields:**

| Section | Field | Rules |
|---|---|---|
| Names | First names | Free text. |
| | Surname | Free text. |
| | Organisation | The contact's business name. |
| | Type | Tick one or more of Business, Client, Family, Friend, Other. |
| | Hidden | Hides the contact (3.6). |
| Email | Email 1 to Email 4 | Each has a title (Home, Work, Other ...). Must look like `name@domain.com`. |
| | Web Site | Free text. |
| Address | Postal Address | Free text, entered as it would appear on an envelope, without commas or full stops at the end of each line. |
| | Secondary Address | Free text, as for Postal Address. |
| | Second Postal Address | Free text, as for Postal Address. |
| Phone | Home, Work, Mobile, Custom 1, Custom 2, Custom 3 | Six phone numbers, each with a title. Digits, spaces, brackets, hyphens and dots are allowed, with an optional + at the start. Stored in +nn international format. |
| | SIP / VOIP | A SIP address, such as `sip:name@domain`. |
| Birthday | Date of birth | Up/down arrows, or type the date in numbers. The year is optional. Minus removes the year. |
| Other | Comment | Free text, saved with the contact and synced to phones. Do not put passwords here. |
| | Confirm via SMS, Confirm via Email | Whether the contact gets automatic messages on each channel (section 5). |

**Titles.** Press F2 in an address or number box to change its title. A title can also be typed after the entry, as in `name@domain.com (Work)` or `01234 567890 (Home)`. A title can have up to 20 characters, and cannot contain a colon, slash or backslash.

**Checks when saving.** The form is checked when it is saved. A bad entry (for example an address that is not an address) is shown with the cursor in its box, and nothing is saved until it is right. Saving with a bad entry in an older record is also refused, and the entry is shown.

**Saving and leaving.** Ctrl+S saves the open contact. If the contact has unsaved edits and the user tries to leave it (open another contact, quit, sync, or Tidy Records), the program asks Save, Discard or Cancel. Save refuses a bad entry and keeps the edits on the screen.

### 3.3 Storage

Each contact is one file, named by the contact's ID, in the `contact/` folder (2.1). Its to-do list and history are separate files with the same ID. All three are saved when the contact is saved.

- **A new contact** gets a new ID, and its files are created at once, including an empty history.
- **A contact's history** records changes made to it, including changes made by Google sync under Google master, and by Tidy Records. Messages sent for the contact are added to it too.
- **A deleted contact** is moved, with its to-do and history files, into `contact/deleted/`. It is not deleted from disk. It is listed by File > Show > Deleted Contacts.
- **Hidden contacts** stay in `contact/`. They are not listed in the open menu or messaged, but they appear in brackets at the end of a Global Find.

### 3.4 Searching for contacts

| Search | Keys | What it finds |
|---|---|---|
| Find Here | Ctrl+F | The text in the current page. |
| Find Next | F3 | The next match of the text from Find Here. |
| Global Find | Ctrl+G | Contacts whose names contain the text. Case and accents are ignored. Press Enter on a result to open it. |
| Advanced Global Find | Ctrl+Shift+G | A search with more criteria, in its own dialog. |
| Email / SMS to contacts | Search menu | Contacts with an address or number for each channel. |
| Email / SMS to non-clients | Search menu | As above, for contacts not ticked Client. |
| Uncategorised contacts | Search menu | Contacts with no Type ticked. |
| Duplicate contacts | Search menu | Contacts that may be the same person. |
| Duplicate hidden auto-email entries | Search menu | Hidden contacts that share an address with a visible one. |

### 3.5 Tidy Records

File > Tidy Records (in the File menu) first saves the edit on the screen, then tidies the contacts and their history:
- Two contacts that are one person, sharing an address with no conflict between them, are merged. Two that share an address but have different names are not merged.
- An entry the program cannot fix (for example, a title with a colon) is listed in the summary, and left as it is. The other entries on the contact are still fixed.
- Deleted contacts and appointments that are not on Google are moved to the deleted folder. Those still linked to Google are left for the next sync to remove there.
- A repeat with no occurrences left (every date removed) is marked as deleted.

The progress window shows a summary of what was changed and what needs the user's attention.

### 3.6 Hiding and deleting

- **Hiding** a contact (Hidden) keeps it in the program. It is not shown in the open menu, is not messaged, and is not synced to phones, but it is in Global Find (in brackets).
- **Deleting** a contact (Edit > Delete Contact) asks for confirmation, then moves the contact to the deleted folder (3.3).
- Appointments for a deleted or missing contact remain. They are shown as "Deleted contact". The program does not decide which contact they belong to: the user does, by editing the appointment.

### 3.7 Import and export

- **File > Export > Contacts** and **File > Export > Appointments** write the records to a file for use elsewhere.
- **File > Import** reads records in from a file.

### 3.8 Transactions

- **Transaction Income** (Ctrl++) and **Transaction Expense** (Ctrl+-) add an entry for the open contact in its log. Each has an amount and a description.
- **Transaction Report** (Tasks menu) produces a report of the transactions for a year.

---

## 4. Calendar

### 4.1 What is listed

The calendar tab lists appointments in date and time order. The list covers the listing window (2.3): by default, 90 days before today to 730 days after it.

- A **repeat** is listed once for each of its dates in the listing window. Its first occurrence may be before the listing window.
- A **deleted** appointment is listed only when File > Show > Deleted Appointments is ticked. Then it is shown in braces.
- **Birthdays** are listed when Show Birthdays in Calendar List is ticked (2.3).
- The ticks in File > Show are not kept: they are off each time the program starts.

At start-up, the program lists any appointments that overlap, and changes nothing.

### 4.2 Moving around the list

The list has the focus. Each move puts the entry about a quarter of the way down the list, so the entries after it can be seen.

| Key | Action |
|---|---|
| Up, Down | Previous or next entry. |
| T, Ctrl+T | Go to today: the first entry from now on. Ctrl+T is Goto Today. |
| D, Ctrl+D | Go to a date. A date box opens (4.3). Ctrl+D is Goto Date. |
| 0 to 9 | Start typing a date (4.3). |
| W | Go a week on from today. Each W pressed after it goes a further week: WW is two weeks on, and so on. T starts again from today. |
| N, Right | Next entry for the open contact. After the last, it wraps to the first. |
| P, Left | Previous entry for the open contact. |
| Space | Show only the open contact's entries, or all of them again. |
| Enter | Open the contact for the selected entry. |

With no contact open, Space, N, P and the arrow keys beep, and nothing changes.

### 4.3 Typing a date

Press 0, 1, 2 or 3 in the calendar list. A box opens with that digit in it, and the cursor at the end. Type the rest of the date and press Enter. The list goes to the first entry on that date, or to the nearest entry before it. Escape, or Enter on an empty box, closes the box and leaves the list as it was. Text that is not a date closes the box and beeps.

Accepted forms:

| Typed | Means |
|---|---|
| `2/12` | 2 December, the next one on or after today |
| `2-12-26` | 2 December 2026 |
| `2/12/2026`, `2/12/26` | 2 December 2026 |
| `12 oct 2026` | 12 October 2026 |
| `1 sep` | 1 September, the next one on or after today |
| `1aug28` | 1 August 2028 |

Month names can be typed in full or in their first three letters or more. Day names (Mon, Tue ...) are not accepted. A two-digit year takes the first two digits of the current year.

### 4.4 Creating and changing appointments

| Action | Key | Result |
|---|---|---|
| New appointment for the open contact | Ctrl+N | The appointment form, with For set to the contact. |
| New appointment for me | Ctrl+Shift+N | As above, For set to the user. |
| Insert after the selected one | Ctrl+I | A new appointment placed after the selected entry. |
| Insert for me after the selected one | Ctrl+Shift+I | As above, for the user. |
| Update the selected appointment | Ctrl+U | The form for the selected entry (Edit > Update Appointment). |
| Remove the selected appointment | Ctrl+R | Deletes the entry (4.8). |

### 4.5 The appointment form

| Field | Meaning |
|---|---|
| For | The contact (or the user) the appointment is for. Blank is shown as "Unnamed Contact". |
| Summary | A one-line description. |
| From | The start date and time. The duration is shown in brackets; up and down change it. |
| To | The end date and time. Changing it changes the duration. |
| Description | A multi-line description. |

From and To are linked: changing one changes the other. Normally only From is needed. Other durations than those offered are set by changing To.

Enter saves; Escape cancels. Before saving, the program checks for clashes, and offers the chance to change the time.

### 4.6 Repeats

An appointment can repeat. The form gives:
- the **type** of repeat and the **interval** (for example every week, or every two weeks);
- an **end**: after a number of times, or on a date.

A repeat is stored once, and listed on each of its dates in the listing window.

Changes to one occurrence of a repeat:
- **Deleting one occurrence** removes only that date; the others are kept. On Google, that date is left out of the repeat.
- **Changing one occurrence and all that follow** splits the repeat: the earlier dates keep the old details and the later dates take the change. Google receives both parts.
- **Cancelling a change** of a repeat changes nothing.
- **Shortening a repeat** from a later entry keeps the focus on the nearest entry of that repeat.

A repeat with no occurrence left (every date removed) is marked deleted.

### 4.7 Clashes

The program checks for clashes when an appointment is saved, and offers the user a chance to change it. Removed occurrences of a repeat do not clash. Appointments that touch at an end do not clash.

### 4.8 Storage, archive and deletion

- Appointments are stored one per file in `calendar/`.
- **Deleting** an appointment (Ctrl+R) marks it deleted. A deleted appointment that is not on Google is moved to the deleted folder by Tidy Records. One still linked to Google is removed from Google by the next sync.
- **Archive.** An appointment that has ended before the download window, that is on Google, that has not changed since the last sync, and that has nothing waiting to be uploaded or downloaded, is moved to `calendar/archive/` by the sync. The archive is not read at start-up, so the calendar stays quick on large data sets.
- **Restore.** If Google moves an archived appointment back into the download window, it is restored from the archive.
- **Cancel from the archive.** If Google cancels an archived appointment, it is moved to the archive's deleted folder, not deleted.

### 4.9 Birthdays

A birthday saved on a contact appears in the calendar at once, when birthdays are shown. A leap-day or no-year birthday is not changed by a sync.

---

## 5. Announcements and alerts

This section sets out the rules the program follows when it sends messages. The rules were checked on an accelerated clock: earlier runs at 60 times real time, and the test specification's area 2 at 360 times. The full statement of the rules and the earlier test evidence is in `documents/messaging-rules.md`.

### 5.1 Which messages exist

Every message in this table is sent only inside the send window (5.5). The timing column says when a message becomes due. If it is due outside the window, it waits for the window to open.

| Message | Sent when | Timing | Channel | Sent only in the send window |
|---|---|---|---|---|
| Confirmation | An appointment is new, in the future, and not yet messaged. | After the hold (5.4). | Email, if Email is ticked and the contact has an email address; otherwise text, if Text is ticked and the contact has a mobile number (5.2). | Yes |
| Update | An already confirmed appointment changes in start, end or repeat. | After the hold, with the final details. | As the confirmation. | Yes |
| Cancellation | An already confirmed appointment is deleted. | Not held: at the first check inside the window. | As the confirmation. | Yes |
| Reminder | The appointment is within `remindhoursbefore` hours, it is not confirm-only, and it is not today. | Once per occurrence. | As the confirmation. | Yes |
| On the day | The appointment is today. | From `earliestsend` on the day, once per channel. | Each channel that is ticked and has its address or number: email, then text. | Yes |
| Removed occurrence | One occurrence of a repeat is removed. | Not held: at the first check inside the window. | Email. | Yes |

Nothing is sent for a past occurrence, or for an appointment deleted before it was confirmed.

### 5.2 Who is messaged

- A contact must have a channel ticked (Confirm via Email, Confirm via SMS) and an address or number for it. A channel that is not ticked, or has no address or number, is not usable.
- For a confirmation, update, cancellation or reminder, the email channel is used if it is usable. Otherwise the text channel is used, if it is usable. Each such message is sent on one channel only.
- On the day, each channel is used separately: a usable email channel sends its own on-the-day email, and a usable text channel sends its own on-the-day text (5.1).
- A deleted or hidden contact is not messaged.
- A contact with neither channel usable gets nothing.

### 5.3 By channel setting

| Contact is set to | Confirmation, update, cancellation, reminder | On the day |
|---|---|---|
| Email only | Email | Email |
| Text only | Text | Text |
| Both | Email (text, if there is no email address) | Email, then text |

A confirmation or update sent on the day, by a channel, counts as that channel's on-the-day message, so a same-day booking is not sent twice. For a text-only contact, a same-day booking is confirmed by text and gets no second text.

### 5.4 The hold

A new or changed appointment waits `waitbeforesendmins` (5 minutes) after its last change before its message is sent. Each change restarts the hold, so the message carries the final details. Examples:

| What happens | Message |
|---|---|
| Created 10:00, edited 10:02 and 10:04 | One confirmation at 10:09, with the final details. |
| Created 10:00, changed 10:02, changed back 10:04 | One confirmation at 10:09, with the original details. |
| Created 10:00, deleted 10:01 | Nothing. It was never confirmed. |
| Confirmed, then moved, and moved back | Nothing: the details match what was last sent. |
| Confirmed, then moved and cancelled | Cancellation sent at once. No update. |
| Confirmed, then title or location changed only | Nothing. Only start, end and repeat count as changes. |

A title or location change never sends an update. This is a confirmed decision.

### 5.5 Send window and silence

Automatic messages (every message in 5.1) are sent only between `earliestsend` (07:00) and `latestsend` (22:00), local time. The window includes 07:00 and does not include 22:00. A message due outside the window waits until the window opens. The window opens with the first check after `earliestsend`, so a message due at 23:00 is sent at 07:00.

A message the user sends by hand (Tasks > SMS Contact, Ctrl+Shift+M) is not automatic, and the window does not apply to it.

### 5.6 Confirm-only appointments

An appointment made or changed less than `confirmonlyhours` (36 hours) before its start gets its confirmation or update only. That message takes the place of the reminder. A reminder is not sent once the appointment is on that day, as the on-the-day message takes its place.

### 5.7 Limits

A contact is sent no more than `limitperhour` (5) messages in any hour and `limitperday` (10) in any 24 hours. On-the-day messages count. Messages over a limit wait, and are sent when the limit allows. None are dropped.

Example with the defaults: a contact has 12 appointments created overnight, all due at 07:00. Five are sent at 07:00, five at 08:00, and the last two wait until the daily limit clears.

### 5.8 Missed messages, restarts and backlogs

- **Not running.** If the program is not running when a message is due, the message is sent when it can be, showing the current details.
- **Reminders while off.** One reminder per occurrence. A reminder is not sent once the appointment is on that day.
- **Backlogs.** The program looks at each appointment's current state at each check. A backlog therefore gives one message per appointment, not one per change made while the program was off.
- **Restart in the day.** On-the-day messages are recorded per channel, so they are not sent again after a restart.
- **Limits across restarts.** The count of messages in the last hour and day is kept, so limits hold across a restart.
- **Sent state.** Each appointment records what was sent. With Google sync, that record is kept on the Google event, so a second copy of the program does not send the same message.

### 5.9 Two copies of the program

Only the copy that holds the lock file (`ContactManager.lock`, in the database folder) sends messages. A second copy shows a warning at start-up: "Messages Not Sent", and it will not send confirmations, updates, cancellations or reminders. Both copies can be used for everything else.

### 5.10 Failures

- A send that fails (the transport refuses it, or the server does not answer) is retried after 5 minutes (`SENDRETRYMINS`).
- It is tried at most 10 times (`MAXMESSAGERETRIES`). After that it is given up, and the log says so.
- A send is recorded as sent only when it is sent.
- A message is not sent while its channel's sender is blank. Nothing is added to the contact's history for it, and the log says "sender missing". A warning is shown at start-up.
- A template that cannot be read means the message is not sent, and the log says so.

### 5.11 Expiry and warnings

- **SMS key.** A warning at start-up from 30 days before the expiry date set in Setup.
- **Sender.** A warning at start-up for each channel with no sender, and the messages for that channel are not sent until one is given.
- **Lock.** The warning in 5.9.

### 5.12 What is not messaged

- Only appointments produce messages. Changing a contact's details does not send a message by itself.
- A title or location change does not send an update (5.4).
- An appointment for a contact that has been deleted or hidden is not messaged.
- Past appointments are not messaged.

---

## 6. Menus and keys

### 6.1 The menu bar

The menus are File (Alt+F), Search (Alt+S), Goto (Alt+G), Edit (Alt+E), Tasks (Alt+T) and Help (Alt+H). Items are marked with their keys in brackets. A menu item that does not apply (for example Contact Details when no contact is open) is disabled.

The menus are:

- **File**
  - Open (Ctrl+O)
  - Save (Ctrl+S)
  - Export
    - Contacts
    - Appointments
  - Import…
  - Google Sync (F9)
  - Tidy Records
  - Exit
  - Key and Password
    - Set File Encryption Key
    - Change Login Password
    - Logout
  - Setup
  - Show
    - Logs
    - Deleted Appointments
    - Deleted Contacts
    - Hidden Contacts
    - Show Screen Reader Text
- **Search**
  - Find Here (Ctrl+F)
  - Find Next (F3)
  - Global Find (Ctrl+G)
  - Advanced Global Find (Ctrl+Shift+G)
  - Email / SMS To Contacts
  - Email / SMS To Non-Clients
  - Uncategorised Contacts
  - Duplicate Contacts
  - Duplicate Hidden Auto-Email Entries
- **Goto**
  - Goto Me (Ctrl+Shift+O)
  - Goto Journal (Ctrl+J)
  - Goto To Do List (Ctrl+L)
  - Goto My To Do List (Ctrl+Shift+L)
  - Goto Today (Ctrl+T)
  - Goto Date (Ctrl+D)
  - Goto Contact Details
    - Name(s) (Alt+N)
    - eMail (Alt+M)
    - Address (Alt+A)
    - Phone (Alt+P)
    - Birthday (Alt+B)
- **Edit**
  - Insert Appointment
    - Insert Appointment After (Ctrl+I)
    - Insert My Appointment After (Ctrl+Shift+I)
  - New Appointment
    - New Appointment (Ctrl+N)
    - New My Appointment (Ctrl+Shift+N)
  - Update Appointment (Ctrl+U)
  - Remove Appointment (Ctrl+R)
  - Edit History
  - New Contact
  - Contact Details
  - Delete Contact
- **Tasks**
  - Send EMail Contact (Ctrl+M)
  - SMS Contact (Ctrl+Shift+M)
  - Transaction Income (Ctrl++)
  - Transaction Expense (Ctrl+-)
  - Transaction Report
- **Help**
  - Manual (F1)
  - About

### 6.2 Tabs

| Tab | Key | Contents |
|---|---|---|
| Overview | Alt+O | A summary of the open contact and its next appointments. |
| Calendar | Alt+C | The calendar list (section 4). |
| To-Do List | Alt+L | The open contact's to-do list. |
| Contact Details | Alt+D | The contact form (section 3.2). Disabled when no contact is open or when editing is off. |
| History | Alt+Y | The open contact's history: its recorded changes, and the messages sent for it. |

Journal (Ctrl+J) is not a tab: it opens the contact's journal in Easy Notepad (2.6).

### 6.3 Other keys

| Key | Action |
|---|---|
| F1 | The manual. |
| F3 | Find next. |
| F9 | Google Sync (section 7). |
| Alt+E, D | Enable editing of the open contact's details. |
| Alt+N, M, A, P | Move to the Names, Email, Address and Phone sections of the form. |
| Ctrl+S | Save the open contact. |
| Ctrl+M | Email the open contact in the user's mail program. |
| Ctrl+Shift+M | Text the open contact. |

---

## 7. Synchronisation with Google

Pressing F9 (File > Google Sync) synchronises contacts and appointments with the Google account, and with any phone or mail program that uses it. The sync reports its progress in a window, and ends with a summary.

### 7.1 What is synchronised

- **Contacts.** Names, the four email addresses, the six phone numbers, the addresses, the web site, the birthday, the organisation and the comment. A contact is linked to its Google record by ID, and the link is kept on the contact.
- **Appointments.** Summary, start, end, description and location, and repeats. Downloads are limited to the download window (7.4); uploads are not. An appointment that is organised by someone else (read-only) is not uploaded.

**Slots.** A contact has four email slots and six phone slots. Google may hold more. The extra values are kept in the contact's Comments, so no data is lost, but they are not editable as separate slots. Labels (Home, Work, Mobile ...) are kept with each value.

### 7.2 Masters and conflicts

Each of calendar and contacts has a **master** (2.3). The master is the side whose version wins when both sides have changed an item since the last sync.

| Situation | ContactManager is master | Google is master |
|---|---|---|
| Only the app changed the item | The change is uploaded. | The app's change is overwritten by Google's copy at the next sync. |
| Only Google changed the item | The change is downloaded. | The change is downloaded. |
| Both changed since the last sync | The app's version is sent; the report says it was a conflict. | Google's version is kept; the report says so. |
| Only one side has the item | The other side is created. | The other side is created. |
| Deleted in the app | Deleted on Google. | Deleted on Google only if Google's copy is unchanged since the last sync; otherwise Google's change is kept. |
| Deleted on Google | Deleted in the app. | Deleted in the app. |
| Deleted on one side, changed on the other | The master's version wins. | The master's version wins. |

For each conflict, the sync report says which version was kept and why.

### 7.3 The sync process

1. Contacts are synced first, then appointments. The progress bar fills the first half for contacts and the second for the calendar.
2. The Google side is read. For appointments, only changes are listed after the first full sync, using Google's sync token. If Google refuses the token, the whole download window is listed again.
3. Changes are applied and uploads are sent. Each Google record carries its version (ETag), so a change made on Google since the last read is not overwritten silently: it is a conflict (7.2).
4. Links between contacts and appointments are saved as they are made, so an interrupted sync does not lose them.
5. Appointments that have ended and are on Google are archived (4.8).
6. The sync time is saved for each phase, and the summary is written.

### 7.4 The download window

Google appointments are downloaded only if they fall in the download window. Its start and end are set in the ini file, in the `[google]` section (2.2):

- `downloaddaysbefore` (default 90): the window starts this many days before today.
- `downloaddaysafter` (default 730): the window ends this many days after today.

An appointment that starts before the window and ends inside it is downloaded. An appointment that ends before the window is not downloaded, and stays on Google. Widening the window downloads the entries it now reaches.

The window limits downloads only. A local appointment is uploaded whatever its date.

A repeat is downloaded with its first occurrence. A repeat whose first occurrence is before the download window is still downloaded, and its occurrences are listed in the listing window.

The listing window set in Setup (2.3) decides which downloaded appointments are shown in the calendar list.

### 7.5 Retries and rate limits

Google limits how many requests a user may make. The program:

- retries a request only when Google says it will clear (a rate limit: `rateLimitExceeded`, `userRateLimitExceeded`, or HTTP 429) or when the server fails (5xx). Other failures (authorisation, permission, not found, a request Google rejects) are reported at once;
- waits 0.5 s before the first retry, doubling each time up to 4 s, with a random addition, and retries four times. If the fifth attempt also fails, the item is a failure;
- counts the transactions it makes. When five of the last hundred needed a retry, it slows down: the pause before each transaction starts at 4 s, and halves every 500 transactions, down to half a second.

### 7.6 Interrupted syncs

Upload progress is recorded in the `sync-<phase>.progress` file. If a sync stops, the next sync skips the uploads already recorded, so none are repeated and none are lost. The file is removed when its phase completes.

### 7.7 Failures and what to do

When a sync stops, the last line of the report gives the reason and what to do:

| Reason | Recovery |
|---|---|
| Rate limit | Wait five minutes and sync again. The progress file means finished uploads are not repeated. |
| A field Google does not accept | Edit the contact or appointment to remove the characters or values Google changes, then sync again. |
| Connection closed | Sync again. It resumes from the progress file. |
| Authentication | Sign in to Google again (Setup > Google Account > Change). |

#---

## Appendix B. Glossary

| Term | Meaning |
|---|---|
| Master | The side (program or Google) whose version wins a conflict (7.2). |
| Listing window | The dates of appointments shown in the calendar list. Set in Setup (2.3, 4.1). |
| Download window | The dates of Google appointments that are downloaded. Set in the ini file (2.2, 7.4). |
| Hold | The wait after a change before its message is sent (5.4). |
| Send window | The hours when messages are sent: `earliestsend` to `latestsend` (5.5). |
| Confirm-only | An appointment made close to its start, which gets only its confirmation (5.6). |
| On the day | The message sent on the day of an appointment (5.1). |
| Limit | The most messages to one contact in an hour or a day (5.7). |
| Lock | The lock file that marks the copy allowed to send messages (5.9). |
| Archive | The calendar folder for old appointments, which is not read at start-up (4.8). |
| Slot | One of the fixed places for emails (four) or phones (six) on a contact (7.1). |
| Progress file | The record of an interrupted sync (7.6). |
| Sync token | Google's marker for the last listing, so only changes are listed (7.3). |
| ETag | Google's version tag for a record, used to detect a change made on Google (7.3). |
| File encryption key | The key that encrypts the program's files. Created once, never changed (2.5). |
| Login password | The password typed at start-up. It decrypts the file encryption key (2.5). |
| Accessible text | The text a screen reader speaks, or a braille display shows, for a control or item. Short when spoken, compact on braille (1.4). |
