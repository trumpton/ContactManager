#!/usr/bin/env python3
"""Google bench for ContactManager: contacts and appointments.

Runs the app's Google sync headlessly (autotest/bench/bench_driver) against a test Google account, and
checks the result on both sides: the local dump the driver writes and the account as Google holds it.

Only use a scratch account (see HEADLESS-TEST.md, section 0).  Everything the bench creates is named "Bench ..." (contacts) or
"BENCH ..." (appointments), and the bench deletes those names before and after each scenario.

Configuration (environment variables, with defaults):
  CM_BENCH_DRIVER   path to the built bench_driver (default: bench_driver on PATH)
  CM_TEST_SECRET    the local encryption key / password the app asks for
  CM_CONF           the app's settings file holding googlerefreshtoken
  CM_HEADER         account-googleaccess.h (client ID and secret)
  CM_BENCH_RUNS     directory for run output (default: a temporary directory)

usage:
  google_bench.py run full                 all three groups: uploads, updates, local (slow: about an hour or more)
  google_bench.py run quick                groups 2 and 3 (updates and local)
  google_bench.py run google               group 2: every Google sync test
  google_bench.py run google-quick         a quick subset of group 2
  google_bench.py run local                group 3: tests that need no Google sync
  google_bench.py run <code> [code ...]    named scenarios (U01, R01, L02, ...)
  google_bench.py groups                   list each group's scenarios
  google_bench.py purge contacts|appointments|both --yes
                                           remove everything of that kind from the account (scratch account only)
  google_bench.py gchange <cmd> <json>     change the account from inside a run (used by the driver)
"""
import datetime
import inspect
import json
import os
import random
import re
import shutil
import subprocess
import sys
import tempfile
import uuid
import time
import urllib.error
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
DRIVER = os.environ.get("CM_BENCH_DRIVER", "bench_driver")
CONF = os.environ.get("CM_CONF", "/root/.config/trumpton.org.uk/ContactManager.conf")
HEADER = os.environ.get("CM_HEADER", os.path.join(REPO, "account-googleaccess.h"))
RUNS = os.environ.get("CM_BENCH_RUNS") or tempfile.mkdtemp(prefix="cm-bench-")
SCRIPT = os.path.abspath(__file__)

PEOPLE = "https://people.googleapis.com/v1"
CALENDAR = "https://www.googleapis.com/calendar/v3/calendars/primary"
PERSON_FIELDS = "names,emailAddresses,phoneNumbers,addresses,urls,organizations,biographies,nicknames,birthdays,userDefined,metadata"

# ----------------------------------------------------------------------------------------------
# Google access

_token = {"value": None}


def _define(name):
    with open(HEADER) as f:
        m = re.search(r'#define\s+%s\s+"(.*)"' % name, f.read())
    if not m:
        raise SystemExit("cannot read %s from %s" % (name, HEADER))
    return m.group(1)


def _refresh_token():
    with open(CONF) as f:
        m = re.search(r"^googlerefreshtoken=(\S+) ", f.read(), re.M)
    if not m:
        raise SystemExit("no googlerefreshtoken in %s - sign in through the app first" % CONF)
    return m.group(1)


def access_token():
    if _token["value"] is None:
        data = urllib.parse.urlencode({
            "client_id": _define("CLIENTID"), "client_secret": _define("SECRET"),
            "refresh_token": _refresh_token(), "grant_type": "refresh_token"}).encode()
        with urllib.request.urlopen("https://oauth2.googleapis.com/token", data, timeout=60) as r:
            _token["value"] = json.load(r)["access_token"]
    return _token["value"]


def api(method, url, body=None):
    data = None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(url, data=data, method=method)
    req.add_header("Authorization", "Bearer " + access_token())
    if data is not None:
        req.add_header("Content-Type", "application/json")
    for attempt in range(9):
        try:
            with urllib.request.urlopen(req, timeout=60) as r:
                text = r.read().decode()
                return json.loads(text) if text else {}
        except urllib.error.HTTPError as e:
            # Quota (429): Google's limits are per minute for some writes, so back off up to a minute between tries
            if e.code == 429 and attempt < 8:
                time.sleep(min(2 ** attempt, 60))
                continue
            raise RuntimeError("%s %s -> %s %s" % (method, url, e.code, e.read().decode()[:300]))


def all_connections():
    out, token = [], None
    while True:
        q = "personFields=%s&pageSize=1000" % PERSON_FIELDS + ("&pageToken=" + token if token else "")
        page = api("GET", PEOPLE + "/people/me/connections?" + q)
        out += page.get("connections", [])
        token = page.get("nextPageToken")
        if not token:
            return out


def bench_contacts():
    """Contacts this bench created: given name 'Bench', or the test address for contacts with no name."""
    return [p for p in all_connections()
            if any(n.get("givenName") == "Bench" for n in p.get("names", []))
            or any((e.get("value") or "").endswith("@test.invalid") for e in p.get("emailAddresses", []))]


def contact_info(p):
    names = p.get("names", [{}])[0]
    return {
        "resourceName": p["resourceName"], "etag": p.get("etag"),
        "tag": names.get("familyName", ""),
        "emails": [(e.get("value"), e.get("type", "")) for e in p.get("emailAddresses", [])],
        "phones": [(x.get("value"), x.get("type", "")) for x in p.get("phoneNumbers", [])],
        "addresses": [x.get("formattedValue", "") for x in p.get("addresses", [])],
        "urls": [x.get("value") for x in p.get("urls", [])],
        "nicknames": [x.get("value") for x in p.get("nicknames", [])],
        "biographies": [x.get("value") for x in p.get("biographies", [])],
    }


def delete_bench_contacts():
    for p in bench_contacts():
        try:
            api("DELETE", PEOPLE + "/" + p["resourceName"] + ":deleteContact")
        except RuntimeError as e:
            print("cleanup: could not delete contact:", e)


def create_contact(tag, emails=(), phones=(), urls=(), nicknames=(), biographies=(), addresses=(),
                   given="Bench", family=None):
    body = {"names": [{"givenName": given, "familyName": tag if family is None else family}]}
    if emails:
        body["emailAddresses"] = [{"value": v, "type": t} for v, t in emails]
    if phones:
        body["phoneNumbers"] = [{"value": v, "type": t} for v, t in phones]
    if urls:
        body["urls"] = [{"value": v} for v in urls]
    if nicknames:
        body["nicknames"] = [{"value": v} for v in nicknames]
    if biographies:
        body["biographies"] = [{"value": v} for v in biographies]
    if addresses:
        body["addresses"] = [{"formattedValue": v} for v in addresses]
    return api("POST", PEOPLE + "/people:createContact?personFields=" + PERSON_FIELDS, body)["resourceName"]


def update_contact_surname(resource, surname):
    current = api("GET", PEOPLE + "/" + resource + "?personFields=names,metadata")
    name = current["names"][0]
    name["familyName"] = surname
    body = {"etag": current["etag"], "names": [name]}
    api("PATCH", PEOPLE + "/" + resource + ":updateContact?updatePersonFields=names", body)


def bench_events():
    out, token = [], None
    while True:
        q = "maxResults=2500&singleEvents=false&showDeleted=false" + ("&pageToken=" + token if token else "")
        page = api("GET", CALENDAR + "/events?" + q)
        out += page.get("items", [])
        token = page.get("nextPageToken")
        if not token:
            break
    return [e for e in out if (e.get("summary") or "").startswith("BENCH ")]


def event_info(e):
    return {"id": e["id"], "summary": e.get("summary", ""), "location": e.get("location", ""),
            "start": (e.get("start") or {}).get("dateTime", ""), "end": (e.get("end") or {}).get("dateTime", ""),
            "etag": e.get("etag"), "organizer_self": (e.get("organizer") or {}).get("self", False)}


def create_event(summary, start, end, location=""):
    body = {"summary": summary, "start": {"dateTime": start}, "end": {"dateTime": end}}
    if location:
        body["location"] = location
    return api("POST", CALENDAR + "/events", body)["id"]


def import_external_event(summary, start, end):
    """An event whose organiser is another address, with this account as an attendee (read-only here)."""
    me = api("GET", "https://www.googleapis.com/oauth2/v3/userinfo").get("email")
    body = {"iCalUID": "bench-%s@test.invalid" % uuid.uuid4().hex, "summary": summary,
            "start": {"dateTime": start}, "end": {"dateTime": end},
            "organizer": {"email": "organiser@example.com"},
            "attendees": [{"email": me, "self": True, "responseStatus": "accepted"}], "status": "confirmed"}
    return api("POST", CALENDAR + "/events/import", body)["id"]


def delete_bench_events():
    for e in bench_events():
        try:
            api("DELETE", CALENDAR + "/events/" + e["id"])
        except RuntimeError as err:
            print("cleanup: could not delete event:", err)


def patch_event(event_id, fields):
    api("PATCH", CALENDAR + "/events/" + event_id, fields)


def get_event(event_id):
    return api("GET", CALENDAR + "/events/" + event_id)


def cleanup():
    delete_bench_contacts()
    delete_bench_events()


# ----------------------------------------------------------------------------------------------
# Running the app

class Run:
    def __init__(self, name, ops, session=None, tz=None, conf=()):
        self.name = name
        self.tz = tz
        self.conf = list(conf)
        self.dir = os.path.join(RUNS, name)
        shutil.rmtree(self.dir, ignore_errors=True)
        os.makedirs(self.dir)
        # The app keeps its local data in its working directory.  A session shares one working directory
        # across runs, so the local data carries from one run to the next (used to switch master setting).
        if session:
            self.workdir = os.path.join(RUNS, "session-" + session)
            os.makedirs(self.workdir, exist_ok=True)
        else:
            self.workdir = self.dir
        self.home = os.path.join(self.workdir, "home")
        self.fresh = not os.path.exists(self.home)
        self.ops = ops
        self.dump = None
        with open(os.path.join(self.dir, "ops.json"), "w") as f:
            json.dump(ops, f, indent=1)

    def execute(self, max_writes=None):
        """Runs the driver.  With max_writes, the run is stopped once the app has sent that many updates or creations to
        Google: a safety limit for a test that could otherwise send far more than it expects (the run is then marked aborted)."""
        secret = os.environ.get("CM_TEST_SECRET", "")
        if not secret:
            raise SystemExit("set CM_TEST_SECRET to the local test password")
        prepare_home(self.home, self.fresh, MODE["master"], self.conf)
        env = dict(os.environ, CM_TEST_SECRET=secret, QT_QPA_PLATFORM="offscreen", HOME=self.home)
        env.pop("XDG_CONFIG_HOME", None)
        if self.tz:
            env["TZ"] = self.tz
        self.aborted = False
        log = os.path.join(self.dir, "stdout.log")
        with open(log, "w") as out:
            proc = subprocess.Popen([DRIVER, self.dir, os.path.join(self.dir, "ops.json")], env=env,
                                    stdout=out, stderr=subprocess.STDOUT, cwd=self.workdir)
            started = time.time()
            while proc.poll() is None:
                if max_writes is not None and self.writes_sent(log) > max_writes:
                    proc.kill() ; proc.wait() ; self.aborted = True
                    break
                if time.time() - started > 900:
                    proc.kill() ; proc.wait() ; break
                time.sleep(0.5)
        path = os.path.join(self.dir, "dump.json")
        if os.path.exists(path):
            with open(path) as f:
                self.dump = json.load(f)
        return self

    def writes_sent(self, log):
        """How many updates and creations the app has sent to Google so far, from the driver's log."""
        try:
            with open(log, errors="replace") as f:
                text = f.read()
        except OSError:
            return 0
        return text.count("TIMING http PUT updateAppointment") + text.count("TIMING http POST updateAppointment")

    def dump_file(self, name):
        with open(os.path.join(self.dir, name)) as f:
            return json.load(f)

    def report(self, n):
        path = os.path.join(self.dir, "report%d.txt" % n)
        return open(path).read() if os.path.exists(path) else ""

    def log(self):
        path = os.path.join(self.dir, "driver.log")
        return open(path).read() if os.path.exists(path) else ""


PHONE_KEYS = ("phone", "work", "mobile", "phone2", "phone3", "phone4")
EMAIL_KEYS = ("email", "email2", "email3", "email4")


def digits(s):
    """A phone number as its digits (and a leading +), so spaces, dashes and brackets do not count."""
    return "".join(ch for ch in (s or "") if ch.isdigit() or ch == "+")


def canon_app(c):
    """A live local contact as comparable values: emails, phones and notes, formatting ignored."""
    return {"emails": sorted(norm(c[k]) for k in EMAIL_KEYS if c.get(k)),
            "phones": sorted(digits(c[k]) for k in PHONE_KEYS if c.get(k)),
            "notes": norm(c.get("comments", ""))}


def canon_google(p):
    info = contact_info(p)
    return {"emails": sorted(norm(v) for v, _ in info["emails"]),
            "phones": sorted(digits(v) for v, _ in info["phones"]),
            "notes": norm(" ".join(info["biographies"]))}


def app_live_contacts(dump, tag):
    return [c for c in dump["contacts"] if c.get("names") == "Bench" and c.get("surname") == tag
            and c.get("deleted") != "true"]


def google_contacts_tagged(tag):
    return [p for p in bench_contacts() if contact_info(p)["tag"] == tag]


def same_contact(dump, tag):
    """Exactly one live app contact and one Google contact for tag, with the same values."""
    app = app_live_contacts(dump, tag)
    goog = google_contacts_tagged(tag)
    if len(app) != 1 or len(goog) != 1:
        return Check("one contact each side for %s" % tag, False,
                     "app %d, Google %d" % (len(app), len(goog)))
    a, g = canon_app(app[0]), canon_google(goog[0])
    return Check("app and Google agree for %s" % tag, a == g,
                 "" if a == g else "app %s / Google %s" % (a, g))


def local_contacts(dump, tag=None):
    return [c for c in dump["contacts"] if c.get("names") == "Bench" and (tag is None or c.get("surname") == tag)]


def local_events(dump, prefix):
    return [a for a in dump["appointments"] if a["summary"].startswith(prefix) and not a["deleted"]]


MODE = {"master": 0}


def set_mode(master):
    """Set the app's master setting: 0 = ContactManager master (local), 1 = Google master.
    It applies to the next run, which writes it into its own settings file."""
    MODE["master"] = master


def prepare_home(home, fresh, master, extra=()):
    """Give a run its own HOME, so its settings, phone settings and local data are its own.

    A fresh home gets the sign-in from the source settings file, the sync switches, and no sync dates, so the
    first sync is a full one.  A continuing home (a session) keeps its own settings, and only the master
    setting is changed."""
    cfg_dir = os.path.join(home, ".config", "trumpton.org.uk")
    ini_dir = os.path.join(home, "TrumptonApps")
    os.makedirs(cfg_dir, exist_ok=True)
    os.makedirs(ini_dir, exist_ok=True)
    # Phone codes blank, so numbers are stored as typed (see HEADLESS-TEST.md, section 4a)
    with open(os.path.join(ini_dir, "ContactManager.ini"), "w") as f:
        f.write("[phone]\nlocalcode=\ncountrycode=\n")
    cfg = os.path.join(cfg_dir, "ContactManager.conf")
    if fresh or not os.path.exists(cfg):
        with open(CONF) as src:
            keep = [l for l in src.read().splitlines()
                    if re.match(r"^(googlerefreshtoken|googleusername)=", l)]
        lines = ["[General]"] + keep + ["calendarsync=yes", "contactsync=yes"]
    else:
        with open(cfg) as f:
            lines = [l for l in f.read().splitlines() if not l.startswith("databasemaster=")]
    lines += list(extra)
    lines.append("databasemaster=%d" % master)
    with open(cfg, "w") as f:
        f.write("\n".join(lines) + "\n")


# ----------------------------------------------------------------------------------------------
# Checks and scenarios

RESULTS = []


class Check:
    def __init__(self, label, ok, detail=""):
        self.label, self.ok, self.detail = label, bool(ok), detail


def norm(s):
    """Compare text ignoring formatting: stray spaces, tabs, newlines and case."""
    return " ".join((s or "").split()).lower()


SCENARIOS = {}


def scenario(code, title):
    def wrap(fn):
        SCENARIOS[code] = (title, fn)
        return fn
    return wrap


def add_contact_op(tag, **fields):
    f = {"names": "Bench", "surname": tag}
    f.update(fields)
    return {"op": "add_contact", "fields": f}


def sync():
    return {"op": "sync"}


def dump(file="dump.json"):
    return {"op": "dump", "file": file}


def gchange(*args):
    return {"op": "exec", "program": sys.executable, "args": [SCRIPT, "gchange"] + list(args)}


# --- contacts ---------------------------------------------------------------------------------

@scenario("C01", "Add a contact locally, no sync: Google unchanged")
def c01():
    run = Run("C01", [add_contact_op("C01", email="c01@test.invalid"), dump()]).execute()
    return [Check("no Bench contact on Google", len(bench_contacts()) == 0),
            Check("app holds the contact", len(local_contacts(run.dump, "C01")) == 1)]


@scenario("C02", "Contact made on Google: downloaded once across two syncs")
def c02():
    create_contact("C02", emails=[("c02@test.invalid", "work")])
    run = Run("C02", [sync(), dump("d1.json"), sync(), dump("d2.json")]).execute()
    d1, d2 = run.dump_file("d1.json"), run.dump_file("d2.json")
    g = [contact_info(p) for p in bench_contacts()]
    return [Check("app holds one contact after first sync", len(local_contacts(d1, "C02")) == 1),
            Check("still one after second sync", len(local_contacts(d2, "C02")) == 1),
            Check("Google still holds one", len([x for x in g if x["tag"] == "C02"]) == 1)]


@scenario("C03", "Add then delete locally before any sync: nothing reaches Google")
def c03():
    run = Run("C03", [add_contact_op("C03", email="c03@test.invalid"),
                      {"op": "delete_contact", "match": {"names": "Bench", "surname": "C03"}}, sync(), dump()]).execute()
    live = [c for c in local_contacts(run.dump, "C03") if c.get("deleted") != "true"]
    return [Check("no Bench C03 on Google", not [p for p in bench_contacts() if contact_info(p)["tag"] == "C03"]),
            Check("app no longer holds C03 as live", len(live) == 0)]


@scenario("C04", "Contact deleted on Google between syncs: removed from the app")
def c04():
    resource = create_contact("C04", emails=[("c04@test.invalid", "work")])
    run = Run("C04", [sync(), gchange("delete-contact", resource), sync(), dump()]).execute()
    live = [c for c in local_contacts(run.dump, "C04") if c.get("deleted") != "true"]
    return [Check("Google has no C04", not [p for p in bench_contacts() if contact_info(p)["tag"] == "C04"]),
            Check("app holds no live C04", len(live) == 0, "live entries: %d" % len(live))]


@scenario("C05", "Contact deleted in the app after it synced: deleted on Google")
def c05():
    create_contact("C05", emails=[("c05@test.invalid", "work")])
    run = Run("C05", [sync(), {"op": "delete_contact", "match": {"names": "Bench", "surname": "C05"}},
                      sync(), dump()]).execute()
    left = [p for p in bench_contacts() if contact_info(p)["tag"] == "C05"]
    return [Check("Google no longer holds C05", len(left) == 0, "still on Google: %d" % len(left))]


@scenario("C06", "Contact hidden in the app after it synced: Google agrees with the app")
def c06():
    create_contact("C06", emails=[("c06@test.invalid", "work")])
    run = Run("C06", [sync(), {"op": "hide_contact", "match": {"names": "Bench", "surname": "C06"}},
                      sync(), dump()]).execute()
    hidden = [c for c in local_contacts(run.dump, "C06") if c.get("hidden") == "true"]
    left = [p for p in bench_contacts() if contact_info(p)["tag"] == "C06"]
    # The app hides the entry; the expected result is that the Google record no longer shows as visible
    return [Check("app holds C06 as hidden", len(hidden) == 1),
            Check("Google record matches the app's hidden state", len(left) == 0,
                  "still on Google: %d" % len(left))]


@scenario("C07", "Same name on both sides before sync: both kept")
def c07():
    create_contact("C07", phones=[("01111 111111", "work")], emails=[("c07g@test.invalid", "work")])
    run = Run("C07", [add_contact_op("C07", phone="02222 222222", email="c07l@test.invalid"), sync(), dump()]).execute()
    g = [contact_info(p) for p in bench_contacts() if contact_info(p)["tag"] == "C07"]
    phones = sorted(v for x in g for v, _ in x["phones"])
    return [Check("Google holds two C07 contacts", len(g) == 2, "found %d" % len(g)),
            Check("both phone numbers on Google", any("111111" in p for p in phones) and any("222222" in p for p in phones),
                  "phones: %s" % phones),
            Check("app holds two C07 contacts", len(local_contacts(run.dump, "C07")) == 2)]


@scenario("C08", "Edit a downloaded contact before a second sync: change reaches Google")
def c08():
    create_contact("C08", emails=[("c08@test.invalid", "work")])
    run = Run("C08", [sync(), {"op": "edit_contact", "match": {"names": "Bench", "surname": "C08"},
                               "fields": {"surname": "C08 edited"}}, sync(), dump()]).execute()
    g = [contact_info(p) for p in bench_contacts()]
    edited = [x for x in g if x["tag"] == "C08 edited"]
    return [Check("Google shows the new surname", len(edited) == 1, "tags on Google: %s" % [x["tag"] for x in g]),
            Check("no duplicate C08", len([x for x in g if x["tag"].startswith("C08")]) == 1)]


@scenario("C09", "Edit on Google between syncs: app picks it up without duplicating")
def c09():
    resource = create_contact("C09", emails=[("c09@test.invalid", "work")])
    run = Run("C09", [sync(), gchange("edit-contact", resource, "C09 changed"), sync(), dump()]).execute()
    local = local_contacts(run.dump, "C09 changed")
    return [Check("app shows the Google change", len(local) == 1),
            Check("app holds one contact", len([c for c in run.dump["contacts"] if c.get("names") == "Bench" and c.get("surname", "").startswith("C09")]) == 1)]


@scenario("C10", "Out-of-sequence phones and emails: each value kept in some slot")
def c10():
    emails = [("e10h@test.invalid", "home"), ("e10o@test.invalid", "other"), ("e10w@test.invalid", "work")]
    phones = [("0101 010101", "mobile"), ("0202 020202", "home"), ("0303 030303", "work"), ("0404 040404", "other")]
    create_contact("C10", emails=emails, phones=phones)
    run = Run("C10", [sync(), dump("d1.json"), sync(), dump("d2.json")]).execute()
    c = local_contacts(run.dump_file("d1.json"), "C10")[0] if local_contacts(run.dump_file("d1.json"), "C10") else {}
    app_values = [c.get(k, "") for k in ("email", "email2", "email3", "email4", "phone", "mobile", "work",
                                          "phone2", "phone3", "phone4")]
    checks = []
    for value in ("e10h@", "e10o@", "e10w@"):
        checks.append(Check("app holds %s" % value, any(value in v for v in app_values + [c.get("comments", "")])))
    for value in ("0101", "0202", "0303", "0404"):
        checks.append(Check("app holds phone %s" % value, any(value in v for v in app_values + [c.get("comments", "")])))
    g = [contact_info(p) for p in bench_contacts() if contact_info(p)["tag"] == "C10"]
    checks.append(Check("Google still holds all emails after the second sync",
                        g and len(g[0]["emails"]) == 3, "emails: %s" % (g[0]["emails"] if g else None)))
    return checks


@scenario("C11", "Stray spaces and newlines in Google data: no spurious upload")
def c11():
    resource = create_contact("C11", emails=[("  c11@test.invalid \n", "work")],
                              addresses=["12 High St\n  Town  "])
    before = contact_info(api("GET", PEOPLE + "/" + resource + "?personFields=" + PERSON_FIELDS))
    run = Run("C11", [sync(), dump()]).execute()
    after = contact_info(api("GET", PEOPLE + "/" + resource + "?personFields=" + PERSON_FIELDS))
    local = local_contacts(run.dump, "C11")
    return [Check("Google not written by the sync (etag unchanged)", before["etag"] == after["etag"]),
            Check("app holds the trimmed email", bool(local) and any(norm(local[0].get(k)) == norm("c11@test.invalid") for k in EMAIL_KEYS))]


@scenario("C12", "Large upload: 40 local contacts reach Google once")
def c12():
    ops = [add_contact_op("C12-%02d" % i, email="c12-%02d@test.invalid" % i) for i in range(40)]
    run = Run("C12", ops + [sync(), dump()]).execute()
    g = [contact_info(p) for p in bench_contacts() if contact_info(p)["tag"].startswith("C12-")]
    tags = sorted(x["tag"] for x in g)
    return [Check("Google holds 40 C12 contacts", len(g) == 40, "found %d" % len(g)),
            Check("no duplicates", len(set(tags)) == len(tags))]


@scenario("C13", "Unicode and apostrophes in names round-trip exactly")
def c13():
    surname = "Ørsted-O'Brien Ångström"
    run = Run("C13", [add_contact_op(surname, email="c13@test.invalid"), sync(), dump()]).execute()
    g = [contact_info(p) for p in bench_contacts() if contact_info(p)["tag"] == surname]
    return [Check("Google holds the name exactly", len(g) == 1)]


@scenario("C14", "Email-only contact (no name) is uploaded with its email")
def c14():
    run = Run("C14", [{"op": "add_contact", "fields": {"email": "c14@test.invalid"}}, sync(), dump()]).execute()
    found = [p for p in all_connections() if any(e.get("value") == "c14@test.invalid" for e in p.get("emailAddresses", []))]
    return [Check("Google holds a contact with that email", len(found) == 1, "found %d" % len(found))]


@scenario("C15", "Repeated syncs with no changes do not write to Google")
def c15():
    resource = create_contact("C15", emails=[("c15@test.invalid", "work")])
    first = Run("C15a", [sync()]).execute()
    etag1 = contact_info(api("GET", PEOPLE + "/" + resource + "?personFields=" + PERSON_FIELDS))["etag"]
    Run("C15b", [sync(), sync()]).execute()
    etag2 = contact_info(api("GET", PEOPLE + "/" + resource + "?personFields=" + PERSON_FIELDS))["etag"]
    return [Check("etag unchanged after two more syncs", etag1 == etag2)]


@scenario("C16", "Edit keeps URLs the app does not hold (option 1 regression)")
def c16():
    resource = create_contact("C16", urls=["https://example.test/16-a", "https://example.test/16-b"],
                              nicknames=["Nick16"], biographies=["Bio16"])
    Run("C16", [sync(), {"op": "edit_contact", "match": {"names": "Bench", "surname": "C16"},
                         "fields": {"surname": "C16 edited"}}, sync(), dump()]).execute()
    info = contact_info(api("GET", PEOPLE + "/" + resource + "?personFields=" + PERSON_FIELDS))
    return [Check("both URLs kept", sorted(info["urls"]) == ["https://example.test/16-a", "https://example.test/16-b"],
                  "urls: %s" % info["urls"]),
            Check("nickname and biography kept", info["nicknames"] == ["Nick16"] and info["biographies"] == ["Bio16"]),
            Check("the edit reached Google", info["tag"] == "C16 edited", "tag: %s" % info["tag"])]


# --- appointments -----------------------------------------------------------------------------

DAY = "2026-11-%02dT%s:00Z"


@scenario("A01", "Add an appointment locally, no sync: Google unchanged")
def a01():
    run = Run("A01", [{"op": "add_appointment", "fields": {"summary": "BENCH A01", "from": DAY % (2, "09:00"),
                                                            "to": DAY % (2, "10:00")}}, dump()]).execute()
    return [Check("no BENCH A01 on Google", not [e for e in bench_events() if e["summary"] == "BENCH A01"]),
            Check("app holds it", len(local_events(run.dump, "BENCH A01")) == 1)]


@scenario("A02", "Event made on Google: downloaded once across two syncs")
def a02():
    create_event("BENCH A02", DAY % (3, "09:00"), DAY % (3, "10:00"))
    run = Run("A02", [sync(), dump("d1.json"), sync(), dump("d2.json")]).execute()
    return [Check("app holds one after the first sync", len(local_events(run.dump_file("d1.json"), "BENCH A02")) == 1),
            Check("still one after the second", len(local_events(run.dump_file("d2.json"), "BENCH A02")) == 1),
            Check("Google holds one", len([e for e in bench_events() if e["summary"] == "BENCH A02"]) == 1)]


@scenario("A03", "Identical events on Google (duplicates): both kept, no growth on re-sync")
def a03():
    create_event("BENCH A03", DAY % (4, "09:00"), DAY % (4, "10:00"))
    create_event("BENCH A03", DAY % (4, "09:00"), DAY % (4, "10:00"))
    run = Run("A03", [sync(), dump("d1.json"), sync(), dump("d2.json")]).execute()
    d1, d2 = run.dump_file("d1.json"), run.dump_file("d2.json")
    return [Check("app holds both after the first sync", len(local_events(d1, "BENCH A03")) == 2,
                  "found %d" % len(local_events(d1, "BENCH A03"))),
            Check("still two after the second sync", len(local_events(d2, "BENCH A03")) == 2),
            Check("Google still holds two", len([e for e in bench_events() if e["summary"] == "BENCH A03"]) == 2)]


@scenario("A04", "Same event added locally and on Google before sync: both kept")
def a04():
    create_event("BENCH A04", DAY % (5, "09:00"), DAY % (5, "10:00"))
    run = Run("A04", [{"op": "add_appointment", "fields": {"summary": "BENCH A04", "from": DAY % (5, "09:00"),
                                                            "to": DAY % (5, "10:00")}}, sync(), dump()]).execute()
    return [Check("Google holds two BENCH A04", len([e for e in bench_events() if e["summary"] == "BENCH A04"]) == 2)]


@scenario("A05", "Edit a downloaded event before a second sync: change reaches Google")
def a05():
    create_event("BENCH A05", DAY % (6, "09:00"), DAY % (6, "10:00"), location="Room 1")
    Run("A05", [sync(), {"op": "edit_appointment", "match": {"summary": "BENCH A05"},
                         "fields": {"location": "Room 2"}}, sync(), dump()]).execute()
    events = [e for e in bench_events() if e["summary"] == "BENCH A05"]
    return [Check("Google shows the new location", len(events) == 1 and events[0].get("location") == "Room 2",
                  "locations: %s" % [e.get("location") for e in events]),
            Check("no duplicate", len(events) == 1)]


@scenario("A06", "Event moved on Google between syncs: app picks it up")
def a06():
    eid = create_event("BENCH A06", DAY % (7, "09:00"), DAY % (7, "10:00"))
    run = Run("A06", [sync(), gchange("patch-event", eid, "Room 6"), sync(), dump()]).execute()
    local = local_events(run.dump, "BENCH A06")
    return [Check("app shows the Google change", len(local) == 1 and local[0]["location"] == "Room 6",
                  "locations: %s" % [a["location"] for a in local]),
            Check("app holds one", len(local) == 1)]


@scenario("A07", "Event deleted in the app after it synced: deleted on Google")
def a07():
    create_event("BENCH A07", DAY % (8, "09:00"), DAY % (8, "10:00"))
    Run("A07", [sync(), {"op": "delete_appointment", "match": {"summary": "BENCH A07"}}, sync(), dump()]).execute()
    return [Check("Google no longer holds BENCH A07", not [e for e in bench_events() if e["summary"] == "BENCH A07"])]


@scenario("A08", "Event deleted on Google between syncs: app does not keep a live copy")
def a08():
    eid = create_event("BENCH A08", DAY % (9, "09:00"), DAY % (9, "10:00"))
    run = Run("A08", [sync(), gchange("delete-event", eid), sync(), dump()]).execute()
    live = local_events(run.dump, "BENCH A08")
    return [Check("app holds no live BENCH A08", len(live) == 0, "live: %d" % len(live))]


@scenario("A09", "Overlapping appointments added locally: both uploaded, neither merged")
def a09():
    run = Run("A09", [{"op": "add_appointment", "fields": {"summary": "BENCH A09a", "from": DAY % (10, "09:00"),
                                                            "to": DAY % (10, "11:00")}},
                      {"op": "add_appointment", "fields": {"summary": "BENCH A09b", "from": DAY % (10, "10:00"),
                                                            "to": DAY % (10, "12:00")}}, sync(), dump()]).execute()
    names = sorted(e["summary"] for e in bench_events() if e["summary"].startswith("BENCH A09"))
    return [Check("Google holds both", names == ["BENCH A09a", "BENCH A09b"], "found %s" % names)]


@scenario("A10", "Large upload: 40 local appointments reach Google once")
def a10():
    ops = [{"op": "add_appointment", "fields": {"summary": "BENCH A10-%02d" % i,
                                                 "from": DAY % (11, "%02d:00" % (i % 12 + 7)),
                                                 "to": DAY % (11, "%02d:30" % (i % 12 + 7))}} for i in range(40)]
    Run("A10", ops + [sync(), dump()]).execute()
    names = [e["summary"] for e in bench_events() if e["summary"].startswith("BENCH A10-")]
    return [Check("Google holds 40", len(names) == 40, "found %d" % len(names)),
            Check("no duplicates", len(set(names)) == len(names))]


@scenario("A11", "Read-only event (organised by someone else): not uploaded across syncs")
def a11():
    import_external_event("BENCH A11", DAY % (12, "09:00"), DAY % (12, "10:00"))
    before = len([e for e in bench_events() if e["summary"] == "BENCH A11"])
    run = Run("A11", [sync(), sync(), dump()]).execute()
    after = len([e for e in bench_events() if e["summary"] == "BENCH A11"])
    local = local_events(run.dump, "BENCH A11")
    return [Check("Google count unchanged", before == after == 1, "before %d after %d" % (before, after)),
            Check("app holds it as read-only", len(local) == 1 and any(a["readonly"] for a in local))]


@scenario("A12", "Stray spaces and newlines in a location: no spurious upload")
def a12():
    eid = create_event("BENCH A12", DAY % (13, "09:00"), DAY % (13, "10:00"), location="  Room 12 \n  ")
    before = get_event(eid)["etag"]
    run = Run("A12", [sync(), dump()]).execute()
    after = get_event(eid)["etag"]
    local = local_events(run.dump, "BENCH A12")
    return [Check("Google not written by the sync (etag unchanged)", before == after),
            Check("app holds the location", len(local) == 1 and norm(local[0]["location"]) == norm("Room 12"))]


@scenario("A13", "Local ID of a Google event is the same across syncs")
def a13():
    create_event("BENCH A13", DAY % (14, "09:00"), DAY % (14, "10:00"))
    run = Run("A13", [sync(), dump("d1.json"), sync(), dump("d2.json")]).execute()
    a = local_events(run.dump_file("d1.json"), "BENCH A13")
    b = local_events(run.dump_file("d2.json"), "BENCH A13")
    return [Check("one entry each sync", len(a) == 1 and len(b) == 1),
            Check("same local ID both times", len(a) == 1 and len(b) == 1 and a[0]["id"] == b[0]["id"])]


# ----------------------------------------------------------------------------------------------
# Running

def run_scenario(code):
    title, fn = SCENARIOS[code]
    set_mode(CM_MASTER)          # each scenario starts on the default master, whatever the one before left
    local = group_of(code) == GROUP_LOCAL
    if not local:
        cleanup()                # local tests never touch the account, so they do not clean it either
    # Sessions carry local data between runs, so each scenario starts from none
    for name in os.listdir(RUNS):
        if name.startswith("session-"):
            shutil.rmtree(os.path.join(RUNS, name), ignore_errors=True)
    started = time.time()
    try:
        checks = fn()
    except Exception as e:                       # a scenario that cannot run is a failure, not a pass
        checks = [Check("scenario ran", False, "%s: %s" % (type(e).__name__, e))]
    seconds = time.time() - started
    ok = all(c.ok for c in checks)
    RESULTS.append({"code": code, "title": title, "ok": ok, "seconds": round(seconds, 1),
                    "checks": [{"label": c.label, "ok": c.ok, "detail": c.detail} for c in checks]})
    print("%s %s  %s (%.0fs)" % ("PASS" if ok else "FAIL", code, title, seconds))
    for c in checks:
        if not c.ok:
            print("     - %s %s" % (c.label, ("(" + c.detail + ")") if c.detail else ""))
    if not local:
        cleanup()


# --- master setting: switching between ContactManager master and Google master ----------------
# Each run starts the app with the setting as it is when the run starts.  Runs in one session share a
# working folder, so local data carries across a switch.  Mode 0 is ContactManager master, 1 is Google master.

CM_MASTER, GOOGLE_MASTER = 0, 1


@scenario("M01", "ContactManager master adds a contact; Google master syncs it: no duplicate")
def m01():
    set_mode(CM_MASTER)
    Run("M01a", [add_contact_op("M01", email="m01@test.invalid", phone="0101 010101"), sync(), dump()],
        session="M01").execute()
    set_mode(GOOGLE_MASTER)
    run = Run("M01b", [sync(), dump()], session="M01").execute()
    return [same_contact(run.dump, "M01"),
            Check("no extra copy on Google", len(google_contacts_tagged("M01")) == 1)]


@scenario("M02", "Google master downloads; ContactManager master edits; Google master re-syncs: one copy")
def m02():
    create_contact("M02", emails=[("m02@test.invalid", "work")], phones=[("0202 020202", "mobile")],
                   biographies=["Note M02"])
    set_mode(GOOGLE_MASTER)
    Run("M02a", [sync(), dump()], session="M02").execute()
    set_mode(CM_MASTER)
    Run("M02b", [{"op": "edit_contact", "match": {"names": "Bench", "surname": "M02"},
                  "fields": {"surname": "M02 edited"}}, sync(), dump()], session="M02").execute()
    set_mode(GOOGLE_MASTER)
    run = Run("M02c", [sync(), dump()], session="M02").execute()
    return [Check("one Google copy after the edit", len(google_contacts_tagged("M02 edited")) == 1),
            Check("no copy under the old name", len(google_contacts_tagged("M02")) == 0),
            same_contact(run.dump, "M02 edited")]


@scenario("M03", "Deleted in ContactManager master: Google master does not bring it back")
def m03():
    create_contact("M03", emails=[("m03@test.invalid", "work")])
    set_mode(CM_MASTER)
    Run("M03a", [sync(), {"op": "delete_contact", "match": {"names": "Bench", "surname": "M03"}}, sync(), dump()],
        session="M03").execute()
    set_mode(GOOGLE_MASTER)
    run = Run("M03b", [sync(), dump()], session="M03").execute()
    live = app_live_contacts(run.dump, "M03")
    return [Check("no M03 on Google", len(google_contacts_tagged("M03")) == 0),
            Check("no live M03 in the app", len(live) == 0, "live: %d" % len(live))]


@scenario("M04", "Deleted on Google in Google master: ContactManager master does not bring it back")
def m04():
    resource = create_contact("M04", emails=[("m04@test.invalid", "work")])
    set_mode(GOOGLE_MASTER)
    Run("M04a", [sync(), gchange("delete-contact", resource), sync(), dump()], session="M04").execute()
    set_mode(CM_MASTER)
    run = Run("M04b", [sync(), dump()], session="M04").execute()
    return [Check("no M04 on Google", len(google_contacts_tagged("M04")) == 0,
                  "on Google: %d" % len(google_contacts_tagged("M04"))),
            Check("no live M04 in the app", len(app_live_contacts(run.dump, "M04")) == 0)]


@scenario("M05", "Appointment: ContactManager master adds, Google master edits, ContactManager master picks it up")
def m05():
    set_mode(CM_MASTER)
    Run("M05a", [{"op": "add_appointment", "fields": {"summary": "BENCH M05", "location": "Room A",
                                                       "from": DAY % (20, "09:00"), "to": DAY % (20, "10:00")}},
                 sync(), dump()], session="M05").execute()
    events = [e for e in bench_events() if e["summary"] == "BENCH M05"]
    if len(events) != 1:
        return [Check("one event on Google after the first sync", False, "found %d" % len(events))]
    eid = events[0]["id"]
    set_mode(GOOGLE_MASTER)
    Run("M05b", [sync(), gchange("patch-event", eid, "Room G"), sync(), dump()], session="M05").execute()
    set_mode(CM_MASTER)
    run = Run("M05c", [sync(), dump()], session="M05").execute()
    events = [e for e in bench_events() if e["summary"] == "BENCH M05"]
    local = local_events(run.dump, "BENCH M05")
    return [Check("one event on Google throughout", len(events) == 1, "found %d" % len(events)),
            Check("Google keeps the Google-side location", len(events) == 1 and events[0].get("location") == "Room G",
                  "location: %s" % [e.get("location") for e in events]),
            Check("app holds one entry with that location", len(local) == 1 and local[0]["location"] == "Room G",
                  "app: %s" % [a["location"] for a in local])]


@scenario("M06", "Repeated switching with edits: values and notes survive, no duplicates")
def m06():
    create_contact("M06", emails=[("m06w@test.invalid", "work"), ("m06h@test.invalid", "home")],
                   phones=[("0606 060606", "mobile"), ("0707 070707", "work")],
                   biographies=["Line one\nLine two, ünïcode"])
    steps = [(GOOGLE_MASTER, []),
             (CM_MASTER, [{"op": "edit_contact", "match": {"names": "Bench", "surname": "M06"},
                           "fields": {"surname": "M06 b"}}]),
             (GOOGLE_MASTER, []),
             (CM_MASTER, [{"op": "edit_contact", "match": {"names": "Bench", "surname": "M06 b"},
                           "fields": {"comments": "Note M06 changed\nÉtage"}}]),
             (GOOGLE_MASTER, []),
             (CM_MASTER, [])]
    run = None
    for i, (mode, extra) in enumerate(steps):
        set_mode(mode)
        run = Run("M06-%d" % i, extra + [sync(), dump()], session="M06").execute()
    return [Check("one Google copy", len(google_contacts_tagged("M06 b")) == 1,
                  "copies: %d" % len(google_contacts_tagged("M06 b"))),
            same_contact(run.dump, "M06 b")]


@scenario("M07", "Google master: a local-only edit is not sent, and Google's values stay")
def m07():
    create_contact("M07", emails=[("m07@test.invalid", "work")])
    set_mode(GOOGLE_MASTER)
    Run("M07a", [sync(), {"op": "edit_contact", "match": {"names": "Bench", "surname": "M07"},
                          "fields": {"comments": "Local only note M07"}}, sync(), dump()],
        session="M07").execute()
    goog = google_contacts_tagged("M07")
    notes = " ".join(contact_info(goog[0])["biographies"]) if goog else ""
    return [Check("one Google copy", len(goog) == 1, "copies: %d" % len(goog)),
            Check("Google master did not send the local-only note", "Local only note M07" not in notes,
                  "notes on Google: %r" % notes)]


@scenario("M08", "ContactManager master: a change made on Google is kept, not overwritten")
def m08():
    resource = create_contact("M08", emails=[("m08@test.invalid", "work")])
    set_mode(CM_MASTER)
    run = Run("M08", [sync(), gchange("edit-contact", resource, "M08 from Google"), sync(), dump()],
              session="M08").execute()
    goog = google_contacts_tagged("M08 from Google")
    return [Check("Google keeps its change (not overwritten by the app)", len(goog) == 1,
                  "tags on Google: %s" % [contact_info(p)["tag"] for p in google_contacts_tagged("M08")]),
            Check("one copy", len(bench_contacts()) == 1)]


# --- corner cases ---------------------------------------------------------------------------

def create_contact_body(body):
    return api("POST", PEOPLE + "/people:createContact?personFields=" + PERSON_FIELDS, body)["resourceName"]


def create_event_body(body):
    return api("POST", CALENDAR + "/events", body)["id"]


def google_person(resource):
    return api("GET", PEOPLE + "/" + resource + "?personFields=" + PERSON_FIELDS)


@scenario("X01", "Blank and whitespace-only name fields: no crash, no duplicate")
def x01():
    run = Run("X01", [{"op": "add_contact", "fields": {"names": "Bench", "surname": "   "}},
                      {"op": "add_contact", "fields": {"names": "  ", "email": "x01@test.invalid"}},
                      sync(), dump()]).execute()
    emails = [p for p in bench_contacts()
              if any(e.get("value") == "x01@test.invalid" for e in p.get("emailAddresses", []))]
    return [Check("the run completed", run.dump is not None),
            Check("no more than one Google copy of the email contact", len(emails) <= 1, "found %d" % len(emails))]


@scenario("X02", "Very long comment (6000 characters) round-trips intact")
def x02():
    long_text = ("Lorem ipsum dolor sit amet, consectetur. " * 150)[:6000]
    create_contact("X02", biographies=[long_text])
    run = Run("X02", [sync(), dump()]).execute()
    goog = google_contacts_tagged("X02")
    notes = norm(" ".join(contact_info(goog[0])["biographies"])) if goog else ""
    local = app_live_contacts(run.dump, "X02")
    return [Check("Google copy exists", len(goog) == 1),
            Check("Google text unchanged", notes == norm(long_text), "length %d" % len(notes)),
            Check("app holds the full text", bool(local) and norm(local[0].get("comments", "")) == norm(long_text),
                  "length %d" % len(norm(local[0].get("comments", ""))) if local else "no contact")]


@scenario("X03", "Quotes, backslashes, ampersands, angle brackets, emoji and right-to-left text round-trip")
def x03():
    name = 'O\'Neil & "Co" <b>\\ é中文 \U0001F600 שלום'
    run = Run("X03", [{"op": "add_contact", "fields": {"names": "Bench", "surname": name,
                                                       "comments": 'a "quoted" \\ & <tag>'}},
                      sync(), dump()]).execute()
    goog = google_contacts_tagged(name)
    local = app_live_contacts(run.dump, name)
    return [Check("Google holds the name exactly", len(goog) == 1, "copies: %d" % len(goog)),
            Check("app holds the name exactly", len(local) == 1)]


@scenario("X04", "Phone number formats (international, brackets, extensions) keep their digits")
def x04():
    phones = [("+44 (0)20 7946 0000", "work"), ("07700-900123", "mobile"), ("+1 555 0100 ext 5", "home")]
    create_contact("X04", phones=phones)
    run = Run("X04", [sync(), dump()]).execute()
    goog = google_contacts_tagged("X04")
    digits = lambda s: "".join(ch for ch in s if ch.isdigit())
    gvals = sorted(digits(v) for v, _ in contact_info(goog[0])["phones"]) if goog else []
    want = sorted(digits(v) for v, _ in phones)
    # The app drops the (0) after a country code, so it holds the number without it
    want_app = sorted(digits(v.replace("(0)", "")) for v, _ in phones)
    return [Check("Google digits unchanged", gvals == want, "got %s" % gvals),
            Check("app holds all three numbers, without the (0) trunk zero",
                  len(app_live_contacts(run.dump, "X04")) == 1 and
                  sorted(digits(app_live_contacts(run.dump, "X04")[0].get(k, "")) for k in PHONE_KEYS
                         if app_live_contacts(run.dump, "X04")[0].get(k)) == want_app)]


@scenario("X05", "Same email on two different contacts: both kept, not merged")
def x05():
    create_contact("X05A", emails=[("x05@test.invalid", "work")])
    run = Run("X05", [add_contact_op("X05B", email="x05@test.invalid"), sync(), dump()]).execute()
    return [Check("Google keeps both", len(google_contacts_tagged("X05A")) == 1 and len(google_contacts_tagged("X05B")) == 1),
            Check("app keeps both", len(app_live_contacts(run.dump, "X05A")) == 1 and len(app_live_contacts(run.dump, "X05B")) == 1)]


@scenario("X06", "Six emails on Google: the app holds four, and the rest go to Comments")
def x06():
    emails = [("x06-%d@test.invalid" % i, "work" if i % 2 else "other") for i in range(6)]
    create_contact("X06", emails=emails)
    run = Run("X06", [sync(), dump("d1.json"), sync(), dump("d2.json")]).execute()
    d1 = run.dump_file("d1.json")
    local = app_live_contacts(d1, "X06")
    held = sum(1 for k in EMAIL_KEYS if local and local[0].get(k))
    goog = google_contacts_tagged("X06")
    gcount = len(contact_info(goog[0])["emails"]) if goog else 0
    return [Check("app holds four emails", held == 4, "held %d" % held),
            Check("the other two are kept in Comments", bool(local) and "x06-4@" in local[0].get("comments", "")
                  and "x06-5@" in local[0].get("comments", "")),
            Check("Google still holds all six emails after a second sync", gcount == 6, "Google holds %d" % gcount)]


@scenario("X07", "Birthdays: leap day and no-year birthday are not changed by the sync")
def x07():
    # Google allows one birthday per contact, so the leap day and the no-year birthday are separate contacts
    leap = create_contact_body({"names": [{"givenName": "Bench", "familyName": "X07 leap"}],
                                "birthdays": [{"date": {"year": 1980, "month": 2, "day": 29}}]})
    noyear = create_contact_body({"names": [{"givenName": "Bench", "familyName": "X07 no year"}],
                                  "birthdays": [{"date": {"month": 12, "day": 25}}]})
    before = {r: google_person(r)["etag"] for r in (leap, noyear)}
    Run("X07", [sync(), sync(), dump()]).execute()
    after = {r: google_person(r)["etag"] for r in (leap, noyear)}
    return [Check("Google not written by the syncs (leap day)", before[leap] == after[leap]),
            Check("Google not written by the syncs (no year)", before[noyear] == after[noyear])]


@scenario("X08", "Multi-line address round-trips with formatting ignored")
def x08():
    addr = "12 High Street\n  London\n\tSW1A 1AA"
    create_contact("X08", addresses=[addr])
    run = Run("X08", [sync(), dump()]).execute()
    goog = google_contacts_tagged("X08")
    gaddr = [norm(a) for a in contact_info(goog[0])["addresses"]] if goog else []
    local = app_live_contacts(run.dump, "X08")
    return [Check("Google address unchanged (formatting ignored)", gaddr == [norm(addr)], "got %s" % gaddr),
            Check("app holds the address", bool(local) and any(norm(local[0].get(k, "")) == norm(addr) for k in ("address", "address2", "address3")))]


@scenario("X09", "Hidden, then shown again: the contact is back on Google")
def x09():
    create_contact("X09", emails=[("x09@test.invalid", "work")])
    run = Run("X09", [sync(),
                      {"op": "hide_contact", "match": {"names": "Bench", "surname": "X09"}}, sync(),
                      {"op": "edit_contact", "match": {"names": "Bench", "surname": "X09"},
                       "fields": {"hidden": "false"}}, sync(), dump()]).execute()
    return [Check("Google copy back after unhiding", len(google_contacts_tagged("X09")) == 1,
                  "copies: %d" % len(google_contacts_tagged("X09"))),
            Check("app shows it as not hidden", len([c for c in app_live_contacts(run.dump, "X09")
                                                     if c.get("hidden") != "true"]) == 1)]


@scenario("X10", "Edit then delete between syncs: the stale edit is not uploaded")
def x10():
    create_contact("X10", emails=[("x10@test.invalid", "work")])
    run = Run("X10", [sync(),
                      {"op": "edit_contact", "match": {"names": "Bench", "surname": "X10"},
                       "fields": {"surname": "X10 stale edit"}},
                      {"op": "delete_contact", "match": {"names": "Bench", "surname": "X10 stale edit"}},
                      sync(), dump()]).execute()
    return [Check("Google no longer holds X10 in any form", len(google_contacts_tagged("X10")) == 0
                  and len(google_contacts_tagged("X10 stale edit")) == 0)]


@scenario("X11", "All-day event on Google: read without crashing, not rewritten")
def x11():
    eid = create_event_body({"summary": "BENCH X11 all day", "start": {"date": "2026-11-21"},
                             "end": {"date": "2026-11-22"}})
    before = get_event(eid)["etag"]
    run = Run("X11", [sync(), sync(), dump()]).execute()
    after = get_event(eid)["etag"]
    return [Check("Google event not rewritten", before == after),
            Check("app holds one entry", len(local_events(run.dump, "BENCH X11")) == 1)]


@scenario("X12", "Event crossing midnight with a timezone offset keeps the same instant")
def x12():
    create_event("BENCH X12", "2026-11-22T23:30:00+01:00", "2026-11-23T01:00:00+01:00")
    run = Run("X12", [sync(), dump("d1.json"), sync(), dump("d2.json")]).execute()
    a = local_events(run.dump_file("d1.json"), "BENCH X12")
    b = local_events(run.dump_file("d2.json"), "BENCH X12")
    return [Check("one entry after two syncs", len(a) == 1 and len(b) == 1),
            Check("same local ID after two syncs", len(a) == 1 and len(b) == 1 and a[0]["id"] == b[0]["id"])]


@scenario("X13", "Local appointment with no end time: uploaded or refused, but no crash and no duplicate")
def x13():
    run = Run("X13", [{"op": "add_appointment", "fields": {"summary": "BENCH X13", "from": DAY % (23, "09:00")}},
                      sync(), sync(), dump()]).execute()
    events = [e for e in bench_events() if e["summary"] == "BENCH X13"]
    return [Check("the run completed", run.dump is not None),
            Check("no more than one Google copy", len(events) <= 1, "found %d" % len(events))]


@scenario("X14", "Unicode summary and multi-line description round-trip")
def x14():
    summary = "BENCH X14 Café meeting – \U0001F4C5"
    desc = "Line one\nLine two é\n\nLine four"
    create_event_body({"summary": summary, "description": desc,
                       "start": {"dateTime": DAY % (24, "09:00")}, "end": {"dateTime": DAY % (24, "10:00")}})
    run = Run("X14", [sync(), dump()]).execute()
    local = [a for a in run.dump["appointments"] if a["summary"] == summary]
    goog = [e for e in api("GET", CALENDAR + "/events?maxResults=2500")["items"] if e.get("summary") == summary]
    return [Check("Google summary unchanged", len(goog) == 1),
            Check("description intact in the app", bool(local) and norm(local[0]["description"]) == norm(desc))]


@scenario("X15", "Event outside the sync window is not duplicated across syncs")
def x15():
    create_event("BENCH X15", "2019-03-01T09:00:00Z", "2019-03-01T10:00:00Z")
    run = Run("X15", [sync(), sync(), dump()]).execute()
    return [Check("Google holds one", len([e for e in bench_events() if e["summary"] == "BENCH X15"]) == 1),
            Check("app holds no more than one", len(local_events(run.dump, "BENCH X15")) <= 1)]


@scenario("X16", "Appointment deleted on both sides before sync: no error, nothing left")
def x16():
    eid = create_event("BENCH X16", DAY % (25, "09:00"), DAY % (25, "10:00"))
    run = Run("X16", [sync(), {"op": "delete_appointment", "match": {"summary": "BENCH X16"}},
                      gchange("delete-event", eid), sync(), dump()]).execute()
    return [Check("Google holds none", not [e for e in bench_events() if e["summary"] == "BENCH X16"]),
            Check("app holds no live copy", len(local_events(run.dump, "BENCH X16")) == 0)]


@scenario("X17", "Conflict: location changed in the app and on Google; ContactManager master sync")
def x17():
    eid = create_event("BENCH X17", DAY % (26, "09:00"), DAY % (26, "10:00"), location="Start")
    set_mode(CM_MASTER)
    run = Run("X17", [sync(), {"op": "edit_appointment", "match": {"summary": "BENCH X17"},
                               "fields": {"location": "From app"}},
                      gchange("patch-event", eid, "From Google"), sync(), dump()], session="X17").execute()
    events = [e for e in bench_events() if e["summary"] == "BENCH X17"]
    return [Check("one event on Google", len(events) == 1, "found %d" % len(events)),
            Check("record which side won (app expected under ContactManager master)",
                  len(events) == 1 and events[0].get("location") == "From app",
                  "Google location: %s" % [e.get("location") for e in events])]


@scenario("X18", "Five back-to-back syncs with no change: no writes after the first")
def x18():
    resource = create_contact("X18", emails=[("x18@test.invalid", "work")])
    Run("X18a", [sync()]).execute()
    etag = google_person(resource)["etag"]
    Run("X18b", [sync(), sync(), sync(), sync(), sync()]).execute()
    return [Check("etag unchanged", google_person(resource)["etag"] == etag)]


@scenario("X19", "Twenty contacts made in ContactManager master, then edits on Google under Google master")
def x19():
    ops = [add_contact_op("X19-%02d" % i, email="x19-%02d@test.invalid" % i) for i in range(20)]
    set_mode(CM_MASTER)
    Run("X19a", ops + [sync()], session="X19").execute()
    set_mode(GOOGLE_MASTER)
    tagged = sorted(bench_contacts(), key=lambda p: contact_info(p)["tag"])
    edits = [gchange("edit-contact", p["resourceName"], contact_info(p)["tag"] + " edited")
             for p in tagged[:5]]
    run = Run("X19b", edits + [sync(), dump()], session="X19").execute()
    set_mode(CM_MASTER)
    run = Run("X19c", [sync(), dump()], session="X19").execute()
    goog = bench_contacts()
    return [Check("20 contacts on Google, no duplicates", len(goog) == 20, "found %d" % len(goog)),
            Check("five edits from Google survive the switch back",
                  len([p for p in goog if contact_info(p)["tag"].endswith(" edited")]) == 5)]


# --- email labels (#59) and an address with no type (#61) -----------------------------------

def google_email_types(resource):
    """The address -> type (or custom label) Google holds for each email on a contact."""
    out = {}
    for e in google_person(resource).get("emailAddresses", []):
        label = e.get("type") or e.get("formattedType") or ""
        if (e.get("type") or "").lower() == "custom" and e.get("formattedType"):
            label = e["formattedType"]
        out[(e.get("value") or "").strip()] = label.lower()
    return out


@scenario("L01", "Four emails from Google (home, other, work, Partner): each keeps its slot and label, no write")
def l01():
    resource = create_contact_body({"names": [{"givenName": "Bench", "familyName": "L01"}],
        "emailAddresses": [{"value": "l01a@test.invalid", "type": "home"},
                           {"value": "l01b@test.invalid", "type": "other"},
                           {"value": "l01c@test.invalid", "type": "work"},
                           {"value": "l01d@test.invalid", "type": "Partner"}]})
    before = google_person(resource)["etag"]
    run = Run("L01", [sync(), dump()]).execute()
    after = google_person(resource)["etag"]
    local = app_live_contacts(run.dump, "L01")
    c = local[0] if local else {}
    got = (c.get("email"), c.get("email2"), c.get("email3"), c.get("email4"))
    labels = (c.get("emailtitle"), c.get("email2title"), c.get("email3title"), c.get("email4title"))
    return [Check("Google not written by the sync", before == after),
            Check("app holds one address per slot, in order",
                  got == ("l01a@test.invalid", "l01b@test.invalid", "l01c@test.invalid", "l01d@test.invalid"), "got %s" % (got,)),
            Check("app labels them Home, Other, Work, Partner",
                  labels == ("Home", "Other", "Work", "Partner"), "got %s" % (labels,))]


@scenario("L02", "A label changed in the app is sent to Google as that email's type")
def l02():
    resource = create_contact_body({"names": [{"givenName": "Bench", "familyName": "L02"}],
        "emailAddresses": [{"value": "l02a@test.invalid", "type": "home"},
                           {"value": "l02b@test.invalid", "type": "other"}]})
    ops = [sync(),
           {"op": "edit_contact", "match": {"surname": "L02"}, "fields": {"email2title": "Mobile"}},
           sync(), dump()]
    run = Run("L02", ops).execute()
    types = google_email_types(resource)
    return [Check("the second email is sent as Mobile", types.get("l02b@test.invalid") == "mobile",
                  "Google types: %s" % types),
            Check("the first email keeps its home type", types.get("l02a@test.invalid") == "home",
                  "Google types: %s" % types),
            Check("the app still holds the label", bool(app_live_contacts(run.dump, "L02"))
                  and app_live_contacts(run.dump, "L02")[0].get("email2title") == "Mobile")]


@scenario("L03", "Emails with no labels set are sent with the labels they always had")
def l03():
    run = Run("L03", [add_contact_op("L03", email="l03a@test.invalid", email2="l03b@test.invalid",
                                     email3="l03c@test.invalid", email4="l03d@test.invalid"),
                      sync(), dump()]).execute()
    goog = google_contacts_tagged("L03")
    types = google_email_types(contact_info(goog[0])["resourceName"]) if goog else {}
    return [Check("one copy on Google", len(goog) == 1),
            Check("Home, Other, Other, Other as Google types",
                  (types.get("l03a@test.invalid"), types.get("l03b@test.invalid"),
                   types.get("l03c@test.invalid"), types.get("l03d@test.invalid")) == ("home", "other", "other", "other"),
                  "Google types: %s" % types)]


@scenario("X20", "An address with no type is held in the first address slot")
def x20():
    create_contact_body({"names": [{"givenName": "Bench", "familyName": "X20"}],
                         "addresses": [{"formattedValue": "1 Test Road London"}]})
    run = Run("X20", [sync(), dump()]).execute()
    local = app_live_contacts(run.dump, "X20")
    return [Check("app holds the address in the first slot",
                  bool(local) and norm(local[0].get("address", "")) == norm("1 Test Road London"),
                  "address slots: %s" % ({k: v for k, v in (local[0].items() if local else []) if "address" in k},))]

def google_phone_types(resource):
    """The number -> type (or custom label) Google holds for each phone on a contact."""
    out = {}
    for n in google_person(resource).get("phoneNumbers", []):
        label = n.get("type") or n.get("formattedType") or ""
        if (n.get("type") or "").lower() == "custom" and n.get("formattedType"):
            label = n["formattedType"]
        out[digits(n.get("value") or "")] = label.lower()
    return out


@scenario("P01", "Four phones from Google (home, work, mobile, Pager): each keeps its slot and label, no write")
def p01():
    resource = create_contact_body({"names": [{"givenName": "Bench", "familyName": "P01"}],
        "phoneNumbers": [{"value": "+44 20 7946 0101", "type": "home"},
                         {"value": "+44 20 7946 0102", "type": "work"},
                         {"value": "+44 20 7946 0103", "type": "mobile"},
                         {"value": "+44 20 7946 0104", "type": "Pager"}]})
    before = google_person(resource)["etag"]
    run = Run("P01", [sync(), dump()]).execute()
    after = google_person(resource)["etag"]
    local = app_live_contacts(run.dump, "P01")
    c = local[0] if local else {}
    got = tuple(digits(c.get(k, "")) for k in ("phone", "work", "mobile", "phone2"))
    labels = tuple(c.get(k, "") for k in ("phonetitle", "worktitle", "mobiletitle", "phone2title"))
    return [Check("Google not written by the sync", before == after),
            Check("app holds the four numbers in slots Phone, Work, Mobile, Phone2",
                  got == ("+442079460101", "+442079460102", "+442079460103", "+442079460104"), "got %s" % (got,)),
            Check("app labels them Home, Work, Mobile, Pager",
                  labels == ("Home", "Work", "Mobile", "Pager"), "got %s" % (labels,))]


@scenario("P02", "A phone label changed in the app is sent to Google as that number's type")
def p02():
    create_contact_body({"names": [{"givenName": "Bench", "familyName": "P02"}],
        "phoneNumbers": [{"value": "+44 20 7946 0201", "type": "home"},
                         {"value": "+44 20 7946 0202", "type": "other"}]})
    ops = [sync(),
           {"op": "edit_contact", "match": {"surname": "P02"}, "fields": {"phone2title": "Fax"}},
           sync(), dump()]
    run = Run("P02", ops).execute()
    goog = google_contacts_tagged("P02")
    types = google_phone_types(contact_info(goog[0])["resourceName"]) if goog else {}
    local = app_live_contacts(run.dump, "P02")
    return [Check("the second number is sent as Fax", types.get(digits("+44 20 7946 0202")) == "fax",
                  "Google types: %s" % types),
            Check("the first number keeps its home type", types.get(digits("+44 20 7946 0201")) == "home",
                  "Google types: %s" % types),
            Check("the app still holds the label", bool(local) and local[0].get("phone2title") == "Fax")]


@scenario("P03", "Phones with no labels set are sent with the labels they always had")
def p03():
    run = Run("P03", [add_contact_op("P03", phone="+44 20 7946 0301", work="+44 20 7946 0302",
                                     mobile="+44 20 7946 0303", phone2="+44 20 7946 0304",
                                     phone3="+44 20 7946 0305", phone4="+44 20 7946 0306"),
                      sync(), dump()]).execute()
    goog = google_contacts_tagged("P03")
    types = google_phone_types(contact_info(goog[0])["resourceName"]) if goog else {}
    want = [("+44 20 7946 030%d" % i, t) for i, t in enumerate(("home", "work", "mobile", "other", "other", "other"), 1)]
    return [Check("one copy on Google", len(goog) == 1),
            Check("Home, Work, Mobile, Other, Other, Other as Google types",
                  [types.get(digits(v)) for v, _ in want] == [t for _, t in want], "Google types: %s" % types)]


@scenario("P04", "Seven phones on Google: the app holds six, the seventh goes to Comments")
def p04():
    phones = [("+44 20 7946 04%02d" % i, t) for i, t in enumerate(
        ("home", "work", "mobile", "other", "other", "other", "other"), 1)]
    create_contact_body({"names": [{"givenName": "Bench", "familyName": "P04"}],
        "phoneNumbers": [{"value": v, "type": t} for v, t in phones]})
    run = Run("P04", [sync(), dump("d1.json"), sync(), dump("d2.json")]).execute()
    d1 = run.dump_file("d1.json")
    local = app_live_contacts(d1, "P04")
    held = sum(1 for k in PHONE_KEYS if local and local[0].get(k))
    goog = google_contacts_tagged("P04")
    gcount = len(contact_info(goog[0])["phones"]) if goog else 0
    seventh = digits("+44 20 7946 0407")
    return [Check("app holds six phones", held == 6, "held %d" % held),
            Check("the seventh number is kept in Comments",
                  bool(local) and seventh in digits(local[0].get("comments", "")),
                  "comments: %r" % (local[0].get("comments", "") if local else "")),
            Check("Google still holds all seven after a second sync", gcount == 7, "Google holds %d" % gcount)]


@scenario("D01", "Clearing an email and a phone in the app removes them from Google")
def d01():
    resource = create_contact_body({"names": [{"givenName": "Bench", "familyName": "D01"}],
        "emailAddresses": [{"value": "d01a@test.invalid", "type": "home"},
                           {"value": "d01b@test.invalid", "type": "work"}],
        "phoneNumbers": [{"value": "+44 20 7946 0501", "type": "home"},
                         {"value": "+44 20 7946 0502", "type": "work"}]})
    ops = [sync(),
           {"op": "edit_contact", "match": {"surname": "D01"},
            "fields": {"email3": "", "email3title": "", "work": "", "worktitle": ""}},
           sync(), dump()]
    run = Run("D01", ops).execute()
    info = contact_info(google_person(resource))
    local = app_live_contacts(run.dump, "D01")
    c = local[0] if local else {}
    return [Check("the app has no second email", not c.get("email2")),
            Check("Google no longer holds the second email",
                  [v for v, _ in info["emails"]] == ["d01a@test.invalid"], "Google emails: %s" % (info["emails"],)),
            Check("Google no longer holds the work phone",
                  [digits(v) for v, _ in info["phones"]] == [digits("+44 20 7946 0501")],
                  "Google phones: %s" % (info["phones"],))]


def local_with_email(dump, address):
    """The live local contact that holds this address in any email slot."""
    return [c for c in dump["contacts"] if c.get("deleted") != "true"
            and address in [c.get(k) for k in EMAIL_KEYS]]


def form_scenario_contact(tag, address=None, home=None):
    """A Google contact with the given email address (type home), for the editor scenarios."""
    emails = [{"value": address, "type": "home"}] if address else []
    phones = [{"value": home, "type": "home"}] if home else []
    return create_contact_body({"names": [{"givenName": "Bench", "familyName": tag}],
                                "emailAddresses": emails, "phoneNumbers": phones})


@scenario("E01", "Editor: a label typed after an address becomes its title, and Google gets it as that type")
def e01():
    resource = form_scenario_contact("E01", address="e01@test.invalid")
    ops = [sync(), {"op": "open_contact", "match": {"surname": "E01"}},
           {"op": "form_set", "fields": {"email": "e01@test.invalid (Work)"}, "tidy": True},
           {"op": "form_save", "name": "e01"}, sync(), dump()]
    run = Run("E01", ops).execute()
    results = run.dump_file("results.json")
    local = app_live_contacts(run.dump, "E01")
    c = local[0] if local else {}
    types = google_email_types(resource)
    return [Check("the form was saved", results.get("e01_saved") is True, "results %s" % results),
            Check("the address is held without the label", c.get("email") == "e01@test.invalid", "got %r" % c.get("email")),
            Check("the label is the title", c.get("emailtitle") == "Work", "got %r" % c.get("emailtitle")),
            Check("Google holds the address as work", types.get("e01@test.invalid") == "work", "Google types: %s" % types)]


@scenario("E02", "Editor: a save with a bad entry is refused, focus goes to the first, nothing is saved, and the typed text is kept")
def e02():
    form_scenario_contact("E02", address="e02@test.invalid")
    bad_email = "Home: e02b@test.invalid - Home (Work)"
    bad_phone = "+44 20 7946 0401 ext 5"
    good_voip = "sip:e02@test.invalid"
    ops = [sync(), {"op": "open_contact", "match": {"surname": "E02"}},
           {"op": "form_set", "fields": {"email": bad_email, "phone": bad_phone, "voip": good_voip}, "tidy": True},
           {"op": "form_save", "name": "e02"},
           {"op": "menu_save", "name": "e02m", "fields": ["email", "phone", "voip"]},
           dump(file="d_refused.json"),
           {"op": "form_set", "fields": {"email": "e02c@test.invalid", "phone": "+44 20 7946 0401"}, "tidy": True},
           {"op": "form_save", "name": "e02ok"},
           dump()]
    run = Run("E02", ops).execute()
    results = run.dump_file("results.json")
    local = app_live_contacts(run.dump, "E02")
    c = local[0] if local else {}
    held = app_live_contacts(run.dump_file("d_refused.json"), "E02")
    h = held[0] if held else {}
    return [Check("the save with a bad entry was refused", results.get("e02_saved") is False, "results %s" % results),
            Check("focus is on the first bad entry, the email", results.get("e02_focus") == "editEmail",
                  "focus was %r" % results.get("e02_focus")),
            Check("the user was told, with a message from Contact Details", "Contact Details" in run.log()),
            Check("the typed email is kept", results.get("e02m_email") == bad_email, "shows %r" % results.get("e02m_email")),
            Check("the typed phone is kept", results.get("e02m_phone") == bad_phone, "shows %r" % results.get("e02m_phone")),
            Check("nothing was saved while an entry was wrong",
                  h.get("voip") != good_voip and h.get("email") == "e02@test.invalid",
                  "voip %r, email %r" % (h.get("voip"), h.get("email"))),
            Check("once everything is right, the save stores it all",
                  results.get("e02ok_saved") is True and c.get("email") == "e02c@test.invalid"
                  and c.get("emailtitle") == "Home", "results %s, got %r" % (results, c.get("email")))]


@scenario("E06", "Editor: an older bad entry that was never edited still stops a save, and is shown with focus")
def e06():
    bad_older = "Home: e06@test.invalid - Home (Work)"
    ops = [add_contact_op("E06", email=bad_older, phone="+44 20 7946 0601"),
           {"op": "open_contact", "match": {"surname": "E06"}},
           {"op": "form_set", "fields": {"phone": "+44 20 7946 0602"}},
           {"op": "form_save", "name": "e06"},
           dump(file="d_e06.json")]
    run = Run("E06", ops).execute()
    results = run.dump_file("results.json")
    held = app_live_contacts(run.dump_file("d_e06.json"), "E06")
    h = held[0] if held else {}
    return [Check("the save was refused", results.get("e06_saved") is False, "results %s" % results),
            Check("focus is on the older bad email", results.get("e06_focus") == "editEmail",
                  "focus was %r" % results.get("e06_focus")),
            Check("the change to the phone was not saved", digits(h.get("phone", "")) == digits("+44 20 7946 0601"),
                  "phone %r" % h.get("phone")),
            Check("the older entry is still held as it was", h.get("email") == bad_older, "email %r" % h.get("email"))]


@scenario("F01", "More emails on Google than the app has slots: all of them are still on Google after an upload")
def f01():
    addresses = ["f01a@test.invalid", "f01b@test.invalid", "f01c@test.invalid", "f01d@test.invalid",
                 "f01e@test.invalid", "f01f@test.invalid", "f01g@test.invalid"]
    types = ["Label0", "Email2", "work", "Label1", "Label2", "Label3", "Label4"]
    resource = create_contact_body({"names": [{"givenName": "Bench", "familyName": "F01"}],
        "emailAddresses": [{"value": a, "type": t} for a, t in zip(addresses, types)]})
    ops = [sync(), {"op": "edit_contact", "match": {"surname": "F01"}, "fields": {"emailtitle": "Home"}},
           sync(), dump()]
    run = Run("F01", ops).execute()
    local = app_live_contacts(run.dump, "F01")
    c = local[0] if local else {}
    held = sorted(v for v, _ in contact_info(google_person(resource))["emails"])
    return [Check("the app holds four addresses in its slots",
                  sum(1 for k in EMAIL_KEYS if c.get(k)) == 4, "slots %s" % ({k: c.get(k) for k in EMAIL_KEYS},)),
            Check("the other three are kept in Comments, with their types",
                  c.get("comments", "").count("Also on Google (") == 3, "comments: %r" % c.get("comments", "")),
            Check("Google still holds all seven addresses", held == sorted(addresses), "Google holds %s" % held)]


@scenario("F02", "Google master: a bad entry downloaded from Google stops a save of the form, and nothing is uploaded")
def f02():
    bad = "Bad Field 3: f02c@test.invalid - Bad Field 3 (Bad Field 4)"
    resource = create_contact_body({"names": [{"givenName": "Bench", "familyName": "F02"}],
        "emailAddresses": [{"value": "f02a@test.invalid", "type": "work"}, {"value": bad, "type": "Label3"}]})
    set_mode(GOOGLE_MASTER)
    try:
        ops = [sync(), {"op": "open_contact", "match": {"surname": "F02"}},
               {"op": "form_set", "fields": {"phone": "+44 20 7946 0951"}},
               {"op": "menu_save", "name": "f02", "fields": ["phone", "email"]}, dump(file="d_f02.json")]
        run = Run("F02", ops).execute()
    finally:
        set_mode(CM_MASTER)
    results = run.dump_file("results.json")
    held = app_live_contacts(run.dump_file("d_f02.json"), "F02")
    h = held[0] if held else {}
    emails = sorted(v for v, _ in contact_info(google_person(resource))["emails"])
    return [Check("File > Save refused the save: the user was told", "Contact Details" in run.log(),
                  "no Contact Details message in the log"),
            Check("the bad entry is still on the form", results.get("f02_email") == bad,
                  "shows %r" % results.get("f02_email")),
            Check("the phone change was not saved", not h.get("phone"), "phone %r" % h.get("phone")),
            Check("Google still holds the entries it had", emails == sorted(["f02a@test.invalid", bad]),
                  "Google holds %s" % emails)]


@scenario("F03", "More phones on Google than the app has slots: all of them are still on Google after an upload")
def f03():
    phones = [("+44 20 7946 0731", "home"), ("+44 20 7946 0732", "work"), ("+44 20 7946 0733", "mobile"),
              ("+44 20 7946 0734", "other"), ("+44 20 7946 0735", "other"), ("+44 20 7946 0736", "other"),
              ("+44 20 7946 0737", "Pager")]
    resource = create_contact_body({"names": [{"givenName": "Bench", "familyName": "F03"}],
        "phoneNumbers": [{"value": v, "type": t} for v, t in phones]})
    ops = [sync(), {"op": "edit_contact", "match": {"surname": "F03"}, "fields": {"phonetitle": "Home"}},
           sync(), dump()]
    run = Run("F03", ops).execute()
    local = app_live_contacts(run.dump, "F03")
    c = local[0] if local else {}
    held = sorted(digits(v) for v, _ in contact_info(google_person(resource))["phones"])
    types = google_phone_types(resource)
    return [Check("the app holds six numbers in its slots", sum(1 for k in PHONE_KEYS if c.get(k)) == 6,
                  "slots %s" % ({k: c.get(k) for k in PHONE_KEYS},)),
            Check("the seventh is kept in Comments, with its type", "Also on Google (Pager):" in c.get("comments", ""),
                  "comments: %r" % c.get("comments", "")),
            Check("Google still holds all seven numbers", held == sorted(digits(v) for v, _ in phones),
                  "Google holds %s" % held),
            Check("Google keeps the seventh number's type", types.get(digits("+44 20 7946 0737")) == "pager",
                  "Google types: %s" % types)]


@scenario("F04", "A save refused for a bad entry stops the next sync, so nothing is sent to Google")
def f04():
    bad = "Bad Field 3: f04c@test.invalid - Bad Field 3 (Bad Field 4)"
    resource = create_contact_body({"names": [{"givenName": "Bench", "familyName": "F04"}],
        "emailAddresses": [{"value": "f04a@test.invalid", "type": "work"}, {"value": bad, "type": "Label3"}]})
    ops = [sync(), {"op": "open_contact", "match": {"surname": "F04"}},
           {"op": "form_set", "fields": {"phone": "+44 20 7946 0941"}},
           {"op": "menu_save", "name": "f04", "fields": ["phone"]},
           sync(), dump()]
    run = Run("F04", ops).execute()
    log = run.log()
    phones = contact_info(google_person(resource))["phones"]
    emails = sorted(v for v, _ in contact_info(google_person(resource))["emails"])
    return [Check("the save was refused", "Contact Details" in log, "no Contact Details message"),
            Check("the sync after it did not run", "sync 1 finished" in log and "sync 2 finished" not in log,
                  "driver log has no 'sync 2 finished' line"),
            Check("nothing reached Google", not phones and emails == sorted(["f04a@test.invalid", bad]),
                  "phones %s, emails %s" % (phones, emails))]


@scenario("MG01", "Tidy Contacts merges two entries for one person when they share an address and do not conflict")
def mg01():
    ops = [add_contact_op("MG01", email="mg01@test.invalid", phone="+44 20 7946 0601"),
           add_contact_op("MG01", email="mg01@test.invalid", comments="Note from the second entry"),
           {"op": "merge_duplicates", "name": "mg01"}, dump()]
    run = Run("MG01", ops).execute()
    results = run.dump_file("results.json")
    live = app_live_contacts(run.dump, "MG01")
    c = live[0] if live else {}
    return [Check("the merge ran", results.get("mg01_merged") == 1, "results %s" % results),
            Check("one live entry is left", len(live) == 1, "live entries %d" % len(live)),
            Check("the merged entry holds the address, the phone and the note",
                  c.get("email") == "mg01@test.invalid" and digits(c.get("phone", "")) == digits("+44 20 7946 0601")
                  and "Note from the second entry" in c.get("comments", ""),
                  "email %r, phone %r, comments %r" % (c.get("email"), c.get("phone"), c.get("comments"))),
            Check("the other entry is marked as merged away", sum(1 for x in run.dump["contacts"]
                  if x.get("surname") == "MG01" and x.get("mergedinto")) == 1, "")]


@scenario("MG02", "Tidy Contacts does not merge two entries that share an address but have different names")
def mg02():
    ops = [add_contact_op("MG02A", email="mg02@test.invalid", phone="+44 20 7946 0621"),
           add_contact_op("MG02B", email="mg02@test.invalid", phone="+44 20 7946 0622"),
           {"op": "merge_duplicates", "name": "mg02"}, dump()]
    run = Run("MG02", ops).execute()
    results = run.dump_file("results.json")
    live = app_live_contacts(run.dump, "MG02A") + app_live_contacts(run.dump, "MG02B")
    return [Check("nothing was merged", results.get("mg02_merged") == 0, "results %s" % results),
            Check("both entries are still there", len(live) == 2, "live entries %d" % len(live)),
            Check("the pair is not offered for a merge, as the names differ", results.get("mg02_left") == 0,
                  "results %s" % results)]


@scenario("F05", "#66: an unchanged bad entry does not stop switching to another contact")
def f05():
    bad = "Bad Field 1: f05a@test.invalid - Bad Field 2 (Bad Field 3)"
    create_contact_body({"names": [{"givenName": "Bench", "familyName": "F05A"}],
        "emailAddresses": [{"value": bad, "type": "Label3"}]})
    create_contact_body({"names": [{"givenName": "Bench", "familyName": "F05B"}],
        "emailAddresses": [{"value": "f05b@test.invalid", "type": "home"}]})
    ops = [sync(), {"op": "open_contact", "name": "f05a", "match": {"surname": "F05A"}},
           {"op": "open_contact", "name": "f05b", "match": {"surname": "F05B"}}, dump()]
    run = Run("F05", ops).execute()
    results = run.dump_file("results.json")
    return [Check("the second contact is selected", results.get("f05b_selected") == "F05B",
                  "selected %r" % results.get("f05b_selected")),
            Check("the first contact opened", results.get("f05a_selected") == "F05A",
                  "selected %r" % results.get("f05a_selected"))]


@scenario("F06", "#66: with a contact holding an unchanged bad entry open, a Google sync still runs")
def f06():
    bad = "Bad Field 1: f06a@test.invalid - Bad Field 2 (Bad Field 3)"
    create_contact_body({"names": [{"givenName": "Bench", "familyName": "F06"}],
        "emailAddresses": [{"value": bad, "type": "Label3"}]})
    ops = [sync(), {"op": "open_contact", "name": "f06", "match": {"surname": "F06"}}, sync(), dump()]
    run = Run("F06", ops).execute()
    log = run.log()
    return [Check("the second sync ran", "sync 2 finished" in log, "driver log has no 'sync 2 finished' line")]


@scenario("F07", "#66: a contact kept with an unfixable entry is uploaded once, then not again on later syncs")
def f07():
    bad = "Bad Field 1: f07a@test.invalid - Bad Field 2 (Bad Field 3)"
    resource = create_contact_body({"names": [{"givenName": "Bench", "familyName": "F07"}],
        "emailAddresses": [{"value": bad, "type": "Label3"}]})
    ops = [sync(), {"op": "edit_contact", "match": {"surname": "F07"}, "fields": {"phone": "+44 20 7946 0701"}},
           sync(), dump()]
    run = Run("F07", ops, session="F07").execute()
    after_upload = google_person(resource).get("etag")
    # The same local data, in the same session: two more syncs with nothing changed
    Run("F07b", [sync(), sync(), dump()], session="F07").execute()
    after_more = google_person(resource).get("etag")
    return [Check("the change reached Google", digits("+44 20 7946 0701") in
                  [digits(v) for v, _ in contact_info(google_person(resource))["phones"]]),
            Check("two more syncs with no change leave Google's copy alone", after_upload == after_more,
                  "etag changed on later syncs")]


@scenario("F08", "A contact with a clean address and no titles set: uploaded once, then not again on later syncs")
def f08():
    resource = create_contact_body({"names": [{"givenName": "Bench", "familyName": "F08"}],
        "emailAddresses": [{"value": "f08a@test.invalid", "type": "home"}]})
    Run("F08", [sync(), {"op": "edit_contact", "match": {"surname": "F08"}, "fields": {"phone": "+44 20 7946 0801"}},
                sync(), dump()], session="F08").execute()
    after_upload = google_person(resource).get("etag")
    Run("F08b", [sync(), sync(), dump()], session="F08").execute()
    after_more = google_person(resource).get("etag")
    return [Check("two more syncs with no change leave Google's copy alone", after_upload == after_more,
                  "etag changed on later syncs")]


@scenario("F09", "#66: moving the selection on, as Tidy Contacts does, does not check the open contact's entries")
def f09():
    bad = "Bad Field 1: f09a@test.invalid - Bad Field 2 (Bad Field 3)"
    create_contact_body({"names": [{"givenName": "Bench", "familyName": "F09"}],
        "emailAddresses": [{"value": bad, "type": "Label3"}]})
    ops = [sync(), {"op": "open_contact", "name": "f09", "match": {"surname": "F09"}},
           {"op": "clear_selection"}, dump()]
    run = Run("F09", ops).execute()
    log = run.log()
    return [Check("the form was not checked when the selection moved on", "Contact Details" not in log,
                  "a Contact Details message was shown")]


@scenario("F10", "#69: the report's steps: a bad entry from Google, Alt-E,D, Ctrl-S, Exit, relaunch, Tidy Contacts, Alt-E,D, Exit")
def f10():
    bad = "Bad Field 1: f10@test.invalid - Bad Field 2 (Bad Field 3)"
    create_contact_body({"names": [{"givenName": "Bench", "familyName": "F10"}],
        "emailAddresses": [{"value": bad, "type": "Label3"}]})
    set_mode(GOOGLE_MASTER)
    try:
        # Steps 1 to 6: sync, Alt-E,D, Control-S, Exit
        a = Run("F10a", [sync(), {"op": "open_contact", "name": "f10a", "match": {"surname": "F10"}, "details": False},
                         {"op": "alt_edit", "name": "f10a"},
                         {"op": "menu_save", "name": "f10a", "fields": ["email"]},
                         {"op": "exit", "name": "f10a"}, dump()], session="F10").execute()
        # Steps 7 to 9: relaunch, open the contact, Exit
        b = Run("F10b", [{"op": "open_contact", "name": "f10b", "match": {"surname": "F10"}, "details": False},
                         {"op": "exit", "name": "f10b"}, dump()], session="F10").execute()
        # Steps 10 to 14: relaunch, open the contact on the Overview, Tidy Contacts, Alt-E,D, Exit
        c = Run("F10c", [{"op": "open_contact", "name": "f10c", "match": {"surname": "F10"}, "details": False},
                         {"op": "tidy_full", "name": "f10c", "surname": "F10"},
                         {"op": "tidy_contacts", "name": "f10c"},
                         {"op": "alt_edit", "name": "f10c"},
                         {"op": "exit", "name": "f10c"}, dump()], session="F10").execute()
    finally:
        set_mode(CM_MASTER)
    ra = a.dump_file("results.json")
    rb = b.dump_file("results.json")
    rc = c.dump_file("results.json")
    return [Check("step 4: Alt-E,D opens Contact Details, showing the bad entry",
                  ra.get("f10a_details") is True and ra.get("f10a_email") == bad,
                  "details %r, email %r" % (ra.get("f10a_details"), ra.get("f10a_email"))),
            Check("step 5: Control-S shows the warning, then the details tab with focus on the bad entry",
                  "Contact Details" in a.log() and ra.get("f10a_details") is True and ra.get("f10a_focus") == "editEmail",
                  "details %r, focus %r" % (ra.get("f10a_details"), ra.get("f10a_focus"))),
            Check("step 6: Exit closes", ra.get("f10a_exit_accepted") is True,
                  "exit accepted %r" % ra.get("f10a_exit_accepted")),
            Check("step 9: Exit closes after a relaunch", rb.get("f10b_exit_accepted") is True,
                  "exit accepted %r" % rb.get("f10b_exit_accepted")),
            Check("step 12: Tidy Contacts reports the bad entry",
                  any("f10@test.invalid" in line for line in rc.get("f10c_unfixed", [])),
                  "unfixed %r" % rc.get("f10c_unfixed")),
            Check("step 13: Alt-E,D after Tidy Contacts opens Contact Details", rc.get("f10c_details") is True,
                  "details %r (no contact is selected after Tidy Contacts)" % rc.get("f10c_details")),
            Check("step 14: Exit closes after Tidy Contacts, with no prompt", rc.get("f10c_exit_accepted") is True
                  and "Contact Details" not in c.log(),
                  "exit accepted %r" % rc.get("f10c_exit_accepted"))]


@scenario("F12", "#68: quitting with edits asks Save, Discard or Cancel: Save refuses a bad entry, Discard keeps the stored contact")
def f12():
    bad = "Bad Field 1: f12@test.invalid - Bad Field 2 (Bad Field 3)"
    a = Run("F12a", [add_contact_op("F12", email="f12@test.invalid"),
                     {"op": "open_contact", "name": "f12a", "match": {"surname": "F12"}},
                     {"op": "form_set", "fields": {"email": bad, "phone": "+44 20 7946 1201"}},
                     {"op": "answer", "text": "Save"},
                     {"op": "exit", "name": "f12save"},
                     {"op": "answer", "text": "Cancel"},
                     {"op": "exit", "name": "f12cancel"},
                     {"op": "answer", "text": "Discard"},
                     {"op": "exit", "name": "f12discard"},
                     dump()], session="F12").execute()
    # A fresh launch, to see what was saved
    b = Run("F12b", [{"op": "open_contact", "name": "f12b", "match": {"surname": "F12"}, "details": False},
                     dump()], session="F12").execute()
    ra = a.dump_file("results.json")
    saved = app_live_contacts(b.dump_file("dump.json"), "F12")
    stored = saved[0] if saved else {}
    return [Check("Save refuses the bad entry: the window stays open, with the warning shown",
                  ra.get("f12save_exit_accepted") is False and "Contact Details" in a.log(),
                  "exit accepted %r" % ra.get("f12save_exit_accepted")),
            Check("Cancel keeps the window open", ra.get("f12cancel_exit_accepted") is False,
                  "exit accepted %r" % ra.get("f12cancel_exit_accepted")),
            Check("Discard closes the window", ra.get("f12discard_exit_accepted") is True,
                  "exit accepted %r" % ra.get("f12discard_exit_accepted")),
            Check("Discard keeps the stored contact: no bad email, and no phone change",
                  len(saved) == 1 and stored.get("email") == "f12@test.invalid" and not stored.get("phone"),
                  "email %r, phone %r" % (stored.get("email"), stored.get("phone")))]


@scenario("F13", "#68: switching contact, Tidy Contacts and sync ask Save or Discard when the contact has edits")
def f13():
    run = Run("F13", [add_contact_op("F13A", email="f13a@test.invalid"),
                      add_contact_op("F13B", email="f13b@test.invalid"),
                      # Switching away from an edit, and discarding it
                      {"op": "open_contact", "name": "f13a", "match": {"surname": "F13A"}},
                      {"op": "form_set", "fields": {"phone": "+44 20 7946 1301"}},
                      {"op": "answer", "text": "Discard"},
                      {"op": "open_contact", "name": "f13b", "match": {"surname": "F13B"}, "details": False},
                      dump(file="d_switch_discard.json"),
                      # Switching away from an edit, and saving it
                      {"op": "form_set", "fields": {"phone": "+44 20 7946 1302"}},
                      {"op": "answer", "text": "Save"},
                      {"op": "open_contact", "name": "f13a2", "match": {"surname": "F13A"}, "details": False},
                      dump(file="d_switch_save.json"),
                      # Tidy Contacts with an edit, and discarding it
                      {"op": "form_set", "fields": {"phone": "+44 20 7946 1303"}},
                      {"op": "answer", "text": "Discard"},
                      {"op": "tidy_full", "name": "f13t", "surname": "F13A"},
                      dump(file="d_tidy_discard.json"),
                      # Sync with an edit, and saving it
                      {"op": "open_contact", "name": "f13b2", "match": {"surname": "F13B"}, "details": False},
                      {"op": "form_set", "fields": {"phone": "+44 20 7946 1304"}},
                      {"op": "answer", "text": "Save"},
                      sync(),
                      dump(file="d_sync_save.json")], session="F13").execute()
    checks = [Check("switching with Discard does not save the phone",
                    not any(c.get("phone") for c in app_live_contacts(run.dump_file("d_switch_discard.json"), "F13A")),
                    "F13A phone was saved"),
              Check("switching with Save saves the phone",
                    any(digits(c.get("phone", "")).endswith("79461302") for c in app_live_contacts(run.dump_file("d_switch_save.json"), "F13B")),
                    "F13B phone not saved"),
              Check("Tidy Contacts with Discard does not save the phone",
                    not any(c.get("phone") for c in app_live_contacts(run.dump_file("d_tidy_discard.json"), "F13A")),
                    "F13A phone was saved"),
              Check("sync with Save saves the phone first",
                    any(digits(c.get("phone", "")).endswith("79461304") for c in app_live_contacts(run.dump_file("d_sync_save.json"), "F13B")),
                    "F13B phone not saved")]
    return checks


@scenario("E03","Editor: an older entry with its label in the address is tidied when opened, and saved with the next change")
def e03():
    resource = form_scenario_contact("E03", address="e03@test.invalid (Work)")
    ops = [sync(), {"op": "open_contact", "match": {"surname": "E03"}},
           {"op": "form_set", "fields": {"phone": "+44 20 7946 0301"}},
           {"op": "form_save", "name": "e03"}, sync(), dump()]
    run = Run("E03", ops).execute()
    results = run.dump_file("results.json")
    local = app_live_contacts(run.dump, "E03")
    c = local[0] if local else {}
    types = google_email_types(resource)
    return [Check("the form was saved", results.get("e03_saved") is True, "results %s" % results),
            Check("the address is held without the label", c.get("email") == "e03@test.invalid", "got %r" % c.get("email")),
            Check("the label is the title", c.get("emailtitle") == "Work", "got %r" % c.get("emailtitle")),
            Check("Google holds the address as work", types.get("e03@test.invalid") == "work", "Google types: %s" % types)]


@scenario("E04", "Editor: a phone typed with (0) after the country code and a label after the number is saved tidily")
def e04():
    run = Run("E04", [add_contact_op("E04", phone="+44 20 7946 0401"), sync(),
                                     {"op": "open_contact", "match": {"surname": "E04"}},
                                     {"op": "form_set", "fields": {"work": "+44 (0)20 7946 0402 - Office"}, "tidy": True},
                                     {"op": "form_save", "name": "e04"}, sync(), dump()]).execute()
    results = run.dump_file("results.json")
    local = app_live_contacts(run.dump, "E04")
    c = local[0] if local else {}
    goog = google_contacts_tagged("E04")
    types = google_phone_types(contact_info(goog[0])["resourceName"]) if goog else {}
    return [Check("the form was saved", results.get("e04_saved") is True, "results %s" % results),
            Check("the number is held without the (0) and the label",
                  digits(c.get("work", "")) == digits("+44 20 7946 0402"), "got %r" % c.get("work")),
            Check("the label is the title", c.get("worktitle") == "Office", "got %r" % c.get("worktitle")),
            Check("Google holds the number as office", types.get(digits("+44 20 7946 0402")) == "office",
                  "Google types: %s" % types)]


@scenario("E05", "Editor: clearing the only detail of a contact saves the change")
def e05():
    ops = [{"op": "add_contact", "fields": {"email": "e05@test.invalid"}},
           {"op": "open_contact", "match": {"email": "e05@test.invalid"}},
           {"op": "form_set", "fields": {"email": ""}},
           {"op": "form_save", "name": "e05"}, dump()]
    run = Run("E05", ops).execute()
    results = run.dump_file("results.json")
    still = [c for c in run.dump["contacts"] if c.get("deleted") != "true" and c.get("email") == "e05@test.invalid"]
    return [Check("the form was saved", results.get("e05_saved") is True, "results %s" % results),
            Check("the address is gone from the contact", not still, "still held by %d contact(s)" % len(still))]


@scenario("T01", "Tidy Contacts puts right an older address label and a phone label; they go to Google; a second tidy changes nothing")
def t01():
    resource = create_contact_body({"names": [{"givenName": "Bench", "familyName": "T01"}],
        "emailAddresses": [{"value": "t01@test.invalid (Work)", "type": "home"}],
        "phoneNumbers": [{"value": "+44 20 7946 0701 - Mobile", "type": "home"}]})
    ops = [sync(), {"op": "tidy_contacts", "name": "tidy1"}, {"op": "tidy_contacts", "name": "tidy2"},
           sync(), dump()]
    run = Run("T01", ops).execute()
    results = run.dump_file("results.json")
    local = app_live_contacts(run.dump, "T01")
    c = local[0] if local else {}
    info = contact_info(google_person(resource))
    types = google_email_types(resource)
    ptypes = google_phone_types(resource)
    return [Check("the first tidy changed the contact", results.get("tidy1_contacts", 0) >= 1, "results %s" % results),
            Check("the address is held without the label, with its title",
                  c.get("email") == "t01@test.invalid" and c.get("emailtitle") == "Work",
                  "got %r / %r" % (c.get("email"), c.get("emailtitle"))),
            Check("the phone is held without the label, with its title",
                  digits(c.get("phone", "")) == digits("+44 20 7946 0701") and c.get("phonetitle") == "Mobile",
                  "got %r / %r" % (c.get("phone"), c.get("phonetitle"))),
            Check("Google holds the address as work", types.get("t01@test.invalid") == "work", "Google types: %s" % types),
            Check("Google holds the phone as mobile", ptypes.get(digits("+44 20 7946 0701")) == "mobile",
                  "Google phones: %s" % (info["phones"],)),
            Check("the second tidy changed nothing", results.get("tidy2_changed") == 0, "results %s" % results)]


@scenario("T02", "Tidy Contacts lists an entry it cannot fix, leaves it alone, and still fixes the other entry on the contact")
def t02():
    ops = [add_contact_op("T02", email="Home: t02@test.invalid - Home (Work)", phone="+44 20 7946 0702 (Mobile)"),
           {"op": "tidy_contacts", "name": "tidy"}, dump()]
    run = Run("T02", ops).execute()
    results = run.dump_file("results.json")
    local = app_live_contacts(run.dump, "T02")
    c = local[0] if local else {}
    unfixed = results.get("tidy_unfixed", [])
    return [Check("the unfixable email is listed", any("t02@test.invalid" in line and "more than one label" in line
                                                       for line in unfixed), "listed: %s" % unfixed),
            Check("the unfixable email is left as it was", c.get("email") == "Home: t02@test.invalid - Home (Work)",
                  "got %r" % c.get("email")),
            Check("the phone on the same contact is still fixed",
                  digits(c.get("phone", "")) == digits("+44 20 7946 0702") and c.get("phonetitle") == "Mobile",
                  "got %r / %r" % (c.get("phone"), c.get("phonetitle")))]


@scenario("T03", "Tidy Contacts lists a title with a colon, and leaves it and its number as they are")
def t03():
    ops = [add_contact_op("T03", phone="+44 20 7946 0703", phonetitle="Home: Mobile"),
           {"op": "tidy_contacts", "name": "tidy"}, dump()]
    run = Run("T03", ops).execute()
    results = run.dump_file("results.json")
    local = app_live_contacts(run.dump, "T03")
    c = local[0] if local else {}
    unfixed = results.get("tidy_unfixed", [])
    return [Check("the title is listed as not allowed", any("T03" in line and "not allowed" in line for line in unfixed),
                  "listed: %s" % unfixed),
            Check("the title is unchanged", c.get("phonetitle") == "Home: Mobile", "got %r" % c.get("phonetitle")),
            Check("the number is unchanged", digits(c.get("phone", "")) == digits("+44 20 7946 0703"),
                  "got %r" % c.get("phone"))]


def require_bench_event(event_id):
    """Refuses to change an event on the account unless it is a test event (its summary has the BENCH prefix).

    Ticket #105: the script changes nothing without the prefix, so a wrong ID cannot reach a real event."""
    summary = get_event(event_id).get("summary") or ""
    if not summary.startswith("BENCH "):
        raise SystemExit("refused: event %s is not a BENCH event (summary %r)" % (event_id, summary))


def require_bench_contact(resource):
    """Refuses to change a contact unless it is a test contact (given name Bench, or a @test.invalid address)."""
    p = api("GET", PEOPLE + "/" + resource + "?personFields=names,emailAddresses")
    given = (p.get("names") or [{}])[0].get("givenName", "")
    mails = [e.get("value", "") for e in p.get("emailAddresses", [])]
    if not (given == "Bench" or any(m.endswith("@test.invalid") for m in mails)):
        raise SystemExit("refused: %s is not a bench contact" % resource)


def gchange_main(argv):
    cmd = argv[0]
    if cmd == "edit-contact":
        resource, surname = argv[1], argv[2]
        require_bench_contact(resource)
        update_contact_surname(resource, surname)
    elif cmd == "delete-contact":
        require_bench_contact(argv[1])
        api("DELETE", PEOPLE + "/" + argv[1] + ":deleteContact")
    elif cmd == "patch-event":
        require_bench_event(argv[1])
        patch_event(argv[1], {"location": argv[2]})
    elif cmd == "patch-event-fields":
        # patch-event-fields <id> <json>: changes fields of the event, such as colorId, which the app does not hold
        require_bench_event(argv[1])
        patch_event(argv[1], json.loads(argv[2]))
    elif cmd == "move-event":
        # move-event <id> <start> <end>: both UTC times in the form of at_day()
        require_bench_event(argv[1])
        patch_event(argv[1], {"start": {"dateTime": argv[2]}, "end": {"dateTime": argv[3]}})
    elif cmd == "delete-event":
        require_bench_event(argv[1])
        api("DELETE", CALENDAR + "/events/" + argv[1])
    else:
        raise SystemExit("unknown gchange command: " + cmd)


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    if argv[0] == "gchange":
        gchange_main(argv[1:])
        return 0
    if argv[0] == "groups":
        for g in (GROUP_UPLOADS, GROUP_UPDATES, GROUP_LOCAL):
            members = [c for c in SCENARIOS if group_of(c) == g]
            print("%s (%d tests)" % (g, len(members)))
            for c in members:
                typical = TYPICAL_SECONDS.get(c)
                print("  %-6s %-10s %s" % (c, ("~%ds" % typical) if typical else "not measured", SCENARIOS[c][0]))
        print("google-quick: " + " ".join(GOOGLE_QUICK))
        return 0
    if argv[0] == "purge":
        if len(argv) < 2 or argv[1] not in ("contacts", "appointments", "both"):
            print("usage: google_bench.py purge contacts|appointments|both --yes")
            return 2
        if "--yes" not in argv:
            print("refused: purge removes everything of that kind on the account. Add --yes to run it.")
            return 2
        print("removed:", purge(argv[1]))
        return 0
    if argv[0] != "run" or len(argv) < 2:
        print(__doc__)
        return 2
    os.makedirs(RUNS, exist_ok=True)
    codes = []
    for name in argv[1:]:
        chosen = selection(name)
        codes += chosen if chosen is not None else [name]
    unknown = [c for c in codes if c not in SCENARIOS]
    if unknown:
        print("unknown scenario or selection:", " ".join(unknown))
        return 2
    print("results in", RUNS)
    print("running %d scenarios: %s" % (len(codes), " ".join(codes)))
    for code in codes:
        run_scenario(code)
    set_mode(CM_MASTER)          # leave the app on its default master setting
    passed = sum(1 for r in RESULTS if r["ok"])
    print("\n%d of %d scenarios passed" % (passed, len(RESULTS)))
    with open(os.path.join(RUNS, "results.json"), "w") as f:
        json.dump(RESULTS, f, indent=1)
    return 0 if passed == len(RESULTS) else 1


@scenario("U34", "#34: double-clicking a calendar entry opens its contact, and keeps the calendar row and its focus")
def u34():
    run = Run("U34", [add_contact_op("U34", email="u34@test.invalid"),
                      {"op": "add_appointment", "fields": {"summary": "BENCH U34 meeting", "from": DAY % (7, "09:00"),
                                                         "to": DAY % (7, "10:00")}},
                      {"op": "calendar_click", "name": "u34", "summary": "BENCH U34 meeting", "surname": "U34"},
                      dump()]).execute()
    r = run.dump_file("results.json")
    return [Check("the calendar entry was found in the list", r.get("u34_expected_row", -1) >= 0,
                  "row %r" % r.get("u34_expected_row")),
            Check("the entry's contact is opened", r.get("u34_selected") == "U34",
                  "selected %r" % r.get("u34_selected")),
            Check("the calendar row is kept", r.get("u34_row") == r.get("u34_expected_row"),
                  "row %r, expected %r" % (r.get("u34_row"), r.get("u34_expected_row"))),
            Check("focus stays on the calendar list", r.get("u34_focus") == "listCalendar",
                  "focus %r" % r.get("u34_focus"))]


@scenario("K24", "#24: when the app opens, overlapping appointments are listed, and nothing is changed")
def k24():
    clash = [{"op": "add_appointment", "fields": {"summary": "BENCH K24 a", "from": DAY % (8, "09:00"), "to": DAY % (8, "10:00")}},
             {"op": "add_appointment", "fields": {"summary": "BENCH K24 b", "from": DAY % (8, "09:30"), "to": DAY % (8, "10:30")}},
             {"op": "add_appointment", "fields": {"summary": "BENCH K24 c", "from": DAY % (8, "12:00"), "to": DAY % (8, "13:00")}}]
    Run("K24a", clash + [dump()], session="K24").execute()
    # A fresh launch on the same data: the clash is listed when the window opens
    b = Run("K24b", [dump()], session="K24").execute()
    held = sorted(x["summary"] for x in b.dump["appointments"] if x.get("summary", "").startswith("BENCH K24"))
    # No overlap, on its own: nothing is listed
    c = Run("K24c", [{"op": "add_appointment", "fields": {"summary": "BENCH K24 d", "from": DAY % (9, "09:00"), "to": DAY % (9, "10:00")}},
                     {"op": "add_appointment", "fields": {"summary": "BENCH K24 e", "from": DAY % (9, "10:00"), "to": DAY % (9, "11:00")}},
                     dump()], session="K24n").execute()
    d = Run("K24d", [dump()], session="K24n").execute()
    return [Check("the overlapping appointments are listed when the app opens", 'Appointment Clashes' in b.log(),
                  "no Appointment Clashes dialog"),
            Check("the clash is listed once, for the overlapping pair", b.log().count('dialog WarningOk "Appointment Clashes"') == 1,
                  "dialog count %d" % b.log().count('dialog WarningOk "Appointment Clashes"')),
            Check("nothing is changed: all three appointments are still held", held == ["BENCH K24 a", "BENCH K24 b", "BENCH K24 c"],
                  "held %r" % held),
            Check("back-to-back appointments do not clash", 'Appointment Clashes' not in d.log(),
                  "a clash was listed for appointments that only touch")]


def at_day(days, time_of_day):
    """A UTC time this many days from now, so repeats are checked against the window from today."""
    when = datetime.datetime.utcnow() + datetime.timedelta(days=days)
    return when.strftime("%Y-%m-%d") + "T" + time_of_day + ":00Z"


@scenario("P30", "#30: a weekly appointment is listed on each week in the repeats window, and stored once")
def p30():
    start, end = at_day(2, "09:00"), at_day(2, "10:00")
    # Weeks from the start that fall in the appointment window (90 days before today to 730 days after)
    weeks = [k for k in range(0, 200) if 2 + 7 * k <= 730]
    run = Run("P30", [add_contact_op("P30C", email="p30c@test.invalid"),
                      {"op": "add_appointment", "fields": {"summary": "BENCH P30 weekly", "from": start, "to": end,
                                                         "repeat": "2", "repeatinterval": "1"}},
                      {"op": "calendar_rows", "name": "p30"},
                      {"op": "calendar_click", "name": "p30c", "summary": "BENCH P30 weekly", "surname": "P30C",
                       "occurrence": at_day(2 + 7 * 3, "09:00")},
                      dump()]).execute()
    r = run.dump_file("results.json")
    stored = [x for x in run.dump["appointments"] if x.get("summary") == "BENCH P30 weekly"]
    listed = [h for h in r.get("p30_hints", []) if h.split("@")[0] == (stored[0]["id"] if stored else "")]
    return [Check("one row for each week in the repeats window", len(listed) == len(weeks),
                  "rows %d, expected %d" % (len(listed), len(weeks))),
            Check("the appointment is stored once, not once per repeat", len(stored) == 1,
                  "stored %d times" % len(stored)),
            Check("selecting a repeat opens the contact of its appointment", r.get("p30c_selected") == "P30C",
                  "selected %r" % r.get("p30c_selected")),
            Check("the repeat is the row that was selected", r.get("p30c_row") == r.get("p30c_expected_row", -1) >= 0,
                  "row %r, expected %r" % (r.get("p30c_row"), r.get("p30c_expected_row")))]


@scenario("P31", "#30: an edit of a repeat changes its appointment; a move to another day moves the appointment and the focus with it")
def p31():
    start = at_day(2, "09:00")
    third = at_day(2 + 7 * 2, "09:00")       # the third week's repeat
    run = Run("P31", [{"op": "add_appointment", "fields": {"summary": "BENCH P31 weekly", "from": start,
                                                         "to": at_day(2, "10:00"), "repeat": "2", "repeatinterval": "1"}},
                      {"op": "choose", "letter": "c"},
                      {"op": "repeat_edit", "name": "p31a", "summary": "BENCH P31 weekly", "occurrence": third,
                       "shiftdays": 0, "newsummary": "BENCH P31 edited"},
                      dump(file="d_inplace.json"),
                      {"op": "choose", "letter": "c"},
                      {"op": "repeat_edit", "name": "p31b", "summary": "BENCH P31 edited", "occurrence": third,
                       "shiftdays": 1},
                      dump()]).execute()
    r = run.dump_file("results.json")
    stored = [x for x in run.dump["appointments"] if x.get("summary") == "BENCH P31 edited"]
    inplace = run.dump_file("d_inplace.json")
    stored_inplace = [x for x in inplace["appointments"] if x.get("summary") == "BENCH P31 edited"]
    origin_id = stored[0]["id"] if stored else ""
    return [Check("an edit of a repeat changes the appointment, and it is stored once",
                  len(stored_inplace) == 1 and len([x for x in inplace["appointments"] if x.get("summary") == "BENCH P31 weekly"]) == 0,
                  "stored %d times" % len(stored_inplace)),
            Check("an edit within the same day keeps the focus on the repeat",
                  r.get("p31a_selected_hint", "").startswith(origin_id + "@"),
                  "focus %r" % r.get("p31a_selected_hint")),
            Check("a move to another day moves the appointment by the same amount",
                  len(stored) == 1 and stored[0].get("from") == at_day(3, "09:00"),
                  "from %r" % (stored[0].get("from") if stored else None)),
            Check("a move to another day puts the focus on the appointment",
                  r.get("p31b_selected_hint") == origin_id,
                  "focus %r, expected the appointment %r" % (r.get("p31b_selected_hint"), origin_id))]


@scenario("D30", "#30: the appointment form keeps a repeat's type and interval, and gives back none for an appointment that does not repeat")
def d30():
    r = Run("D30", [{"op": "form_repeat", "name": "weekly3", "repeat": "2", "every": "3"},
                    {"op": "form_repeat", "name": "none", "repeat": "0", "every": "1"}]).execute()
    res = r.dump_file("results.json")
    return [Check("a weekly repeat every 3 is kept", res.get("weekly3_repeat") == "2" and res.get("weekly3_every") == "3",
                  "repeat %r, every %r" % (res.get("weekly3_repeat"), res.get("weekly3_every"))),
            Check("an appointment that does not repeat stays so", res.get("none_repeat") == "0",
                  "repeat %r" % res.get("none_repeat"))]


def create_recurring_event(summary, start, end, rule):
    # A repeating event needs a time zone on its start and end
    body = {"summary": summary, "start": {"dateTime": start, "timeZone": "UTC"},
            "end": {"dateTime": end, "timeZone": "UTC"}, "recurrence": [rule]}
    return api("POST", CALENDAR + "/events", body)["id"]


@scenario("G40", "#30: a repeating Google event is stored once, with its repeat, and listed on each week")
def g40():
    start, end = at_day(2, "09:00"), at_day(2, "10:00")
    create_recurring_event("BENCH G40 weekly", start, end, "RRULE:FREQ=WEEKLY;INTERVAL=1")
    weeks = [k for k in range(0, 200) if 2 + 7 * k <= 730]
    run = Run("G40", [sync(), {"op": "calendar_rows", "name": "g40"}, dump()]).execute()
    r = run.dump_file("results.json")
    stored = [x for x in run.dump["appointments"] if x.get("summary") == "BENCH G40 weekly"]
    listed = [h for h in r.get("g40_hints", []) if stored and h.split("@")[0] == stored[0]["id"]]
    return [Check("the event is stored once, not once per week", len(stored) == 1, "stored %d times" % len(stored)),
            Check("the event is listed on each week in the repeats window", len(listed) == len(weeks),
                  "rows %d, expected %d" % (len(listed), len(weeks)))]


@scenario("T40", "#75: a weekly repeat keeps its 14:00 London time across the clocks going back")
def t40():
    from zoneinfo import ZoneInfo
    london = ZoneInfo("Europe/London")
    # 14:00 London time (summer time) on 11 October 2026; the clocks go back on 25 October 2026
    start, end = "2026-10-11T13:00:00Z", "2026-10-11T14:00:00Z"
    # The app runs in London time, so the repeat is kept in London time
    run = Run("T40", [{"op": "add_appointment", "fields": {"summary": "BENCH T40 weekly", "from": start, "to": end,
                                                         "repeat": "2", "repeatinterval": "1"}},
                      sync(), {"op": "calendar_rows", "name": "t40"}, dump()], tz="Europe/London").execute()
    r = run.dump_file("results.json")
    stored = [x for x in run.dump["appointments"] if x.get("summary") == "BENCH T40 weekly"]
    listed = [h for h in r.get("t40_hints", []) if stored and h.split("@")[0] == stored[0]["id"]]
    # The first occurrence is listed by its bare id, so the times come from the occurrences that carry a start
    local = [datetime.datetime.fromisoformat(h.split("@", 1)[1].replace("Z", "+00:00")).astimezone(london)
             for h in listed if "@" in h]
    clocks = sorted({t.strftime("%H:%M") for t in local})
    change = datetime.datetime(2026, 10, 25, tzinfo=london)
    events = api("GET", CALENDAR + "/events?q=" + urllib.parse.quote("BENCH T40 weekly")).get("items", [])
    events = [e for e in events if e.get("summary") == "BENCH T40 weekly"]
    gstart = events[0].get("start", {}) if events else {}
    return [Check("the repeat is listed both before and after the clocks go back",
                  any(t < change for t in local) and any(t > change for t in local),
                  "%d rows" % len(listed)),
            Check("every week is listed at 14:00 London time", clocks == ["14:00"], "clock times %r" % clocks),
            Check("Google is sent the repeat in London time",
                  gstart.get("timeZone") == "Europe/London" and gstart.get("dateTime", "").startswith("2026-10-11T14:00:00+01:00"),
                  "start %r" % gstart)]
@scenario("B78", "#78: a birthday saved on a contact shows in the calendar straight away, with birthdays shown")
def b78():
    # Birthdays are shown from the contacts, so the saved change must reach the calendar without a restart.  The
    # calendar is read as it is shown after the save (no rebuild), as a user sees it.
    year = datetime.datetime.utcnow().year
    run = Run("B78", [add_contact_op("B78", birthday="1980-11-03"),
                      {"op": "open_contact", "match": {"surname": "B78"}},
                      {"op": "form_set", "fields": {"birthday": "1980-11-10"}},
                      {"op": "form_save", "name": "b78"},
                      {"op": "calendar_rows", "name": "b78", "reload": False},
                      dump()], conf=["showbirthdays=yes"]).execute()
    r = run.dump_file("results.json")
    titles = r.get("b78_titles", [])
    # A birthday row's title carries its date, as "03 Nov 2026 : Bench B78 - Birthday"
    saved = [t for t in titles if "B78" in t and "irthday" in t and "10 Nov %d" % year in t]
    old = [t for t in titles if "B78" in t and "irthday" in t and "03 Nov %d" % year in t]
    return [Check("the calendar shows the saved birthday on its new date", bool(saved), "rows %r" % titles[:3]),
            Check("the old birthday date is gone", not old, "still listed: %r" % old[:2])]


@scenario("O79", "#79: a repeat that started before today, with later dates, is in the overview's future appointments")
def o79():
    run = Run("O79", [add_contact_op("O79"),
                      {"op": "add_appointment", "surname": "O79", "fields": {"summary": "BENCH O79 one-off",
                                                                            "from": at_day(5, "09:00"), "to": at_day(5, "10:00")}},
                      {"op": "add_appointment", "surname": "O79", "fields": {"summary": "BENCH O79 weekly",
                                                                            "from": at_day(-21, "09:00"), "to": at_day(-21, "10:00"),
                                                                            "repeat": "2", "repeatinterval": "1"}},
                      {"op": "open_contact", "match": {"surname": "O79"}},
                      {"op": "overview", "name": "o79"},
                      dump()]).execute()
    r = run.dump_file("results.json")
    overview = r.get("o79_overview", "")
    return [Check("a one-off appointment in the future is listed (control)", "BENCH O79 one-off" in overview,
                  "overview %r" % overview[:160]),
            Check("a weekly repeat that started 21 days ago is listed, as it has later dates",
                  "BENCH O79 weekly" in overview, "overview has the repeat: %s" % ("BENCH O79 weekly" in overview))]


def google_master(summary):
    """The repeating event on Google with this summary (not an instance of a repeat)."""
    items = api("GET", CALENDAR + "/events?q=" + urllib.parse.quote(summary) + "&singleEvents=false").get("items", [])
    masters = [e for e in items if e.get("summary") == summary and not e.get("recurringEventId") and e.get("status") != "cancelled"]
    return masters[0] if masters else {}


def google_instances(master_id):
    url = CALENDAR + "/events/%s/instances?showDeleted=true&timeMin=%s&timeMax=%s" % (
        master_id, at_day(-1, "00:00"), at_day(90, "00:00"))
    return api("GET", url).get("items", [])


@scenario("X81D", "#81: deleting just one occurrence of a repeat keeps the others, and Google leaves that date out")
def x81d():
    summary = "BENCH X81D weekly"
    gone = datetime.datetime.utcnow() + datetime.timedelta(days=2 + 14)
    run = Run("X81D", [{"op": "add_appointment", "fields": {"summary": summary, "from": at_day(2, "09:00"), "to": at_day(2, "10:00"),
                                                         "repeat": "2", "repeatinterval": "1", "repeatcount": "6"}},
                       sync(), {"op": "choose", "letter": "a"},
                       {"op": "repeat_delete", "summary": summary, "occurrence": at_day(2 + 14, "09:00")},
                       sync(), {"op": "calendar_rows", "name": "x81d", "reload": False}, dump()]).execute()
    r = run.dump_file("results.json")
    mine = [t for t in r.get("x81d_titles", []) if summary in t]
    gone_text = gone.strftime("%d %b %Y")
    master = google_master(summary)
    target = gone.strftime("%Y%m%dT090000")
    exdate = any(line.startswith("EXDATE") and target in line for line in master.get("recurrence", []))
    cancelled = any(i.get("status") == "cancelled" and (i.get("originalStartTime") or {}).get("dateTime", "").startswith(gone.strftime("%Y-%m-%dT09"))
                    for i in google_instances(master.get("id", "x")))
    return [Check("the other five occurrences are still listed", len(mine) == 5, "rows %r" % mine),
            Check("the deleted date is not listed", not any(gone_text in t for t in mine), "rows %r" % mine),
            Check("Google leaves that date out of the repeat", exdate or cancelled,
                  "EXDATE %s, cancelled instance %s, recurrence %r" % (exdate, cancelled, master.get("recurrence")))]


@scenario("X81E", "#81: changing one occurrence and all following splits the repeat, and Google gets both parts")
def x81e():
    summary = "BENCH X81E weekly"
    run = Run("X81E", [{"op": "add_appointment", "fields": {"summary": summary, "from": at_day(2, "09:00"), "to": at_day(2, "10:00"),
                                                         "repeat": "2", "repeatinterval": "1", "repeatcount": "6"}},
                       sync(), {"op": "choose", "letter": "b"},
                       {"op": "repeat_edit", "name": "x81e", "summary": summary, "occurrence": at_day(2 + 21, "09:00"),
                        "shiftdays": 0, "newsummary": "BENCH X81E later"},
                       sync(), {"op": "calendar_rows", "name": "x81e", "reload": True}, dump()]).execute()
    r = run.dump_file("results.json")
    titles = r.get("x81e_titles", [])
    before = [t for t in titles if "BENCH X81E weekly" in t]
    after = [t for t in titles if "BENCH X81E later" in t]
    origin = google_master(summary)
    later = google_master("BENCH X81E later")
    return [Check("the first three occurrences keep the old summary", len(before) == 3, "rows %r" % before),
            Check("the next three have the new summary", len(after) == 3, "rows %r" % after),
            Check("Google keeps the first part, with three occurrences", "COUNT=3" in " ".join(origin.get("recurrence", [])),
                  "recurrence %r" % origin.get("recurrence")),
            Check("Google has the new part as its own repeat, with three occurrences",
                  later.get("id") and later.get("id") != origin.get("id") and "COUNT=3" in " ".join(later.get("recurrence", [])),
                  "recurrence %r" % later.get("recurrence"))]


@scenario("X81C", "#81: cancelling the change of a repeat changes nothing")
def x81c():
    summary = "BENCH X81C weekly"
    run = Run("X81C", [{"op": "add_appointment", "fields": {"summary": summary, "from": at_day(2, "09:00"), "to": at_day(2, "10:00"),
                                                         "repeat": "2", "repeatinterval": "1", "repeatcount": "6"}},
                       sync(), {"op": "choose", "letter": "d"},
                       {"op": "repeat_edit", "name": "x81c", "summary": summary, "occurrence": at_day(2 + 14, "09:00"),
                        "shiftdays": 0, "newsummary": "BENCH X81C changed"},
                       sync(), {"op": "calendar_rows", "name": "x81c", "reload": True}, dump()]).execute()
    r = run.dump_file("results.json")
    titles = r.get("x81c_titles", [])
    master = google_master(summary)
    return [Check("no row shows the change", not any("changed" in t for t in titles), "rows %r" % titles[:3]),
            Check("Google still has the original repeat, with six occurrences",
                  master.get("summary") == summary and "COUNT=6" in " ".join(master.get("recurrence", [])),
                  "summary %r recurrence %r" % (master.get("summary"), master.get("recurrence")))]


@scenario("X81G", "#81: an occurrence deleted on Google is left out of the repeat in the app, when Google is the master")
def x81g():
    summary = "BENCH X81G weekly"
    gone = datetime.datetime.utcnow() + datetime.timedelta(days=2 + 14)
    # Uploaded with the app as master, so the repeat is on Google; then Google is the master for the next sync
    Run("X81G1", [{"op": "add_appointment", "fields": {"summary": summary, "from": at_day(2, "09:00"), "to": at_day(2, "10:00"),
                                                    "repeat": "2", "repeatinterval": "1", "repeatcount": "6"}},
                  sync()], session="x81g").execute()
    master = google_master(summary)
    instances = [i for i in google_instances(master.get("id", "none"))
                 if i.get("status") != "cancelled" and (i.get("originalStartTime") or {}).get("dateTime", "").startswith(gone.strftime("%Y-%m-%dT09"))]
    if instances:
        api("DELETE", CALENDAR + "/events/" + instances[0]["id"])
    set_mode(1)
    try:
        run = Run("X81G2", [sync(), {"op": "calendar_rows", "name": "x81g", "reload": True}, dump()], session="x81g").execute()
    finally:
        set_mode(0)
    r = run.dump_file("results.json")
    mine = [t for t in r.get("x81g_titles", []) if summary in t]
    return [Check("Google deleted an occurrence", len(instances) == 1, "instances found %d" % len(instances)),
            Check("the deleted occurrence is not listed in the app", not any(gone.strftime("%d %b %Y") in t for t in mine),
                  "rows %r" % mine),
            Check("the other five occurrences are still listed", len(mine) == 5, "rows %d" % len(mine))]


@scenario("X81H", "#81: a split that moves the time keeps Google's deleted occurrence on the right time, in London time")
def x81h():
    summary = "BENCH X81H weekly"
    third = at_day(2 + 14, "13:45")
    run1 = Run("X81H1", [{"op": "add_appointment", "fields": {"summary": summary, "from": at_day(2, "13:45"), "to": at_day(2, "14:45"),
                                                         "repeat": "2", "repeatinterval": "1", "repeatcount": "5"}},
                         sync()], session="x81h", tz="Europe/London").execute()
    master = google_master(summary)
    instances = [i for i in google_instances(master.get("id", "none"))
                 if i.get("status") != "cancelled" and (i.get("originalStartTime") or {}).get("dateTime", "").startswith(
                     datetime.datetime.utcnow().__add__(datetime.timedelta(days=16)).strftime("%Y-%m-%dT13"))]
    if instances:
        api("DELETE", CALENDAR + "/events/" + instances[0]["id"])
    # Google is the master for this sync, as when the user brings Google's changes in
    set_mode(1)
    try:
        Run("X81H2", [sync()], session="x81h", tz="Europe/London").execute()
    finally:
        set_mode(0)
    Run("X81H3", [{"op": "choose", "letter": "b"},
                  {"op": "repeat_edit", "name": "x81h", "summary": summary, "occurrence": at_day(2 + 7, "13:45"),
                   "shiftdays": 0, "shiftminutes": 62, "newsummary": "BENCH X81H later"},
                  sync(), {"op": "calendar_rows", "name": "x81h", "reload": True}, dump()], session="x81h", tz="Europe/London").execute()
    later = google_master("BENCH X81H later")
    exdates = [l for l in later.get("recurrence", []) if l.startswith("EXDATE")]
    return [Check("Google deleted an occurrence", len(instances) == 1, "instances found %d" % len(instances)),
            Check("the new part of the repeat is on Google", later.get("id") is not None, "later %r" % later.get("summary")),
            Check("the deleted occurrence is left out at the new time (14:47), not the old (13:45)",
                  any("T144700" in l for l in exdates) and not any("T134500" in l for l in exdates),
                  "EXDATE lines %r" % exdates)]


@scenario("G41", "#30: an edit of a repeat in the app changes the Google event, and the event stays repeating")
def g41():
    start, end = at_day(2, "09:00"), at_day(2, "10:00")
    eid = create_recurring_event("BENCH G41 weekly", start, end, "RRULE:FREQ=WEEKLY;INTERVAL=1")
    third = at_day(2 + 7 * 2, "09:00")
    Run("G41", [sync(), {"op": "choose", "letter": "c"}, {"op": "repeat_edit", "name": "g41", "summary": "BENCH G41 weekly", "occurrence": third,
                         "shiftdays": 0, "newsummary": "BENCH G41 edited"}, sync(), dump()]).execute()
    event = api("GET", CALENDAR + "/events/" + eid)
    rules = event.get("recurrence", [])
    return [Check("Google holds the edited summary", event.get("summary") == "BENCH G41 edited",
                  "summary %r" % event.get("summary")),
            Check("the Google event still repeats weekly", any("FREQ=WEEKLY" in r for r in rules),
                  "recurrence %r" % rules)]


def repeats_in_window(source_days):
    """Weeks from a source that fall in the appointment window (90 days before today to 730 days after), counted from 1."""
    return [k for k in range(1, 400) if -90 <= source_days + 7 * k <= 730]


@scenario("W30", "#30: a repeat whose source is older than the window is listed, and the source is not")
def w30():
    run = Run("W30", [{"op": "add_appointment", "fields": {"summary": "BENCH W30 weekly", "from": at_day(-120, "09:00"),
                                                         "to": at_day(-120, "10:00"), "repeat": "2", "repeatinterval": "1"}},
                      {"op": "calendar_rows", "name": "w30"}, dump()]).execute()
    r = run.dump_file("results.json")
    stored = [x for x in run.dump["appointments"] if x.get("summary") == "BENCH W30 weekly"]
    origin_id = stored[0]["id"] if stored else ""
    hints = r.get("w30_hints", [])
    repeats = [h for h in hints if h.split("@")[0] == origin_id and "@" in h]
    return [Check("the source, older than the window, is not listed", origin_id != "" and origin_id not in hints,
                  "source listed"),
            Check("its repeats in the window are listed", len(repeats) == len(repeats_in_window(-120)),
                  "repeats %d, expected %d" % (len(repeats), len(repeats_in_window(-120)))),
            Check("the source is stored once", len(stored) == 1, "stored %d times" % len(stored))]


@scenario("G42", "#30: a repeating Google event whose first occurrence is older than the window still lists its repeats")
def g42():
    create_recurring_event("BENCH G42 weekly", at_day(-120, "09:00"), at_day(-120, "10:00"), "RRULE:FREQ=WEEKLY;INTERVAL=1")
    run = Run("G42", [sync(), {"op": "calendar_rows", "name": "g42"}, dump()]).execute()
    r = run.dump_file("results.json")
    stored = [x for x in run.dump["appointments"] if x.get("summary") == "BENCH G42 weekly"]
    origin_id = stored[0]["id"] if stored else ""
    repeats = [h for h in r.get("g42_hints", []) if origin_id and h.split("@")[0] == origin_id and "@" in h]
    return [Check("the event is stored once", len(stored) == 1, "stored %d times" % len(stored)),
            Check("its repeats in the window are listed", len(repeats) == len(repeats_in_window(-120)),
                  "repeats %d, expected %d" % (len(repeats), len(repeats_in_window(-120))))]


@scenario("E30", "#30: a repeat ends after a number of times, or on a date, in the list and in the form")
def e30():
    weekly = {"summary": "BENCH E30 count", "from": at_day(2, "09:00"), "to": at_day(2, "10:00"),
              "repeat": "2", "repeatinterval": "1", "repeatcount": "3"}
    until_ = dict(weekly, summary="BENCH E30 until", repeatcount="", repeatuntil=at_day(16, "09:00"))
    run = Run("E30", [{"op": "add_appointment", "fields": weekly},
                      {"op": "add_appointment", "fields": until_},
                      {"op": "calendar_rows", "name": "e30"},
                      {"op": "form_repeat", "name": "formcount", "repeat": "2", "every": "1", "count": "3", "until": ""},
                      {"op": "form_repeat", "name": "formuntil", "repeat": "2", "every": "1", "count": "",
                       "until": at_day(16, "09:00")},
                      dump()]).execute()
    r = run.dump_file("results.json")
    rows = r.get("e30_hints", [])
    stored = {x.get("summary"): x.get("id") for x in run.dump["appointments"]}
    count_id = stored.get("BENCH E30 count", "")
    until_id = stored.get("BENCH E30 until", "")
    count_rows = [h for h in rows if count_id and h.split("@")[0] == count_id]
    until_rows = [h for h in rows if until_id and h.split("@")[0] == until_id]
    return [Check("a repeat with a count is listed that many times, including the first", len(count_rows) == 3,
                  "rows %d, expected 3" % len(count_rows)),
            Check("a repeat with an until date stops on that date", len(until_rows) == 3,
                  "rows %d, expected 3 (weeks 0, 1 and 2)" % len(until_rows)),
            Check("the form keeps a count", r.get("formcount_count") == "3" and r.get("formcount_until") == "",
                  "count %r, until %r" % (r.get("formcount_count"), r.get("formcount_until"))),
            Check("the form keeps an until date", r.get("formuntil_until", "")[:10] == at_day(16, "09:00")[:10],
                  "until %r" % r.get("formuntil_until"))]


@scenario("G43", "#30: a Google repeat with a count is stored once and listed that many times, and an app edit keeps the count")
def g43():
    eid = api("POST", CALENDAR + "/events", {"summary": "BENCH G43 count", "start": {"dateTime": at_day(2, "09:00"), "timeZone": "UTC"},
                                              "end": {"dateTime": at_day(2, "10:00"), "timeZone": "UTC"},
                                              "recurrence": ["RRULE:FREQ=WEEKLY;COUNT=3"]})["id"]
    third = at_day(2 + 7 * 2, "09:00")
    run = Run("G43", [sync(), {"op": "calendar_rows", "name": "g43"},
                      {"op": "repeat_edit", "name": "g43", "summary": "BENCH G43 count", "occurrence": third,
                       "shiftdays": 0, "newsummary": "BENCH G43 edited"},
                      sync(), dump()]).execute()
    stored = [x for x in run.dump["appointments"] if x.get("summary") in ("BENCH G43 count", "BENCH G43 edited")]
    origin_id = stored[0]["id"] if stored else ""
    rows = [h for h in run.dump_file("results.json").get("g43_hints", []) if origin_id and h.split("@")[0] == origin_id]
    event = api("GET", CALENDAR + "/events/" + eid)
    return [Check("the Google repeat is stored once, and listed three times", len(stored) == 1 and len(rows) == 3,
                  "stored %d, rows %d" % (len(stored), len(rows))),
            Check("the count survives an edit in the app", any("COUNT=3" in r for r in event.get("recurrence", [])),
                  "recurrence %r" % event.get("recurrence"))]


@scenario("T30", "#30: Goto Today selects the first entry that has not ended, including a repeat, rather than the end of the list")
def t30():
    run = Run("T30", [{"op": "add_appointment", "fields": {"summary": "BENCH T30 weekly", "from": at_day(-3, "09:00"),
                                                         "to": at_day(-3, "10:00"), "repeat": "2", "repeatinterval": "1"}},
                      {"op": "calendar_rows", "name": "t30"},
                      {"op": "goto_today", "name": "t30"},
                      dump()]).execute()
    r = run.dump_file("results.json")
    hint = r.get("t30_hint") or ""
    return [Check("the selected row is the repeat four days from now", hint.endswith("@" + at_day(4, "09:00")),
                  "selected %r" % hint),
            Check("Goto Today does not go to the end of the list", r.get("t30_row", -1) != len(r.get("t30_hints", [])) - 1,
                  "row %r of %d" % (r.get("t30_row"), len(r.get("t30_hints", []))))]


@scenario("G44", "#30: an appointment on Google older than the one-year lookback is not downloaded")
def g44():
    api("POST", CALENDAR + "/events", {"summary": "BENCH G44 old", "start": {"dateTime": at_day(-400, "09:00"), "timeZone": "UTC"},
                                       "end": {"dateTime": at_day(-400, "10:00"), "timeZone": "UTC"}})
    run = Run("G44", [sync(), dump()]).execute()
    held = [x for x in run.dump["appointments"] if x.get("summary") == "BENCH G44 old"]
    return [Check("an event 400 days old is not downloaded", len(held) == 0, "held %d" % len(held))]


@scenario("P33", "#73: shortening a repeat from a later entry keeps the focus on the nearest entry of that repeat")
def p33():
    run = Run("P33", [{"op": "add_appointment", "fields": {"summary": "BENCH P33 weekly", "from": at_day(2, "09:00"),
                                                         "to": at_day(2, "10:00"), "repeat": "2", "repeatinterval": "1"}},
                      {"op": "calendar_rows", "name": "p33a"},
                      {"op": "choose", "letter": "c"},
                      {"op": "repeat_edit", "name": "p33", "summary": "BENCH P33 weekly", "occurrence": at_day(2 + 7 * 15, "09:00"),
                       "shiftdays": 0, "count": "2"},
                      dump()]).execute()
    r = run.dump_file("results.json")
    stored = [x for x in run.dump["appointments"] if x.get("summary") == "BENCH P33 weekly"]
    origin_id = stored[0]["id"] if stored else ""
    listed = r.get("p33_listed") or ""
    row, rows = r.get("p33_row", -1), r.get("p33_rows", 0)
    return [Check("the list has the two remaining entries and the Add row", rows == 3, "rows %d, expected 3" % rows),
            Check("the focus is on an entry of this repeat", row >= 0 and listed.split("@")[0] == origin_id and origin_id != "",
                  "row %r, listed %r" % (row, listed)),
            Check("the focus is on the nearest remaining entry to the one edited",
                  listed.endswith("@" + at_day(2 + 7, "09:00")), "listed %r" % listed)]


@scenario("Q34", "#74: OK on the edit of a repeat that started 11 days ago closes the form, with no dialog")
def q34():
    run = Run("Q34", [{"op": "add_appointment", "fields": {"summary": "BENCH Q34 repeat", "from": at_day(-11, "10:00"),
                                                         "to": at_day(-11, "11:00"), "repeat": "2", "repeatinterval": "3",
                                                         "repeatcount": "32"}},
                      {"op": "edit_ok", "name": "q34", "summary": "BENCH Q34 repeat", "occurrence": at_day(-11, "10:00")},
                      dump()]).execute()
    r = run.dump_file("results.json")
    log = run.log()
    dialogs = [l for l in log.splitlines() if " dialog " in l and "Login" not in l and "failed to login" not in l
               and "AppointmentForm" not in l]
    return [Check("the edit path returns", r.get("q34_returned") is True, "not returned"),
            Check("no dialog is shown when OK is pressed", len(dialogs) == 0, "dialogs: %r" % dialogs[:3])]


@scenario("Q35", "#74: a start three months back is accepted, and OK then closes the form, as with a start 11 days back")
def q35():
    run = Run("Q35", [{"op": "add_appointment", "fields": {"summary": "BENCH Q35 repeat", "from": at_day(2, "09:00"),
                                                         "to": at_day(2, "10:00"), "repeat": "2", "repeatinterval": "3",
                                                         "repeatcount": "32"}},
                      {"op": "edit_ok", "name": "q35", "summary": "BENCH Q35 repeat", "occurrence": at_day(2 + 21, "09:00"),
                       "froms": [at_day(-112, "09:00"), at_day(-11, "09:00")]},
                      dump()]).execute()
    r = run.dump_file("results.json")
    log = run.log()
    forms = [l for l in log.splitlines() if "form start set" in l]
    invalid = [l for l in log.splitlines() if "Invalid Date" in l]
    return [Check("the edit path returns", r.get("q35_returned") is True, "not returned"),
            Check("a start three months back is not refused as an invalid date", len(invalid) == 0,
                  "refused %d time(s)" % len(invalid)),
            Check("the form closes on the first OK", len(forms) == 1, "form shown %d times" % len(forms))]


# ----------------------------------------------------------------------------------------------
# Ticket #105: the four exchanges for appointments (single and repeat), the archive, and the scale run.
#
#                         edit in ContactManager        edit in Google
#   ContactManager master   A05 / G41 (single / repeat)   A06 / R01 (single / repeat)
#   Google master           R02 / R03 (single / repeat)   R04 / R05 (single / repeat)
#
# Edits made on Google go through gchange_main, which refuses anything without the BENCH prefix.

def edit_op(summary, **fields):
    return {"op": "edit_appointment", "match": {"summary": summary}, "fields": fields}


def bench_event(summary):
    return [e for e in bench_events() if e["summary"] == summary]


@scenario("L05", "#101: Show Screen Reader Text: shows the name, role, value and description of the focused control, and says it is an approximation")
def l05():
    set_mode(CM_MASTER)
    b = Run("L05", [{"op": "call", "name": "l05", "fn": "showacc"}, {"op": "accinfo", "name": "l05"}, dump()]).execute()
    text = b.dump_file("results.json").get("l05_text", "NONE")
    return [Check("the dialog opens", text != "NONE", "no dialog text recorded"),
            Check("it shows the control that had the focus", "Control: listCalendar" in text, text[:120]),
            Check("it shows name, role, value and description", all(k in text for k in ("Name:", "Role:", "Value:", "Description:")),
                  text[:200]),
            Check("it says the text is an approximation", "approximation" in b.dump_file("results.json").get("l05_note", "").lower(),
                  "")]


@scenario("L06", "#101: the accessibility debug checkbox in the screen reader text dialog turns console tracing on and off")
def l06():
    set_mode(CM_MASTER)
    b = Run("L06", [{"op": "call", "name": "l06", "fn": "showacc"},
                    {"op": "accdebug", "name": "l06on", "value": True},
                    {"op": "accdebug", "name": "l06off", "value": False}, dump()]).execute()
    res = b.dump_file("results.json")
    log = open(os.path.join(b.dir, "stdout.log"), errors="replace").read()
    return [Check("the checkbox is ticked", res.get("l06on_box") is True, "box %r" % res.get("l06on_box")),
            Check("and cleared again", res.get("l06off_box") is False, "box %r" % res.get("l06off_box")),
            Check("switching on is logged to the console", "AccessibleDebug on" in log, ""),
            Check("switching off is logged to the console", "AccessibleDebug off" in log, "")]


def etag_stored(workdir, summary):
    """The stored ETag of the appointment with this summary in a run's local data, or None when it is not there."""
    name = folder_holding(workdir, "", summary)
    if name is None:
        return None
    with open(os.path.join(workdir, "home", "TrumptonApps", "ContactManager", "calendar", name)) as f:
        for line in f:
            if line.startswith("googleetag="):
                return line.split("=", 1)[1].strip()
    return ""


@scenario("E01", "#86: an ETag is stored from Google and sent with a change: a local edit reaches Google, with no conflict")
def e01():
    set_mode(CM_MASTER)
    eid = create_event("BENCH E01", at_day(10, "09:00"), at_day(10, "10:00"), location="Room 1")
    workdir = os.path.join(RUNS, "session-E01")
    Run("E01a", [sync()], session="E01").execute()
    stored = etag_stored(workdir, "BENCH E01")
    b = Run("E01b", [edit_op("BENCH E01", location="Room 2"), sync(), dump()], session="E01").execute()
    goog = bench_event("BENCH E01")
    local = local_events(b.dump, "BENCH E01")
    reports = "".join(open(os.path.join(b.dir, f), errors="replace").read() for f in os.listdir(b.dir) if f.startswith("report"))
    return [Check("Google keeps the ETag it gave the event", bool(stored), "stored ETag: %r" % stored),
            Check("the local edit reaches Google", len(goog) == 1 and goog[0].get("location") == "Room 2",
                  "Google: %s" % [g.get("location") for g in goog]),
            Check("no conflict is reported", "CONFLICT" not in reports, ""),
            Check("the app holds it once, with the new location", len(local) == 1 and local[0]["location"] == "Room 2",
                  "app: %s" % [a["location"] for a in local])]


@scenario("E02", "#86: Google's copy changed in its details since the last sync (non-matching ETag): Google's version is kept, the local edit is not sent, and the conflict is reported")
def e02():
    set_mode(CM_MASTER)
    eid = create_event("BENCH E02", at_day(11, "09:00"), at_day(11, "10:00"), location="Room 1")
    workdir = os.path.join(RUNS, "session-E02")
    Run("E02a", [sync()], session="E02").execute()
    gchange_main(["patch-event", eid, "Room G"])     # Google changes the location: its ETag changes
    b = Run("E02b", [edit_op("BENCH E02", location="Room L"), sync(), dump()], session="E02").execute()
    goog = bench_event("BENCH E02")
    local = local_events(b.dump, "BENCH E02")
    reports = "".join(open(os.path.join(b.dir, f), errors="replace").read() for f in os.listdir(b.dir) if f.startswith("report"))
    return [Check("Google keeps its own location (the local edit was not sent)", len(goog) == 1 and goog[0].get("location") == "Room G",
                  "Google: %s" % [g.get("location") for g in goog]),
            Check("the app takes Google's version", len(local) == 1 and local[0]["location"] == "Room G",
                  "app: %s" % [a["location"] for a in local]),
            Check("the conflict is reported", "CONFLICT" in reports, "")]


@scenario("E03", "#86: Google's ETag changed, but not its details (a field the app does not hold): the change is sent again with the new ETag, and reaches Google")
def e03():
    set_mode(CM_MASTER)
    eid = create_event("BENCH E03", at_day(12, "09:00"), at_day(12, "10:00"), location="Room 1")
    Run("E03a", [sync()], session="E03").execute()
    gchange_main(["patch-event-fields", eid, json.dumps({"colorId": "5"})])     # changes the ETag, not the details the app holds
    b = Run("E03b", [edit_op("BENCH E03", location="Room L"), sync(), dump()], session="E03").execute()
    goog = bench_event("BENCH E03")
    local = local_events(b.dump, "BENCH E03")
    reports = "".join(open(os.path.join(b.dir, f), errors="replace").read() for f in os.listdir(b.dir) if f.startswith("report"))
    return [Check("the local edit reaches Google, after the snapshot check", len(goog) == 1 and goog[0].get("location") == "Room L",
                  "Google: %s" % [g.get("location") for g in goog]),
            Check("no conflict is reported", "CONFLICT" not in reports, ""),
            Check("the app holds it once, with the new location", len(local) == 1 and local[0]["location"] == "Room L",
                  "app: %s" % [a["location"] for a in local])]


def state_op(name):
    """Records what the screen shows: the window title, the contact on the form, and the calendar's current entry."""
    return {"op": "state", "name": name}


@scenario("C103", "#103: the contact on the form stays loaded through a sync, in ContactManager master and in Google master, and the calendar entry does not move")
def c103():
    set_mode(CM_MASTER)
    Run("C103a", [add_contact_op("C103", email="c103@test.invalid"), sync(), dump()], session="C103").execute()
    cm = Run("C103b", [{"op": "open_contact", "match": {"surname": "C103"}}, {"op": "setrow", "row": 40}, state_op("cm_before"), sync(),
                       state_op("cm_after"), dump()], session="C103").execute().dump_file("results.json")
    set_mode(GOOGLE_MASTER)
    gm = Run("C103c", [{"op": "open_contact", "match": {"surname": "C103"}}, {"op": "setrow", "row": 40}, state_op("gm_before"), sync(),
                       state_op("gm_after"), dump()], session="C103").execute().dump_file("results.json")
    set_mode(CM_MASTER)
    return [Check("ContactManager master: the contact stays loaded through the sync",
                  cm.get("cm_after_loaded") == "C103", "loaded: %r" % cm.get("cm_after_loaded")),
            Check("ContactManager master: the window still shows the contact",
                  "C103" in str(cm.get("cm_after_title", "")), "title: %r" % cm.get("cm_after_title")),
            Check("Google master: the contact stays loaded through the sync",
                  gm.get("gm_after_loaded") == "C103", "loaded: %r" % gm.get("gm_after_loaded")),
            Check("Google master: the window still shows the contact",
                  "C103" in str(gm.get("gm_after_title", "")), "title: %r" % gm.get("gm_after_title")),
            Check("the calendar has a current entry to keep (row 40, away from the top)", cm.get("cm_before_entry", "none") != "none",
                  "entry: %r" % cm.get("cm_before_entry")),
            Check("the calendar's current entry is the same after the sync",
                  cm.get("cm_before_entry") == cm.get("cm_after_entry"),
                  "before %r, after %r" % (cm.get("cm_before_entry"), cm.get("cm_after_entry")))]


def calendar_report(run):
    """The text of a run's sync reports, as written by the app."""
    return "".join(open(os.path.join(run.dir, f), errors="replace").read()
                   for f in sorted(os.listdir(run.dir)) if f.startswith("report"))


def http_count(run, verb, what):
    """How many Google requests of this kind the app made in a run (from its driver log)."""
    text = open(os.path.join(run.dir, "stdout.log"), errors="replace").read()
    return text.count("TIMING http %s %s status=" % (verb, what))


@scenario("N107", "#107: an appointment with no end time, or one ending on 1 Jan 1970, is not uploaded; the sync completes and the others reach Google")
def n107():
    set_mode(CM_MASTER)
    run = Run("N107", [
        {"op": "add_appointment", "fields": {"summary": "BENCH N107 noend", "from": at_day(31, "09:00")}},
        {"op": "add_appointment", "fields": {"summary": "BENCH N107 epoch", "from": at_day(32, "09:00"),
                                             "to": "1970-01-01T00:00:00Z"}},
        {"op": "add_appointment", "fields": {"summary": "BENCH N107 ok", "from": at_day(33, "09:00"),
                                             "to": at_day(33, "10:00")}},
        sync(), dump()]).execute()
    report = calendar_report(run)
    return [Check("the calendar sync completes", "CALENDAR SYCHRONISATION SUCCESS" in report and "FAILED" not in report,
                  "report ends: %r" % report[-120:]),
            Check("the appointment with a valid end reaches Google", len(bench_event("BENCH N107 ok")) == 1,
                  "Google holds %d" % len(bench_event("BENCH N107 ok"))),
            Check("no end time: not uploaded", not bench_event("BENCH N107 noend"),
                  "Google holds %d" % len(bench_event("BENCH N107 noend"))),
            Check("1 Jan 1970 end: not uploaded", not bench_event("BENCH N107 epoch"),
                  "Google holds %d" % len(bench_event("BENCH N107 epoch"))),
            Check("the report says why they were not uploaded", "no end time" in report, "")]


@scenario("N108", "#108: with the last calendar sync back at the epoch (as after a failed sync), the next sync sends only the appointment that was edited, not the ones it downloaded")
def n108():
    set_mode(CM_MASTER)
    for i in range(40):
        create_event("BENCH N108 %02d" % i, at_day(40 + i, "09:00"), at_day(40 + i, "10:00"), location="Room")
    # The first sync downloads the 40
    Run("N108a", [sync(), dump()], session="N108").execute()
    # A failed sync leaves the last-sync date at the epoch, so the next sync is run with it at the epoch.  One of the 40 is
    # edited in the app: that is the only change that should be sent
    b = Run("N108b", [{"op": "setting", "key": "lastgooglecalendarsyncdate", "value": ""},
                      edit_op("BENCH N108 05", location="Room Edited"), sync(), dump()], session="N108").execute(max_writes=20)
    puts = http_count(b, "PUT", "updateAppointment")
    edited = bench_event("BENCH N108 05")
    return [Check("the run was not stopped for sending too many updates (more than 20)", not b.aborted,
                  "%d updates sent before it was stopped" % puts),
            Check("the next sync completes", "FAILED" not in calendar_report(b), ""),
            Check("only the edited appointment is sent to Google", puts == 1, "%d updates sent" % puts),
            Check("the edit reaches Google", len(edited) == 1 and edited[0].get("location") == "Room Edited",
                  "Google: %s" % [e.get("location") for e in edited]),
            Check("the other 39 are unchanged on Google", all(
                len(bench_event("BENCH N108 %02d" % i)) == 1 and bench_event("BENCH N108 %02d" % i)[0].get("location") == "Room"
                for i in range(40) if i != 5), "")]


def run_path(run_name, name):
    return os.path.join(RUNS, run_name, name)


def listdir_op(run_name, sub, name):
    """Lists a calendar sub-folder (archive, deleted) into a file in the run's folder.  The driver runs in the
    session's folder, so the file is given by its full path."""
    return {"op": "listdir", "sub": sub, "file": run_path(run_name, name)}


def parse_listing(path):
    """The files a listdir operation wrote: name -> size in bytes."""
    with open(path) as f:
        lines = f.read().splitlines()
    return {l.rsplit(" ", 1)[0]: int(l.rsplit(" ", 1)[1]) for l in lines[1:-1] if " " in l}


@scenario("R01", "#105 ContactManager master, edit in Google, repeat: the Google change reaches the app, one repeat on Google")
def r01():
    set_mode(CM_MASTER)
    eid = create_recurring_event("BENCH R01 weekly", at_day(2, "09:00"), at_day(2, "10:00"), "RRULE:FREQ=WEEKLY;INTERVAL=1")
    run = Run("R01", [sync(), gchange("patch-event", eid, "Room R1"), sync(), dump()], session="R01").execute()
    goog = bench_event("BENCH R01 weekly")
    local = local_events(run.dump, "BENCH R01")
    return [Check("one repeat on Google", len(goog) == 1, "found %d" % len(goog)),
            Check("the Google repeat still repeats weekly",
                  len(goog) == 1 and any("FREQ=WEEKLY" in r for r in goog[0].get("recurrence", [])),
                  "recurrence %r" % [g.get("recurrence") for g in goog]),
            Check("the app shows the Google location on the repeat",
                  len(local) == 1 and local[0]["location"] == "Room R1",
                  "app: %s" % [a["location"] for a in local]),
            Check("the app holds one entry", len(local) == 1, "found %d" % len(local))]


@scenario("R02", "#105 Google master, edit in ContactManager, single: the next sync takes Google's location, and nothing is uploaded")
def r02():
    create_event("BENCH R02", at_day(8, "09:00"), at_day(8, "10:00"), location="Room 1")
    set_mode(GOOGLE_MASTER)
    run = Run("R02", [sync(), edit_op("BENCH R02", location="Room L"), sync(), dump()], session="R02").execute()
    goog = bench_event("BENCH R02")
    local = local_events(run.dump, "BENCH R02")
    return [Check("Google keeps its location (the app's edit is not uploaded)",
                  len(goog) == 1 and goog[0].get("location") == "Room 1",
                  "Google: %s" % [g.get("location") for g in goog]),
            Check("the app takes Google's location at the next sync",
                  len(local) == 1 and local[0]["location"] == "Room 1",
                  "app: %s" % [a["location"] for a in local])]


@scenario("R03", "#105 Google master, edit in ContactManager, repeat: the next sync keeps the repeat and Google's location")
def r03():
    eid = create_recurring_event("BENCH R03 weekly", at_day(2, "09:00"), at_day(2, "10:00"), "RRULE:FREQ=WEEKLY;INTERVAL=1")
    patch_event(eid, {"location": "Room 3"})
    set_mode(GOOGLE_MASTER)
    run = Run("R03", [sync(), edit_op("BENCH R03 weekly", location="Room L"), sync(), dump()], session="R03").execute()
    goog = bench_event("BENCH R03 weekly")
    local = local_events(run.dump, "BENCH R03")
    return [Check("one repeat on Google", len(goog) == 1, "found %d" % len(goog)),
            Check("Google keeps its location, and the repeat its weekly rule",
                  len(goog) == 1 and goog[0].get("location") == "Room 3"
                  and any("FREQ=WEEKLY" in r for r in goog[0].get("recurrence", [])),
                  "Google: %s" % [(g.get("location"), g.get("recurrence")) for g in goog]),
            Check("the app takes Google's location; one entry",
                  len(local) == 1 and local[0]["location"] == "Room 3",
                  "app: %s" % [a["location"] for a in local])]


@scenario("R04", "#105 Google master, edit in Google, single: the Google change is downloaded")
def r04():
    eid = create_event("BENCH R04", at_day(9, "09:00"), at_day(9, "10:00"), location="Room 1")
    set_mode(GOOGLE_MASTER)
    run = Run("R04", [sync(), gchange("patch-event", eid, "Room G"), sync(), dump()], session="R04").execute()
    goog = bench_event("BENCH R04")
    local = local_events(run.dump, "BENCH R04")
    return [Check("one event on Google", len(goog) == 1, "found %d" % len(goog)),
            Check("the app shows the Google location; one entry",
                  len(local) == 1 and local[0]["location"] == "Room G",
                  "app: %s" % [a["location"] for a in local])]


@scenario("R05", "#105 Google master, edit in Google, repeat: the Google change is downloaded, and the repeat is kept")
def r05():
    eid = create_recurring_event("BENCH R05 weekly", at_day(2, "09:00"), at_day(2, "10:00"), "RRULE:FREQ=WEEKLY;INTERVAL=1")
    set_mode(GOOGLE_MASTER)
    run = Run("R05", [sync(), gchange("patch-event", eid, "Room G"), sync(), dump()], session="R05").execute()
    goog = bench_event("BENCH R05 weekly")
    local = local_events(run.dump, "BENCH R05")
    return [Check("one repeat on Google, still weekly",
                  len(goog) == 1 and any("FREQ=WEEKLY" in r for r in goog[0].get("recurrence", [])),
                  "Google: %s" % [g.get("recurrence") for g in goog]),
            Check("the app shows the Google location on the repeat; one entry",
                  len(local) == 1 and local[0]["location"] == "Room G",
                  "app: %s" % [a["location"] for a in local])]


def folder_holding(workdir, sub, text):
    """The file in a calendar sub-folder of a run's data that contains text (appointment files hold the summary)."""
    folder = os.path.join(workdir, "home", "TrumptonApps", "ContactManager", "calendar", sub)
    if not os.path.isdir(folder):
        return None
    for name in os.listdir(folder):
        if not os.path.isfile(os.path.join(folder, name)):
            continue                 # the archive and deleted sub-folders are not entries
        with open(os.path.join(folder, name), errors="replace") as f:
            if text in f.read():
                return name
    return None


@scenario("AR01", "#105 Archive: an entry that the window leaves behind is archived; moved into the window on Google, it is restored and leaves the archive")
def ar01():
    set_mode(CM_MASTER)
    eid = create_event("BENCH AR01", at_day(-30, "09:00"), at_day(-30, "10:00"))
    workdir = os.path.join(RUNS, "session-AR01")
    # The app loads only the window at start-up, and archives an entry only while it holds it in a sync, and only when it
    # was not changed since the sync before.  So: download it inside the window; then, in the next sync, shrink the window
    # to 10 days, so it has ended and is archived; then move it into the window on Google and sync with the 90-day window.
    Run("AR01a", [sync(), dump()], session="AR01").execute()
    b = Run("AR01b", [{"op": "setting", "key": "windowbefore", "value": "10"}, sync(),
                      listdir_op("AR01b", "archive", "archive-b.txt"), dump()], session="AR01").execute()
    archived = folder_holding(workdir, "archive", "BENCH AR01")
    gchange_main(["move-event", eid, at_day(5, "09:00"), at_day(5, "10:00")])
    c = Run("AR01c", [{"op": "setting", "key": "windowbefore", "value": "90"}, sync(),
                      listdir_op("AR01c", "archive", "archive-c.txt"), dump()], session="AR01").execute()
    still_archived = folder_holding(workdir, "archive", "BENCH AR01")
    local = local_events(c.dump, "BENCH AR01")
    goog = bench_event("BENCH AR01")
    target = at_day(5, "00:00")[:10]
    return [Check("the entry the window left behind is archived", archived is not None,
                  "archive holds %s" % archived),
            Check("moved into the window, it is restored and leaves the archive", still_archived is None,
                  "archive still holds %s" % still_archived),
            Check("the app holds it once, in the window", len(local) == 1 and target in local[0]["from"],
                  "app: %s" % [x["from"] for x in local]),
            Check("Google holds it once", len(goog) == 1, "found %d" % len(goog))]


def in_scale_window(days, step_days, occurrences):
    """Whether any occurrence of an entry falls in the window: 90 days before today to 730 days after.  The lower edge
    is 89 days, not 90: the sync runs minutes after the set is generated, so an entry on the edge has already left it."""
    return any(-89 <= days + step_days * k <= 730 for k in range(occurrences))


# --- purge: removes entries from the account (test area utility) ------------------------------------------------

def all_events():
    """Every event on the account, as Google lists them (repeats once, changed occurrences as their own items)."""
    out, token = [], None
    while True:
        q = "maxResults=2500&singleEvents=false&showDeleted=false" + ("&pageToken=" + token if token else "")
        page = api("GET", CALENDAR + "/events?" + q)
        out += page.get("items", [])
        token = page.get("nextPageToken")
        if not token:
            return out


def purge(what):
    """Removes every contact and/or appointment from the account.  what is 'contacts', 'appointments' or 'both'.

    It removes everything on the account, not only the test entries, so use it only on the scratch account (HEADLESS-TEST.md,
    section 0).  The command asks for --yes before it runs.  Returns how many of each were removed."""
    if what not in ("contacts", "appointments", "both"):
        raise SystemExit("purge: what must be contacts, appointments or both")
    removed = {"contacts": 0, "appointments": 0}
    if what in ("contacts", "both"):
        for p in all_connections():
            api("DELETE", PEOPLE + "/" + p["resourceName"] + ":deleteContact")
            removed["contacts"] += 1
    if what in ("appointments", "both"):
        for e in all_events():
            try:
                api("DELETE", CALENDAR + "/events/" + e["id"])
                removed["appointments"] += 1
            except RuntimeError as err:
                # Already gone: a changed occurrence goes with its repeat, so Google answers 404 or 410 for it
                if re.search(r" -> (404|410) ", str(err)):
                    continue
                raise
    return removed


# --- ticket #105: test groups ------------------------------------------------------------------------------------
#
# Group 1, uploads: slow.  Clears the account, then uploads 250 contacts and 1,000 appointments, half through the Google
#                   API and half through ContactManager, and leaves them in place.
# Group 2, updates: every other test that syncs with Google.  Each one puts on Google the details it needs first.
# Group 3, local:   tests that need no Google sync.
#
# Selections (main): full = 1,2,3; quick = 2,3; google = 2; google-quick = a subset of 2; local = 3.

GROUP_UPLOADS, GROUP_UPDATES, GROUP_LOCAL = "uploads", "updates", "local"
UPLOADS = ["U01"]
GOOGLE_QUICK = ["A01", "A05", "A06", "C01", "M05", "R01", "R02", "R03", "R04", "R05", "AR01", "E01", "E02", "E03"]
GOOGLE_TOKENS = ("sync(", "gchange", "create_event", "create_recurring_event", "create_contact", "api(",
                 "bench_event", "patch_event", "import_external_event", "google_master", "all_connections",
                 "bench_contacts", "purge(")


# Typical durations, in seconds, as measured on the scratch account (they vary with the account's size and with
# Google's rate limits).  Printed by "google_bench.py groups".  U01 is the time measured for its earlier all-API version (S01).
TYPICAL_SECONDS = {
    "U01": 1080,
    "R01": 45, "R02": 42, "R03": 45, "R04": 43, "R05": 47, "AR01": 81,
    "L01": 5, "L02": 10, "L03": 10, "L04": 6, "E06": 6, "MG01": 4, "MG02": 6, "F12": 11, "E05": 6, "T02": 6,
    "T03": 6, "U34": 6, "K24": 20, "P30": 6, "P31": 7, "D30": 6, "B78": 6, "O79": 6, "W30": 6, "E30": 6,
    "T30": 6, "P33": 6, "Q34": 7, "Q35": 7,
}


def group_of(code):
    if code in UPLOADS:
        return GROUP_UPLOADS
    src = inspect.getsource(SCENARIOS[code][1])
    return GROUP_UPDATES if any(tok in src for tok in GOOGLE_TOKENS) else GROUP_LOCAL


def selection(name):
    """The scenario codes for a named selection, or None when the name is not one."""
    if name == "full":
        return [c for c in SCENARIOS if group_of(c) in (GROUP_UPLOADS, GROUP_UPDATES, GROUP_LOCAL)]
    if name == "quick":
        return [c for c in SCENARIOS if group_of(c) in (GROUP_UPDATES, GROUP_LOCAL)]
    if name == "google":
        return [c for c in SCENARIOS if group_of(c) == GROUP_UPDATES]
    if name == "google-quick":
        bad = [c for c in GOOGLE_QUICK if group_of(c) != GROUP_UPDATES]
        if bad:
            raise SystemExit("google-quick lists codes that are not in group 2: %s" % bad)
        return list(GOOGLE_QUICK)
    if name == "local":
        return [c for c in SCENARIOS if group_of(c) == GROUP_LOCAL]
    return None


# --- ticket #105: group 1, uploads -----------------------------------------------------------------------------

UPLOAD_CONTACTS, UPLOAD_APPOINTMENTS, UPLOAD_SEED = 250, 1000, 105
DIRECT_CONTACTS, DIRECT_APPOINTMENTS = 125, 500      # the first half of each goes through the Google API; the rest through ContactManager
FIRST_NAMES = ["Mira", "Tomas", "Ines", "Oskar", "Priya", "Lewis", "Chloe", "Dara", "Felix", "Nadia", "Rhys", "Sofia",
               "Imran", "Hana", "Tobias", "Ruth", "Aaron", "Joanna", "Kofi", "Lucia"]
LAST_NAMES = ["Okafor", "Lindqvist", "Moreau", "Haddad", "Brennan", "Kowalski", "Tanaka", "Serrano", "Quinn", "Patel",
              "Novak", "Hughes", "Aesop", "Marlowe", "Esposito", "Vance"]
TITLES = ["Team call", "Dentist", "Review", "Lunch", "Gym", "Planning", "Call home", "Interview", "Site visit"]


def upload_plan(rnd):
    """The random records: contacts (index 0 is Aaron Aesop) and appointments, from one seeded generator."""
    contacts = []
    for i in range(UPLOAD_CONTACTS):
        first, last = ("Aaron", "Aesop") if i == 0 else (rnd.choice(FIRST_NAMES), rnd.choice(LAST_NAMES))
        contacts.append({"i": i, "first": first, "last": last,
                         "email": "%s.%s%04d@example.com" % (first.lower(), last.lower(), i),
                         "phone": "+44 7700 9%05d" % i})
    appts = []
    for i in range(UPLOAD_APPOINTMENTS):
        days, hour = rnd.randint(-400, 900), rnd.randint(8, 17)
        repeat = i % 5 == 0
        interval, occurrences = (rnd.choice([1, 2]), rnd.randint(4, 12)) if repeat else (1, 1)
        appts.append({"i": i, "summary": "Upload S%04d %s" % (i, rnd.choice(TITLES)), "days": days,
                      "start": at_day(days, "%02d:00" % hour), "end": at_day(days, "%02d:30" % hour),
                      "repeat": repeat, "interval": interval, "occurrences": occurrences,
                      "step": 7 * interval if repeat else 0, "direct": i < DIRECT_APPOINTMENTS})
    return contacts, appts


def appointment_summary_id(text):
    m = re.search(r"Upload S(\d{4})", text or "")
    return int(m.group(1)) if m else None


@scenario("U01", "#105 Uploads: purge the account, then upload 250 contacts and 1,000 appointments (half by the Google API, half by ContactManager), left in place")
def u01():
    set_mode(CM_MASTER)
    removed = purge("both")
    rnd = random.Random(UPLOAD_SEED)
    contacts, appts = upload_plan(rnd)
    # Half through the Google API: the first half of the contacts (with Aaron Aesop) and of the appointments
    for c in contacts[:DIRECT_CONTACTS]:
        create_contact(c["last"], emails=[(c["email"], "work")], phones=[(c["phone"], "mobile")],
                       given=c["first"], family=c["last"])
    for a in appts:
        if not a["direct"]:
            continue
        if a["repeat"]:
            eid = create_recurring_event(a["summary"], a["start"], a["end"],
                                         "RRULE:FREQ=WEEKLY;INTERVAL=%d;COUNT=%d" % (a["interval"], a["occurrences"]))
            if a["i"] % 50 == 0 and a["occurrences"] >= 2:      # one occurrence changed on Google
                items = api("GET", CALENDAR + "/events/%s/instances?timeMin=%s&maxResults=3"
                            % (eid, urllib.parse.quote(at_day(a["days"] - 1, "00:00"))))["items"]
                patch_event(items[1]["id"], {"location": "Changed occurrence"})
            if a["i"] % 50 == 25 and a["occurrences"] >= 3:     # one occurrence cancelled on Google
                items = api("GET", CALENDAR + "/events/%s/instances?timeMin=%s&maxResults=3"
                            % (eid, urllib.parse.quote(at_day(a["days"] - 1, "00:00"))))["items"]
                api("DELETE", CALENDAR + "/events/" + items[2]["id"])
        else:
            create_event(a["summary"], a["start"], a["end"], location="Room %d" % (a["i"] % 9))
    # Half through ContactManager: added in the app, then uploaded by its sync
    ops = []
    for c in contacts[DIRECT_CONTACTS:]:
        ops.append({"op": "add_contact", "fields": {"names": c["first"], "surname": c["last"],
                                                     "email": c["email"], "phone": c["phone"]}})
    for a in appts:
        if a["direct"]:
            continue
        fields = {"summary": a["summary"], "from": a["start"], "to": a["end"], "location": "Room %d" % (a["i"] % 9)}
        if a["repeat"]:
            fields.update({"repeat": "2", "repeatinterval": str(a["interval"]), "repeatcount": str(a["occurrences"])})
        ops.append({"op": "add_appointment", "fields": fields})
    a_run = Run("U01a", ops + [sync(), dump()], session="U01").execute()
    b_run = Run("U01b", [sync(), dump()], session="U01").execute()
    c_run = Run("U01c", [{"op": "setting", "key": "windowbefore", "value": "10"}, sync(),
                         listdir_op("U01c", "archive", "archive-c.txt"), dump()], session="U01").execute()

    people = all_connections()
    goog_contacts = len(people)
    aaron_goog = [p for p in people if any(n.get("givenName") == "Aaron" and n.get("familyName") == "Aesop"
                                           for n in p.get("names", []))]
    goog_events = [e for e in all_events() if e.get("summary", "").startswith("Upload S") and not e.get("recurringEventId")]
    goog_ids = {appointment_summary_id(e["summary"]) for e in goog_events}
    cm_ids = {a["i"] for a in appts if not a["direct"]}
    cm_missing = sorted(cm_ids - goog_ids)

    app_contacts = [c for c in a_run.dump["contacts"] if c.get("deleted") != "true"]
    app_aaron = [c for c in app_contacts if c.get("names") == "Aaron" and c.get("surname") == "Aesop"]
    held = [x for x in a_run.dump["appointments"] if x["summary"].startswith("Upload S") and not x["deleted"]]
    held_ids = [appointment_summary_id(x["summary"]) for x in held]
    expected = {a["i"] for a in appts if in_scale_window(a["days"], a["step"], a["occurrences"])}
    missing = sorted(expected - set(held_ids))
    dup_ids = {i for i in held_ids if held_ids.count(i) > 1}
    lb = [x for x in b_run.dump["appointments"] if x["summary"].startswith("Upload S") and not x["deleted"]]
    archived = parse_listing(run_path("U01c", "archive-c.txt"))
    return [Check("%d contacts on Google" % UPLOAD_CONTACTS, goog_contacts == UPLOAD_CONTACTS,
                  "Google holds %d" % goog_contacts),
            Check("Aaron Aesop is on Google once", len(aaron_goog) == 1, "found %d" % len(aaron_goog)),
            Check("Aaron Aesop is in the app once", len(app_aaron) == 1, "found %d" % len(app_aaron)),
            Check("%d contacts in the app" % UPLOAD_CONTACTS, len(app_contacts) == UPLOAD_CONTACTS,
                  "app holds %d" % len(app_contacts)),
            Check("%d appointments on Google (masters)" % UPLOAD_APPOINTMENTS, len(goog_events) == UPLOAD_APPOINTMENTS,
                  "Google holds %d" % len(goog_events)),
            Check("the ContactManager half reached Google", not cm_missing,
                  "%d missing, e.g. %s" % (len(cm_missing), cm_missing[:3])),
            Check("every in-window appointment is held by the app", not missing,
                  "%d missing, e.g. %s" % (len(missing), missing[:3])),
            Check("an appointment is held twice only for a changed occurrence",
                  all(i % 50 == 0 for i in dup_ids), "duplicated: %s" % sorted(dup_ids)[:5]),
            Check("a second sync changes nothing in the app", len(lb) == len(held),
                  "held %d, then %d" % (len(held), len(lb))),
            Check("the entries the shrunken window left behind are archived", len(archived) > 0,
                  "archive holds %d files" % len(archived))]


# --- ticket #105: group 3, local ------------------------------------------------------------------------------------

def build_folder():
    return os.environ.get("CM_BUILD", os.path.dirname(os.path.abspath(DRIVER)) if os.path.isabs(DRIVER) else "")


@scenario("L01", "Local: the message and appointment unit tests (autotest/messages, QtTest), with no Google")
def l01():
    build = build_folder()
    if not build:
        raise SystemExit("set CM_BUILD to the build folder (the one that holds the application's objects)")
    r = subprocess.run(["sh", os.path.join(HERE, "messages", "run-messages-tests.sh"), build],
                       capture_output=True, text=True)
    text = r.stdout + r.stderr
    m = re.search(r"Totals: (\d+) passed, (\d+) failed", text)
    passed, failed = (int(m.group(1)), int(m.group(2))) if m else (0, -1)
    return [Check("the unit tests ran", m is not None, "no QtTest totals in the output"),
            Check("no unit test failed", r.returncode == 0 and failed == 0,
                  "%d passed, %d failed" % (passed, failed))]


@scenario("L02", "Local: an appointment and a contact made in ContactManager are saved, and are there after a restart (no Google)")
def l02():
    set_mode(CM_MASTER)
    Run("L02a", [add_contact_op("L02", email="l02@test.invalid"),
                 {"op": "add_appointment", "fields": {"summary": "BENCH L02", "from": at_day(3, "09:00"),
                                                      "to": at_day(3, "10:00")}}, dump()], session="L02").execute()
    b = Run("L02b", [dump()], session="L02").execute()
    contacts = [c for c in b.dump["contacts"] if c.get("surname") == "L02" and c.get("deleted") != "true"]
    appts = local_events(b.dump, "BENCH L02")
    return [Check("the contact is there after a restart", len(contacts) == 1, "found %d" % len(contacts)),
            Check("the appointment is there after a restart", len(appts) == 1, "found %d" % len(appts))]


@scenario("L03", "Local: an appointment file moved into the archive is not loaded at start-up, and start-up leaves it in the archive")
def l03():
    set_mode(CM_MASTER)
    Run("L03a", [{"op": "add_appointment", "fields": {"summary": "BENCH L03", "from": at_day(3, "09:00"),
                                                      "to": at_day(3, "10:00")}}, dump()], session="L03").execute()
    workdir = os.path.join(RUNS, "session-L03")
    calendar = os.path.join(workdir, "home", "TrumptonApps", "ContactManager", "calendar")
    archive = os.path.join(calendar, "archive")
    os.makedirs(archive, exist_ok=True)
    name = folder_holding(workdir, "", "BENCH L03")
    moved = name is not None
    if moved:
        shutil.move(os.path.join(calendar, name), os.path.join(archive, name))
    b = Run("L03b", [listdir_op("L03b", "archive", "archive-b.txt"), dump()], session="L03").execute()
    listing = parse_listing(run_path("L03b", "archive-b.txt"))
    return [Check("the appointment file was saved, then moved to the archive", moved, "saved file: %s" % name),
            Check("the archived appointment is not loaded at start-up", not local_events(b.dump, "BENCH L03"),
                  "loaded: %d" % len(local_events(b.dump, "BENCH L03"))),
            Check("start-up leaves the archived file in place", name in listing, "archive holds %s" % sorted(listing))]


@scenario("L04", "Local: a start with no to-do or history file opens and writes its data (#100)")
def l04():
    set_mode(CM_MASTER)
    b = Run("L04", [dump()]).execute()
    return [Check("the app starts and dumps its data", b.dump is not None and "contacts" in b.dump,
                  "no dump" if b.dump is None else "dump written")]



if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
