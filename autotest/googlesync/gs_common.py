#
# Helpers for the Google sync tests (autotest/googlesync/googlesync_tests.py).
#
# Nothing here imports from the archive.  Google calls, the account file and the test identifiers come from
# autotest/utilities; the driver and its build script are in driver/.  Code that was copied from the earlier bench is
# marked "copied".
#
# The tests never list the whole account: the clean-up searches for the test prefix and the fixture domain.
#

import datetime
import json
import os
import re
import shutil
import subprocess
import sys
import time
import uuid
import urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
UTIL = os.path.join(REPO, "autotest", "utilities")
sys.path.insert(0, UTIL)

import googleclient as gc                     # noqa: E402
import setup_test_account as sta              # noqa: E402
import testaccount as ta                      # noqa: E402

FIX = ta.load_fixtures()
PREFIX = FIX["identifier"]["event_prefix"]    # "TEST "
DOMAIN = FIX["identifier"]["email_domain"]    # sgc.contacmanagertest
PEOPLE = gc.PEOPLE_URL                        # https://people.googleapis.com/v1
CALENDAR = gc.CALENDAR_URL                    # https://www.googleapis.com/calendar/v3/calendars/primary
PERSON_FIELDS = "names,emailAddresses,phoneNumbers,birthdays,metadata"


# ------------------------------------------------------------------------------------------------ check results

class Check:
    """One expected result.  ok is True (passed), False (failed) or None (blocked, with the reason in detail)."""

    def __init__(self, ok, detail):
        self.ok = ok
        self.detail = detail


def check(label, condition, detail=""):
    return Check(bool(condition), label + ("" if condition else (": " + detail if detail else "")))


def blocked(label, reason):
    return Check(None, "%s: BLOCKED, %s" % (label, reason))


# ------------------------------------------------------------------------------------------------ Google access

_token = {"value": None}


def token():
    if _token["value"] is None:
        acc = ta.load_account()["google"]
        _token["value"] = sta.refresh_access_token(acc["client_id"], acc["client_secret"], acc["refresh_token"])
    return _token["value"]


def account_email():
    return ta.load_account()["google"]["account_email"]


def api(method, url, body=None):
    """A Google request.  Rate limits and server errors are retried inside googleclient; an expired token is refreshed once."""
    data = None if body is None else json.dumps(body).encode()
    headers = {"Authorization": "Bearer " + token()}
    if data is not None:
        headers["Content-Type"] = "application/json"
    try:
        return gc.request(method, url, data=data, headers=headers) or {}
    except gc.GoogleError as e:
        if e.status != 401:
            raise
        _token["value"] = None
        headers["Authorization"] = "Bearer " + token()
        return gc.request(method, url, data=data, headers=headers) or {}


# ------------------------------------------------------------------------------------------------ fixture contacts on Google

def fixture(n):
    return FIX["contacts"][n - 1]


def person_body(n):
    """The Google person for fixture contact n: name, email, mobile, and a birthday for the odd numbers."""
    c = fixture(n)
    body = {"names": [{"givenName": c["given"], "familyName": c["surname"]}],
            "emailAddresses": [{"value": c["email"], "type": "home"}],
            "phoneNumbers": [{"value": c["phone"], "type": "mobile"}]}
    if c["birthday"]:
        y, m, d = (int(x) for x in c["birthday"].split("-"))
        body["birthdays"] = [{"date": {"year": y, "month": m, "day": d}}]
    return body


def create_google_contact(body):
    return api("POST", PEOPLE + "/people:createContact?personFields=" + PERSON_FIELDS, body)["resourceName"]


def search_people(query):
    """Contacts that match the query (the People API search, not a listing of the account)."""
    q = urllib.parse.urlencode({"query": query, "readMask": PERSON_FIELDS})
    reply = api("GET", PEOPLE + "/people:searchContacts?" + q)
    return [r["person"] for r in reply.get("results", []) if "person" in r]


def emails_of(person):
    return [e.get("value", "") for e in person.get("emailAddresses", [])]


def fixture_people():
    """Every contact on the account whose email is in the test domain."""
    out = {}
    for p in search_people(DOMAIN):
        for e in emails_of(p):
            if e.lower().endswith("@" + DOMAIN):
                out[p["resourceName"]] = p
    return list(out.values())


def fixture_people_for(n):
    return [p for p in fixture_people() if fixture(n)["email"] in emails_of(p)]


def google_person(resource):
    return api("GET", PEOPLE + "/" + resource + "?personFields=" + PERSON_FIELDS)


def update_google_contact(resource, field, value):
    """Changes one field of a Google contact.  field is names, emailAddresses or phoneNumbers.  value is the new list."""
    current = google_person(resource)
    body = {"etag": current["etag"], field: value}
    api("PATCH", PEOPLE + "/" + resource + ":updateContact?updatePersonFields=" + field, body)


def delete_google_contact(resource):
    api("DELETE", PEOPLE + "/" + resource + ":deleteContact")


def surname_of(person):
    names = person.get("names", [])
    return names[0].get("familyName", "") if names else ""


def phone_of(person):
    phones = person.get("phoneNumbers", [])
    return phones[0].get("value", "") if phones else ""


# ------------------------------------------------------------------------------------------------ fixture events on Google

def create_event(summary, start, end, tz=None, recurrence=None, location=None):
    if not summary.startswith(PREFIX):
        raise ValueError("a test event must start with %r" % PREFIX)
    body = {"summary": summary, "start": {"dateTime": start}, "end": {"dateTime": end}}
    if tz:
        body["start"]["timeZone"] = tz
        body["end"]["timeZone"] = tz
    if recurrence:
        body["recurrence"] = recurrence
    if location:
        body["location"] = location
    return api("POST", CALENDAR + "/events", body)["id"]


def get_event(event_id):
    return api("GET", CALENDAR + "/events/" + event_id)


def patch_event(event_id, fields):
    api("PATCH", CALENDAR + "/events/" + event_id, fields)


def delete_event(event_id):
    api("DELETE", CALENDAR + "/events/" + event_id)


def import_external_event(summary, start, end):
    """An event organised by another address, with this account as an attendee (copied from the earlier bench)."""
    if not summary.startswith(PREFIX):
        raise ValueError("a test event must start with %r" % PREFIX)
    me = api("GET", "https://www.googleapis.com/oauth2/v3/userinfo").get("email")
    body = {"iCalUID": "gs-%s@test.invalid" % uuid.uuid4().hex, "summary": summary,
            "start": {"dateTime": start}, "end": {"dateTime": end},
            "organizer": {"email": "organiser@example.com"},
            "attendees": [{"email": me, "self": True, "responseStatus": "accepted"}], "status": "confirmed"}
    return api("POST", CALENDAR + "/events/import", body)["id"]


def test_events(summary=None):
    """Events on the primary calendar whose summary starts with the test prefix.  Searched, not listed."""
    out, token_ = [], None
    while True:
        q = {"q": PREFIX.strip(), "singleEvents": "false", "showDeleted": "false", "maxResults": 250}
        if token_:
            q["pageToken"] = token_
        page = api("GET", CALENDAR + "/events?" + urllib.parse.urlencode(q))
        out += [e for e in page.get("items", []) if (e.get("summary") or "").startswith(PREFIX)]
        token_ = page.get("nextPageToken")
        if not token_:
            break
    if summary is not None:
        out = [e for e in out if e.get("summary") == summary]
    return out


def clean_account():
    """Removes the fixture contacts and the test events, found by search.  Returns how many of each were removed."""
    contacts = fixture_people()
    for p in contacts:
        delete_google_contact(p["resourceName"])
    events = test_events()
    for e in events:
        delete_event(e["id"])
    return len(contacts), len(events)


# ------------------------------------------------------------------------------------------------ the app: home folders and the driver

INI_BASE = """[phone]
localcode=
countrycode=

[reminders]
earliestsend=00:00
latestsend=23:59
"""


class Home:
    """A HOME folder for one test: the ini file, the settings file, and the data folder the app writes."""

    def __init__(self, path, ini_extra=""):
        self.path = path
        if os.path.isdir(path):
            shutil.rmtree(path)
        os.makedirs(os.path.join(path, "TrumptonApps"))
        os.makedirs(os.path.join(path, ".config", "trumpton.org.uk"))
        with open(os.path.join(path, "TrumptonApps", "ContactManager.ini"), "w") as f:
            f.write(INI_BASE + ini_extra)
        self.set_settings({"googlerefreshtoken": "%s %s" % (ta.load_account()["google"]["refresh_token"],
                                                             account_email()),
                           "googleusername": account_email(),
                           "calendarsync": "yes", "contactsync": "yes", "databasemaster": "0"})

    @property
    def conf(self):
        return os.path.join(self.path, ".config", "trumpton.org.uk", "ContactManager.conf")

    def set_settings(self, values):
        """Sets settings in the app's settings file, keeping every other line (the sync state is kept between runs)."""
        lines = []
        if os.path.exists(self.conf):
            with open(self.conf, encoding="utf-8") as f:
                lines = f.read().splitlines()
        if not lines:
            lines = ["[General]"]
        for key, value in values.items():
            lines = [l for l in lines if not re.match(r"^%s=" % re.escape(key), l)]
            lines.append("%s=%s" % (key, value))
        with open(self.conf, "w", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")

    def set_master(self, google_master):
        self.set_settings({"databasemaster": "1" if google_master else "0"})

    @property
    def data(self):
        return os.path.join(self.path, "TrumptonApps", "ContactManager")

    def files(self, sub="contact", suffix=None):
        folder = os.path.join(self.data, sub)
        if not os.path.isdir(folder):
            return []
        names = sorted(os.listdir(folder))
        return [n for n in names if suffix is None or n.endswith(suffix)]


class Run:
    """One run of the driver: its results, its log, and the dump it saved."""

    def __init__(self, outdir, rc, results, log, dump, seconds):
        self.outdir = outdir
        self.rc = rc
        self.results = results
        self.log = log
        self.dump = dump or {"contacts": [], "appointments": []}
        self.seconds = seconds

    @property
    def contacts(self):
        return self.dump.get("contacts", [])

    @property
    def appointments(self):
        return self.dump.get("appointments", [])

    def contact(self, n):
        """The app's contact for fixture n, by its email address."""
        for c in self.contacts:
            if c.get("email") == fixture(n)["email"]:
                return c
        return None

    def appointments_titled(self, summary):
        return [a for a in self.appointments if a.get("summary") == summary]

    def logged(self, text):
        return text.lower() in self.log.lower()

    def listing(self, name):
        """A folder listing the driver wrote with listdir (the file names, one per line)."""
        path = os.path.join(self.outdir, name)
        if not os.path.exists(path):
            return []
        with open(path, encoding="utf-8", errors="replace") as f:
            return [l.strip() for l in f if l.strip()]


def run_driver(driver, home, name, ops, outdir, secret="gs-test-secret-only", setkey=True, faults=None,
               timeout=900, extra_env=None):
    """Runs the driver once, on the home.  Faults is the CM_TEST_FAULTS text (see the driver), or None."""
    os.makedirs(outdir, exist_ok=True)
    opsfile = os.path.join(outdir, "ops.json")
    with open(opsfile, "w", encoding="utf-8") as f:
        json.dump(ops, f, indent=1)
    env = dict(os.environ)
    env.update({"HOME": home.path, "QT_QPA_PLATFORM": "offscreen", "CM_TEST_SECRET": secret, "TZ": "Europe/London"})
    env.pop("CM_TEST_FAULTS", None)
    env.pop("CM_TEST_SETKEY", None)
    if setkey:
        env["CM_TEST_SETKEY"] = "1"
    if faults:
        env["CM_TEST_FAULTS"] = faults
    if extra_env:
        env.update(extra_env)
    start = time.time()
    try:
        proc = subprocess.run([driver, outdir, opsfile], env=env, timeout=timeout,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        rc, out = proc.returncode, proc.stdout.decode("utf-8", "replace")
    except subprocess.TimeoutExpired:
        rc, out = "timeout", "the driver did not finish within %d seconds" % timeout
    results = {}
    rp = os.path.join(outdir, "results.json")
    if os.path.exists(rp):
        with open(rp, encoding="utf-8") as f:
            try:
                results = json.load(f)
            except ValueError:
                results = {}
    log = out
    lp = os.path.join(outdir, "driver.log")
    if os.path.exists(lp):
        with open(lp, encoding="utf-8", errors="replace") as f:
            log += "\n" + f.read()
    dump = None
    dp = os.path.join(outdir, "dump.json")
    if os.path.exists(dp):
        with open(dp, encoding="utf-8") as f:
            try:
                dump = json.load(f)
            except ValueError:
                dump = None
    return Run(outdir, rc, results, log, dump, time.time() - start)


# ------------------------------------------------------------------------------------------------ operations for the driver

def save():
    return {"op": "save"}


def sync():
    return {"op": "sync"}


def dump(file="dump.json"):
    return {"op": "dump", "file": file}


def add_fixture_contacts(ns=(1, 2, 3, 4, 5, 6)):
    ops = []
    for n in ns:
        c = fixture(n)
        ops.append({"op": "add_contact", "fields": {"surname": c["surname"], "names": c["given"],
                                                    "email": c["email"], "mobile": c["phone"]}})
    return ops


def add_appointment(n, summary, start, end, **extra):
    fields = {"summary": summary, "from": start, "to": end}
    fields.update(extra)
    op = {"op": "add_appointment", "fields": fields}
    if n:
        op["surname"] = fixture(n)["surname"]
    return op


def listdir(sub, file):
    return {"op": "listdir", "sub": sub, "file": file}


def setting(key, value):
    return {"op": "setting", "key": key, "value": value}


def edit_contact(n, fields):
    return {"op": "edit_contact", "match": {"surname": fixture(n)["surname"]}, "fields": fields}


def hide_contact(n):
    return {"op": "hide_contact", "match": {"surname": fixture(n)["surname"]}}


def delete_contact_in_app(n):
    return {"op": "delete_contact", "match": {"surname": fixture(n)["surname"]}}


def contact_notes(n, todo, history):
    return {"op": "contact_notes", "match": {"surname": fixture(n)["surname"]}, "todo": todo, "history": history}


def tidy():
    return {"op": "tidy_full", "name": "tidy"}


def delete_appointment_in_app(summary):
    return {"op": "delete_appointment", "match": {"summary": summary}}


def edit_appointment_in_app(summary, fields):
    return {"op": "edit_appointment", "match": {"summary": summary}, "fields": fields}


# ------------------------------------------------------------------------------------------------ dates

def london_hour(iso_text):
    """The hour, in London time, of an ISO date-time text (as the dump writes them)."""
    from zoneinfo import ZoneInfo
    text = iso_text.replace("Z", "+00:00")
    when = datetime.datetime.fromisoformat(text)
    if when.tzinfo is None:
        when = when.replace(tzinfo=datetime.timezone.utc)
    return when.astimezone(ZoneInfo("Europe/London")).hour


def days_from_now(days, hhmm="10:00"):
    """A UTC date-time text, the given number of days from today at the given time."""
    day = datetime.datetime.now(datetime.timezone.utc).date() + datetime.timedelta(days=days)
    return "%sT%s:00Z" % (day.isoformat(), hhmm)


def future(day, hhmm="10:00"):
    return "%sT%s:00Z" % (day, hhmm)


def in_window_repeat_count(hh="09:00"):
    """How many weekly occurrences, from two days ahead, fall in the calendar window (copied from the earlier bench).
    The window's far edge is 730 days from now, less an hour, so the last occurrence depends on the time of day."""
    now = datetime.datetime.now(datetime.timezone.utc).replace(tzinfo=None)
    edge = now + datetime.timedelta(days=730, hours=-1)
    hour, minute = (int(x) for x in hh.split(":"))
    first = now.replace(hour=hour, minute=minute, second=0, microsecond=0) + datetime.timedelta(days=2)
    return len([k for k in range(200) if first + datetime.timedelta(days=7 * k) <= edge])
