# Accessibility set-up (area 4)

The manual tests of section 4 of `documents/test-specification.md` (AC-001 to AC-018) are carried out by a person, with a
screen reader, and are not automated. Their only automated step is `setup_manual_data.py`, which prepares the data they
start from.

**Status: written, not run.**

## What the set-up does

1. Checks that the disposable Google account holds no test data. If it does, it stops and tells you to clear it.
2. Uploads the six fixture contacts (test_fixtures.json) and five appointments to the account.
3. Asks you to do three things by hand in ContactManager (the copy signed in to the disposable account):
   - Setup > Synchronisation Selection: set the calendar master and the contacts master to **Google**.
   - Run a sync (F9), and wait until it reports that it has finished.
   - Close Setup.
4. Waits for you to press Enter, then checks that Google holds the six contacts and five appointments.

It does not start ContactManager, and it runs no test.

## Running it

    python3 autotest/accessibility/setup_manual_data.py

The account is the one in the account file made by `autotest/utilities/setup_test_account.py`. To clear it first:

    python3 autotest/utilities/clean_test_data.py --dry-run
    python3 autotest/utilities/clean_test_data.py

## Then

Run the manual tests (AC-001 to AC-018) on that ContactManager, with the screen reader on, and record the results by hand.

The set-up uses the Google helpers of the Google bench (`autotest/googlesync/gs_common.py`), so it needs the same Google
set-up (the client, the account file, and the device sign-in).
