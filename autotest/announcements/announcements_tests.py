#!/usr/bin/env python3
#
# Announcement tests (area 2): AN-001 to AN-048, section 2 of documents/test-specification.md.
#
# Each run is one or more processes of the program (segments: a shutdown and a restart are two segments), with its own home
# folder and its own clock.  The clock is fakeclock.so, which runs the program's wall clock at 360 times real time from each
# segment's start.  The send transports are the bench's stubs (stubs/), so every message the program would send is written
# as a file in the run's outbox, with the result.  The runs run in parallel, as section 2.0 of the specification says.
#
# usage:
#     python3 announcements_tests.py --plan                 check the plan (model.py) and print it.  Starts no program.
#     python3 announcements_tests.py [--runs A,B] [--driver PATH] [--clock PATH] [--out DIR]
#
# The driver and the clock are built by driver/build_driver.sh.  The evidence goes to --out (default: a timestamped folder in
# your home folder, outside the repository).  The report is report.md and summary.json in that folder.
#

import argparse
import datetime as dt
import json
import os
import re
import shutil
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import model as M                                         # noqa: E402

TEMPLATES = os.path.join(HERE, "templates")
DEFAULT_DRIVER = os.path.join(os.path.expanduser("~"), "build-contactmanager", "driver")
DEFAULT_CLOCK = os.path.join(os.path.expanduser("~"), "build-contactmanager", "fakeclock.so")
DEFAULT_OUT = os.path.join(os.path.expanduser("~"), "contactmanager-test-runs")
SECRET = os.environ.get("CM_TEST_SECRET") or "announcement-bench-only"   # a test value, for the key and login dialogs
CHECK = M.CHECK

ALL_RUNS = {r["id"]: r for r in M.RUNS}


# ------------------------------------------------------------------------------------------------ the plan

def tests_by_id():
    return {t["id"]: t for t in M.ALL}


def run_events(run, seg, final):
    """The events of a segment, by time: the run's contacts (first segment), and its tests' events in the segment."""
    start, end = seg[1], seg[2]
    events = []
    if seg is run["segments"][0] and not run.get("shares_home"):
        events += M.contacts_setup(run["base"])
    for tid in run["tests"]:
        t = tests_by_id()[tid]
        if t.get("aggregate") or t["build"] is None:
            continue
        events += t["build"](run["base"])["events"]
    inside = []
    for when, op in events:
        if when < start:
            continue
        if when > end or (when == end and not final):
            continue
        inside.append((when, op))
    inside.sort(key=lambda e: e[0])          # stable: contacts (added first) and tests' events at one time keep their order
    return inside


def expectations_of(run):
    """All the expected messages and groups of a run, by test."""
    out = {}
    for tid in run["tests"]:
        t = tests_by_id()[tid]
        if t.get("aggregate") or t["build"] is None:
            continue
        out[tid] = t["build"](run["base"])
    return out


def plan_problems():
    problems = []
    seen = {}
    for run in M.RUNS:
        for tid in run["tests"]:
            if tid not in tests_by_id():
                problems.append("run %s names an unknown test %s" % (run["id"], tid))
            seen.setdefault(tid, []).append(run["id"])
    for t in M.ALL:
        if t["id"] not in seen:
            problems.append("%s is in no run" % t["id"])
        elif len(seen[t["id"]]) > 1:
            problems.append("%s is in more than one run: %s" % (t["id"], seen[t["id"]]))
        if t["run"] not in ALL_RUNS:
            problems.append("%s names an unknown run %s" % (t["id"], t["run"]))
        elif t["id"] not in ALL_RUNS[t["run"]]["tests"]:
            problems.append("%s is not in its run %s" % (t["id"], t["run"]))
    for run in M.RUNS:
        segs = run["segments"]
        for a, b in zip(segs, segs[1:]):
            if a[2] > b[1]:
                problems.append("run %s: segment %s ends after %s starts" % (run["id"], a[0], b[0]))
        if segs[-1][2] != run["end"]:
            problems.append("run %s: the last segment does not end at the run's end" % run["id"])
        for tid, res in expectations_of(run).items():
            for e in res["expect"]:
                if not (run["base"] <= e["when"] <= run["end"]):
                    problems.append("%s: expected %s at %s is outside run %s" % (tid, e["kind"], e["when"], run["id"]))
            for g in res.get("groups", []):
                if not (run["base"] <= g["start"] <= run["end"]):
                    problems.append("%s: group window outside run %s" % (tid, run["id"]))
            for when, _op in res["events"]:
                if not (run["base"] <= when <= run["end"]):
                    problems.append("%s: event at %s is outside run %s" % (tid, when, run["id"]))
        if run.get("shares_home") and run["shares_home"] not in ALL_RUNS:
            problems.append("run %s shares a home with unknown run %s" % (run["id"], run["shares_home"]))
    return problems


def print_plan():
    for run in M.RUNS:
        print("run %-4s  %s to %s  limits %-7s  tests %s" % (run["id"], run["base"].strftime("%d %b %Y %H:%M"),
                                                           run["end"].strftime("%d %b %Y %H:%M"), run.get("limits"),
                                                           ", ".join(run["tests"])))
        for seg in run["segments"]:
            events = run_events(run, seg, seg is run["segments"][-1])
            print("    segment %-5s %s to %s  events %d  conf %s" % (seg[0], seg[1].strftime("%d %b %H:%M"),
                                                                    seg[2].strftime("%d %b %H:%M"), len(events),
                                                                    seg[3] or "-"))
        n = sum(len(v["expect"]) for v in expectations_of(run).values())
        print("    expected messages %d" % n)
    total = sum(len(v["expect"]) for r in M.RUNS for v in expectations_of(r).values())
    print("tests %d, runs %d, expected messages %d" % (len(M.ALL), len(M.RUNS), total))


# ------------------------------------------------------------------------------------------------ the homes

def set_conf(home, updates):
    """Sets settings in the program's settings file, keeping every other line (the message counts are kept there)."""
    path = os.path.join(home, ".config", "trumpton.org.uk", "ContactManager.conf")
    lines = []
    if os.path.exists(path):
        with open(path, encoding="utf-8") as f:
            lines = f.read().splitlines()
    if not lines:
        lines = ["[General]"]
    for key, value in updates.items():
        lines = [line for line in lines if not re.match(r"^%s=" % re.escape(key), line)]
        lines.append("%s=%s" % (key, value))
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def build_home(run, home):
    if os.path.isdir(home):
        shutil.rmtree(home)
    os.makedirs(os.path.join(home, ".config", "trumpton.org.uk"))
    msgdir = os.path.join(home, "TrumptonApps", "ContactManager", "messages")
    os.makedirs(msgdir)
    for name in os.listdir(TEMPLATES):
        if not name.endswith(".msg"):
            continue
        with open(os.path.join(TEMPLATES, name), encoding="utf-8") as f:
            text = f.read()
        key = name[:-4]
        if key in run.get("two_line", ()):
            text = "$TESTTAG Reminder $TIME $DATE $MONTH\nSecond line of the reminder, which must not be sent.\n"
        with open(os.path.join(msgdir, name), "w", encoding="utf-8") as f:
            f.write(text)
    limits = (20000, 20000) if run.get("limits") == "raised" else (10, 5)
    ini = {"emailconfirmation": "emailconfirmation", "emaildelete": "emaildelete", "emailupdate": "emailupdate",
           "emailreminder": "emailreminder", "emailtoday": "emailtoday", "smstoday": "smstoday",
           "smsconfirmation": "smsconfirmation", "smsupdate": "smsupdate", "smsdelete": "smsdelete",
           "smsreminder": "smsreminder"}
    for key in run.get("blank", ()):
        ini[key] = ""
    for key in run.get("missing", ()):
        ini[key] = "missing-" + key
    msg = "\n".join('%s="$CONFIG/ContactManager/messages/%s.msg"' % (k, v) if v else "%s=" % k for k, v in ini.items())
    email_sender = run.get("email_sender", M.SENDER_EMAIL)
    sms_sender = run.get("sms_sender", M.SENDER_SMS)
    text = """[phone]
localcode=
countrycode=

[reminders]
earliestsend=07:00
latestsend=22:00
remindhoursbefore=24
limitperday=%d
limitperhour=%d

[message]
%s

[sms-reminder]
agent=Esendex
username=bench
sender=%s
debug=no

[email-reminder]
server=bench.invalid
username=bench
port=465
sender=%s
debug=no
""" % (limits[0], limits[1], msg, sms_sender, email_sender)
    with open(os.path.join(home, "TrumptonApps", "ContactManager.ini"), "w", encoding="utf-8") as f:
        f.write(text)
    set_conf(home, {"databasemaster": "0", "enablereminders": "yes"})


def epoch(naive):
    return naive.replace(tzinfo=M.LONDON).timestamp()


def ops_for(run, seg, final):
    ops = []
    for when, op in run_events(run, seg, final):
        ops.append({"op": "waituntil", "at": M.local_text(when)})
        ops += op if isinstance(op, list) else [op]
        ops.append({"op": "save"})
    ops.append({"op": "waituntil", "at": M.local_text(seg[2])})
    ops.append({"op": "save"})
    if final:
        ops.append({"op": "dump", "file": "end.json"})
        ops.append({"op": "dumplog", "file": "end.log"})
    ops.append({"op": "exit", "name": seg[0]})
    return ops


# ------------------------------------------------------------------------------------------------ running

class RunState:
    """What the other runs need to know about this one: when its first process started, and its clock then."""

    def __init__(self):
        self.started = threading.Event()
        self.real0 = None
        self.fake0 = None


def launch(run, seg, index, out, home, outbox, driver, clock, states):
    """Runs one segment (one process) to its exit."""
    final = index == len(run["segments"]) - 1
    seg_dir = os.path.join(out, seg[0])
    os.makedirs(seg_dir, exist_ok=True)
    if seg[3]:
        set_conf(home, seg[3])
    if index == 0 and not run.get("shares_home"):
        states[run["id"]].real0 = time.time()
        states[run["id"]].fake0 = seg[1]
        states[run["id"]].started.set()
    ops_path = os.path.join(seg_dir, "ops.json")
    with open(ops_path, "w", encoding="utf-8") as f:
        json.dump(ops_for(run, seg, final), f, indent=1)
    env = dict(os.environ)
    env.update({
        "HOME": home,
        "QT_QPA_PLATFORM": "offscreen",
        "TZ": "Europe/London",
        "CM_TEST_SECRET": SECRET,
        "CM_TEST_SETKEY": "1",
        "CM_TEST_OUTBOX": outbox,
        "CM_TEST_MESSAGE_TAGS": "1",
        "LD_PRELOAD": clock,
        "FAKE_START": repr(epoch(seg[1])),
        "FAKE_SPEED": str(M.SPEED),
    })
    env.pop("CM_TEST_FAULTS", None)
    if run.get("fail_email"):
        env["CM_TEST_FAIL_EMAIL"] = run["fail_email"]
    if run.get("fail_sms"):
        env["CM_TEST_FAIL_SMS"] = run["fail_sms"]
    seconds = (seg[2] - seg[1]).total_seconds() / M.SPEED
    with open(os.path.join(seg_dir, "stdout.log"), "w", encoding="utf-8") as log:
        proc = subprocess.run([driver, seg_dir, ops_path], cwd=seg_dir, env=env, stdout=log, stderr=subprocess.STDOUT,
                              timeout=seconds + 600)
    return proc.returncode


def run_thread(run, out, driver, clock, states, errors):
    try:
        rid = run["id"]
        home = os.path.join(out, rid, "home")
        if run.get("shares_home"):
            home = os.path.join(out, run["shares_home"], "home")
            outbox = os.path.join(out, run["shares_home"], "outbox")
            # the second copy starts when run A's clock reaches its first segment (the same moment, in the program's time)
            states[run["shares_home"]].started.wait()
            a = states[run["shares_home"]]
            due = a.real0 + (epoch(run["segments"][0][1]) - epoch(a.fake0)) / M.SPEED
            time.sleep(max(0.0, due - time.time()))
        else:
            build_home(run, home)
            outbox = os.path.join(out, rid, "outbox")
        os.makedirs(outbox, exist_ok=True)
        run_dir = os.path.join(out, rid)
        os.makedirs(run_dir, exist_ok=True)
        for index, seg in enumerate(run["segments"]):
            seg_out = run_dir
            rc = launch(run, seg, index, seg_out, home, outbox, driver, clock, states)
            if rc != 0:
                errors.append("run %s, segment %s: the program exited with status %s" % (rid, seg[0], rc))
    except Exception as e:           # noqa: BLE001  (reported, so that the other runs still finish)
        errors.append("run %s: %s" % (run["id"], e))
        states[run["id"]].started.set()


def run_all(runs, out, driver, clock):
    states = {r["id"]: RunState() for r in runs}
    errors = []
    threads = [threading.Thread(target=run_thread, args=(r, out, driver, clock, states, errors)) for r in runs]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    return errors


# ------------------------------------------------------------------------------------------------ reading the outcomes

class Msg:
    def __init__(self, path):
        self.path = path
        with open(path, encoding="utf-8", errors="replace") as f:
            text = f.read()
        head, _, body = text.partition("\nbody:\n")
        fields = {}
        for line in head.splitlines():
            key, _, value = line.partition(": ")
            fields[key] = value
        self.time = dt.datetime.fromisoformat(fields.get("sent", "1970-01-01T00:00:00"))
        self.result = fields.get("result", "")
        self.channel = fields.get("channel", "")
        self.to = fields.get("to", "")
        self.subject = fields.get("subject", "")
        self.reason = fields.get("reason", "")
        self.body = body
        code = re.match(r"(AN-\d{3})", self.subject)
        self.code = code.group(1) if code else ""
        words = self.subject.split()
        self.kind = words[1] if len(words) > 1 else ""
        quoted = re.search(r"(\d\d:\d\d) (\d\d) ([A-Za-z]+)", self.subject)
        self.quoted = " ".join(quoted.groups()) if quoted else ""


def load_outbox(path):
    if not os.path.isdir(path):
        return []
    msgs = [Msg(os.path.join(path, n)) for n in sorted(os.listdir(path)) if n.endswith(".txt")]
    return sorted(msgs, key=lambda m: m.time)


def quoted_of(occ):
    return "%s %s %s" % (occ.strftime("%H:%M"), occ.strftime("%d"), occ.strftime("%B"))


def matches(m, e):
    return (m.result == "sent" and m.code == e["code"] and m.channel == e["channel"] and m.to == e["to"]
            and m.kind == e["kind"] and m.quoted == quoted_of(e["occ"]))


def check_expectations(expects, groups, sent, codes):
    """Each expected message is sent once, on time; each group has its count in its window; nothing else is sent."""
    results = []
    used = set()
    for e in expects:
        label = "%s %s to %s, appointment %s" % (e["code"], e["kind"], e["to"], quoted_of(e["occ"]))
        hits = [i for i, m in enumerate(sent) if matches(m, e)]
        if not hits:
            results.append((False, "not sent: " + label + " (due %s)" % e["when"]))
            continue
        if len(hits) > 1:
            results.append((False, "sent %d times: %s" % (len(hits), label)))
        used.update(hits)
        m = sent[hits[0]]
        # A confirmation may arrive up to CONFIRM_LATE after its time; other messages, one check (CHECK) after it.
        late = M.CONFIRM_LATE if e["kind"] == M.KIND["confirm"] else CHECK
        if not (e["when"] <= m.time <= e["when"] + late):
            results.append((False, "late or early: %s sent %s, expected %s to %s" % (label, m.time, e["when"],
                                                                                 e["when"] + late)))
        elif e["must_not"] and (e["must_not"] in m.subject or e["must_not"] in m.body):
            results.append((False, "contains '%s': %s" % (e["must_not"], label)))
        else:
            results.append((True, "on time: " + label + " at %s" % m.time))
    for g in groups:
        inside = [i for i, m in enumerate(sent) if m.code == g["code"] and m.channel == g["channel"]
                  and m.kind == g["kind"] and g["start"] <= m.time <= g["end"]]
        used.update(inside)
        results.append((len(inside) == g["count"], "%d %s %s in %s to %s (expected %d)" % (
            len(inside), g["code"], g["kind"], g["start"], g["end"], g["count"])))
    for i, m in enumerate(sent):
        if m.code in codes and i not in used:
            results.append((False, "unexpected: %s %s to %s at %s" % (m.code, m.kind, m.to, m.time)))
    return results


def evaluate(out):
    tests = tests_by_id()
    report = []
    for t in M.ALL:
        rid = t["run"]
        run = ALL_RUNS[rid]
        run_dir = os.path.join(out, rid)
        outbox_dir = os.path.join(out, run["shares_home"] if run.get("shares_home") else rid, "outbox")
        outbox = load_outbox(outbox_dir)
        sent = [m for m in outbox if m.result == "sent"]
        refused = [m for m in outbox if m.result == "refused"]
        results = []
        if t.get("aggregate"):
            ids = [x for x in ALL_RUNS[rid]["tests"] if not tests[x].get("aggregate")]
            expects, groups = [], []
            for x in ids:
                res = expectations_of(run)[x] if x in expectations_of(run) else None
                if res:
                    expects += res["expect"]
                    groups += res.get("groups", [])
            results = check_expectations(expects, groups, sent, {tests[x]["id"] for x in ids})
            results.append((True, "the run's messages were checked test by test above") if results else (False, "no tests"))
        else:
            res = expectations_of(run)[t["id"]]
            codes = {t["id"]}
            results += check_expectations(res["expect"], res.get("groups", []),
                                          [m for m in sent if m.code in codes], codes)
            for code, kind, count in res.get("refused", []):
                n = sum(1 for m in refused if m.code == code and m.kind == kind)
                results.append((n == count, "refused %d %s %s times (expected %d)" % (n, code, kind, count)))
            last = os.path.join(run_dir, run["segments"][-1][0])
            log_path = os.path.join(last, "end.log")
            log = open(log_path, encoding="utf-8", errors="replace").read().lower() if os.path.exists(log_path) else ""
            for text, present in res.get("logs", []):
                found = text.lower() in log
                results.append((found == present, "log %s '%s'" % ("holds" if present else "does not hold", text)))
            dump_path = os.path.join(last, "end.json")
            dump = json.load(open(dump_path, encoding="utf-8")) if os.path.exists(dump_path) else {"contacts": []}
            for n, text, count in res.get("history", []):
                surname = M.CONTACT[n]["surname"]
                entries = [c for c in dump.get("contacts", []) if c.get("surname") == surname]
                history = entries[0].get("history", "") if entries else ""
                got = history.count(text)
                results.append((got == count, "contact %d history has '%s' %d times (expected %d)" % (n, text, got, count)))
            for (dialog_run, index), title, present in res.get("dialogs", []):
                seg_name = ALL_RUNS[dialog_run]["segments"][index][0]
                path = os.path.join(out, dialog_run, seg_name, "driver.log")
                text = open(path, encoding="utf-8", errors="replace").read() if os.path.exists(path) else ""
                results.append(((title in text) == present, "dialog '%s' %s in %s" % (
                    title, "shown" if present else "not shown", seg_name)))
            for quiet_run, start, end in res.get("quiet", []):
                q_out = os.path.join(out, quiet_run, "outbox")
                inside = [m for m in load_outbox(q_out) if start <= m.time <= end]
                results.append((not inside, "no message from run %s between %s and %s (%d found)" % (
                    quiet_run, start, end, len(inside))))
            if not results:
                results.append((True, "no message is sent"))
            results = [r for r in results]
        ok = all(r[0] for r in results)
        note = t.get("note") or (expectations_of(run)[t["id"]].get("note") if not t.get("aggregate") else "")
        report.append(dict(id=t["id"], title=t["title"], run=rid, ok=ok, results=results, note=note or ""))
    return report


def write_report(report, out, errors):
    passed = sum(1 for r in report if r["ok"])
    lines = ["# Announcement tests: results", "", "%d of %d tests pass." % (passed, len(report)), ""]
    if errors:
        lines += ["## Errors while running", ""] + ["- " + e for e in errors] + [""]
    lines += ["| Test | Run | Result | Title |", "|---|---|---|---|"]
    for r in report:
        lines.append("| %s | %s | %s | %s |" % (r["id"], r["run"], "PASS" if r["ok"] else "FAIL", r["title"]))
    lines.append("")
    for r in report:
        lines += ["## %s (run %s)" % (r["id"], r["run"]), "", r["title"], ""]
        if r["note"]:
            lines += ["Note: " + r["note"], ""]
        for ok, text in r["results"]:
            lines.append("- %s %s" % ("pass" if ok else "**FAIL**", text))
        lines.append("")
    with open(os.path.join(out, "report.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    with open(os.path.join(out, "summary.json"), "w", encoding="utf-8") as f:
        json.dump([{k: v for k, v in r.items() if k != "results"} | {"checks": len(r["results"]),
                    "failed": [t for ok, t in r["results"] if not ok]} for r in report], f, indent=1)
    return passed, len(report)


def main(argv=None):
    p = argparse.ArgumentParser(description="Announcement tests (area 2, AN-001 to AN-048).")
    p.add_argument("--plan", action="store_true", help="check and print the plan, and start nothing")
    p.add_argument("--runs", default="", help="comma-separated run ids to run (default: all)")
    p.add_argument("--driver", default=os.environ.get("CM_TEST_ANNOUNCE_DRIVER", DEFAULT_DRIVER))
    p.add_argument("--clock", default=os.environ.get("CM_TEST_ANNOUNCE_CLOCK", DEFAULT_CLOCK))
    p.add_argument("--out", default=None)
    args = p.parse_args(argv)

    problems = plan_problems()
    if args.plan:
        if problems:
            print("the plan has problems:")
            for x in problems:
                print("  " + x)
            return 1
        print_plan()
        return 0
    if problems:
        print("the plan has problems; fix them before running:")
        for x in problems:
            print("  " + x)
        return 2
    for path in (args.driver, args.clock):
        if not os.path.exists(path):
            print("not built: %s (see driver/build_driver.sh)" % path)
            return 2

    wanted = [x for x in args.runs.split(",") if x] or [r["id"] for r in M.RUNS]
    unknown = [x for x in wanted if x not in ALL_RUNS]
    if unknown:
        print("unknown run: %s" % ", ".join(unknown))
        return 2
    runs = [ALL_RUNS[x] for x in wanted]
    # a second copy needs run A's home, so run A is started with it
    for r in list(runs):
        if r.get("shares_home") and r["shares_home"] not in wanted:
            runs.append(ALL_RUNS[r["shares_home"]])
    out = args.out or os.path.join(DEFAULT_OUT, "announcements-" + time.strftime("%Y%m%d-%H%M%S"))
    os.makedirs(out, exist_ok=True)
    started = time.time()
    errors = run_all(runs, out, args.driver, args.clock)
    print("runs finished in %.0f real seconds" % (time.time() - started))
    report = evaluate(out)
    passed, total = write_report(report, out, errors)
    for r in report:
        print("%-5s %s  %s" % ("PASS" if r["ok"] else "FAIL", r["id"], r["title"]))
    print("%d of %d tests pass. Evidence: %s" % (passed, total, out))
    return 0 if passed == total and not errors else 1


if __name__ == "__main__":
    sys.exit(main())
