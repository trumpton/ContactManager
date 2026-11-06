#
# Shared code for the operation tests (area 3 of the test specification): the home folders, the driver, the operations,
# the checks, and the registry of tests.  The tests themselves are in tests_startup.py, tests_menus.py and tests_dialogs.py.
#
# Nothing here imports from, or reads, the archive.  The fixture contacts come from autotest/utilities/test_fixtures.json.
#

import datetime as dt
import hashlib
import json
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
UTIL = os.path.join(REPO, "autotest", "utilities")
sys.path.insert(0, UTIL)
import testaccount as ta                                   # noqa: E402

FIX = ta.load_fixtures()
SECRET = "operation-bench-only"                            # a test value: the driver enters it for the key and login dialogs
TODAY = dt.date.today()

# The home folder's ini file: blank phone codes (so numbers are stored as typed), and send times that never hold a message
INI_BASE = """[phone]
localcode=
countrycode=

[reminders]
earliestsend=00:00
latestsend=23:59
"""


def data_folder(home):
    return os.path.join(home, "TrumptonApps", "ContactManager")


def make_home(home, ini=INI_BASE):
    """A fresh HOME with the ini file (or none) and an empty settings folder.  The data folder is made by the program."""
    if os.path.isdir(home):
        import shutil
        shutil.rmtree(home)
    os.makedirs(os.path.join(home, "TrumptonApps"))
    os.makedirs(os.path.join(home, ".config", "trumpton.org.uk"))
    if ini is not None:
        with open(os.path.join(home, "TrumptonApps", "ContactManager.ini"), "w", encoding="utf-8") as f:
            f.write(ini)
    return home


def snapshot(folder):
    """A hash of every file under a folder, so a test can show that nothing was changed."""
    out = {}
    for root, _dirs, names in os.walk(folder):
        for name in names:
            path = os.path.join(root, name)
            with open(path, "rb") as f:
                out[os.path.relpath(path, folder)] = hashlib.sha256(f.read()).hexdigest()
    return out


def dates_ahead(days):
    """A date in day/month/year, the given number of days from today (the program's SMS key format)."""
    return (TODAY + dt.timedelta(days=days)).strftime("%d/%m/%Y")


# ------------------------------------------------------------------------------------------------ the driver

class Run:
    """One run of the driver: what it recorded (results), what it logged, and the files it wrote."""

    def __init__(self, outdir, rc, results, log, dumps, seconds):
        self.outdir = outdir
        self.rc = rc
        self.results = results
        self.log = log
        self.dumps = dumps
        self.seconds = seconds

    def value(self, key):
        return self.results.get(key)

    def logged(self, text):
        return text.lower() in self.log.lower()

    def dialog_titles(self):
        """The titles of the dialogs the driver answered (the log's 'dialog' lines)."""
        titles = []
        for line in self.log.splitlines():
            if line.startswith("dialog ") and '"' in line:
                titles.append(line.split('"')[1])
        return titles

    def dump(self, name="dump.json"):
        return self.dumps.get(name) or {}

    def contacts(self, name="dump.json"):
        return self.dump(name).get("contacts", [])

    def appointments(self, name="dump.json"):
        return self.dump(name).get("appointments", [])

    def contact(self, surname, name="dump.json"):
        for c in self.contacts(name):
            if c.get("surname") == surname:
                return c
        return {}


def _read(outdir, rc, seconds):
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
    return Run(outdir, rc, results, log, dumps, seconds)


def _env(home, secret, setkey, extra):
    env = dict(os.environ)
    env.update({"HOME": home, "QT_QPA_PLATFORM": "offscreen", "CM_TEST_SECRET": secret, "TZ": "Europe/London"})
    if setkey:
        env["CM_TEST_SETKEY"] = "1"
    else:
        env.pop("CM_TEST_SETKEY", None)
    env.pop("CM_TEST_FAULTS", None)
    env.pop("CM_TEST_LOGIN_ANSWER", None)
    if extra:
        env.update(extra)
    return env


def run_driver(driver, home, outdir, ops, secret=SECRET, setkey=True, extra_env=None, timeout=240):
    """Runs the driver once to its end (or its exit), and reads what it wrote."""
    os.makedirs(outdir, exist_ok=True)
    opsfile = os.path.join(outdir, "ops.json")
    with open(opsfile, "w", encoding="utf-8") as f:
        json.dump(ops, f, indent=1)
    start = time.time()
    try:
        proc = subprocess.run([driver, outdir, opsfile], env=_env(home, secret, setkey, extra_env),
                              timeout=timeout, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        rc = proc.returncode
        with open(os.path.join(outdir, "stdout.txt"), "wb") as f:
            f.write(proc.stdout)
    except subprocess.TimeoutExpired:
        rc = "timeout"
    return _read(outdir, rc, time.time() - start)


def start_driver(driver, home, outdir, ops, secret=SECRET, setkey=True, extra_env=None):
    """Starts the driver in the background (a second copy, or a run to be killed).  Collect it with collect()."""
    os.makedirs(outdir, exist_ok=True)
    opsfile = os.path.join(outdir, "ops.json")
    with open(opsfile, "w", encoding="utf-8") as f:
        json.dump(ops, f, indent=1)
    logf = open(os.path.join(outdir, "stdout.txt"), "w", encoding="utf-8")
    proc = subprocess.Popen([driver, outdir, opsfile], env=_env(home, secret, setkey, extra_env),
                            stdout=logf, stderr=subprocess.STDOUT)
    return proc, logf


def collect(proc, logf, outdir, kill=False, timeout=240):
    if kill:
        proc.kill()
        rc = "killed"
    else:
        try:
            rc = proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            proc.kill()
            rc = "timeout"
    logf.close()
    return _read(outdir, rc, 0)


# ------------------------------------------------------------------------------------------------ the operations

def fixtures_ops():
    """The six fixture contacts, added and saved (as the program stores them)."""
    ops = []
    for c in FIX["contacts"]:
        ops.append({"op": "add_contact", "fields": {"surname": c["surname"], "names": c["given"],
                                                    "email": c["email"], "mobile": c["phone"],
                                                    "emailme": "true" if "email" in c["channels"] else "false",
                                                    "textme": "true" if "sms" in c["channels"] else "false"}})
    ops.append({"op": "save"})
    return ops


def surname(n):
    return FIX["contacts"][n - 1]["surname"]


def email_of(n):
    return FIX["contacts"][n - 1]["email"]


def appt_op(n, summary, start, end):
    """An appointment for contact n (n = 0 for none), with the program's own summary, start and end (UTC text)."""
    op = {"op": "add_appointment", "fields": {"summary": summary, "from": start, "to": end}}
    if n:
        op["surname"] = surname(n)
    return op


def at(day, hhmm):
    """A UTC date-time text, the given number of days from today."""
    d = TODAY + dt.timedelta(days=day)
    return "%sT%s:00Z" % (d.isoformat(), hhmm)


def save_op():
    return {"op": "save"}


def dump_op(file="dump.json"):
    return {"op": "dump", "file": file}


def dumplog_op(file="log.txt"):
    return {"op": "dumplog", "file": file}


def wait_op(ms):
    return {"op": "wait", "ms": ms}


def setting_op(key, value):
    return {"op": "setting", "key": key, "value": value}


def open_op(n, name, details=True):
    return {"op": "open_contact", "name": name, "match": {"surname": surname(n)}, "details": details}


def edit_contact_op(n, fields):
    return {"op": "edit_contact", "match": {"surname": surname(n)}, "fields": fields}


def delete_contact_op(n):
    return {"op": "delete_contact", "match": {"surname": surname(n)}}


def hide_contact_op(n):
    return {"op": "hide_contact", "match": {"surname": surname(n)}}


def notes_op(n, todo, history):
    return {"op": "contact_notes", "match": {"surname": surname(n)}, "todo": todo, "history": history}


def delete_appt_op(summary):
    return {"op": "delete_appointment", "match": {"summary": summary}}


def edit_appt_op(summary, fields):
    return {"op": "edit_appointment", "match": {"summary": summary}, "fields": fields}


def tidy_op(name="tidy"):
    return {"op": "tidy_full", "name": name}


def menu_op(name, action, trigger=True):
    """Records the action's text, whether it is enabled and checked, and triggers it when asked (the menu item)."""
    return {"op": "menu", "name": name, "action": action, "trigger": trigger}


def actions_op(name, actions):
    return {"op": "actions", "name": name, "actions": list(actions)}


def keys_op(name, sequence):
    """Keys typed as a user types them (to the window with the focus).  Records the focus afterwards."""
    return {"op": "keys", "name": name, "sequence": sequence}


def shortcut_op(name, sequence):
    return {"op": "shortcut", "name": name, "sequence": sequence}


def key_op(name, key, target="list"):
    """A key pressed in the calendar list (space, t, d, n, w, x, p, right, left, or a digit)."""
    return {"op": "key", "name": name, "key": key, "target": target}


def hold_op(on=True):
    return {"op": "hold", "on": on}


def press_op(name, button):
    return {"op": "press", "name": name, "button": button}


def widgets_op(name, objects=(), rows=False):
    return {"op": "widgets", "name": name, "objects": list(objects), "rows": rows}


def set_op(name, obj, **value):
    """Sets one control in the top window: text=, checked=, or index=."""
    op = {"op": "set_widget", "name": name, "object": obj}
    op.update(value)
    return op


def windows_op(name):
    return {"op": "windows", "name": name}


def state_op(name):
    return {"op": "state", "name": name}


def answer_key_op(key):
    """The next yes/no question is answered by this key (Escape or Enter), as a user would press it."""
    return {"op": "answer", "key": key}


def answer_op(text):
    """Clicks the button with this text on the next question (a message box with buttons)."""
    return {"op": "answer", "text": text}


def choose_op(letter):
    """The letter of the next choice dialog (the program's own choice buttons)."""
    return {"op": "choose", "letter": letter}


def dates_op(answers):
    """The texts for the next date entries (the input boxes), in order; an empty text cancels one."""
    return {"op": "date_answers", "answers": list(answers)}


def form_set_op(fields, tidy=False):
    op = {"op": "form_set", "fields": fields}
    if tidy:
        op["tidy"] = True
    return op


def form_save_op(name):
    return {"op": "form_save", "name": name}


def exit_op(name):
    return {"op": "exit", "name": name}


def overview_op(name):
    return {"op": "overview", "name": name}


def select_overview_op(name):
    return {"op": "select_overview", "name": name}


def calendar_rows_op(name, reload=True):
    return {"op": "calendar_rows", "name": name, "reload": reload}


def calendar_click_op(name, summary, surname_text):
    return {"op": "calendar_click", "name": name, "summary": summary, "surname": surname_text}


def findnext_op(name, text):
    return {"op": "findnext", "name": name, "text": text}


def goto_today_op(name):
    return {"op": "goto_today", "name": name}


def run_spec(ops, **kw):
    """One run of the driver for a test: its operations, and any of: secret, setkey, env, before (a function of the
    home folder, called before the run), bg (start in the background), kill (kill the background run after this many
    seconds, instead of waiting for it)."""
    spec = {"ops": ops}
    spec.update(kw)
    return spec


# ------------------------------------------------------------------------------------------------ checks and tests

class Check:
    """One result.  ok is True (passed), False (failed) or None (blocked, with the reason)."""

    def __init__(self, ok, detail):
        self.ok = ok
        self.detail = detail


def check(label, condition, detail=""):
    return Check(bool(condition), label + ("" if condition else (": " + detail if detail else "")))


def blocked(label, reason):
    return Check(None, "%s: BLOCKED, %s" % (label, reason))


TESTS = []


def test(code, title, build, check_fn, blocked_reason=None):
    """Registers a test.  build(home) returns the list of run specs (the home is created by the runner); check_fn(runs,
    home) returns a list of Check objects.  blocked_reason, when given, makes the test BLOCKED before any run."""
    TESTS.append({"id": code, "title": title, "build": build, "check": check_fn, "blocked": blocked_reason})


def status_of(checks):
    if any(c.ok is False for c in checks):
        return "FAIL"
    if any(c.ok is None for c in checks):
        return "BLOCKED" if all(c.ok is None for c in checks) else "PARTIAL"
    return "PASS"
