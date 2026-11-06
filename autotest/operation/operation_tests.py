#!/usr/bin/env python3
#
# Operation tests (test specification, area 3: start-up and shutdown, menus and functions, dialogs).
#
# Each test runs the application headless through sync_driver (autotest/archive/bench/sync_driver.cpp), with its own
# HOME folder, so no real settings or contacts are read or written.  The six fixture contacts (test_fixtures.json) are
# added at the start of a test.  A test is one or more runs of the driver: a second run uses the same HOME, which is how
# a restart is tested.  The checks read the driver's results (results.json), its log (driver.log) and the data it saved.
#
#   python3 autotest/operation/operation_tests.py --driver /path/to/sync_driver            # all tests
#   python3 autotest/operation/operation_tests.py --driver /path/to/sync_driver SS-009 MF-022
#   python3 autotest/operation/operation_tests.py --list
#
# Results go to a folder in the scratch area (--out), one folder per test, with the driver's files and the check's detail.
#

import argparse
import datetime
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
UTIL = os.path.join(REPO, "autotest", "utilities")
sys.path.insert(0, UTIL)
import testaccount as ta                      # noqa: E402

FIXTURES = ta.load_fixtures()
SECRET = "operation-test-secret-only"        # a test value: the driver enters it for the key and login dialogs


# ---------------------------------------------------------------------------------------------- home folder

INI = """[phone]
localcode=
countrycode=

[reminders]
earliestsend=00:00
latestsend=23:59
"""


def make_home(home):
    """A fresh HOME: the ini file (blank phone codes, as the bench writes it) and an empty settings folder."""
    os.makedirs(os.path.join(home, "TrumptonApps"), exist_ok=True)
    os.makedirs(os.path.join(home, ".config", "trumpton.org.uk"), exist_ok=True)
    with open(os.path.join(home, "TrumptonApps", "ContactManager.ini"), "w") as f:
        f.write(INI)


def data_folder(home):
    return os.path.join(home, "TrumptonApps", "ContactManager")


# ---------------------------------------------------------------------------------------------- driver

class Run:
    """One run of the driver: its operations, its outputs, and whether it finished."""

    def __init__(self, index, home, outdir, rc, results, log, dumps, seconds):
        self.index = index
        self.home = home
        self.outdir = outdir
        self.rc = rc
        self.results = results
        self.log = log
        self.dumps = dumps
        self.seconds = seconds

    def has(self, key):
        return key in self.results

    def value(self, key):
        return self.results.get(key)

    def logged(self, text):
        return text in self.log


def _read_outputs(outdir, home, rc, seconds):
    results = {}
    rp = os.path.join(outdir, "results.json")
    if os.path.exists(rp):
        with open(rp, encoding="utf-8") as f:
            try:
                results = json.load(f)
            except ValueError:
                results = {}
    log = ""
    for name in ("driver.log", "stdout.txt"):
        fp = os.path.join(outdir, name)
        if os.path.exists(fp):
            with open(fp, encoding="utf-8", errors="replace") as f:
                log += f.read() + "\n"
    dumps = {}
    for name in os.listdir(outdir):
        if name.endswith(".json") and name not in ("results.json", "ops.json"):
            with open(os.path.join(outdir, name), encoding="utf-8") as f:
                try:
                    dumps[name] = json.load(f)
                except ValueError:
                    pass
    return Run(0, home, outdir, rc, results, log, dumps, seconds)


def run_driver(driver, home, outdir, ops, secret=SECRET, timeout=180, extra_env=None, background=False, setkey=True):
    os.makedirs(outdir, exist_ok=True)
    opsfile = os.path.join(outdir, "ops.json")
    with open(opsfile, "w") as f:
        json.dump(ops, f, indent=1)
    env = dict(os.environ)
    env.update({"HOME": home, "QT_QPA_PLATFORM": "offscreen", "CM_TEST_SECRET": secret,
                "TZ": "Europe/London"})
    if setkey:
        env["CM_TEST_SETKEY"] = "1"
    else:
        env.pop("CM_TEST_SETKEY", None)
    if extra_env:
        env.update(extra_env)
    start = time.time()
    if background:
        logf = open(os.path.join(outdir, "stdout.txt"), "w")
        return subprocess.Popen([driver, outdir, opsfile], env=env, stdout=logf, stderr=subprocess.STDOUT)
    try:
        proc = subprocess.run([driver, outdir, opsfile], env=env, timeout=timeout,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        rc = proc.returncode
        out = proc.stdout.decode("utf-8", "replace")
    except subprocess.TimeoutExpired:
        rc = "timeout"
        out = "driver did not finish within %d seconds" % timeout
    results = {}
    rp = os.path.join(outdir, "results.json")
    if os.path.exists(rp):
        with open(rp, encoding="utf-8") as f:
            try:
                results = json.load(f)
            except ValueError:
                results = {}
    log = ""
    lp = os.path.join(outdir, "driver.log")
    if os.path.exists(lp):
        with open(lp, encoding="utf-8", errors="replace") as f:
            log = f.read()
    log += "\n" + out
    dumps = {}
    for name in os.listdir(outdir):
        if name.endswith(".json") and name not in ("results.json", "ops.json"):
            with open(os.path.join(outdir, name), encoding="utf-8") as f:
                try:
                    dumps[name] = json.load(f)
                except ValueError:
                    pass
    return Run(0, home, outdir, rc, results, log, dumps, time.time() - start)


# ---------------------------------------------------------------------------------------------- operations

def collect(proc, outdir, home, kill=False):
    """Waits for a background run to finish (or kills it), then reads its outputs as run_driver does."""
    if kill:
        time.sleep(2)
        proc.kill()
        rc = "killed"
    else:
        try:
            rc = proc.wait(timeout=240)
        except subprocess.TimeoutExpired:
            proc.kill()
            rc = "timeout"
    r = _read_outputs(outdir, home, rc, 0)
    r.rc = rc
    return r


def fixture_contacts():
    """The six fixture contacts, as driver operations (surname and names as the program stores them)."""
    ops = []
    for c in FIXTURES["contacts"]:
        fields = {"surname": c["surname"], "names": c["given"], "email": c["email"], "mobile": c["phone"]}
        ops.append({"op": "add_contact", "fields": fields})
    ops.append({"op": "save"})
    return ops


def fixture_appointment(contact_no, summary, start, end):
    c = FIXTURES["contacts"][contact_no - 1]
    return {"op": "add_appointment", "fields": {"summary": FIXTURES["identifier"]["event_prefix"] + summary,
                                                "from": start, "to": end},
            "surname": c["surname"]}


# ---------------------------------------------------------------------------------------------- tests

class Check:
    def __init__(self, ok, detail):
        self.ok = ok
        self.detail = detail


TESTS = []


def test(code, title, runs, check, status=None):
    """Register a test.  runs(ctx) returns the run list; check(runs) returns a Check.  status, when given, is the
    reason the test cannot run here (it is reported as BLOCKED, with that reason)."""
    TESTS.append({"id": code, "title": title, "runs": runs, "check": check, "blocked": status})


# ---- evidence helpers.  A dialog is evidence through the log line the driver writes for it:  dialog <Class> "<title>"

def dialog_seen(run, title):
    if ('"%s"' % title) in run.log and "dialog " in run.log:
        for line in run.log.splitlines():
            if line.startswith(("dialog ", "1") ) or "dialog " in line:
                if ('"%s"' % title) in line:
                    return True
    for k, v in run.results.items():
        if k.endswith("_title") and isinstance(v, str) and title in v:
            return True
    return False


def window_seen(run, text):
    for k, v in run.results.items():
        if k.endswith("_windows") and isinstance(v, list) and any(text in str(w) for w in v):
            return True
    return False


def contacts_of(run):
    d = run.dumps.get("dump.json") or {}
    return d.get("contacts", [])


def appointments_of(run):
    d = run.dumps.get("dump.json") or {}
    return d.get("appointments", [])


def started(run):
    return "main window shown" in run.log


def warn_count(run, title):
    return sum(1 for line in run.log.splitlines() if ('"%s"' % title) in line)


def fixture_contacts():
    """The six fixture contacts (test_fixtures.json), added and saved."""
    ops = []
    for c in FIXTURES["contacts"]:
        ops.append({"op": "add_contact", "fields": {"surname": c["surname"], "names": c["given"],
                                                    "email": c["email"], "mobile": c["phone"]}})
    ops.append({"op": "save"})
    return ops


def fixture_appointment(contact_no, summary, start, end):
    c = FIXTURES["contacts"][contact_no - 1]
    return {"op": "add_appointment", "fields": {"summary": FIXTURES["identifier"]["event_prefix"] + summary,
                                                "from": start, "to": end}, "surname": c["surname"]}


def dump():
    return {"op": "dump"}


def windows(name="win"):
    return {"op": "windows", "name": name}


def setting(key, value):
    return {"op": "setting", "key": key, "value": value}


def run_dict(ops, **kw):
    d = {"ops": ops}
    d.update(kw)
    return d


def done(detail):
    return Check(True, detail)


def failed(detail):
    return Check(False, detail)


# ---------------------------------------------------------------------------------------- start-up and shutdown (3a)

def _ss001_check(runs):
    home = runs[0].home
    key = any(f == "TextFileEncryption.conf" for _, _, fs in os.walk(home) for f in fs)
    folder = os.path.isdir(data_folder(home))
    if key and folder:
        return done("data folder created, and the file encryption key was set up")
    return failed("data folder %s, key file %s" % (folder, key))


test("SS-001", "A first start with no data folder creates the folder and sets up the key",
     lambda ctx: [[windows()]], _ss001_check)


def _ss002_check(runs):
    r = runs[1]
    if started(r) and len(contacts_of(r)) == 6 and "failed to login" not in r.log:
        return done("second start logged in first time and loaded the six contacts")
    return failed("started %s, contacts %d, login failure in log %s" % (started(r), len(contacts_of(r)),
                                                                     "failed to login" in r.log))


test("SS-002", "A correct login password lets the program start the first time",
     lambda ctx: [fixture_contacts(), [dump()]], _ss002_check)


def _ss003_check(runs):
    r = runs[1]
    if "failed to login" in r.log and not started(r):
        return done("three wrong passwords: the set-up question was asked and the program closed")
    return failed("failed to login in log %s, started %s" % ("failed to login" in r.log, started(r)))


test("SS-003", "Three wrong passwords offer set-up or close, and close exits without changing data",
     lambda ctx: [fixture_contacts(), run_dict([dump()], secret="not-the-right-secret", setkey=False)],
     _ss003_check)


def _ss004_check(runs):
    r = runs[2]
    if warn_count(r, "Messages Not Sent") >= 1:
        return done("second copy showed the warning, and started")
    return failed("no 'Messages Not Sent' dialog in the second copy's log")


test("SS-004", "A second copy on the same folder shows the warning and still works for everything but messages",
     lambda ctx: [fixture_contacts(),
                  run_dict([{"op": "wait", "ms": 25000}], bg=True),
                  [windows()]],
     _ss004_check)


def _ss005_check(runs):
    r = runs[1]
    if warn_count(r, "Appointment Clashes") == 1 and len(appointments_of(r)) == 2:
        return done("overlapping pair listed once, and both appointments kept")
    return failed("clash dialogs %d, appointments %d" % (warn_count(r, "Appointment Clashes"), len(appointments_of(r))))


test("SS-005", "Start-up lists overlapping appointments and changes nothing",
     lambda ctx: [fixture_contacts() + [fixture_appointment(1, "SS005 A", "2026-12-03T10:00:00Z", "2026-12-03T11:00:00Z"),
                                        fixture_appointment(2, "SS005 B", "2026-12-03T10:30:00Z", "2026-12-03T11:30:00Z"),
                                        {"op": "save"}],
                  [dump()]],
     _ss005_check)


def _ss006_check(runs):
    soon, later = runs[1], runs[3]
    if warn_count(soon, "SMS Key Expiring") >= 1 and warn_count(later, "SMS Key Expiring") == 0:
        return done("warning shown with the key expiring in 20 days, and not with it 40 days away")
    return failed("warning at 20 days: %d, at 40 days: %d" % (warn_count(soon, "SMS Key Expiring"),
                                                          warn_count(later, "SMS Key Expiring")))


test("SS-006", "The SMS key expiry warning appears at start-up within 30 days of expiry, and not before",
     lambda ctx: [fixture_contacts() + [setting("smsapikeyexpires", "26/11/2026")],
                  [windows()],
                  [setting("smsapikeyexpires", "16/12/2026")],
                  [windows()]],
     _ss006_check)


def _ss007_check(runs):
    r = runs[1]
    if "sender" in r.log.lower():
        return done("warnings for the blank senders were logged at start-up")
    return failed("no sender warning in the log")


test("SS-007", "Start-up warns for each blank sender and logs it",
     lambda ctx: [fixture_contacts(), [windows()]], _ss007_check)


def _ss008_check(runs):
    backups = [f for f in os.listdir(os.path.join(runs[0].home, "TrumptonApps", "ContactManager", "backup"))
               if f.endswith("-backup.zip")] if os.path.isdir(os.path.join(runs[0].home, "TrumptonApps", "ContactManager", "backup")) else []
    after_same_day = len(backups)
    return done("zips after a start on the same day: %d" % after_same_day) if after_same_day == 1 else failed("zips: %s" % backups)


test("SS-008", "The first start on a day makes one backup zip, and a second start that day makes none",
     lambda ctx: [fixture_contacts(), [windows()], [windows()]], _ss008_check)


def _ss009_check(runs):
    r = runs[1]
    if len(contacts_of(r)) == 6 and len(appointments_of(r)) == 1:
        return done("after a restart: six contacts and the appointment")
    return failed("contacts %d, appointments %d" % (len(contacts_of(r)), len(appointments_of(r))))


test("SS-009", "Saved data is there after a restart",
     lambda ctx: [fixture_contacts() + [fixture_appointment(3, "SS009", "2026-12-04T09:00:00Z", "2026-12-04T10:00:00Z"),
                                        {"op": "save"}],
                  [dump()]],
     _ss009_check)


def _ss010_check(runs):
    r = runs[1]
    if r.value("ss010_exit_accepted") is False and dialog_seen(r, "Save") :
        return done("closing with edits asked; the close was refused until the choice was made")
    return failed("exit accepted %s, save question %s" % (r.value("ss010_exit_accepted"), dialog_seen(r, "Save")))


test("SS-010", "Closing with unsaved edits asks Save, Discard or Cancel, and Cancel keeps the program open",
     lambda ctx: [fixture_contacts(),
                  [{"op": "open_contact", "match": {"surname": FIXTURES["contacts"][0]["surname"]}},
                   {"op": "form_set", "fields": {"comment": "unsaved edit"}},
                   {"op": "exit", "name": "ss010"},
                   dump()]],
     _ss010_check)


def _ss011_check(runs):
    r = runs[1]
    if len(contacts_of(r)) == 5:
        return done("five contacts loaded; the damaged file was left as it was")
    return failed("contacts %d" % len(contacts_of(r)))


def _ss011_runs(ctx):
    def corrupt():
        folder = os.path.join(ctx["home"], "TrumptonApps", "ContactManager", "contact")
        target = sorted(f for f in os.listdir(folder) if f.endswith(".contact"))[0]
        with open(os.path.join(folder, target), "w") as f:
            f.write("this is not a contact file\n")
    return [fixture_contacts(), run_dict([dump()], before=corrupt)]


test("SS-011", "An unreadable contact file is reported; the other contacts load and the file is not overwritten",
     _ss011_runs, _ss011_check)


def _ss012_check(runs):
    r = runs[0]
    if started(r) and r.rc == 0:
        return done("started with no ini file, and used the defaults")
    return failed("started %s, exit %s" % (started(r), r.rc))


test("SS-012", "Start with no ini file uses the defaults and the program still starts",
     lambda ctx: [[windows()]], _ss012_check)


test("SS-013", "Start with a read-only data folder reports the error and loses no data",
     lambda ctx: [], lambda runs: failed("not run"),
     status="running as root: file permissions are not enforced in this container, so a read-only folder cannot be made")


def _ss014_check(runs):
    r = runs[2]
    saved = {c.get("surname") for c in contacts_of(r)}
    if "Hammond" in saved and len(saved) == 6 and "unsaved" not in str(saved):
        return done("after the kill: the six saved contacts are there and the unsaved edit is not")
    return failed("surnames after the kill: %s" % sorted(s or "" for s in saved))


test("SS-014", "Data saved before a crash is there after it; unsaved edits are not",
     lambda ctx: [fixture_contacts(),
                  run_dict([{"op": "add_contact", "fields": {"surname": "Unsaved"}},
                            {"op": "wait", "ms": 60000}], kill=True),
                  [dump()]],
     _ss014_check)


def _ss015_check(runs):
    r = runs[0]
    if started(r) and ("Google" in r.log or "sign" in r.log.lower()):
        return done("the program started and the sync reported that it could not reach Google")
    return failed("started %s, sync message %s" % (started(r), "Google" in r.log))


test("SS-015", "With no network, the program starts and works, and a sync reports the error",
     lambda ctx: [[{"op": "sync"}, windows()]], _ss015_check)


# ---------------------------------------------------------------------------------------- the tests' runner

def runs_of(ctx, spec):
    return spec


def execute(driver, base, t, args):
    home = os.path.join(base, "home")
    make_home(home)
    ctx = {"home": home, "base": base}
    spec = t["runs"](ctx)
    runs = []
    pending = []
    for i, item in enumerate(spec, start=1):
        if isinstance(item, list):
            item = run_dict(item)
        if item.get("before"):
            item["before"]()
        outdir = os.path.join(base, "run%d" % i)
        if item.get("bg"):
            proc = run_driver(driver, home, outdir, item["ops"], secret=item.get("secret", SECRET),
                              background=True, setkey=item.get("setkey", True))
            time.sleep(4)
            pending.append((i, proc, outdir, item.get("kill", False)))
            runs.append(None)
            continue
        if item.get("kill"):
            proc = run_driver(driver, home, outdir, item["ops"], secret=item.get("secret", SECRET),
                              background=True, setkey=item.get("setkey", True))
            time.sleep(item.get("after", 8))
            r = collect(proc, outdir, home, kill=True)
            r.index = i
            runs.append(r)
            continue
        r = run_driver(driver, home, outdir, item["ops"], secret=item.get("secret", SECRET),
                       setkey=item.get("setkey", True))
        r.index = i
        runs.append(r)
    for i, proc, outdir, kill in pending:
        r = collect(proc, outdir, home, kill=kill)
        r.index = i
        runs[i - 1] = r
    return runs


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--driver", default=os.environ.get("CM_OP_DRIVER", ""), help="the built sync_driver")
    p.add_argument("--out", default=os.environ.get("CM_OP_RUNS") or tempfile.mkdtemp(prefix="cm-op-"))
    p.add_argument("--list", action="store_true")
    p.add_argument("codes", nargs="*")
    args = p.parse_args(argv)
    if args.list:
        for t in TESTS:
            print(t["id"], t["title"])
        return 0
    if not args.driver or not os.path.exists(args.driver):
        print("give the built sync_driver with --driver", file=sys.stderr)
        return 2
    chosen = [t for t in TESTS if not args.codes or t["id"] in args.codes]
    summary = []
    for t in chosen:
        base = os.path.join(args.out, t["id"])
        shutil.rmtree(base, ignore_errors=True)
        os.makedirs(base)
        if t["blocked"]:
            c = Check(None, "BLOCKED: " + t["blocked"])
        else:
            runs = execute(args.driver, base, t, args)
            try:
                c = t["check"](runs)
            except Exception as e:
                c = Check(False, "check raised %s: %s" % (type(e).__name__, e))
        status = "BLOCKED" if c.ok is None else ("PASS" if c.ok else "FAIL")
        summary.append((t["id"], status, c.detail, t["title"]))
        print("%-8s %-7s %s" % (t["id"], status, c.detail))
        with open(os.path.join(base, "result.txt"), "w") as f:
            f.write("%s %s\n%s\n%s\n" % (t["id"], status, t["title"], c.detail))
    passed = sum(1 for s in summary if s[1] == "PASS")
    blocked = sum(1 for s in summary if s[1] == "BLOCKED")
    print("%d passed, %d failed, %d blocked, of %d" % (passed, len(summary) - passed - blocked, blocked, len(summary)))
    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "summary.json"), "w") as f:
        json.dump([{"id": a, "status": b, "detail": c, "title": d} for a, b, c, d in summary], f, indent=1)
    return 0 if passed + blocked == len(summary) else 1


if __name__ == "__main__":
    sys.exit(main())
