# Capacity stress test (section 5)

CS-001 of `documents/test-specification.md`: purge the disposable Google account, then upload 250 contacts and 1,000
appointments, half by the Google API and half by ContactManager, and leave them in place.

**Status: written, not run. Scratch account only, and not part of the normal cycle.**

## What it does

1. Asks you to type the account's address. Nothing is done unless it matches.
2. Purges the account with `autotest/utilities/clean_test_data.py --all-data`, which also asks for the address.
3. Uploads through the Google API: 125 contacts and 500 appointments.
4. Runs ContactManager headless (the operation driver) to add 125 contacts and 500 appointments locally, then sync. The sync
   uploads those.
5. Counts what is on Google, and checks the expected results of CS-001:
   - 250 contacts, each once;
   - 1,000 appointments, each once;
   - the sync within 1,062 seconds (the measured U01 time, 17 min 42 s, from the duration estimate);
   - ContactManager holds all 250 contacts and 1,000 appointments after the sync.
6. Leaves the records in place.

## Running it

    python3 autotest/capacity/cs001_capacity.py --driver ~/build-contactmanager/driver

The driver is built as in `autotest/operation/readme.md`. Results go to a folder in your home folder: `report.md` and
`summary.json`, and the driver's files for the sync.

## Notes

- The purge removes everything on the account, so run it only on the scratch account.
- Counting the contacts on Google lists the whole account, and the count is the test's result. The clean-up does not do this.
- The 1,062-second limit is the measured U01 figure; the duration estimate is not updated by this test. Change
  `SYNC_LIMIT_SECONDS` in the script if the estimate is changed.
- The upload uses the Google helpers of the Google bench (`autotest/googlesync/gs_common.py`).
