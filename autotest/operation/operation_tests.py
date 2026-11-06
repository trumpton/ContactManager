#!/usr/bin/env python3
#
# Operation tests (test specification, area 3): SS-001 to SS-015 (start-up and shutdown), MF-001 to MF-053 (by menu and
# function) and DL-001 to DL-030 (by dialog).  Each test runs the program headless, through the operation driver
# (driver/driver.cpp, built by driver/build_driver.sh), in a fresh HOME folder.  Nothing here reaches Google or sends a
# message, so the tests need only the program.
#
# usage:
#     python3 autotest/operation/operation_tests.py --list
#     python3 autotest/operation/operation_tests.py [--driver PATH] [--out DIR] [CODE ...]
#
# The tests run one after another.  Each test's runs share one HOME folder, so a second run is a restart (as the
# specification's tests say).  A test with a background run starts a second copy of the program, or a run to be killed.
#
# Results: one folder per test under --out, with each run's files; summary.json and report.md at the top.
#

import argparse
import json
import os
import shutil
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ops_common as oc                                    # noqa: E402
import tests_startup                                       # noqa: E402,F401  (registers SS-001 to SS-015)
import tests_menus                                         # noqa: E402,F401  (registers MF-001 to MF-053)
import tests_dialogs                                       # noqa: E402,F401  (registers DL-001 to DL-030)

DEFAULT_DRIVER = os.path.join(os.path.expanduser("~"), "build-contactmanager", "driver")
DEFAULT_OUT = os.path.join(os.path.expanduser("~"), "contactmanager-test-runs")


def execute(driver, home, base, specs):
    """Runs a test's runs in order.  Background runs are started, and collected at the end (or killed after their time)."""
    runs = []
    pending = []
    for i, spec in enumerate(specs, start=1):
        if spec.get("before"):
            spec["before"](home)
        outdir = os.path.join(base, "run%d" % i)
        kw = dict(secret=spec.get("secret", oc.SECRET), setkey=spec.get("setkey", True), extra_env=spec.get("env"))
        if spec.get("kill"):
            proc, logf = oc.start_driver(driver, home, outdir, spec["ops"], **kw)
            time.sleep(spec["kill"])
            runs.append(oc.collect(proc, logf, outdir, kill=True))
        elif spec.get("bg"):
            proc, logf = oc.start_driver(driver, home, outdir, spec["ops"], **kw)
            time.sleep(spec.get("settle", 8))              # let the background copy start before the next run
            pending.append((len(runs), proc, logf, outdir))
            runs.append(None)
        else:
            runs.append(oc.run_driver(driver, home, outdir, spec["ops"], **kw))
    for index, proc, logf, outdir in pending:
        runs[index] = oc.collect(proc, logf, outdir)
    return runs


def main(argv=None):
    p = argparse.ArgumentParser(description="Operation tests (area 3): SS, MF and DL.")
    p.add_argument("--list", action="store_true", help="list the tests and exit")
    p.add_argument("--driver", default=os.environ.get("CM_OP_DRIVER", DEFAULT_DRIVER), help="the built operation driver")
    p.add_argument("--out", default=None, help="where the results go (default: a timestamped folder in your home folder)")
    p.add_argument("codes", nargs="*", help="test IDs to run (default: all)")
    args = p.parse_args(argv)

    if args.list:
        for t in oc.TESTS:
            print("%-7s %s" % (t["id"], t["title"]))
        print("%d tests" % len(oc.TESTS))
        return 0
    known = {t["id"] for t in oc.TESTS}
    unknown = [c for c in args.codes if c not in known]
    if unknown:
        print("unknown test: %s" % ", ".join(unknown))
        return 2
    if not os.path.exists(args.driver):
        print("the driver is not built: %s (see driver/build_driver.sh)" % args.driver)
        return 2

    out = args.out or os.path.join(DEFAULT_OUT, "operation-" + time.strftime("%Y%m%d-%H%M%S"))
    os.makedirs(out, exist_ok=True)
    chosen = [t for t in oc.TESTS if not args.codes or t["id"] in args.codes]
    summary = []
    for t in chosen:
        base = os.path.join(out, t["id"])
        shutil.rmtree(base, ignore_errors=True)
        os.makedirs(base)
        home = oc.make_home(os.path.join(base, "home"))
        if t["blocked"]:
            checks = [oc.blocked(t["id"], t["blocked"])]
        else:
            try:
                specs = t["build"](home)
                runs = execute(args.driver, home, base, specs)
                checks = t["check"](runs, home)
            except Exception as e:                       # noqa: BLE001  (reported as a failed check, not hidden)
                checks = [oc.Check(False, "the test raised %s: %s" % (type(e).__name__, e))]
        status = oc.status_of(checks)
        detail = "; ".join(c.detail for c in checks)
        summary.append({"id": t["id"], "title": t["title"], "status": status,
                        "checks": [{"ok": c.ok, "detail": c.detail} for c in checks]})
        print("%-8s %-8s %s" % (t["id"], status, t["title"]))
        for c in checks:
            if c.ok is not True:
                print("           %s" % c.detail)
        with open(os.path.join(base, "result.txt"), "w", encoding="utf-8") as f:
            f.write("%s %s\n%s\n\n" % (t["id"], status, t["title"]))
            for c in checks:
                f.write("%s %s\n" % ({True: "pass", False: "FAIL", None: "BLOCKED"}[c.ok], c.detail))

    counts = {}
    for s in summary:
        counts[s["status"]] = counts.get(s["status"], 0) + 1
    with open(os.path.join(out, "summary.json"), "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=1)
    lines = ["# Operation tests: results", ""]
    lines.append(", ".join("%s %d" % (k, counts[k]) for k in sorted(counts)) + ".")
    lines.append("")
    lines.append("| Test | Result | Title |")
    lines.append("|---|---|---|")
    for s in summary:
        lines.append("| %s | %s | %s |" % (s["id"], s["status"], s["title"]))
    lines.append("")
    for s in summary:
        lines.append("## %s" % s["id"])
        lines.append("")
        for c in s["checks"]:
            lines.append("- %s %s" % ({True: "pass", False: "**FAIL**", None: "BLOCKED"}[c["ok"]], c["detail"]))
        lines.append("")
    with open(os.path.join(out, "report.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("%s. Evidence: %s" % (", ".join("%s %d" % (k, counts[k]) for k in sorted(counts)), out))
    return 1 if counts.get("FAIL") else 0


if __name__ == "__main__":
    sys.exit(main())
