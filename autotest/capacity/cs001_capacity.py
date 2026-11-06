#!/usr/bin/env python3
#
# CS-001: the capacity stress test (section 5 of documents/test-specification.md).
#
# Purges the disposable Google account, then uploads 250 contacts and 1,000 appointments, half by the Google API and half by
# ContactManager (its own sync uploads its half), and leaves them in place.  It is run on the scratch account only, when the
# capacity of the sync is in question.  It is not part of the normal cycle.
#
# The purge removes everything on the account, so this script asks the tester to type the account's address, as the
# clean-up utility does, before it starts.
#
# usage:
#     python3 autotest/capacity/cs001_capacity.py --driver PATH [--out DIR]
#
# The driver is the operation driver (autotest/operation/driver), built as in its readme.  Results go to --out: the driver's
# files, summary.json and report.md.
#
# Checks (the expected results of CS-001):
#   every one of the 250 contacts is on Google once (125 by the API, 125 by ContactManager's sync)
#   every one of the 1,000 appointments is on Google once (500 by the API, 500 by ContactManager's sync)
#   the sync finishes within its time: the measured figure for U01, 1,062 seconds (17 min 42 s), from the duration estimate
# The records are left in place: nothing is cleaned up afterwards.
#

import argparse
import json
import os
import shutil
import subprocess
import sys
import time
import urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
UTIL = os.path.join(REPO, "autotest", "utilities")
GS = os.path.join(REPO, "autotest", "googlesync")
sys.path.insert(0, GS)
sys.path.insert(0, UTIL)
import gs_common as g                                      # noqa: E402
import testaccount as ta                                   # noqa: E402

CONTACTS_API = 125
CONTACTS_APP = 125
APPOINTMENTS_API = 500
APPOINTMENTS_APP = 500
SYNC_LIMIT_SECONDS = 1062                                  # the measured U01 time (17 min 42 s); see the duration estimate
CONTACT_TAG = "cap"                                        # the email tag, so the records are found and counted
EVENT_TAG = "CAP-"                                         # the appointment summary tag, after the test prefix


def contact_email(kind, n):
    return "cap-%s-%03d@%s" % (kind, n, g.DOMAIN)


def slot(n):
    """The start and end of appointment n, spread over sixty days and the hours 09:00 to 18:00."""
    day = 1 + (n % 60)
    hour = 9 + ((n // 60) % 10)
    return g.days_from_now(day, "%02d:00" % hour), g.days_from_now(day, "%02d:30" % hour)


def purge(account_email):
    """The clean-up utility's full purge.  It asks for the account's address itself."""
    print("Purging the account %s (every contact and appointment). Type its address when asked." % account_email)
    rc = subprocess.call([sys.executable, os.path.join(UTIL, "clean_test_data.py"), "--all-data"])
    return rc == 0


def upload_by_api(report):
    started = time.time()
    for n in range(1, CONTACTS_API + 1):
        body = {"names": [{"givenName": "CAP", "familyName": "API %03d" % n}],
                "emailAddresses": [{"value": contact_email("api", n), "type": "home"}]}
        g.create_google_contact(body)
    for n in range(1, APPOINTMENTS_API + 1):
        start, end = slot(n)
        g.create_event("%sAPI %03d" % (g.PREFIX + EVENT_TAG, n), start, end)
    report["api_upload_seconds"] = round(time.time() - started, 1)
    print("Uploaded by the API in %.0f seconds." % report["api_upload_seconds"])


def sync_by_contactmanager(driver, out, report):
    home = os.path.join(out, "home")
    if os.path.isdir(home):
        shutil.rmtree(home)
    h = g.Home(home)
    ops = []
    for n in range(1, CONTACTS_APP + 1):
        ops.append({"op": "add_contact", "fields": {"surname": "APP %03d" % n, "names": "CAP",
                                                    "email": contact_email("app", n),
                                                    "mobile": "+44 7700 9%05d" % n}})
    for n in range(1, APPOINTMENTS_APP + 1):
        start, end = slot(n + APPOINTMENTS_API)
        ops.append({"op": "add_appointment", "fields": {"summary": "%sAPP %03d" % (g.PREFIX + EVENT_TAG, n),
                                                        "from": start, "to": end}})
    ops += [{"op": "save"}, {"op": "sync"}, {"op": "dump", "file": "dump.json"}]
    run = g.run_driver(driver, h, "cs001-sync", ops, os.path.join(out, "cs001-sync"), timeout=7200)
    report["sync_seconds"] = round(run.seconds, 1)
    report["driver_exit"] = str(run.rc)
    report["local_contacts"] = len(run.contacts)
    report["local_appointments"] = len(run.appointments)
    print("ContactManager's sync finished in %.0f seconds (exit %s)." % (run.seconds, run.rc))
    return run


def count_on_google():
    """The contacts on the account whose email is the capacity tag (listed for this count only; see the readme)."""
    emails = []
    token = None
    while True:
        q = {"personFields": "emailAddresses", "pageSize": 1000}
        if token:
            q["pageToken"] = token
        reply = g.api("GET", g.PEOPLE + "/people/me/connections?" + urllib.parse.urlencode(q))
        for p in reply.get("connections", []):
            for e in p.get("emailAddresses", []):
                value = e.get("value", "")
                if value.startswith(CONTACT_TAG + "-"):
                    emails.append(value)
        token = reply.get("nextPageToken")
        if not token:
            break
    return emails


def check(report, run):
    contacts = count_on_google()
    events = [e.get("summary") for e in g.test_events() if (e.get("summary") or "").startswith(g.PREFIX + EVENT_TAG)]
    results = [
        ("250 contacts are on Google", len(contacts) == CONTACTS_API + CONTACTS_APP,
         "%d found" % len(contacts)),
        ("each contact is on Google once", len(set(contacts)) == len(contacts), "%d distinct" % len(set(contacts))),
        ("1,000 appointments are on Google", len(events) == APPOINTMENTS_API + APPOINTMENTS_APP, "%d found" % len(events)),
        ("each appointment is on Google once", len(set(events)) == len(events), "%d distinct" % len(set(events))),
        ("the sync finishes within its time", report.get("sync_seconds", 1e9) <= SYNC_LIMIT_SECONDS,
         "%s s, limit %d s" % (report.get("sync_seconds"), SYNC_LIMIT_SECONDS)),
        ("ContactManager holds all 250 contacts and 1,000 appointments after the sync",
         run.contacts and len(run.contacts) == CONTACTS_API + CONTACTS_APP
         and len(run.appointments) == APPOINTMENTS_API + APPOINTMENTS_APP,
         "%s contacts, %s appointments" % (len(run.contacts), len(run.appointments))),
    ]
    return results


def main(argv=None):
    p = argparse.ArgumentParser(description="CS-001: the capacity stress test (scratch account only).")
    p.add_argument("--driver", required=True, help="the built operation driver")
    p.add_argument("--out", default=None, help="where the results go (default: a folder in your home folder)")
    args = p.parse_args(argv)
    if not os.path.exists(args.driver):
        print("the driver is not built: %s" % args.driver)
        return 2
    acc = ta.load_account()["google"]
    email = acc.get("account_email", "")
    typed = input("This purges every contact and appointment on %s. Type the address to go on: " % email).strip()
    if typed.lower() != email.lower():
        print("The address does not match. Nothing was done.")
        return 1

    out = args.out or os.path.join(os.path.expanduser("~"), "contactmanager-test-runs",
                                   "cs001-" + time.strftime("%Y%m%d-%H%M%S"))
    os.makedirs(out, exist_ok=True)
    report = {"account": email, "started": time.strftime("%Y-%m-%d %H:%M:%S")}
    started = time.time()
    if not purge(email):
        print("The purge did not finish. Nothing was uploaded.")
        return 1
    report["purge_seconds"] = round(time.time() - started, 1)
    upload_by_api(report)
    run = sync_by_contactmanager(args.driver, out, report)
    results = check(report, run)
    passed = all(ok for _label, ok, _detail in results)
    report["results"] = [{"check": label, "ok": ok, "detail": detail} for label, ok, detail in results]
    report["total_seconds"] = round(time.time() - started, 1)
    with open(os.path.join(out, "summary.json"), "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1)
    lines = ["# CS-001: capacity stress test", "", "Account %s, %s." % (email, report["started"]), ""]
    lines.append("Result: %s." % ("PASS" if passed else "FAIL"))
    lines.append("")
    for label, ok, detail in results:
        lines.append("- %s %s (%s)" % ("pass" if ok else "**FAIL**", label, detail))
    lines.append("")
    lines.append("Purge %s s, API upload %s s, ContactManager sync %s s, total %s s." % (
        report.get("purge_seconds"), report.get("api_upload_seconds"), report.get("sync_seconds"), report["total_seconds"]))
    lines.append("The records are left in place.")
    with open(os.path.join(out, "report.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("\n".join(lines))
    print("Evidence: %s" % out)
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
