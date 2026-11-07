#!/usr/bin/env python3
#
# Automated checks for section 4 of documents/test-specification.md (AC-019 to AC-029).
#
# Each automated test supports a manual test (AC-001 to AC-012, the table in section 4), and checks what the program gives a
# screen reader: the names, the focus, and the text of the screen reader text.  It does not check that a screen reader
# speaks it, or how it sounds, or that focus can be seen.  Those stay with the manual tests.
#
# The tests run the program headless through the operation driver (autotest/operation/driver), in a fresh home folder, one
# after another.  They need the program only, and reach no Google account and send no message.
#
#   python3 autotest/accessibility/automated_tests.py --list
#   python3 autotest/accessibility/automated_tests.py [--driver PATH] [--out DIR] [AC-019 ...]
#

import argparse
import json
import os
import shutil
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
OPS = os.path.join(REPO, "autotest", "operation")
sys.path.insert(0, OPS)
import ops_common as oc                                    # noqa: E402
import operation_tests as ot                               # noqa: E402  (its runner: execute() runs a test's runs)

DEFAULT_DRIVER = os.path.join(os.path.expanduser("~"), "build-contactmanager", "driver")
DEFAULT_OUT = os.path.join(os.path.expanduser("~"), "contactmanager-test-runs")
TESTS = []
SUMMARY_WORDS = 4


def test(code, title, build, check_fn, supports):
    TESTS.append({"id": code, "title": title, "build": build, "check": check_fn, "supports": supports})


def menus_op(name):
    return {"op": "menus", "name": name}


def tabs_op(name):
    return {"op": "tabs", "name": name}


def controls_op(name, scope="top"):
    return {"op": "controls", "name": name, "scope": scope}


def focus_op(name):
    return {"op": "focus", "name": name}


def acc_rows_op(name):
    return {"op": "acc_rows", "name": name}


def wait_op(ms):
    return {"op": "wait", "ms": ms}


# ------------------------------------------------------------------------------------------------ AC-019: menus

def _ac019_build(home):
    return [oc.run_spec([menus_op("m")])]


def _ac019_check(runs, home):
    items = runs[0].value("m_menus") or []
    top = [i["text"] for i in items if i["depth"] == 0]
    unnamed = [i for i in items if not i["text"].strip()]
    no_mnemonic = [t for t in top if "&" not in t]
    return [oc.check("every menu item is named", not unnamed, "%d unnamed" % len(unnamed)),
            oc.check("every top menu has a keyboard letter (&)", top and not no_mnemonic,
                     "without a letter: %s" % no_mnemonic)]


test("AC-019", "Every menu and submenu item is named, and every top menu has its Alt letter (automates AC-001)",
     _ac019_build, _ac019_check, "AC-001")


# ------------------------------------------------------------------------------------------------ AC-020: tabs

def _ac020_build(home):
    return [oc.run_spec([tabs_op("t")])]


def _ac020_check(runs, home):
    tabs = runs[0].value("t_tabs") or []
    no_letter = [t for t in tabs if "&" not in t]
    return [oc.check("there are tabs", len(tabs) >= 3, "tabs: %s" % tabs),
            oc.check("every tab has a name and its Alt letter (&)", not no_letter, "without a letter: %s" % no_letter)]


test("AC-020", "Every tab is named, and has its Alt letter (automates AC-002)", _ac020_build, _ac020_check, "AC-002")


# ------------------------------------------------------------------------------------------------ AC-021: dialogs

def _ac021_build(home):
    # each dialog is opened, its controls are read under its own name (c1 to c5), and it is cancelled
    dialogs = [
        [oc.menu_op("open", "actionSetup")],
        [oc.open_op(1, "o"), oc.menu_op("open", "actionNewAppointment")],
        [oc.menu_op("open", "action_Advanced_Find")],
        [oc.open_op(1, "o"), oc.menu_op("open", "action_SMS_Contact")],
        [oc.open_op(1, "o"), oc.menu_op("open", "actionIncome")],
    ]
    ops = [oc.hold_op()]
    for i, opening in enumerate(dialogs, start=1):
        ops += opening + [controls_op("c%d" % i), oc.press_op("cancel%d" % i, "Cancel")]
    ops.append(oc.hold_op(False))
    return [oc.run_spec(oc.fixtures_ops()), oc.run_spec(ops)]


_AC021_DIALOGS = ["Setup", "Appointment form", "Advanced Global Find", "Send SMS", "Transaction income"]


def _ac021_check(runs, home):
    r = runs[1]
    checks = []
    for i, label in enumerate(_AC021_DIALOGS, start=1):
        controls = r.value("c%d_controls" % i) or []
        unnamed = [c.get("object") or c.get("class") for c in controls if not str(c.get("name", "")).strip()]
        checks.append(oc.check("%s: its controls are reached with Tab, and are named" % label,
                               bool(controls) and not unnamed,
                               "%d controls, %d unnamed: %s" % (len(controls), len(unnamed), unnamed[:8])))
    return checks


test("AC-021", "Every control in each dialog that Tab reaches has a name (automates AC-003)",
     _ac021_build, _ac021_check, "AC-003")


# ------------------------------------------------------------------------------------------------ AC-022: screen reader text

def _ac022_build(home):
    summary = "AC022 Dentist"
    return [oc.run_spec(oc.fixtures_ops() + [oc.appt_op(1, summary, oc.at(20, "10:00"), oc.at(20, "11:00")), oc.save_op()]),
            oc.run_spec([oc.menu_op("info", "actionShowAccessibleText"),
                         oc.calendar_click_op("c", summary, oc.surname(1)), wait_op(1500), {"op": "accinfo", "name": "a1"},
                         oc.keys_op("tab", "Tab"), wait_op(1500), {"op": "accinfo", "name": "a2"}, oc.dump_op()])]


def _ac022_check(runs, home):
    r = runs[1]
    first = r.value("a1_text") or ""
    second = r.value("a2_text") or ""
    return [oc.check("the text names the control with the focus (the calendar list)", "Control: listCalendar" in first,
                     "text starts %r" % first[:60]),
            oc.check("after Tab, the text follows the new focus", second.startswith("Control:") and second != first,
                     "text after Tab starts %r" % second[:60])]


test("AC-022", "Show Screen Reader Text follows the focus and updates its values (automates AC-004)",
     _ac022_build, _ac022_check, "AC-004")


# ------------------------------------------------------------------------------------------------ AC-023: calendar entries

def _ac023_build(home):
    summaries = ["AC023 Dentist", "AC023 Meeting", "AC023 Review"]
    ops = oc.fixtures_ops()
    for i, summary in enumerate(summaries):
        ops.append(oc.appt_op(1, summary, oc.at(20 + i, "10:00"), oc.at(20 + i, "11:00")))
    ops.append(oc.save_op())
    return [oc.run_spec(ops), oc.run_spec([acc_rows_op("rows"), oc.dump_op()])]


def _ac023_check(runs, home):
    rows = runs[1].value("rows_rows") or []
    ours = [r for r in rows if "AC023" in r]
    forbidden = [home, "TrumptonApps", "ContactManager/", "/home/"]
    bad_path = [r for r in ours if any(f and f in r for f in forbidden)]
    long = [r for r in ours if len(r) > 200]
    no_time = [r for r in ours if "10:00" not in r]
    return [oc.check("each of the three entries is read", len(ours) == 3, "%d entries" % len(ours)),
            oc.check("each entry has its time and its summary", not no_time and all("AC023" in r for r in ours),
                     "without the time: %s" % no_time),
            oc.check("no entry is padded with the program's path", not bad_path, "padded: %s" % bad_path),
            oc.check("each entry is a usable length (200 characters or fewer)", not long, "long: %s" % [len(r) for r in long])]


test("AC-023", "Calendar entries are read with their date, time and summary, at a usable length (automates AC-005)",
     _ac023_build, _ac023_check, "AC-005")


# ------------------------------------------------------------------------------------------------ AC-024: contact form

_FORM_FIELDS = ["editFirstName", "editSurname", "editHome", "editWork", "editMobile", "editEmail"]
_FORM_LABELS = ["labelNames", "labelSurname", "editEmailTitle", "editPhoneTitle", "editMobileTitle"]


def _ac024_build(home):
    return [oc.run_spec(oc.fixtures_ops()),
            oc.run_spec([oc.open_op(1, "o"), controls_op("form", scope="main"),
                         {"op": "widgets", "name": "lbl", "objects": _FORM_LABELS}, oc.dump_op()])]


def _ac024_check(runs, home):
    r = runs[1]
    controls = {c.get("object"): c for c in (r.value("form_controls") or [])}
    missing = [f for f in _FORM_FIELDS if f not in controls]
    unnamed = [f for f in _FORM_FIELDS if f in controls and not str(controls[f].get("name", "")).strip()]
    blank_labels = [l for l in _FORM_LABELS if not str(r.value("lbl_" + l) or "").strip()]
    return [oc.check("every contact field can be reached and is named", not missing and not unnamed,
                     "missing %s, unnamed %s" % (missing, unnamed)),
            oc.check("every field has its label, and its title is shown", not blank_labels,
                     "blank: %s" % blank_labels)]


test("AC-024", "Each field of the contact form is read with its label and its title (automates AC-006)",
     _ac024_build, _ac024_check, "AC-006")


# ------------------------------------------------------------------------------------------------ AC-025: warning takes focus

def _ac025_build(home):
    return [oc.run_spec(oc.fixtures_ops()),
            oc.run_spec([oc.hold_op(), oc.menu_op("setup", "actionSetup"),
                         oc.set_op("bad", "smsKeyExpiry", text="not a date"), oc.press_op("ok", "OK"),
                         focus_op("f"), oc.widgets_op("w", ["qt_msgbox_label"]), oc.press_op("warnok", "OK"),
                         oc.hold_op(False), oc.dump_op()])]


def _ac025_check(runs, home):
    r = runs[1]
    return [oc.check("the warning is open", bool(r.value("f_modal")), "no dialog open"),
            # When no window is active (as in the headless run) the application focus is empty, so the control that has
            # the focus inside the warning is read instead
            oc.check("the focus is in the warning", r.value("f_focus_in_modal") is True or bool(r.value("f_modal_focusclass")),
                     "focus in %r (window %r); the warning's own focus control %r" % (r.value("f_focusclass"), r.value("f_focuswindow"),
                                                                                    r.value("f_modal_focusclass"))),
            oc.check("the warning has its message", bool(str(r.value("w_qt_msgbox_label") or "").strip()),
                     "no message text")]


test("AC-025", "An error or warning takes the focus, so that it is read at once (automates AC-008)",
     _ac025_build, _ac025_check, "AC-008")


# ------------------------------------------------------------------------------------------------ AC-026: Escape

def _ac026_build(home):
    summary = "AC026 Dentist"
    ops = [oc.hold_op()]
    for i, action in enumerate(["actionSetup", "action_Advanced_Find", "actionAbout"], start=1):
        ops += [oc.calendar_click_op("c%d" % i, summary, oc.surname(1)), oc.menu_op("m%d" % i, action),
                oc.keys_op("esc%d" % i, "Escape"), focus_op("after%d" % i)]
    ops.append(oc.hold_op(False))
    ops.append(oc.dump_op())
    return [oc.run_spec(oc.fixtures_ops() + [oc.appt_op(1, summary, oc.at(20, "10:00"), oc.at(20, "11:00")), oc.save_op()]),
            oc.run_spec(ops)]


def _ac026_check(runs, home):
    r = runs[1]
    checks = []
    for i, label in enumerate(["Setup", "Advanced Global Find", "About"], start=1):
        focus = r.value("after%d_focus" % i)
        still_open = r.value("after%d_modal" % i)
        checks.append(oc.check("Escape closes %s and the focus returns to the calendar list" % label,
                               focus == "listCalendar" and not still_open,
                               "focus %r, dialog still open %r" % (focus, still_open)))
    return checks


test("AC-026", "Escape closes every dialog and returns focus to the window that opened it (automates AC-009)",
     _ac026_build, _ac026_check, "AC-009")


# ------------------------------------------------------------------------------------------------ AC-027: Enter

def _ac027_build(home):
    summary = "AC027 Dentist"
    return [oc.run_spec(oc.fixtures_ops() + [oc.appt_op(1, summary, oc.at(20, "10:00"), oc.at(20, "11:00")), oc.save_op()]),
            oc.run_spec([oc.calendar_click_op("dbl", summary, oc.surname(1)), wait_op(1500), {"op": "state", "name": "s1"},
                         oc.windows_op("w1"), {"op": "setrow", "row": 0}, oc.key_op("enter", "enter", target="list"),
                         wait_op(1500), {"op": "state", "name": "s2"}, oc.windows_op("w2"), oc.dump_op()])]


def _ac027_check(runs, home):
    r = runs[1]
    return [oc.check("Enter opens what a double-click opens (the same contact)",
                     r.value("s1_loaded") == r.value("s2_loaded") and r.value("s1_loaded") not in (None, "none"),
                     "double-click %r, Enter %r" % (r.value("s1_loaded"), r.value("s2_loaded"))),
            oc.check("Enter selects the same entry", r.value("s1_entry") == r.value("s2_entry"),
                     "double-click %r, Enter %r" % (r.value("s1_entry"), r.value("s2_entry"))),
            oc.check("Enter opens the same windows", r.value("w1_windows") == r.value("w2_windows"),
                     "double-click %s, Enter %s" % (r.value("w1_windows"), r.value("w2_windows")))]


test("AC-027", "Every action has a key: Enter on a calendar entry does what a double-click does (automates AC-010)",
     _ac027_build, _ac027_check, "AC-010")


# ------------------------------------------------------------------------------------------------ AC-028: date box names

def _ac028_build(home):
    return [oc.run_spec(oc.fixtures_ops()),
            oc.run_spec([oc.dates_op([""]), oc.keys_op("d", "Ctrl+D"), oc.dump_op()])]


def _ac028_check(runs, home):
    r = runs[1]
    name = str(r.value("dialog_1_accname") or "")
    desc = str(r.value("dialog_1_accdesc") or "")
    return [oc.check("the date box has a short name", bool(name.strip()) and len(name.split()) <= SUMMARY_WORDS,
                     "name %r" % name),
            oc.check("the date box has a description", bool(desc.strip()), "description %r" % desc)]


test("AC-028", "The typed-date box has a short name and a description (automates AC-011)",
     _ac028_build, _ac028_check, "AC-011")


# ------------------------------------------------------------------------------------------------ AC-029: Tidy keeps focus

def _ac029_build(home):
    summary = "AC029 Dentist"
    return [oc.run_spec(oc.fixtures_ops() + [oc.appt_op(1, summary, oc.at(20, "10:00"), oc.at(20, "11:00")), oc.save_op()]),
            oc.run_spec([oc.calendar_click_op("c", summary, oc.surname(1)), focus_op("before"),
                         oc.tidy_op("tidy"), focus_op("after"), oc.dump_op()])]


def _ac029_check(runs, home):
    r = runs[1]
    # The application focus is empty when no window is active (as after a modal closes in the headless run), so the
    # main window's own focus is read as well
    focus = r.value("after_focus") or r.value("after_window_focus")
    return [oc.check("after Tidy Records the focus is on the calendar entry", focus == "listCalendar",
                     "focus %r, main window focus %r" % (r.value("after_focus"), r.value("after_window_focus"))),
            oc.check("and the same entry is selected", r.value("after_row") == r.value("before_row") and
                     r.value("before_row") is not None, "row before %r, after %r" % (r.value("before_row"),
                                                                                     r.value("after_row")))]


test("AC-029", "Focus stays on the entry after Tidy Records (automates the Tidy part of AC-012; the sync part is manual)",
     _ac029_build, _ac029_check, "AC-012")


# ------------------------------------------------------------------------------------------------ the runner

# ------------------------------------------------------------------------------------------------ IA-002: the debug checkbox

def _ia002_build(home):
    return [oc.run_spec([{"op": "call", "name": "ia002", "fn": "showacc"},
                         {"op": "accdebug", "name": "ia002on", "value": True},
                         {"op": "accdebug", "name": "ia002off", "value": False}, oc.dump_op()])]


def _ia002_check(runs, home):
    r = runs[0]
    return [oc.check("the debug checkbox is ticked", r.value("ia002on_box") is True, "box %r" % r.value("ia002on_box")),
            oc.check("and cleared again", r.value("ia002off_box") is False, "box %r" % r.value("ia002off_box")),
            oc.check("switching on is logged to the console", r.logged("AccessibleDebug on"), ""),
            oc.check("switching off is logged to the console", r.logged("AccessibleDebug off"), "")]


test("IA-002", "The accessibility debug checkbox in the screen reader text dialog turns console tracing on and off",
     _ia002_build, _ia002_check, "imported (archive L06)")


def main(argv=None):
    p = argparse.ArgumentParser(description="Automated checks for section 4 (AC-019 to AC-029).")
    p.add_argument("--list", action="store_true")
    p.add_argument("--driver", default=os.environ.get("CM_OP_DRIVER", DEFAULT_DRIVER))
    p.add_argument("--out", default=None)
    p.add_argument("codes", nargs="*")
    args = p.parse_args(argv)
    if args.list:
        for t in TESTS:
            print("%-7s supports %-7s %s" % (t["id"], t["supports"], t["title"]))
        return 0
    known = {t["id"] for t in TESTS}
    unknown = [c for c in args.codes if c not in known]
    if unknown:
        print("unknown test: %s" % ", ".join(unknown))
        return 2
    if not os.path.exists(args.driver):
        print("the driver is not built: %s (see autotest/operation/driver/build_driver.sh)" % args.driver)
        return 2
    out = args.out or os.path.join(DEFAULT_OUT, "accessibility-automated-" + time.strftime("%Y%m%d-%H%M%S"))
    os.makedirs(out, exist_ok=True)
    summary = []
    failed = 0
    for t in [x for x in TESTS if not args.codes or x["id"] in args.codes]:
        base = os.path.join(out, t["id"])
        shutil.rmtree(base, ignore_errors=True)
        os.makedirs(base)
        home = oc.make_home(os.path.join(base, "home"))
        try:
            runs = ot.execute(args.driver, home, base, t["build"](home))
            checks = t["check"](runs, home)
        except Exception as e:                       # noqa: BLE001  (reported as a failed check, not hidden)
            checks = [oc.Check(False, "the test raised %s: %s" % (type(e).__name__, e))]
        status = oc.status_of(checks)
        failed += status == "FAIL"
        summary.append({"id": t["id"], "title": t["title"], "supports": t["supports"], "status": status,
                        "checks": [{"ok": c.ok, "detail": c.detail} for c in checks]})
        print("%-7s %-7s %s" % (t["id"], status, t["title"]))
        for c in checks:
            if c.ok is not True:
                print("          %s" % c.detail)
    with open(os.path.join(out, "summary.json"), "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=1)
    print("Evidence: %s" % out)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
