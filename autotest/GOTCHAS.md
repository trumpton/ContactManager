# Gotchas: Google contact sync (autotest notes)

Notes from testing the Google contact sync against the throwaway test account. Each entry says what was seen, why it matters, and what is still open.

## Scenario: an edit to a Google contact removes fields the app does not hold

**What happens.** When the app uploads a contact, it asks Google to overwrite each person field group listed in `WRITEPERSONFIELDS` (`googleaccess.h`). That list includes `urls`, `addresses`, `organizations`, `birthdays`, `biographies`, `phoneNumbers`, `emailAddresses`, `sipAddresses`, `names` and `userDefined`. The app only holds some of the data in each group, so any entry it does not hold is removed on Google.

**Seen.** A contact with a URL, a nickname and a biography. After a name edit and a sync, the name was updated and the nickname and biography were kept, but the URL was removed.

**Fix chosen: option 1, send only what changed.** An upload now lists only the groups whose app value differs from Google's (`changedPersonFields`, `googleupdatedialog_contacts.cpp`). `userDefined` is always listed, because it holds the app's ID and upload marker. Groups that are not listed keep their Google value.

**Option not selected: keep.** Before writing, fetch the current Google contact, copy the app's fields into it, and send the whole result. Anything the app does not hold is sent back unchanged, so this needs no per-group checks. Its costs are one extra request per upload and merge logic for each group. It would be the safer choice if option 1 proves wrong for a group the app only partly holds. Switch to it if a group the app holds only in part (addresses, phone numbers, emails beyond four) loses data.

## Gotchas found while testing option 1

- **A changed group is replaced whole.** Clearing the website sends an empty `urls` group, which removes every URL on Google, not only the one the app holds. A contact with two URLs lost both when its website was cleared.
- **Changes that don't touch a group leave it alone.** Renaming a contact with two URLs kept both URLs. This is the case option 1 is meant to fix.
- **The first sync no longer uploads downloaded contacts** that have no app ID. Their ID is derived from the Google resource name (see #49).
- **Emails beyond four** are kept in Comments by the download, and are not sent back as addresses. An upload that changes the emails group can therefore drop them from Google.
- **Phone numbers, addresses and organisations** are held in fixed slots. Values beyond the slots are not held by the app, so an upload that changes that group can drop them.
- **Local master (the default) and Google master behave differently.** In local master every local contact is marked for upload each sync, but it is only uploaded when it differs from Google. In Google master nothing is uploaded unless it is marked. A contact that is not uploaded stays untouched on Google.

## Open

- **Clearing the website did not upload.** In the test, the website was cleared locally, but the next sync neither uploaded nor reported the contact. Google kept its URLs, so nothing was lost, but the clear did not reach Google. The cause has not been found, and it is not yet known whether it predates this change.
- **The URL already lost** from a contact the earlier code rewrote is not restored by this change.
- **Migration** of contacts downloaded by the earlier code is reasoned, not tested. The earlier code uploaded such contacts on their first sync, so they should already have the `id` property.

## How the test runs

- Test contacts are named `Email49 ...` (they become `Edited49 ...` after an edit). Delete them from the throwaway account after each run.
- The app's database path is relative (`ERROR/`), so each run needs its own working directory.
- Drivers that inspect the app's memory use `#define private public` before including `mainwindow.h`. This is for test builds only.
