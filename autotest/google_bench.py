#!/usr/bin/env python3
"""Google bench for ContactManager: contacts and appointments.

Runs the app's Google sync headlessly (autotest/bench/bench_driver) against a test Google account, and
checks the result on both sides: the local dump the driver writes and the account as Google holds it.

Only use a throwaway account.  Everything the bench creates is named "Bench ..." (contacts) or
"BENCH ..." (appointments), and the bench deletes those names before and after each scenario.

Configuration (environment variables, with defaults):
  CM_BENCH_DRIVER   path to the built bench_driver (default: bench_driver on PATH)
  CM_TEST_SECRET    the local encryption key / password the app asks for
  CM_CONF           the app's settings file holding googlerefreshtoken
  CM_HEADER         account-googleaccess.h (client ID and secret)
  CM_BENCH_RUNS     directory for run output (default: a temporary directory)

usage:
  google_bench.py run [scenario ...]       run all scenarios, or the named ones
  google_bench.py gchange <cmd> <json>     change the account from inside a run (used by the driver)
"""
import json
import os
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
        with urllib.request.urlopen("https://oauth2.googleapis.com/token", data) as r:
            _token["value"] = json.load(r)["access_token"]
    return _token["value"]


def api(method, url, body=None):
    data = None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(url, data=data, method=method)
    req.add_header("Authorization", "Bearer " + access_token())
    if data is not None:
        req.add_header("Content-Type", "application/json")
    for attempt in range(5):
        try:
            with urllib.request.urlopen(req) as r:
                text = r.read().decode()
                return json.loads(text) if text else {}
        except urllib.error.HTTPError as e:
            if e.code == 429 and attempt < 4:          # quota: back off and retry
                time.sleep(2 ** attempt)
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
    api("POST", PEOPLE + "/" + resource + ":updateContact?updatePersonFields=names", body)


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
    def __init__(self, name, ops, session=None):
        self.name = name
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
        self.ops = ops
        self.dump = None
        with open(os.path.join(self.dir, "ops.json"), "w") as f:
            json.dump(ops, f, indent=1)

    def execute(self):
        secret = os.environ.get("CM_TEST_SECRET", "")
        if not secret:
            raise SystemExit("set CM_TEST_SECRET to the local test password")
        env = dict(os.environ, CM_TEST_SECRET=secret, QT_QPA_PLATFORM="offscreen")
        subprocess.run([DRIVER, self.dir, os.path.join(self.dir, "ops.json")], env=env,
                       stdout=open(os.path.join(self.dir, "stdout.log"), "w"), stderr=subprocess.STDOUT,
                       timeout=900, check=False, cwd=self.workdir)
        path = os.path.join(self.dir, "dump.json")
        if os.path.exists(path):
            with open(path) as f:
                self.dump = json.load(f)
        return self

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


def canon_app(c):
    """A live local contact as comparable values: emails, phones and notes, formatting ignored."""
    return {"emails": sorted(norm(c[k]) for k in EMAIL_KEYS if c.get(k)),
            "phones": sorted(norm(c[k]) for k in PHONE_KEYS if c.get(k)),
            "notes": norm(c.get("comments", ""))}


def canon_google(p):
    info = contact_info(p)
    return {"emails": sorted(norm(v) for v, _ in info["emails"]),
            "phones": sorted(norm(v) for v, _ in info["phones"]),
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


def set_mode(master):
    """Set the app's master setting: 0 = ContactManager master (local), 1 = Google master.
    The app reads the setting when it starts, so this applies to the next run."""
    with open(CONF) as f:
        text = f.read()
    line = "databasemaster=%d" % master
    if re.search(r"^databasemaster=", text, re.M):
        text = re.sub(r"^databasemaster=.*$", line, text, flags=re.M)
    else:
        text = text.replace("[General]\n", "[General]\n" + line + "\n", 1)
    with open(CONF, "w") as f:
        f.write(text)


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
            Check("app holds the trimmed email", bool(local) and norm(local[0].get("email")) == norm("c11@test.invalid"))]


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
    cleanup()
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
    return [Check("Google digits unchanged", gvals == want, "got %s" % gvals),
            Check("app holds all three numbers",
                  len(app_live_contacts(run.dump, "X04")) == 1 and
                  sorted(digits(app_live_contacts(run.dump, "X04")[0].get(k, "")) for k in PHONE_KEYS
                         if app_live_contacts(run.dump, "X04")[0].get(k)) == want)]


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
    resource = create_contact_body({"names": [{"givenName": "Bench", "familyName": "X07"}],
                                    "birthdays": [{"date": {"year": 1980, "month": 2, "day": 29}},
                                                  {"date": {"month": 12, "day": 25}}]})
    before = google_person(resource)["etag"]
    Run("X07", [sync(), sync(), dump()]).execute()
    after = google_person(resource)
    return [Check("Google not written by the syncs", before == after["etag"])]


@scenario("X08", "Multi-line address round-trips with formatting ignored")
def x08():
    addr = "12 High Street\n  London\n\tSW1A 1AA"
    create_contact("X08", addresses=[addr])
    run = Run("X08", [sync(), dump()]).execute()
    goog = google_contacts_tagged("X08")
    gaddr = [norm(a) for a in contact_info(goog[0])["addresses"]] if goog else []
    local = app_live_contacts(run.dump, "X08")
    return [Check("Google address unchanged (formatting ignored)", gaddr == [norm(addr)], "got %s" % gaddr),
            Check("app holds the address", bool(local) and norm(local[0].get("address", "")) == norm(addr))]


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
    summary = "Café meeting – \U0001F4C5"
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
    edits = [gchange("edit-contact", p["resourceName"], "X19-%02d edited" % i)
             for i, p in enumerate(sorted(google_contacts_tagged("X19-00") + bench_contacts(),
                                          key=lambda p: contact_info(p)["tag"])[:0])]
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


def gchange_main(argv):
    cmd = argv[0]
    if cmd == "edit-contact":
        resource, surname = argv[1], argv[2]
        update_contact_surname(resource, surname)
    elif cmd == "delete-contact":
        api("DELETE", PEOPLE + "/" + argv[1] + ":deleteContact")
    elif cmd == "patch-event":
        patch_event(argv[1], {"location": argv[2]})
    elif cmd == "delete-event":
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
    if argv[0] != "run":
        print(__doc__)
        return 2
    codes = argv[1:] or list(SCENARIOS)
    print("results in", RUNS)
    for code in codes:
        if code not in SCENARIOS:
            print("unknown scenario", code)
            return 2
        run_scenario(code)
    passed = sum(1 for r in RESULTS if r["ok"])
    print("\n%d of %d scenarios passed" % (passed, len(RESULTS)))
    with open(os.path.join(RUNS, "results.json"), "w") as f:
        json.dump(RESULTS, f, indent=1)
    return 0 if passed == len(RESULTS) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
