#!/usr/bin/env python3
#
# Google sync tests: area 1 of documents/test-specification.md, section 1 (GS-001 to GS-041).
#
# Each test runs the program headless through the sync driver (driver/sync_driver.cpp, built by driver/build_driver.sh)
# and checks what it wrote: the driver's dump of the local contacts and appointments, the files in the data folder, and
# the records on Google (People and Calendar APIs, through gs_common).
#
# usage:
#     python3 googlesync_tests.py --list
#     python3 googlesync_tests.py [--driver PATH] [--out DIR] [GS-008 GS-010 ...]
#
# Tests run one after another, never in parallel (Google rate limits).  Each test, and each configuration of it (CM or G),
# starts from an empty test account and clears up afterwards.  A test named on its own runs on its own, merged tests
# included (see section 1 of the specification).  The evidence (homes, dumps, logs, results) is written under --out, which
# is outside the repository.
#
# Nothing here imports from, or reads, autotest/archive.
#

import argparse
import ast
import datetime
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
import traceback
import urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import gs_common as g                 # noqa: E402  (also puts autotest/utilities on the path)
import googleclient as gc             # noqa: E402
import testaccount as ta              # noqa: E402

CM = "CM"                             # ContactManager is the master
G = "G"                               # Google is the master
BOTH = (CM, G)
FMT = "%Y-%m-%dT%H:%M:%SZ"
DEFAULT_DRIVER = os.path.join(os.path.expanduser("~"), "build-contactmanager", "sync_driver")
DEFAULT_OUT = os.path.join(os.path.expanduser("~"), "contactmanager-test-runs")

# Values used by the tests (the phone numbers are in the fixture range, +44 7700 900000 to 900999)
NEW_M1 = "+44 7700 900011"
NEW_M2 = "+44 7700 900022"
NEW_M3 = "+44 7700 900033"
NEW_M4 = "+44 7700 900044"
NEW_EMAIL2 = "samantha.carter.new@sgc.contacmanagertest"
SUR_APP = "Jackson (App)"
SUR_GOOGLE = "Jackson (Google)"
NOTE2_TODO = "Send Samantha the file"
NOTE2_HIST = "Left a message about the file"
NOTE3_TODO = "Ring Daniel about the base"
NOTE3_HIST = "Met at the base, asked about the stones"


# ------------------------------------------------------------------------------------------------ the test context

class Ctx:
    """What one test (in one configuration) gets: its homes and runs, and the checks it adds."""

    def __init__(self, test_id, config, driver, folder):
        self.test_id = test_id
        self.config = config
        self.google_master = config == G
        self.driver = driver
        self.folder = folder
        self.checks = []
        self.steps = 0

    def home(self, name="home", ini_extra=""):
        h = g.Home(os.path.join(self.folder, name), ini_extra)
        h.set_master(self.google_master)
        return h

    def run(self, home, label, ops, **kw):
        """One run of the driver on the home.  Its exit is checked; the caller checks the rest."""
        self.steps += 1
        out = os.path.join(self.folder, "%02d-%s" % (self.steps, re.sub(r"[^A-Za-z0-9]+", "-", label).strip("-")))
        r = g.run_driver(self.driver, home, label, ops, out, **kw)
        self.check("%s: the program finished" % label, r.rc == 0, "exit status %s" % (r.rc,))
        return r

    def check(self, label, condition, detail=""):
        self.checks.append(g.check(label, condition, detail))

    def blocked(self, label, reason):
        self.checks.append(g.blocked(label, reason))


TESTS = []


def gs(test_id, title, configs=BOTH):
    """Registers a test.  configs is CM, G, or both (the Setup choice of section 0.3)."""
    def wrap(fn):
        TESTS.append((test_id, title, tuple(configs), fn))
        return fn
    return wrap


# ------------------------------------------------------------------------------------------------ small helpers

def digits(text):
    return re.sub(r"\D", "", str(text or ""))


def sid(test_id, label):
    """A test appointment's summary: it starts with the test prefix (so it is found and removed) and the test number."""
    return "%s%s %s" % (g.PREFIX, test_id, label)


def cid(contact):
    return str((contact or {}).get("id", ""))


def app_one(run, n, surname=None):
    """The program's contact for fixture n (or the surname given), from the driver's dump; {} when there is none."""
    want = surname or g.fixture(n)["surname"]
    hits = [c for c in run.contacts if c.get("surname") == want]
    return hits[0] if hits else {}


def app_appts(run, summary):
    """The program's appointments with this summary that are not deleted."""
    return [a for a in run.appointments if a.get("summary") == summary and not a.get("deleted")]


def at(days, hhmm="10:00", minutes=60):
    """Start and end, in UTC text, for an appointment that starts the given number of days from today."""
    start = datetime.datetime.strptime(g.days_from_now(days, hhmm), FMT)
    end = start + datetime.timedelta(minutes=minutes)
    return start.strftime(FMT), end.strftime(FMT)


def day_of(days):
    return g.days_from_now(days)[:10]


def london_hour_of(text):
    """The London hour of a program date-time.  Text with no zone is the program's own London time."""
    text = str(text or "")
    if len(text) < 13:
        return -1                                  # no time in it: never 14, so a test using it fails
    if not re.search(r"(Z|[+-]\d\d:?\d\d)$", text):
        return int(text[11:13])
    return g.london_hour(text)


def window_bounds():
    """The calendar window as Google's instances list takes it: now, and 730 days on."""
    now = datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0)
    return now.strftime(FMT), (now + datetime.timedelta(days=730)).strftime(FMT)


def instances(event_id, time_min, time_max):
    """Every occurrence of a repeat on Google, cancelled ones included (Google marks them, it does not drop them)."""
    out, page_token = [], None
    while True:
        q = {"timeMin": time_min, "timeMax": time_max, "showDeleted": "true", "maxResults": 250}
        if page_token:
            q["pageToken"] = page_token
        page = g.api("GET", g.CALENDAR + "/events/%s/instances?%s" % (event_id, urllib.parse.urlencode(q)))
        out += page.get("items", [])
        page_token = page.get("nextPageToken")
        if not page_token:
            return out


def live_days(items):
    """The days of the occurrences that are not cancelled, in order."""
    return sorted(i["start"]["dateTime"][:10] for i in items
                  if i.get("status") != "cancelled" and "dateTime" in i.get("start", {}))


def create_linked_event(n, summary, start, end, recurrence=None, email=None):
    """A Google event with the contact as an attendee.  That is the link this bench makes from Google to the contact;
    the program's rule for it is confirmed by the tests that read the link back from the program (GS-015)."""
    if not summary.startswith(g.PREFIX):
        raise ValueError("a test event must start with %r" % g.PREFIX)
    body = {"summary": summary, "start": {"dateTime": start}, "end": {"dateTime": end},
            "attendees": [{"email": email or g.fixture(n)["email"]}]}
    if recurrence:
        body["recurrence"] = recurrence
    return g.api("POST", g.CALENDAR + "/events", body)["id"]


def google_for(n, surname=None, email=None):
    """The Google contacts that are fixture n: the surname or email the test expects (the test may have changed them)."""
    want_email = {g.fixture(n)["email"]}
    if email:
        want_email.add(email)
    want_surname = surname or g.fixture(n)["surname"]
    return [p for p in g.fixture_people()
            if g.surname_of(p) == want_surname or want_email & set(g.emails_of(p))]


def google_phones(person):
    return [digits(p.get("value")) for p in person.get("phoneNumbers", [])]


def fixture_problems(person, n):
    """The differences between a Google person and fixture n.  Empty when they match."""
    f = g.fixture(n)
    problems = []
    names = (person.get("names") or [{}])[0]
    if names.get("givenName") != f["given"]:
        problems.append("first name")
    if names.get("familyName") != f["surname"]:
        problems.append("surname")
    if f["email"] not in g.emails_of(person):
        problems.append("email")
    if digits(f["phone"]) not in google_phones(person):
        problems.append("mobile")
    births = person.get("birthdays") or []
    if f["birthday"]:
        y, m, d = (int(x) for x in f["birthday"].split("-"))
        if not any((b.get("date") or {}) == {"year": y, "month": m, "day": d} for b in births):
            problems.append("birthday")
    elif births:
        problems.append("a birthday the fixture does not have")
    return problems


def app_problems(c, n):
    """The differences between the program's contact (from the dump) and fixture n."""
    f = g.fixture(n)
    problems = []
    if c.get("names") != f["given"]:
        problems.append("first name")
    if c.get("surname") != f["surname"]:
        problems.append("surname")
    if c.get("email") != f["email"]:
        problems.append("email")
    if digits(c.get("mobile")) != digits(f["phone"]):
        problems.append("mobile")
    return problems


def check_google_copies(t, ns=(1, 2, 3, 4, 5, 6), when="on Google"):
    """Each fixture contact is on Google once, with its values."""
    for n in ns:
        copies = google_for(n)
        t.check("contact %d: one copy %s" % (n, when), len(copies) == 1, "%d copies" % len(copies))
        if len(copies) == 1:
            problems = fixture_problems(copies[0], n)
            t.check("contact %d: values %s" % (n, when), not problems, ", ".join(problems))


def snapshot(home):
    """A hash of every file in the contact and calendar folders, so a test can show that nothing local changed."""
    out = {}
    for sub in ("contact", "calendar"):
        root = os.path.join(home.data, sub)
        for folder, _dirs, names in os.walk(root):
            for name in names:
                path = os.path.join(folder, name)
                with open(path, "rb") as f:
                    out[os.path.relpath(path, home.data)] = hashlib.sha256(f.read()).hexdigest()
    return out


def files_containing(home, needles):
    """Every file in the data folder that holds one of the needles as plain bytes."""
    hits = []
    for folder, _dirs, names in os.walk(home.data):
        for name in names:
            with open(os.path.join(folder, name), "rb") as f:
                blob = f.read()
            for needle in needles:
                if needle.encode("utf-8") in blob:
                    hits.append((name, needle))
    return hits


def check_gone(t, home, run, n, contact_id, label):
    """Contact n is gone from Google and from the program, and its files are in contact/deleted, not contact."""
    left = google_for(n)
    t.check("%s: contact %d is gone from Google" % (label, n), left == [], "%d left" % len(left))
    t.check("%s: contact %d is not in the program" % (label, n), app_one(run, n) == {})
    if contact_id:
        in_deleted = [f for f in home.files("contact/deleted") if f.startswith(contact_id + ".")]
        still_here = [f for f in home.files("contact") if f.startswith(contact_id + ".")]
        t.check("%s: contact %d's files are in contact/deleted" % (label, n), len(in_deleted) >= 1,
                "%d files" % len(in_deleted))
        t.check("%s: contact %d's files are no longer in contact" % (label, n), not still_here,
                ", ".join(still_here))
    else:
        t.check("%s: contact %d has an ID to find its files by" % (label, n), False, "no ID in the dump")


def setup_contacts():
    return g.add_fixture_contacts() + [g.save(), g.sync()]


# ------------------------------------------------------------------------------------------------ 1.1 contacts

@gs("GS-001", "The six fixture contacts created in ContactManager reach Google once, with their values")
def gs_001(t):
    home = t.home()
    t.run(home, "create and sync", g.add_fixture_contacts() + [g.save(), g.sync(), g.dump("d1.json")])
    check_google_copies(t)
    r2 = t.run(home, "second sync", [g.sync(), g.dump("d2.json")])
    check_google_copies(t, when="after a second sync")
    t.check("the second sync adds no contact to the program", len(r2.contacts) == 6, "%d contacts" % len(r2.contacts))
    t.check("the second sync adds no contact file", len(home.files("contact", ".contact")) == 6,
            "%d files" % len(home.files("contact", ".contact")))


@gs("GS-002", "A fixture contact made on Google downloads once, with its contact and history files written")
def gs_002(t):
    home = t.home()
    rid = g.create_google_contact(g.person_body(2))
    r1 = t.run(home, "download", [g.sync(), g.dump("d1.json")])
    c = app_one(r1, 2)
    t.check("contact 2 is in the program, once", len([x for x in r1.contacts if x.get("surname") == "Carter"]) == 1)
    t.check("contact 2 has its values", bool(c) and not app_problems(c, 2), ", ".join(app_problems(c, 2)))
    gid = rid.split("/")[-1]
    keys = [k for k in c if "google" in k.lower()]
    t.check("contact 2 keeps its Google ID", any(gid in str(c[k]) for k in keys), "keys with google: %s" % keys)
    t.check("contact 2's contact file is written", len(home.files("contact", ".contact")) == 1,
            "%d files" % len(home.files("contact", ".contact")))
    files_before = home.files("contact")
    r2 = t.run(home, "second sync", [g.sync(), g.dump("d2.json")])
    t.check("the second sync adds nothing to the program", len([x for x in r2.contacts if x.get("surname") == "Carter"]) == 1)
    t.check("the second sync writes no new file", home.files("contact") == files_before)
    t.check("contact 2 is on Google once", len(google_for(2)) == 1, "%d copies" % len(google_for(2)))


@gs("GS-003", "Edits made in ContactManager and on Google each reach the other side in one sync, and are recorded in history.",
    configs=(CM,))
def gs_003(t):
    home = t.home()
    t.run(home, "set-up", setup_contacts())
    p2 = google_for(2)[0]
    g.update_google_contact(p2["resourceName"], "emailAddresses", [{"value": NEW_EMAIL2, "type": "home"}])
    r = t.run(home, "edits on both sides", [g.edit_contact(1, {"mobile": NEW_M1}), g.save(), g.sync(), g.dump("d2.json")])
    p1 = google_for(1)
    t.check("contact 1: one copy on Google", len(p1) == 1, "%d copies" % len(p1))
    t.check("Google has the new mobile for contact 1", bool(p1) and digits(NEW_M1) in google_phones(p1[0]))
    t.check("the program has the new email for contact 2", app_one(r, 2).get("email") == NEW_EMAIL2,
            str(app_one(r, 2).get("email")))
    t.check("contact 1 has a history entry", "history" in app_one(r, 1))
    t.check("contact 2 has a history entry", "history" in app_one(r, 2))
    t.check("contact 2: one copy on Google", len(google_for(2, email=NEW_EMAIL2)) == 1)


@gs("GS-005", "When both sides change the same field, the master's version is kept, and an edit made only in the app is replaced when Google is master.")
def gs_005(t):
    home = t.home()
    t.run(home, "set-up", setup_contacts())
    p3 = google_for(3)[0]
    g.update_google_contact(p3["resourceName"], "names", [{"givenName": "Daniel", "familyName": SUR_GOOGLE}])
    if t.config == CM:
        r = t.run(home, "both sides changed, ContactManager master", [g.edit_contact(3, {"surname": SUR_APP}), g.save(),
                                                                       g.sync(), g.dump("d2.json")])
        now = google_for(3, surname=SUR_APP)
        t.check("Google has the program's surname", len(now) == 1 and g.surname_of(now[0]) == SUR_APP,
                "%s" % [g.surname_of(p) for p in now])
        t.check("the report says the program's version was sent", r.logged("sent"),
                "report wording is matched loosely; tighten once confirmed")
    else:
        r = t.run(home, "both sides changed, Google master", [g.edit_contact(3, {"surname": SUR_APP}), g.save(),
                                                              g.sync(), g.dump("d2.json")])
        t.check("the program holds Google's surname", bool(app_one(r, 3, surname=SUR_GOOGLE)))
        t.check("a history entry is made", "history" in app_one(r, 3, surname=SUR_GOOGLE))
        p4 = google_for(4)[0]
        etag4 = g.google_person(p4["resourceName"])["etag"]
        r2 = t.run(home, "phone changed in the program only, Google master",
                   [g.edit_contact(4, {"mobile": NEW_M4}), g.save(), g.sync(), g.dump("d3.json")])
        t.check("the program has Google's phone for contact 4", digits(g.fixture(4)["phone"]) == digits(app_one(r2, 4).get("mobile")),
                str(app_one(r2, 4).get("mobile")))
        t.check("nothing was uploaded for contact 4", g.google_person(p4["resourceName"])["etag"] == etag4)


@gs("GS-008", "A fixture contact deleted in the app is deleted on Google under either master, and its files move to the deleted folder.")
def gs_008(t):
    home = t.home()
    setup = t.run(home, "set-up", g.add_fixture_contacts() + [g.save(), g.sync(), g.dump("d1.json")])
    id5 = cid(app_one(setup, 5))
    r = t.run(home, "delete in the program, sync, tidy", [g.delete_contact_in_app(5), g.save(), g.sync(), g.tidy(),
                                                          g.dump("d2.json")])
    check_gone(t, home, r, 5, id5, "after the delete")
    if t.config == G:
        r3 = t.run(home, "sync again, Google master", [g.sync(), g.dump("d3.json")])
        t.check("contact 5 is not brought back by the next sync", app_one(r3, 5) == {} and google_for(5) == [])


@gs("GS-010", "A fixture contact deleted on Google is removed from the app, not restored")
def gs_010(t):
    home = t.home()
    setup = t.run(home, "set-up", g.add_fixture_contacts() + [g.save(), g.sync(), g.dump("d1.json")])
    id6 = cid(app_one(setup, 6))
    copies = google_for(6)
    t.check("contact 6 is on Google before the delete", len(copies) == 1, "%d copies" % len(copies))
    if copies:
        g.delete_google_contact(copies[0]["resourceName"])
    r = t.run(home, "sync after the delete on Google", [g.sync(), g.dump("d2.json")])
    t.check("contact 6 is removed from the program", app_one(r, 6) == {})
    in_deleted = [f for f in home.files("contact/deleted") if id6 and f.startswith(id6 + ".")]
    t.check("contact 6's files are in contact/deleted", len(in_deleted) >= 1, "%d files" % len(in_deleted))
    t.check("contact 6 is not restored on Google", google_for(6) == [], "%d copies" % len(google_for(6)))


@gs("GS-011", "A hidden fixture contact stays on Google as hidden and is not messaged")
def gs_011(t):
    home = t.home()
    t.run(home, "set-up", setup_contacts())
    r = t.run(home, "hide contact 1, sync", [g.hide_contact(1), g.save(), g.sync(), g.dump("d2.json")])
    c1 = app_one(r, 1)
    t.check("contact 1 stays in the program, marked hidden", bool(c1) and c1.get("hidden") is True,
            "hidden=%s" % c1.get("hidden"))
    t.check("contact 1 stays on Google, once", len(google_for(1)) == 1, "%d copies" % len(google_for(1)))
    t.blocked("Google shows contact 1 as hidden",
              "the bench does not know how the program marks a hidden contact on Google; confirm it, then check it here")
    t.blocked("no message is sent for contact 1", "messages are checked in area 2 (AN-027)")


@gs("GS-012", "History and to-do files survive edits on Google, a deletion and a restore, both plain and encrypted.")
def gs_012(t):
    home = t.home()
    t.run(home, "set-up", setup_contacts())
    r2 = t.run(home, "notes on contacts 2 and 3", [g.contact_notes(3, NOTE3_TODO, NOTE3_HIST),
                                                  g.contact_notes(2, NOTE2_TODO, NOTE2_HIST), g.save(), g.sync(),
                                                  g.dump("d2.json")])
    c3 = app_one(r2, 3)
    t.check("contact 3 has its to-do and history", NOTE3_TODO == c3.get("todo") and NOTE3_HIST in c3.get("history", ""))
    p3 = google_for(3)[0]
    g.update_google_contact(p3["resourceName"], "phoneNumbers", [{"value": NEW_M3, "type": "mobile"}])
    t.run(home, "edit on Google, sync", [g.sync(), g.dump("d3.json")])
    r4 = t.run(home, "restart", [g.dump("d4.json")])
    c3 = app_one(r4, 3)
    t.check("contact 3's to-do is unchanged after the Google edit", c3.get("todo") == NOTE3_TODO, str(c3.get("todo")))
    t.check("contact 3's history is unchanged after the Google edit", NOTE3_HIST in c3.get("history", ""))
    t.check("contact 3's mobile is the Google edit", digits(NEW_M3) == digits(c3.get("mobile")), str(c3.get("mobile")))
    plain_hits = files_containing(home, [NOTE3_TODO])
    t.check("the plain-text control: the note is readable in the plain files", len(plain_hits) >= 1,
            "found in %d files" % len(plain_hits))

    # delete contact 2, tidy, move its three files back, and restart
    id2 = cid(app_one(r2, 2))
    r5 = t.run(home, "delete contact 2, sync, tidy", [g.delete_contact_in_app(2), g.save(), g.sync(), g.tidy(),
                                                     g.dump("d5.json")])
    t.check("contact 2 is not in the program after the delete", app_one(r5, 2) == {})
    moved = [f for f in home.files("contact/deleted") if id2 and f.startswith(id2 + ".")]
    t.check("contact 2's three files are in contact/deleted", len(moved) == 3, "%d files" % len(moved))
    for name in moved:
        shutil.move(os.path.join(home.data, "contact", "deleted", name), os.path.join(home.data, "contact", name))
    r6 = t.run(home, "restart after moving the files back", [g.dump("d6.json")])
    c2 = app_one(r6, 2)
    t.check("contact 2 is restored", bool(c2) and not app_problems(c2, 2), ", ".join(app_problems(c2, 2)))
    t.check("contact 2's to-do is restored", c2.get("todo") == NOTE2_TODO, str(c2.get("todo")))
    t.check("contact 2's history is restored", NOTE2_HIST in c2.get("history", ""))

    # encrypted: a clean account, and a home whose database is encrypted from the start
    g.clean_account()
    enc = t.home("encrypted", ini_extra="\n[database]\nencrypted=yes\n")
    t.run(enc, "encrypted set-up", g.add_fixture_contacts() + [g.contact_notes(3, NOTE3_TODO, NOTE3_HIST),
                                                              g.contact_notes(2, NOTE2_TODO, NOTE2_HIST), g.save(),
                                                              g.sync(), g.dump("e1.json")])
    t.check("the encrypted files are written", len(enc.files("contact", ".zcontact")) >= 1,
            "%d files" % len(enc.files("contact", ".zcontact")))
    needles = [g.fixture(n)["given"] for n in range(1, 7)] + [g.fixture(n)["surname"] for n in range(1, 7)]
    needles += [NOTE3_TODO, NOTE3_HIST, NOTE2_TODO, NOTE2_HIST]
    hits = files_containing(enc, needles)
    t.check("no name or note appears as plain text in the encrypted folder", not hits,
            "found: %s" % ", ".join("%s in %s" % (n, f) for f, n in hits[:5]))


@gs("GS-015", "An appointment added in the app, or made on Google, reaches the other side once, linked to its contact.")
def gs_015(t):
    home = t.home()
    s_app, e_app = at(5)
    s_goog, e_goog = at(6)
    app_summary = sid("GS-015", "made in the app for contact 1")
    goog_summary = sid("GS-015", "made on Google for contact 2")
    create_linked_event(2, goog_summary, s_goog, e_goog)
    r = t.run(home, "add, sync twice", g.add_fixture_contacts() + [g.add_appointment(1, app_summary, s_app, e_app),
                                                                  g.save(), g.sync(), g.sync(), g.dump("d1.json")])
    t.check("one event on Google for the appointment made in the app", len(g.test_events(app_summary)) == 1,
            "%d events" % len(g.test_events(app_summary)))
    t.check("one appointment in the program for the appointment made on Google", len(app_appts(r, goog_summary)) == 1,
            "%d entries" % len(app_appts(r, goog_summary)))
    linked = [a for a in app_appts(r, goog_summary) if g.fixture(2)["surname"] in a.get("for", "")]
    t.check("the appointment made on Google is linked to contact 2", len(linked) == 1,
            "for=%r" % (app_appts(r, goog_summary) or [{}])[0].get("for"))
    t.check("no copy of the appointment made in the app comes back", len(app_appts(r, app_summary)) == 1)
    t.blocked("the link to contact 1 on Google",
              "the program's rule for the link it writes to Google is not confirmed; the bench checks the program's side")


@gs("GS-017", "An appointment moved or deleted on one side reaches the other.")
def gs_017(t):
    home = t.home()
    c3 = sid("GS-017", "contact 3 on Google")
    c4_app = sid("GS-017", "contact 4 in the app")
    c4_goog = sid("GS-017", "contact 4 on Google")
    eid3 = create_linked_event(3, c3, *at(12))
    ops = g.add_fixture_contacts()
    eid4 = None
    if t.config == CM:
        ops.append(g.add_appointment(4, c4_app, *at(13)))
    else:
        eid4 = create_linked_event(4, c4_goog, *at(13))
    r1 = t.run(home, "set-up", ops + [g.save(), g.sync(), g.dump("d1.json")])
    t.check("contact 3's appointment is in the program", len(app_appts(r1, c3)) == 1)
    ms, me = at(14, "14:00")
    g.patch_event(eid3, {"start": {"dateTime": ms}, "end": {"dateTime": me}})
    r2 = t.run(home, "moved on Google, sync", [g.sync(), g.dump("d2.json")])
    moved = app_appts(r2, c3)
    t.check("the move reaches the program as one entry", len(moved) == 1, "%d entries" % len(moved))
    t.check("the program has the new time (14:00 London)", bool(moved) and london_hour_of(moved[0].get("from")) == 14,
            "from=%r" % (moved[0].get("from") if moved else None))
    if t.config == CM:
        r3 = t.run(home, "deleted in the program, sync", [g.delete_appointment_in_app(c4_app), g.save(), g.sync(),
                                                          g.dump("d3.json")])
        t.check("contact 4's deleted appointment is gone from Google", g.test_events(c4_app) == [],
                "%d left" % len(g.test_events(c4_app)))
        t.check("and not in the program", app_appts(r3, c4_app) == [])
    else:
        g.delete_event(eid4)
        r3 = t.run(home, "deleted on Google, sync", [g.sync(), g.dump("d3.json")])
        t.check("the deleted appointment is removed from the program", app_appts(r3, c4_goog) == [])
        r4 = t.run(home, "sync again, Google master", [g.sync(), g.dump("d4.json")])
        t.check("it is not restored", app_appts(r4, c4_goog) == [] and g.test_events(c4_goog) == [])


@gs("GS-020", "A weekly repeat is uploaded, one occurrence can be deleted, and the repeat can be split, with no duplicates.")
def gs_020(t):
    home = t.home()
    t.run(home, "set-up", setup_contacts())
    summary = sid("GS-020", "weekly for contact 5")
    split = sid("GS-020", "changed from the fifth week")
    s, e = at(2, "09:00")
    r2 = t.run(home, "add the repeat, sync", [g.add_appointment(5, summary, s, e, repeat="2", repeatinterval="1"),
                                             g.save(), g.sync(), g.dump("d2.json")])
    t.check("the repeat is stored once in the program", len(app_appts(r2, summary)) == 1)
    events = g.test_events(summary)
    t.check("one repeating event on Google", len(events) == 1 and bool(events[0].get("recurrence")),
            "%d events" % len(events))
    if not events:
        return
    eid = events[0]["id"]
    now_text, end_text = window_bounds()
    before = live_days(instances(eid, now_text, end_text))
    expected = g.in_window_repeat_count("09:00")
    t.check("Google lists the repeat on each week in the window", len(before) == expected,
            "%d occurrences, %d expected" % (len(before), expected))

    # delete the third occurrence in the program
    third = day_of(2 + 14)
    r3 = t.run(home, "delete one occurrence, sync",
               [{"op": "choose", "letter": "a"},
                {"op": "repeat_delete", "summary": summary, "occurrence": g.days_from_now(2 + 14, "09:00")[:-1]},
                g.save(), g.sync(), g.dump("d3.json")])
    after = live_days(instances(eid, now_text, end_text))
    t.check("Google omits that date", third not in after)
    t.check("the other dates stay", after == [d for d in before if d != third],
            "%d left, %d before" % (len(after), len(before)))

    # change the fifth occurrence and all that follow
    fifth = day_of(2 + 28)
    r4 = t.run(home, "change one occurrence and all that follow, sync",
               [{"op": "choose", "letter": "b"},
                {"op": "repeat_edit", "name": "gs020", "summary": summary,
                 "occurrence": g.days_from_now(2 + 28, "09:00")[:-1], "shiftdays": 0, "shiftminutes": 60,
                 "newsummary": split},
                g.save(), g.sync(), g.dump("d4.json")])
    t.check("Google has the earlier repeat, once", len(g.test_events(summary)) == 1,
            "%d events" % len(g.test_events(summary)))
    later = g.test_events(split)
    t.check("Google has the later repeat, once", len(later) == 1, "%d events" % len(later))
    if later:
        starts = live_days(instances(later[0]["id"], now_text, end_text))
        t.check("the later repeat begins on the changed occurrence", bool(starts) and starts[0] == fifth,
                "starts %s" % (starts[:1],))
    t.check("the program has one entry for each part", len(app_appts(r4, summary)) == 1 and len(app_appts(r4, split)) == 1)


@gs("GS-023", "Appointments outside the calendar window are not downloaded, and an archived appointment is restored when it moves back into the window.")
def gs_023(t):
    home = t.home()
    t.run(home, "set-up", setup_contacts())
    s_out = sid("GS-023", "before the window")
    s_in = sid("GS-023", "expired, then archived")
    create_linked_event(6, s_out, *at(-200))
    eid_in = create_linked_event(6, s_in, *at(-10))
    r2 = t.run(home, "sync with the window", [g.sync(), g.dump("d2.json")])
    t.check("the appointment before the window is not in the program", app_appts(r2, s_out) == [])
    t.check("and it is still on Google", len(g.test_events(s_out)) == 1)
    t.check("the expired appointment inside the window is in the program", len(app_appts(r2, s_in)) == 1)
    r3 = t.run(home, "narrow the window, sync", [g.setting("windowbefore", "5"), g.sync(), g.dump("d3.json")])
    archived = home.files("calendar/archive")
    t.check("the expired appointment is archived locally", len(archived) == 1, "%d files" % len(archived))
    t.check("it is still on Google", len(g.test_events(s_in)) == 1)
    t.check("it is no longer in the program's calendar", app_appts(r3, s_in) == [])
    g.patch_event(eid_in, {"start": {"dateTime": at(-2)[0]}, "end": {"dateTime": at(-2)[1]}})
    r4 = t.run(home, "moved into the window on Google, sync", [g.sync(), g.dump("d4.json")])
    t.check("it is restored into the program", len(app_appts(r4, s_in)) == 1)
    t.check("and gone from the archive", len(home.files("calendar/archive")) == 0,
            "%d files" % len(home.files("calendar/archive")))
    t.check("the appointment outside the window is still on Google, once", len(g.test_events(s_out)) == 1)


@gs("GS-025", "Appointments that cannot be uploaded are not uploaded, the report says why, and the others sync.")
def gs_025(t):
    home = t.home()
    t.run(home, "set-up", setup_contacts())
    no_end = sid("GS-025", "no end time")
    dated_1970 = sid("GS-025", "dated 1 January 1970")
    good = sid("GS-025", "good one")
    no_end_start, _no_end_end = at(3)
    r2 = t.run(home, "bad and good appointments, sync", [
        g.add_appointment(1, no_end, no_end_start, ""),
        g.add_appointment(2, dated_1970, "1970-01-01T10:00:00Z", "1970-01-01T11:00:00Z"),
        g.add_appointment(3, good, *at(4)),
        g.save(), g.sync(), g.dump("d2.json")])
    t.check("the appointment with no end is not on Google", g.test_events(no_end) == [])
    t.check("the appointment dated 1970 is not on Google", g.test_events(dated_1970) == [])
    t.check("the good appointment is on Google once", len(g.test_events(good)) == 1, "%d events" % len(g.test_events(good)))
    t.check("the report names the two that were not uploaded", r2.logged(no_end) and r2.logged(dated_1970))
    other = sid("GS-025", "organised by another person")
    ext = g.import_external_event(other, *at(9))
    etag = g.get_event(ext)["etag"]
    r3 = t.run(home, "organiser is another person, sync twice", [g.sync(), g.sync(), g.dump("d3.json")])
    evs = g.test_events(other)
    t.check("no upload: the event on Google is the same one, unchanged", len(evs) == 1 and evs[0]["id"] == ext
            and evs[0].get("etag") == etag)
    t.check("the program has one copy, from its download", len(app_appts(r3, other)) == 1,
            "%d entries" % len(app_appts(r3, other)))


@gs("GS-028", "An appointment crossing the clocks going back keeps its London time on Google and in the app")
def gs_028(t):
    home = t.home()
    summary = sid("GS-028", "weekly 14:00 across the clock change")
    # 14:00 London time while British Summer Time holds (13:00 UTC)
    s, e = "2027-10-20T13:00:00Z", "2027-10-20T14:00:00Z"
    r = t.run(home, "add the repeat, sync", g.add_fixture_contacts() + [
        g.add_appointment(1, summary, s, e, repeat="2", repeatinterval="1"), g.save(), g.sync(), g.dump("d1.json")])
    events = g.test_events(summary)
    t.check("one repeating event on Google", len(events) == 1 and bool(events[0].get("recurrence")))
    if events:
        items = instances(events[0]["id"], "2027-10-01T00:00:00Z", "2027-12-01T00:00:00Z")
        live = [i for i in items if i.get("status") != "cancelled"]
        hours = sorted(set(g.london_hour(i["start"]["dateTime"]) for i in live))
        t.check("Google: every occurrence is at 14:00 London time", hours == [14], "hours %s" % hours)
        days = live_days(items)
        t.check("Google: occurrences on both sides of the change", "2027-10-24" in days and "2027-11-07" in days,
                "%d occurrences" % len(days))
    mine = app_appts(r, summary)
    t.check("the program has the repeat once", len(mine) == 1, "%d entries" % len(mine))
    t.check("the program shows 14:00 London time", bool(mine) and london_hour_of(mine[0].get("from")) == 14,
            "from=%r" % (mine[0].get("from") if mine else None))


@gs("GS-029", "An appointment for a deleted contact is kept, shows as \"Deleted contact\", and is not changed on Google until edited")
def gs_029(t):
    home = t.home()
    summary = sid("GS-029", "for contact 1, who is deleted")
    t.run(home, "set-up", g.add_fixture_contacts() + [g.add_appointment(1, summary, *at(5)), g.save(), g.sync(),
                                                     g.dump("d1.json")])
    before = g.test_events(summary)
    etag = before[0]["etag"] if before else None
    t.check("the appointment is on Google before the delete", len(before) == 1)
    r = t.run(home, "delete contact 1, sync, show the calendar", [g.delete_contact_in_app(1), g.save(), g.sync(),
                                                               g.dump("d2.json"),
                                                               {"op": "calendar_rows", "name": "gs029", "reload": True}])
    kept = app_appts(r, summary)
    t.check("the appointment is kept in the program", len(kept) == 1, "%d entries" % len(kept))
    titles = r.results.get("gs029_titles", [])
    t.check("the calendar shows it as Deleted contact", any("Deleted contact" in str(x) for x in titles),
            "no row says Deleted contact")
    after = g.test_events(summary)
    t.check("unchanged on Google (same event, same etag)", len(after) == 1 and after[0].get("etag") == etag)


@gs("GS-030", "Two overlapping appointments for different contacts both reach Google, and neither is merged")
def gs_030(t):
    home = t.home()
    a = sid("GS-030", "contact 1 at 10:00")
    b = sid("GS-030", "contact 2 at 10:30")
    r = t.run(home, "add two overlapping, sync", g.add_fixture_contacts() + [
        g.add_appointment(1, a, *at(6, "10:00")), g.add_appointment(2, b, *at(6, "10:30")),
        g.save(), g.sync(), g.dump("d1.json")])
    for summary, hhmm in ((a, "10:00"), (b, "10:30")):
        events = g.test_events(summary)
        t.check("%s: one event on Google" % summary, len(events) == 1, "%d events" % len(events))
        if events:
            t.check("%s: at its own time" % summary, events[0]["start"]["dateTime"][11:16] == hhmm,
                    events[0]["start"]["dateTime"])
        t.check("%s: one entry in the program" % summary, len(app_appts(r, summary)) == 1)


# ------------------------------------------------------------------------------------------------ 1.3 interruptions etc.

@gs("GS-031", "A sync that cannot run (no network, or a refused sign-in) changes no local file and reports why, and a later sync completes.")
def gs_031(t):
    home = t.home()
    t.run(home, "set-up", setup_contacts())
    t.run(home, "local change", [g.edit_contact(1, {"mobile": NEW_M1}), g.save(), g.dump("d0.json")])
    before = snapshot(home)
    r = t.run(home, "no network", [g.sync(), g.dump("d1.json")], faults="r1=closed")
    t.check("the sync reports the error", r.logged("closed") or r.logged("error"), "no error text in the log")
    t.check("no local file changed", snapshot(home) == before)
    t.check("Google was not changed", not any(digits(NEW_M1) in google_phones(p) for p in google_for(1)))
    t.run(home, "network back, sync", [g.sync(), g.dump("d2.json")])
    t.check("the later sync completes: Google has the change", any(digits(NEW_M1) in google_phones(p) for p in google_for(1)))

    t.run(home, "second local change", [g.edit_contact(2, {"mobile": NEW_M2}), g.save(), g.dump("d3.json")])
    before = snapshot(home)
    r = t.run(home, "sign-in refused", [g.sync(), g.dump("d4.json")], faults="r1=401:authError")
    t.check("the report says to sign in again", r.logged("sign in") or r.logged("sign-in") or r.logged("log in"),
            "no sign-in text in the log")
    t.check("no local file changed", snapshot(home) == before)
    t.check("Google was not changed for contact 2", not any(digits(NEW_M2) in google_phones(p) for p in google_for(2)))
    t.run(home, "signed in again, sync", [g.sync(), g.dump("d5.json")])
    t.check("the later sync completes: Google has the change", any(digits(NEW_M2) in google_phones(p) for p in google_for(2)))
    t.blocked("network and sign-in are faults, not real", "the bench injects the failures (CM_TEST_FAULTS); "
                                                          "a real network loss or a revoked grant is not made")


@gs("GS-032", "A bad entry or reply for one contact is reported with its reason, and the others sync.")
def gs_032(t):
    home = t.home()
    t.run(home, "set-up", setup_contacts())
    bad = "not an email"
    r = t.run(home, "bad email for contact 2 and a good change for contact 1", [
        g.edit_contact(2, {"email": bad}), g.edit_contact(1, {"mobile": NEW_M1}), g.save(), g.sync(), g.dump("d2.json")])
    if t.config == CM:
        t.check("contact 2 is not uploaded (its email on Google is unchanged)",
                len(google_for(2)) == 1 and g.fixture(2)["email"] in g.emails_of(google_for(2)[0]))
        t.check("the report names contact 2 and its reason", r.logged("Carter") or r.logged(bad),
                "no report line for contact 2")
        t.check("the other contact syncs (contact 1's mobile is on Google)",
                any(digits(NEW_M1) in google_phones(p) for p in google_for(1)))
    else:
        t.check("Google master: the program's change to contact 2 is replaced, not uploaded",
                len(google_for(2)) == 1 and g.fixture(2)["email"] in g.emails_of(google_for(2)[0]))
        t.check("Google master: the program holds Google's email for contact 2",
                app_one(r, 2).get("email") == g.fixture(2)["email"])
    t.blocked("malformed reply for contact 3 is skipped and reported",
              "the fault plan has no malformed-reply fault (only status and closed); add one to the driver")


@gs("GS-033", "Google's server errors and rate limits are retried, with no duplicate record, and a failure gives its reason.")
def gs_033(t):
    home = t.home()
    r = t.run(home, "500 and rate limits on uploads", g.add_fixture_contacts() + [g.save(), g.sync(), g.dump("d1.json")],
              faults="w2=500:backendError;w3-4=429:rateLimitExceeded")
    check_google_copies(t)
    t.check("the faults show in the log", r.logged("429") or r.logged("rateLimit") or r.logged("500"),
            "no fault text in the log")
    g.clean_account()
    home2 = t.home("persistent")
    r2 = t.run(home2, "rate limit that persists", g.add_fixture_contacts() + [g.save(), g.sync(), g.dump("d2.json")],
               faults="w1-999=429:rateLimitExceeded")
    t.check("the report gives the reason and the recovery step", r2.logged("rate") and r2.logged("again"),
            "wording is matched loosely; tighten once confirmed")
    for n in range(1, 7):
        t.check("contact %d: no duplicate after the failure" % n, len(google_for(n)) <= 1, "%d copies" % len(google_for(n)))
    t.run(home2, "recovery sync", [g.sync(), g.dump("d3.json")])
    check_google_copies(t, when="after recovery")
    t.blocked("the pacing adapts", "the bench does not read request timings from the log yet")


@gs("GS-035", "A sync stopped part-way resumes from its progress file, with no upload repeated and none lost")
def gs_035(t):
    home = t.home()
    ops = []
    for i in range(1, 21):
        ops.append({"op": "add_contact", "fields": {"names": "Bench", "surname": "GS035 %02d" % i,
                                                    "email": "gs035.%02d@%s" % (i, g.DOMAIN)}})
    r1 = t.run(home, "upload, stopped at the eleventh write", ops + [g.save(), g.sync()], faults="w11=closed")
    t.check("the stopped sync reports the connection closed", r1.logged("closed"), "no text in the log")
    r2 = t.run(home, "resume", [g.sync(), g.dump("d2.json")])
    people = g.fixture_people()
    emails = [e for p in people for e in g.emails_of(p) if e.startswith("gs035.")]
    t.check("20 contacts on Google", len(people) == 20, "%d" % len(people))
    t.check("each contact once on Google", len(set(emails)) == len(emails) == 20, "%d emails, %d distinct"
            % (len(emails), len(set(emails))))
    t.check("20 contacts in the program", len(r2.contacts) == 20, "%d" % len(r2.contacts))


@gs("GS-038", "Changing the calendar window does not delete appointments outside it, locally or on Google")
def gs_038(t):
    home = t.home()
    t.run(home, "set-up", setup_contacts())
    s_old = sid("GS-038", "expired, before the narrowed window")
    s_far = sid("GS-038", "ahead, outside the narrowed window")
    s_in = sid("GS-038", "ahead, inside the window")
    create_linked_event(1, s_old, *at(-20))
    create_linked_event(2, s_far, *at(200))
    create_linked_event(3, s_in, *at(30))
    t.run(home, "sync with the full window", [g.sync(), g.dump("d1.json")])
    r2 = t.run(home, "narrow the window, sync", [g.setting("windowbefore", "5"), g.setting("windowafter", "30"),
                                                g.sync(), g.dump("d2.json")])
    for summary in (s_old, s_far, s_in):
        t.check("on Google: %s" % summary, len(g.test_events(summary)) == 1, "%d events" % len(g.test_events(summary)))
    archived = home.files("calendar/archive")
    t.check("the expired appointment is archived locally", len(archived) == 1, "%d files" % len(archived))
    t.check("the appointment inside the window is in the program", len(app_appts(r2, s_in)) == 1)


@gs("GS-039", "Two copies of the program on one calendar each see the other's change, with no duplicates")
def gs_039(t):
    copy_a = t.home("copy-a")
    copy_b = t.home("copy-b")
    summary = sid("GS-039", "shared appointment")
    s, e = at(7, "11:00")
    t.run(copy_a, "copy A adds the appointment", g.add_fixture_contacts() + [g.add_appointment(1, summary, s, e),
                                                                           g.save(), g.sync(), g.dump("a1.json")])
    rb = t.run(copy_b, "copy B downloads it", [g.sync(), g.dump("b1.json")])
    t.check("copy B has it once", len(app_appts(rb, summary)) == 1, "%d entries" % len(app_appts(rb, summary)))
    t.run(copy_b, "copy B changes the location, sync", [g.edit_appointment_in_app(summary, {"location": "Room 2"}),
                                                       g.save(), g.sync(), g.dump("b2.json")])
    ra = t.run(copy_a, "copy A syncs", [g.sync(), g.dump("a2.json")])
    mine = app_appts(ra, summary)
    t.check("copy A has the change", len(mine) == 1 and mine[0].get("location") == "Room 2",
            "location=%r" % (mine[0].get("location") if mine else None))
    t.check("one event on Google", len(g.test_events(summary)) == 1, "%d events" % len(g.test_events(summary)))


@gs("GS-040", "Switching the master between syncs, with edits on both sides, leaves one copy of each record")
def gs_040(t):
    home = t.home()
    t.run(home, "set-up", setup_contacts())
    g.update_google_contact(google_for(2)[0]["resourceName"], "phoneNumbers", [{"value": NEW_M2, "type": "mobile"}])
    t.run(home, "edits on both sides, first master", [g.edit_contact(1, {"mobile": NEW_M1}),
                                                     g.edit_contact(2, {"email": NEW_EMAIL2}),
                                                     g.save(), g.sync(), g.dump("d2.json")])
    home.set_master(not t.google_master)
    g.update_google_contact(google_for(1)[0]["resourceName"], "phoneNumbers", [{"value": NEW_M3, "type": "mobile"}])
    r = t.run(home, "switch master, edits on both sides", [g.edit_contact(2, {"mobile": NEW_M4}),
                                                          g.save(), g.sync(), g.dump("d3.json")])
    for n in range(1, 7):
        email = NEW_EMAIL2 if n == 2 else None
        copies = google_for(n, email=email)
        t.check("contact %d: one copy on Google" % n, len(copies) == 1, "%d copies" % len(copies))
        local = [c for c in r.contacts if c.get("surname") == g.fixture(n)["surname"]]
        t.check("contact %d: one copy in the program" % n, len(local) == 1, "%d copies" % len(local))


@gs("GS-041", "The clean-up removes only the records of the run, and the set-up's saved account is the one used.")
def gs_041(t):
    home = t.home()
    t.run(home, "set-up", setup_contacts())
    setup = os.path.join(g.UTIL, "setup_test_account.py")
    clean = os.path.join(g.UTIL, "clean_test_data.py")
    rc = subprocess.run([sys.executable, setup, "--check"], capture_output=True, text=True, timeout=300)
    t.check("set-up --check: the saved account works", rc.returncode == 0, (rc.stdout + rc.stderr).strip()[-200:])

    # a clean-up whose account file names a different account must stop before any delete
    data = ta.load_account()
    data["google"]["account_email"] = "someone.else@example.com"
    wrong = os.path.join(t.folder, "other-account.json")
    ta.save_account(data, path=wrong)
    try:
        before = (len(g.fixture_people()), len(g.test_events()))
        rw = subprocess.run([sys.executable, clean, "--config", wrong, "--yes"], capture_output=True, text=True,
                            timeout=600)
        after = (len(g.fixture_people()), len(g.test_events()))
    finally:
        if os.path.exists(wrong):
            os.remove(wrong)
    t.check("the clean-up stops for another account", rw.returncode != 0, "exit %s" % rw.returncode)
    t.check("and deletes nothing when it stops", before == after, "before %s, after %s" % (before, after))

    rd = subprocess.run([sys.executable, clean, "--dry-run"], capture_output=True, text=True, timeout=600)
    t.check("clean-up --dry-run finishes", rd.returncode == 0, "exit %s" % rd.returncode)
    rc2 = subprocess.run([sys.executable, clean, "--yes"], capture_output=True, text=True, timeout=1200)
    t.check("clean-up finishes", rc2.returncode == 0, "exit %s" % rc2.returncode)
    t.check("no test records are left on Google", g.fixture_people() == [] and g.test_events() == [],
            "%d contacts, %d events" % (len(g.fixture_people()), len(g.test_events())))
    fresh = t.home("after")
    rf = t.run(fresh, "next sync starts empty", [g.sync(), g.dump("d2.json")])
    t.check("the next sync starts from an empty program", rf.contacts == [] and rf.appointments == [])

    # The listing calls may appear only inside the --all-data branch of the clean-up.
    with open(clean, encoding="utf-8") as f:
        tree = ast.parse(f.read())
    parents = {}
    for node in ast.walk(tree):
        for child in ast.iter_child_nodes(node):
            parents[child] = node
    unguarded = []
    for node in ast.walk(tree):
        if isinstance(node, ast.Call) and getattr(node.func, "id", "") in ("list_contacts", "list_events"):
            child, guarded = node, False
            while child in parents:
                parent = parents[child]
                if isinstance(parent, ast.If) and "all_data" in ast.unparse(parent.test) and child in parent.body:
                    guarded = True
                    break
                child = parent
            if not guarded:
                unguarded.append("%s at line %d" % (node.func.id, node.lineno))
    t.check("the default clean-up finds records by search, not by listing the account", not unguarded,
            "listing outside --all-data: " + ", ".join(unguarded))


# ------------------------------------------------------------------------------------------------ the runner

def run_one(test_id, config, fn, driver, outroot):
    folder = os.path.join(outroot, "%s-%s" % (test_id, config))
    os.makedirs(folder, exist_ok=True)
    t = Ctx(test_id, config, driver, folder)
    started = time.time()
    cleared = None
    try:
        g.clean_account()
        fn(t)
    except gc.GoogleError as e:
        t.blocked("Google request", "%s %s: %s" % (e.status, getattr(e, "reason", ""), getattr(e, "message", "")))
    except Exception:
        text = traceback.format_exc()
        with open(os.path.join(folder, "error.txt"), "w", encoding="utf-8") as f:
            f.write(text)
        t.check("the test ran to the end", False, text.strip().splitlines()[-1])
    finally:
        try:
            cleared = g.clean_account()
        except Exception as e:                     # noqa: BLE001  (reported, not hidden)
            t.blocked("clean-up after the test", str(e))
    failed = [c for c in t.checks if c.ok is False]
    blocked_ = [c for c in t.checks if c.ok is None]
    status = "FAIL" if failed else ("BLOCKED" if blocked_ else "PASS")
    with open(os.path.join(folder, "checks.txt"), "w", encoding="utf-8") as f:
        for c in t.checks:
            mark = {True: "pass", False: "FAIL", None: "BLOCKED"}[c.ok]
            f.write("%-8s %s\n" % (mark, c.detail))
    return {"test": test_id, "config": config, "status": status, "seconds": round(time.time() - started, 1),
            "checks": len(t.checks), "failed": [c.detail for c in failed], "blocked": [c.detail for c in blocked_],
            "cleared_after": cleared}


def main(argv=None):
    p = argparse.ArgumentParser(description="Google sync tests (area 1, section 1 of the test specification).")
    p.add_argument("tests", nargs="*", help="test IDs to run, for example GS-008 (default: all, one after another)")
    p.add_argument("--driver", default=os.environ.get("CM_TEST_DRIVER", DEFAULT_DRIVER),
                   help="the built sync driver (driver/build_driver.sh)")
    p.add_argument("--out", default=None, help="where the evidence goes (default: a timestamped folder in your home folder)")
    p.add_argument("--list", action="store_true", help="list the tests and exit")
    args = p.parse_args(argv)

    ids = {t[0] for t in TESTS}
    if args.list:
        for test_id, title, configs, _fn in TESTS:
            print("%s  [%s]  %s" % (test_id, " and ".join(configs) if len(configs) > 1 else configs[0], title))
        return 0
    unknown = [x for x in args.tests if x not in ids]
    if unknown:
        print("unknown test: %s" % ", ".join(unknown))
        return 2
    if not os.path.exists(args.driver):
        print("the driver is not built: %s (see driver/build_driver.sh)" % args.driver)
        return 2

    selected = [t for t in TESTS if not args.tests or t[0] in args.tests]
    outroot = args.out or os.path.join(DEFAULT_OUT, "googlesync-" + time.strftime("%Y%m%d-%H%M%S"))
    os.makedirs(outroot, exist_ok=True)
    results = []
    for test_id, title, configs, fn in selected:
        for config in configs:
            res = run_one(test_id, config, fn, args.driver, outroot)
            results.append(res)
            print("%-7s %s [%s]  %s" % (res["status"], test_id, config, title))
            for line in res["failed"]:
                print("        FAIL: %s" % line)
            for line in res["blocked"]:
                print("        BLOCKED: %s" % line)
    with open(os.path.join(outroot, "summary.json"), "w", encoding="utf-8") as f:
        json.dump(results, f, indent=1)
    failed = [r for r in results if r["status"] == "FAIL"]
    print("%d runs: %d pass, %d fail, %d blocked. Evidence: %s" % (
        len(results), sum(r["status"] == "PASS" for r in results), len(failed),
        sum(r["status"] == "BLOCKED" for r in results), outroot))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
