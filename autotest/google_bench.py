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
        with urllib.request.urlopen("https://oauth2.googleapis.com/token", data, timeout=60) as r:
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
            with urllib.request.urlopen(req, timeout=60) as r:
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
        self.home = os.path.join(self.workdir, "home")
        self.fresh = not os.path.exists(self.home)
        self.ops = ops
        self.dump = None
        with open(os.path.join(self.dir, "ops.json"), "w") as f:
            json.dump(ops, f, indent=1)

    def execute(self):
        secret = os.environ.get("CM_TEST_SECRET", "")
        if not secret:
            raise SystemExit("set CM_TEST_SECRET to the local test password")
        prepare_home(self.home, self.fresh, MODE["master"])
        env = dict(os.environ, CM_TEST_SECRET=secret, QT_QPA_PLATFORM="offscreen", HOME=self.home)
        env.pop("XDG_CONFIG_HOME", None)
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


def prepare_home(home, fresh, master):
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
    cleanup()
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
    os.makedirs(RUNS, exist_ok=True)
    codes = argv[1:] or list(SCENARIOS)
    print("results in", RUNS)
    for code in codes:
        if code not in SCENARIOS:
            print("unknown scenario", code)
            return 2
        run_scenario(code)
    set_mode(CM_MASTER)          # leave the app on its default master setting
    passed = sum(1 for r in RESULTS if r["ok"])
    print("\n%d of %d scenarios passed" % (passed, len(RESULTS)))
    with open(os.path.join(RUNS, "results.json"), "w") as f:
        json.dump(RESULTS, f, indent=1)
    return 0 if passed == len(RESULTS) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
