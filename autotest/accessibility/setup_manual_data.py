#!/usr/bin/env python3
#
# Automated set-up for the manual accessibility tests (area 4, section 4 of documents/test-specification.md).
#
# This is the only automated step of section 4.  It uploads the fixture data to the disposable Google account: the six
# fixture contacts (test_fixtures.json) and five appointments.  It then asks the tester to set both masters to Google in
# ContactManager and run a sync (F9), and waits for the tester to confirm.  The manual tests (AC-001 to AC-018) are run by a
# person with a screen reader, on the data the sync brings to that ContactManager.
#
# It runs no test, and it does not start ContactManager.
#
# Before it runs, the account must be empty of test data.  If it is not, the script stops and says so; clear it with
# autotest/utilities/clean_test_data.py (its dry run first) and run this again.
#
#   python3 autotest/accessibility/setup_manual_data.py
#
# Uses the Google helpers of the Google bench (autotest/googlesync/gs_common.py) and the account file made by
# autotest/utilities/setup_test_account.py.
#

import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(REPO, "autotest", "googlesync"))
import gs_common as g                                      # noqa: E402

APPOINTMENTS = 5
SUMMARY = "AC setup"


def upload_fixture_data():
    """The six fixture contacts, and the appointments, on the Google account."""
    for n in range(1, 7):
        resource = g.create_google_contact(g.person_body(n))
        print("  contact %d: %s" % (n, resource))
    for i in range(1, APPOINTMENTS + 1):
        start = g.days_from_now(i, "10:00")
        end = g.days_from_now(i, "11:00")
        event = g.create_event("%s %d" % (g.PREFIX + SUMMARY, i), start, end)
        print("  appointment %d: %s" % (i, event))


def check_account_is_empty():
    people = g.fixture_people()
    events = g.test_events()
    if people or events:
        print("The account holds test data already: %d contact(s) and %d appointment(s)." % (len(people), len(events)))
        print("Clear it first: python3 autotest/utilities/clean_test_data.py --dry-run, then clean_test_data.py.")
        return False
    return True


def check_uploaded():
    people = g.fixture_people()
    events = [e for e in g.test_events() if (e.get("summary") or "").startswith(g.PREFIX + SUMMARY)]
    names = sorted(g.surname_of(p) for p in people)
    ok = len(people) == 6 and len(events) == APPOINTMENTS
    print("Google now holds %d fixture contact(s) (%s) and %d appointment(s)." % (len(people), ", ".join(names), len(events)))
    return ok


def main():
    if not check_account_is_empty():
        return 2
    print("Uploading the fixture data to the disposable Google account...")
    started = time.time()
    upload_fixture_data()
    print("Uploaded in %.0f seconds." % (time.time() - started))
    print()
    print("Next, by hand, in ContactManager (the copy signed in to the disposable account):")
    print("  1. Setup > Synchronisation Selection: set the calendar master and the contacts master to Google.")
    print("  2. Run a sync (F9), and wait until it reports that it has finished.")
    print("  3. Close Setup. The manual tests then start from this data.")
    print()
    try:
        input("Press Enter once the sync has finished (Ctrl+C to stop): ")
    except (KeyboardInterrupt, EOFError):
        print("\nStopped before the confirmation. The data is on Google; run the sync, then the manual tests.")
        return 1
    if check_uploaded():
        print("Ready for the manual tests (section 4), on a ContactManager that has synced to Google.")
        return 0
    print("The data on Google is not what was uploaded. Check the account and run the set-up again after clearing it.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
