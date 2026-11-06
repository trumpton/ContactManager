#
# Imported operation tests (archive scenarios not covered by SS, MF or DL): IO-002, IO-003, IO-004, IO-006, IO-007, IO-009,
# IO-010, IO-011, IO-012, IO-014, IO-015, IO-016, IO-018, IO-019 and IO-020.  Each was a scenario of the archive's Google
# bench, rewritten for the operation driver.  The test specification's section 3d gives each one's procedure and result.
#
# Where the driver cannot yet set or read a value a check needs, that check is BLOCKED with its reason (not passed).
#

import datetime as dt
import glob
import os

import ops_common as oc
from ops_common import check, blocked, run_spec, test


def _add(tag, **fields):
    f = {"surname": tag, "names": "Bench"}
    f.update(fields)
    return {"op": "add_contact", "fields": f}


def _live(run, surname):
    """The contacts of the dump with this surname that are not deleted."""
    return [c for c in run.contacts() if c.get("surname") == surname and c.get("deleted") != "true"]


def _today():
    return dt.date.today()


def _weeks_in_window(start_day, low=-90, high=730):
    """How many weekly occurrences of a repeat that starts on start_day (days from today) fall in the window."""
    n = 0
    k = 0
    while True:
        day = start_day + 7 * k
        if day >= high:
            break
        if day >= low:
            n += 1
        k += 1
    return n


def _home_contacts_dir(home):
    return os.path.join(oc.data_folder(home), "contact")


# ------------------------------------------------------------------------------------------------ IO-002, IO-003: Tidy merges

def _io002_build(home):
    return [run_spec([_add("MG01", email="mg01@test.invalid", phone="+44 20 7946 0601"),
                      _add("MG01", email="mg01@test.invalid"),
                      {"op": "merge_duplicates", "name": "mg01"}, oc.dump_op()])]


def _io002_check(runs, home):
    r = runs[0]
    live = _live(r, "MG01")
    c = live[0] if live else {}
    return [check("the merge ran", r.value("mg01_merged") == 1, "merged %r" % r.value("mg01_merged")),
            check("one live entry is left", len(live) == 1, "live entries %d" % len(live)),
            check("the merged entry holds the address and the phone",
                  c.get("email") == "mg01@test.invalid" and "7946 0601" in str(c.get("phone", "")),
                  "email %r, phone %r" % (c.get("email"), c.get("phone"))),
            blocked("the note of the second entry is kept", "add_contact cannot set comments yet")]


test("IO-002", "Tidy Contacts merges two entries for one person when they share an address and do not conflict",
     _io002_build, _io002_check)


def _io003_build(home):
    return [run_spec([_add("MG02A", email="mg02@test.invalid", phone="+44 20 7946 0621"),
                      _add("MG02B", email="mg02@test.invalid", phone="+44 20 7946 0622"),
                      {"op": "merge_duplicates", "name": "mg02"}, oc.dump_op()])]


def _io003_check(runs, home):
    r = runs[0]
    live = _live(r, "MG02A") + _live(r, "MG02B")
    return [check("nothing was merged", r.value("mg02_merged") == 0, "merged %r" % r.value("mg02_merged")),
            check("both entries are still there", len(live) == 2, "live entries %d" % len(live)),
            check("the pair is not offered for a merge", r.value("mg02_left") == 0, "left %r" % r.value("mg02_left"))]


test("IO-003", "Tidy Contacts does not merge two entries that share an address but have different names",
     _io003_build, _io003_check)


# ------------------------------------------------------------------------------------------------ IO-004: quitting with edits

def _io004_build(home):
    bad = "Bad Field 1: f12@test.invalid - Bad Field 2 (Bad Field 3)"
    return [run_spec([_add("F12", email="f12@test.invalid"),
                      {"op": "open_contact", "name": "f12a", "match": {"surname": "F12"}},
                      {"op": "form_set", "fields": {"email": bad, "phone": "+44 20 7946 1201"}},
                      {"op": "answer", "text": "Save"}, {"op": "exit", "name": "f12save"},
                      {"op": "answer", "text": "Cancel"}, {"op": "exit", "name": "f12cancel"},
                      {"op": "answer", "text": "Discard"}, {"op": "exit", "name": "f12discard"},
                      oc.dump_op()]),
            run_spec([{"op": "open_contact", "name": "f12b", "match": {"surname": "F12"}, "details": False},
                      oc.dump_op()])]


def _io004_check(runs, home):
    a, b = runs[0], runs[1]
    saved = _live(b, "F12")
    stored = saved[0] if saved else {}
    return [check("Save refuses the bad entry: the window stays open", a.value("f12save_exit_accepted") is False,
                  "exit accepted %r" % a.value("f12save_exit_accepted")),
            check("Cancel keeps the window open", a.value("f12cancel_exit_accepted") is False,
                  "exit accepted %r" % a.value("f12cancel_exit_accepted")),
            check("Discard closes the window", a.value("f12discard_exit_accepted") is True,
                  "exit accepted %r" % a.value("f12discard_exit_accepted")),
            check("Discard keeps the stored contact: no bad email, and no phone change",
                  len(saved) == 1 and stored.get("email") == "f12@test.invalid" and not stored.get("phone"),
                  "email %r, phone %r" % (stored.get("email"), stored.get("phone")))]


test("IO-004", "Quitting with edits asks Save, Discard or Cancel: Save refuses a bad entry, Discard keeps the stored contact",
     _io004_build, _io004_check)


# ------------------------------------------------------------------------------------------------ IO-006: Tidy, one entry unfixable

def _io006_build(home):
    return [run_spec([_add("T02", email="Home: t02@test.invalid - Home (Work)", phone="+44 20 7946 0702 (Mobile)"),
                      {"op": "tidy_contacts", "name": "tidy"}, oc.dump_op()])]


def _io006_check(runs, home):
    r = runs[0]
    local = _live(r, "T02")
    c = local[0] if local else {}
    unfixed = r.value("tidy_unfixed") or []
    return [check("the unfixable email is listed",
                  any("t02@test.invalid" in str(line) and "more than one label" in str(line) for line in unfixed),
                  "listed: %s" % unfixed),
            check("the unfixable email is left as it was", c.get("email") == "Home: t02@test.invalid - Home (Work)",
                  "got %r" % c.get("email")),
            check("the phone on the same contact is still fixed", "7946 0702" in str(c.get("phone", "")),
                  "got %r" % c.get("phone"))]


test("IO-006", "Tidy Contacts lists an entry it cannot fix, leaves it alone, and still fixes the other entry on the contact",
     _io006_build, _io006_check)


# ------------------------------------------------------------------------------------------------ IO-007: double-click opens

def _io007_build(home):
    day = oc.at(7, "09:00")
    return [run_spec([_add("U34", email="u34@test.invalid"),
                      {"op": "add_appointment", "fields": {"summary": "BENCH U34 meeting", "from": day,
                                                         "to": oc.at(7, "10:00")}},
                      {"op": "calendar_click", "name": "u34", "summary": "BENCH U34 meeting", "surname": "U34"},
                      oc.dump_op()])]


def _io007_check(runs, home):
    r = runs[0]
    return [check("the calendar entry was found in the list", r.value("u34_expected_row") is not None
                  and r.value("u34_expected_row") >= 0, "row %r" % r.value("u34_expected_row")),
            check("the entry's contact is opened", r.value("u34_selected") == "U34",
                  "selected %r" % r.value("u34_selected")),
            check("the calendar row is kept", r.value("u34_row") == r.value("u34_expected_row"),
                  "row %r, expected %r" % (r.value("u34_row"), r.value("u34_expected_row"))),
            check("focus stays on the calendar list", r.value("u34_focus") == "listCalendar",
                  "focus %r" % r.value("u34_focus"))]


test("IO-007", "Double-clicking a calendar entry opens its contact, and keeps the calendar row and its focus",
     _io007_build, _io007_check)


# ------------------------------------------------------------------------------------------------ IO-009, IO-012, IO-014: repeats in the list

def _weekly(summary, day, hhmm="09:00", end="10:00", count=None):
    fields = {"summary": summary, "from": oc.at(day, hhmm), "to": oc.at(day, end), "repeat": "2", "repeatinterval": "1"}
    if count is not None:
        fields["repeatcount"] = str(count)
    return {"op": "add_appointment", "fields": fields}


def _io009_build(home):
    return [run_spec([_add("P30C", email="p30c@test.invalid"), _weekly("BENCH P30 weekly", 2),
                      {"op": "calendar_rows", "name": "p30"},
                      {"op": "calendar_click", "name": "p30c", "summary": "BENCH P30 weekly", "surname": "P30C",
                       "occurrence": oc.at(2 + 7 * 3, "09:00")},
                      oc.dump_op()])]


def _io009_check(runs, home):
    r = runs[0]
    stored = [x for x in r.appointments() if x.get("summary") == "BENCH P30 weekly"]
    origin = stored[0].get("id") if stored else ""
    listed = [h for h in (r.value("p30_hints") or []) if str(h).split("@")[0] == origin]
    weeks = _weeks_in_window(2)
    return [check("one row for each week in the repeats window", len(listed) == weeks,
                  "rows %d, expected %d" % (len(listed), weeks)),
            check("the appointment is stored once", len(stored) == 1, "stored %d times" % len(stored)),
            check("selecting a repeat opens its contact", r.value("p30c_selected") == "P30C",
                  "selected %r" % r.value("p30c_selected")),
            check("the repeat is the row that was selected", r.value("p30c_row") == r.value("p30c_expected_row")
                  and r.value("p30c_row") is not None, "row %r" % r.value("p30c_row"))]


test("IO-009", "A weekly appointment is listed on each week in the repeats window, and stored once",
     _io009_build, _io009_check)


def _io012_build(home):
    return [run_spec([_weekly("BENCH W30 weekly", -120, end="10:00"), {"op": "calendar_rows", "name": "w30"},
                      oc.dump_op()])]


def _io012_check(runs, home):
    r = runs[0]
    stored = [x for x in r.appointments() if x.get("summary") == "BENCH W30 weekly"]
    origin = stored[0].get("id") if stored else ""
    hints = r.value("w30_hints") or []
    repeats = [h for h in hints if str(h).split("@")[0] == origin and "@" in str(h)]
    return [check("the source, older than the window, is not listed", origin != "" and origin not in hints, "source listed"),
            check("its repeats in the window are listed", len(repeats) == _weeks_in_window(-120),
                  "repeats %d, expected %d" % (len(repeats), _weeks_in_window(-120))),
            check("the source is stored once", len(stored) == 1, "stored %d times" % len(stored))]


test("IO-012", "A repeat whose source is older than the window is listed, and the source is not",
     _io012_build, _io012_check)


def _io014_build(home):
    return [run_spec([_weekly("BENCH T30 weekly", -3, end="10:00"), {"op": "calendar_rows", "name": "t30"},
                      {"op": "goto_today", "name": "t30"}, oc.dump_op()])]


def _io014_check(runs, home):
    r = runs[0]
    hint = str(r.value("t30_hint") or "")
    rows = r.value("t30_hints") or []
    return [check("the selected row is the repeat four days from now", hint.endswith("@" + oc.at(4, "09:00")),
                  "selected %r" % hint),
            check("Goto Today does not go to the end of the list", r.value("t30_row") != len(rows) - 1,
                  "row %r of %d" % (r.value("t30_row"), len(rows)))]


test("IO-014", "Goto Today selects the first entry that has not ended, including a repeat, rather than the end of the list",
     _io014_build, _io014_check)


# ------------------------------------------------------------------------------------------------ IO-010, IO-015: repeat edits

def _io010_build(home):
    third = oc.at(2 + 7 * 2, "09:00")
    return [run_spec([_weekly("BENCH P31 weekly", 2, end="10:00"),
                      {"op": "choose", "letter": "c"},
                      {"op": "repeat_edit", "name": "p31a", "summary": "BENCH P31 weekly", "occurrence": third,
                       "shiftdays": 0, "newsummary": "BENCH P31 edited"},
                      oc.dump_op(file="d_inplace.json"),
                      {"op": "choose", "letter": "c"},
                      {"op": "repeat_edit", "name": "p31b", "summary": "BENCH P31 edited", "occurrence": third,
                       "shiftdays": 1},
                      oc.dump_op()])]


def _io010_check(runs, home):
    r = runs[0]
    inplace = r.dump("d_inplace.json")
    stored_inplace = [x for x in inplace.get("appointments", []) if x.get("summary") == "BENCH P31 edited"]
    stored = [x for x in r.appointments() if x.get("summary") == "BENCH P31 edited"]
    origin = stored[0].get("id") if stored else ""
    return [check("an edit of a repeat changes the appointment, and it is stored once",
                  len(stored_inplace) == 1, "stored %d times" % len(stored_inplace)),
            check("an edit within the same day keeps the focus on the repeat",
                  str(r.value("p31a_selected_hint") or "").startswith(origin + "@"),
                  "focus %r" % r.value("p31a_selected_hint")),
            check("a move to another day moves the appointment by the same amount",
                  len(stored) == 1 and stored[0].get("from") == oc.at(3, "09:00"),
                  "from %r" % (stored[0].get("from") if stored else None)),
            check("a move to another day puts the focus on the appointment",
                  r.value("p31b_selected_hint") == origin, "focus %r, expected %r" % (r.value("p31b_selected_hint"), origin))]


test("IO-010", "An edit of a repeat changes its appointment; a move to another day moves the appointment and the focus with it",
     _io010_build, _io010_check)


def _io015_build(home):
    return [run_spec([_weekly("BENCH P33 weekly", 2, end="10:00"), {"op": "calendar_rows", "name": "p33a"},
                      {"op": "choose", "letter": "c"},
                      {"op": "repeat_edit", "name": "p33", "summary": "BENCH P33 weekly", "occurrence": oc.at(2 + 7 * 15, "09:00"),
                       "shiftdays": 0, "count": "2"},
                      oc.dump_op()])]


def _io015_check(runs, home):
    r = runs[0]
    stored = [x for x in r.appointments() if x.get("summary") == "BENCH P33 weekly"]
    origin = stored[0].get("id") if stored else ""
    listed = str(r.value("p33_listed") or "")
    row, rows = r.value("p33_row"), r.value("p33_rows")
    return [check("the list has the two remaining entries and the Add row", rows == 3, "rows %r, expected 3" % rows),
            check("the focus is on an entry of this repeat", row is not None and row >= 0 and origin != ""
                  and listed.split("@")[0] == origin, "row %r, listed %r" % (row, listed)),
            check("the focus is on the nearest remaining entry to the one edited", listed.endswith("@" + oc.at(2 + 7, "09:00")),
                  "listed %r" % listed)]


test("IO-015", "Shortening a repeat from a later entry keeps the focus on the nearest entry of that repeat",
     _io015_build, _io015_check)


# ------------------------------------------------------------------------------------------------ IO-016: OK on an edit

def _io016_build(home):
    return [run_spec([{"op": "add_appointment", "fields": {"summary": "BENCH Q34 repeat", "from": oc.at(-11, "10:00"),
                                                         "to": oc.at(-11, "11:00"), "repeat": "2", "repeatinterval": "3",
                                                         "repeatcount": "32"}},
                      {"op": "edit_ok", "name": "q34", "summary": "BENCH Q34 repeat", "occurrence": oc.at(-11, "10:00")},
                      oc.dump_op()])]


def _io016_check(runs, home):
    r = runs[0]
    dialogs = [t for t in r.dialog_titles() if "Login" not in t]
    return [check("the edit path returns", r.value("q34_returned") is True, "returned %r" % r.value("q34_returned")),
            check("no dialog is shown when OK is pressed", not dialogs, "dialogs: %r" % dialogs[:3])]


test("IO-016", "OK on the edit of a repeat that started 11 days ago closes the form, with no dialog",
     _io016_build, _io016_check)


# ------------------------------------------------------------------------------------------------ IO-018, IO-019, IO-020: local files

def _calendar_files(home):
    return glob.glob(os.path.join(oc.data_folder(home), "calendar", "*"))


def _io018_build(home):
    moved = {}

    def move_into_archive(h):
        folder = os.path.join(oc.data_folder(h), "calendar")
        archive = os.path.join(folder, "archive")
        os.makedirs(archive, exist_ok=True)
        for path in glob.glob(os.path.join(folder, "*")):
            if os.path.isfile(path) and b"BENCH L03" in open(path, "rb").read():
                os.rename(path, os.path.join(archive, os.path.basename(path)))
                moved["name"] = os.path.basename(path)
    return [run_spec([{"op": "add_appointment", "fields": {"summary": "BENCH L03", "from": oc.at(3, "09:00"),
                                                         "to": oc.at(3, "10:00")}}, oc.dump_op()]),
            run_spec([oc.dump_op("dump.json")], before=move_into_archive)], moved


def _io018_build_wrapper(home):
    specs, moved = _io018_build(home)
    _io018_moved.clear()
    _io018_moved.update(moved)
    return specs


_io018_moved = {}


def _io018_check(runs, home):
    archive = os.path.join(oc.data_folder(home), "calendar", "archive")
    listed = os.listdir(archive) if os.path.isdir(archive) else []
    return [check("the appointment file was saved, then moved to the archive", "name" in _io018_moved,
                  "no saved file was found to move"),
            check("the archived appointment is not loaded at start-up",
                  not [x for x in runs[1].appointments() if x.get("summary") == "BENCH L03"],
                  "loaded at start-up"),
            check("start-up leaves the archived file in place", _io018_moved.get("name") in listed,
                  "archive holds %s" % sorted(listed))]


test("IO-018", "An appointment file moved into the archive is not loaded at start-up, and start-up leaves it in the archive",
     _io018_build_wrapper, _io018_check)


test("IO-019", "A start with no to-do or history file opens and writes its data",
     lambda home: [run_spec([oc.dump_op()])],
     lambda runs, home: [check("the app starts and dumps its data", "contacts" in runs[0].dump(), "no dump")])


def _notes_build(todo, history):
    def build(home):
        return [run_spec([_add("L05", email="l05@test.invalid"),
                          {"op": "contact_notes", "match": {"surname": "L05"}, "todo": todo, "history": history},
                          {"op": "save"}, oc.dump_op()]),
                run_spec([oc.dump_op()])]
    return build


def _notes_check(todo, history):
    def check_fn(runs, home):
        folder = _home_contacts_dir(home)
        names = os.listdir(folder) if os.path.isdir(folder) else []
        todo_files = [n for n in names if n.endswith(".todolist")]
        history_files = [n for n in names if n.endswith(".history")]
        text = "".join(open(os.path.join(folder, n), "rb").read().decode("latin-1") for n in todo_files + history_files)
        back = _live(runs[1], "L05")
        return [check("the .todolist file is written", len(todo_files) >= 1, "files %r" % names),
                check("the .history file is written", len(history_files) >= 1, "files %r" % names),
                check("the to-do is in plain text", todo in text, "not found"),
                check("the history is in plain text", history in text, "not found"),
                check("after a restart, the contact is read back", len(back) == 1, "found %d" % len(back))]
    return check_fn


test("IO-020", "Notes in plain text: a contact's to-do list and history are saved as plain files, and a restart reads them back",
     _notes_build("L05 plain to-do", "L05 plain history"), _notes_check("L05 plain to-do", "L05 plain history"))


# ------------------------------------------------------------------------------------------------ IO-011: birthdays

def _io011_dates():
    # birthdays repeat each year, so two dates a few days ahead of today show in the calendar window
    today = dt.date.today()
    first = today + dt.timedelta(days=3)
    second = today + dt.timedelta(days=6)
    return first.replace(year=1980).isoformat(), second.replace(year=1980).isoformat()


def _io011_build(home):
    first, second = _io011_dates()
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.setting_op("showbirthdays", "yes"), oc.open_op(1, "o"), oc.hold_op(),
                      oc.form_set_op({"birthday": first}), oc.form_save_op("saved1"),
                      oc.calendar_rows_op("cal1"),
                      oc.form_set_op({"birthday": second}), oc.form_save_op("saved2"),
                      oc.calendar_rows_op("cal2"), oc.hold_op(False), oc.dump_op()])]


def _io011_check(runs, home):
    r = runs[1]
    first, second = _io011_dates()
    def shown(d):                                   # the calendar's own text for this year's birthday: "Mon 09 Nov 2026 All Day: ..."
        b = dt.date.fromisoformat(d)
        this_year = dt.date(dt.date.today().year, b.month, b.day)
        return this_year.strftime("%a %d %b %Y") + " All Day: Jack O'Neil - Birthday"
    t1 = " | ".join(r.value("cal1_titles") or [])
    t2 = " | ".join(r.value("cal2_titles") or [])
    return [check("the first birthday is saved", r.value("saved1_saved") is True, "saved %r" % r.value("saved1_saved")),
            check("the new date is shown in the calendar", shown(first) in t1, "titles %r" % t1[:200]),
            check("the second date is shown after the change", shown(second) in t2, "titles %r" % t2[:200]),
            check("and the old date is gone after the change", shown(first) not in t2, "titles %r" % t2[:200])]


test("IO-011", "A birthday saved on a contact shows in the calendar straight away, with birthdays shown",
     _io011_build, _io011_check)
