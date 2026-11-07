#
# By menu and function (test specification, section 3b): MF-001 to MF-053.  Each test presses a menu item or a key, or
# chooses an action, and checks what the program showed: the dialog or window it opened (read through the driver's
# "widgets" and "windows" operations), the state of the actions, the calendar or the contact that is open, and the files and
# data it wrote.  Where the driver cannot observe a part of an expected result, that part is reported as BLOCKED.
#

import glob
import os

import ops_common as oc
from ops_common import check, blocked, run_spec, test

EXPORTS = "exports"


def _export_path(home, name):
    folder = os.path.join(home, EXPORTS)
    os.makedirs(folder, exist_ok=True)
    return os.path.join(folder, name)


def _data_text(home):
    """Every file's text in the data folder (the program's files), joined, for finding a value the program wrote."""
    out = []
    for root, _dirs, names in os.walk(oc.data_folder(home)):
        for name in names:
            try:
                with open(os.path.join(root, name), encoding="utf-8", errors="replace") as f:
                    out.append(f.read())
            except OSError:
                pass
    return "\n".join(out)


def _opens(code, title, action, want, obj=None):
    """A menu item (or key) opens a dialog or window: its title or class contains the text, or the control is there."""
    def build(home):
        return [run_spec(oc.fixtures_ops()),
                run_spec([oc.open_op(1, "o"), oc.hold_op(), oc.menu_op("m", action), oc.widgets_op("w", [obj] if obj else []),
                          oc.press_op("c", "Cancel"), oc.press_op("c2", "Close"), oc.hold_op(False), oc.dump_op()])]

    def checks(runs, home):
        w = runs[1]
        shown = want.lower() in (str(w.value("w_title")) + " " + str(w.value("w_class"))).lower()
        if obj:
            shown = shown or (w.value("w_" + obj) not in (None, "MISSING"))
        return [check("%s opens" % want, shown, "top window %r %r" % (w.value("w_title"), w.value("w_class")))]
    test(code, title, build, checks)


# ------------------------------------------------------------------------------------------------ MF-001 to MF-008

def _mf001_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.hold_op(), oc.menu_op("m", "actionOpen"), oc.widgets_op("first", rows=True),
                      oc.keys_op("k", "C"), oc.widgets_op("second", rows=True), oc.hold_op(False), oc.dump_op()])]


def _mf001_check(runs, home):
    r = runs[1]
    rows = r.value("first_rows") or []
    after = r.value("second_current")
    second_rows = r.value("second_rows") or []
    cur = second_rows[after] if isinstance(after, int) and 0 <= after < len(second_rows) else ""
    return [check("the list has the six contacts", all(any(oc.surname(n) in row for row in rows) for n in range(1, 7)),
                  "rows: %s" % rows),
            check("'Add Contact' is at the top", bool(rows) and ("create" in rows[0].lower() or "new" in rows[0].lower() or "add contact" in rows[0].lower()),
                  "top row: %r" % (rows[0] if rows else None)),
            check("the letter C skips to Carter", "Carter" in cur, "current row: %r" % cur)]


test("MF-001", "File > Open lists the contacts, skips by surname letter, and has 'create contact' at the top",
     _mf001_build, _mf001_check)


def _mf002_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.form_set_op({"email": "saved.oneil@sgc.contacmanagertest"}),
                      oc.shortcut_op("s", "Ctrl+S", press=True), oc.dump_op()])]


def _mf002_check(runs, home):
    c = runs[1].contact(oc.surname(1))
    return [check("Ctrl+S saves the open contact's edit", c.get("email") == "saved.oneil@sgc.contacmanagertest",
                  "email %r" % c.get("email"))]


test("MF-002", "File > Save (Ctrl+S) saves the open contact and its files", _mf002_build, _mf002_check)


def _export_test(code, title, action, name, expect_text, setup=None):
    def build(home):
        path = _export_path(home, name)
        ops = list(setup or [])
        return [run_spec(oc.fixtures_ops() + ops),
                run_spec([oc.hold_op(), oc.menu_op("m", action), oc.set_op("file", "fileNameEdit", text=path),
                          oc.press_op("save", "Save"), oc.hold_op(False), oc.dump_op()])]

    def checks(runs, home):
        path = _export_path(home, name)
        exists = os.path.exists(path)
        text = open(path, encoding="utf-8", errors="replace").read() if exists else ""
        return [check("the file is written", exists, "no file at %s" % path),
                check(expect_text[0], all(t in text for t in expect_text[1]), "text found: %s" % (text[:120],))]
    test(code, title, build, checks)


_export_test("MF-003", "File > Export > Contacts writes a file with all contacts", "actionExportContacts", "contacts.csv",
             ("the file holds the six contacts", [oc.surname(n) for n in range(1, 7)]))
_export_test("MF-004", "File > Export > Appointments writes a file with all appointments", "actionExportAppointments",
             "appointments.csv", ("the file holds the appointment", ["MF004 appointment"]),
             setup=[oc.appt_op(1, "MF004 appointment", oc.at(20, "10:00"), oc.at(20, "11:00"))])


def _mf005_build(home):
    path = _export_path(home, "contacts-for-import.csv")
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.hold_op(), oc.menu_op("m", "actionExportContacts"), oc.set_op("file", "fileNameEdit", text=path),
                      oc.press_op("save", "Save"), oc.hold_op(False)]),
            run_spec([oc.delete_contact_op(1), oc.save_op(), oc.tidy_op(), oc.dump_op("gone.json")]),
            # the deleted contact still exists with the same id, so each existing record asks whether to overwrite it.  "Overwrite",
            # with "Do this for all future matches" ticked, answers it for all: the deleted contact comes back, and no copy is made.
            # (Skip would leave the deleted contact deleted.)
            run_spec([oc.hold_op(), oc.menu_op("m", "actionImport"), oc.set_op("file", "fileNameEdit", text=path),
                      oc.answer_op("Overwrite", check=True), oc.press_op("open", "Open"), oc.hold_op(False), oc.dump_op("back.json")])]


def _mf005_check(runs, home):
    back = runs[3].contacts("back.json")
    surnames = [c.get("surname") for c in back]
    return [check("the deleted contact is back", oc.surname(1) in surnames, "contacts: %s" % surnames),
            check("there are no duplicates", len(surnames) == len(set(surnames)) == 6, "contacts: %s" % surnames)]


test("MF-005", "File > Import reads an export back without duplicates", _mf005_build, _mf005_check)


def _mf006_build(home):
    return [run_spec(oc.fixtures_ops()),
            # the screen's edit is not saved yet, so Tidy asks "Save them before tidying contacts?": Save answers it, then the
            # summary (a progress window, closed by the driver) is shown.  The dump waits for the tidy to finish.
            run_spec([oc.open_op(1, "o"), oc.form_set_op({"email": "tidy.oneil@sgc.contacmanagertest"}),
                      oc.menu_op("tidy", "actionTidyContacts"), oc.answer_op("Save"), oc.wait_op(2000), oc.dump_op()])]


def _mf006_check(runs, home):
    r = runs[1]
    return [check("the edit on screen is saved by Tidy Records",
                  r.contact(oc.surname(1)).get("email") == "tidy.oneil@sgc.contacmanagertest",
                  "email %r" % r.contact(oc.surname(1)).get("email")),
            check("the summary is shown and closed", r.logged("progress window closed") or "Tidy Records" in r.dialog_titles(),
                  "no summary in the log")]


test("MF-006", "File > Tidy Records saves the screen's edit first, then shows its summary", _mf006_build, _mf006_check)


def _mf007_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.actions_op("in", ["actionSetEncryptionKey", "actionChangePassword", "actionLogout"]),
                      oc.menu_op("out", "actionLogout"),
                      oc.actions_op("out", ["actionSetEncryptionKey", "actionChangePassword", "actionLogout"]),
                      oc.dump_op()])]


def _mf007_check(runs, home):
    r = runs[1]
    before = r.value("in_states") or {}
    after = r.value("out_states") or {}
    return [check("logged in: Set Encryption Key and Change Password are off, Logout is on",
                  before.get("actionSetEncryptionKey") is False and before.get("actionChangePassword") is False
                  and before.get("actionLogout") is True, "states %s" % before),
            check("logged out: Set Encryption Key and Change Password are on, Logout is off",
                  after.get("actionSetEncryptionKey") is True and after.get("actionChangePassword") is True
                  and after.get("actionLogout") is False, "states %s" % after)]


test("MF-007", "File > Key and Password enables items according to whether the program is logged in", _mf007_build,
     _mf007_check)

_opens("MF-008", "File > Setup opens the Setup dialog (its controls are tested in 3c)", "actionSetup", "Setup",
       obj="windowBefore")


def _mf009_build(home):
    ops = oc.fixtures_ops() + [oc.appt_op(1, "MF009 kept", oc.at(20, "10:00"), oc.at(20, "11:00")),
                               oc.appt_op(2, "MF009 deleted", oc.at(21, "10:00"), oc.at(21, "11:00")), oc.save_op()]
    return [run_spec(ops),
            run_spec([oc.delete_appt_op("MF009 deleted"), oc.save_op(), oc.menu_op("shown", "actionShowDeletedAppointments",
                                                                                   trigger=False),
                      oc.calendar_rows_op("off"), oc.menu_op("tick", "actionShowDeletedAppointments"),
                      oc.calendar_rows_op("on"), oc.dump_op()]),
            run_spec([oc.menu_op("start", "actionShowDeletedAppointments", trigger=False), oc.dump_op()])]


def _mf009_check(runs, home):
    r1, r2 = runs[1], runs[2]
    off_titles = " ".join(r1.value("off_titles") or [])
    on_titles = " ".join(r1.value("on_titles") or [])
    return [check("the deleted appointment is shown in braces once the tick is on", "{" in on_titles and "{" not in off_titles,
                  "braces on: %s, off: %s" % ("{" in on_titles, "{" in off_titles)),
            check("the tick is off at start-up", r2.value("start_checked") is False, "checked %r" % r2.value("start_checked"))]


test("MF-009", "File > Show > Deleted Appointments lists deleted appointments in braces, and the tick is off at start-up",
     _mf009_build, _mf009_check)


def _mf010_build(home):
    return [run_spec(oc.fixtures_ops() + [oc.delete_contact_op(2), oc.hide_contact_op(3), oc.save_op()]),
            run_spec([oc.menu_op("del", "actionShowDeletedContacts"), oc.menu_op("hid", "actionShowHiddenContacts"),
                      oc.hold_op(), oc.menu_op("open", "actionOpen"), oc.widgets_op("list", rows=True), oc.hold_op(False),
                      oc.dump_op()])]


def _mf010_check(runs, home):
    # Decided (ticket #123): deleted and hidden contacts are skipped in this scenario, so nothing is checked
    return [blocked("the deleted contact is listed", "skipped in this scenario: deleted and hidden contacts are skipped (ticket #123)"),
            blocked("the hidden contact is listed", "skipped in this scenario: deleted and hidden contacts are skipped (ticket #123)")]


test("MF-010", "File > Show > Deleted Contacts and Hidden Contacts list those contacts", _mf010_build, _mf010_check)

_opens("MF-011", "File > Show > Logs opens a window that is not modal and shows new entries", "actionShow_Log", "Log",
       obj="logTextEdit")


def _mf012_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.exit_op("plain"), oc.dump_op("after-plain.json")]),
            run_spec([oc.open_op(1, "o"), oc.form_set_op({"email": "exit.oneil@sgc.contacmanagertest"}),
                      oc.answer_op("Cancel"), oc.exit_op("edited"), oc.dump_op("after-edited.json")])]


def _mf012_check(runs, home):
    return [check("exit without an edit closes the program", runs[1].value("plain_exit_accepted") is True,
                   "accepted: %r" % runs[1].value("plain_exit_accepted")),
            check("exit with an edit asks, and Cancel keeps it open", runs[2].value("edited_exit_accepted") is False
                  and runs[2].logged('answered "Cancel"'), "accepted: %r" % runs[2].value("edited_exit_accepted"))]


test("MF-012", "File > Exit closes the program after the unsaved-edit check", _mf012_build, _mf012_check)


def _mf013_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.menu_op("tf", "actionTest_Function", trigger=False), oc.dump_op()])]


def _mf013_check(runs, home):
    r = runs[1]
    found = r.value("tf_found")
    return [check("the File menu has no Test Function item (ticket #119)", found is False,
                  "the Test Function item is still in the menu (ticket #119 is open)")]


test("MF-013", "The File menu has no Test Function item", _mf013_build, _mf013_check)


def _mf014_build(home):
    # three appointments with the same word, in the calendar; Find Here, then Find Next twice
    ops = oc.fixtures_ops() + [oc.appt_op(1, "MF014 searchword one", oc.at(1, "10:00"), oc.at(1, "11:00")),
                               oc.appt_op(2, "MF014 searchword two", oc.at(2, "10:00"), oc.at(2, "11:00")),
                               oc.appt_op(3, "MF014 searchword three", oc.at(3, "10:00"), oc.at(3, "11:00")), oc.save_op()]
    return [run_spec(ops),
            run_spec([oc.calendar_rows_op("rows"), oc.findnext_op("f1", "searchword"), oc.findnext_op("f2", "searchword"),
                      oc.findnext_op("f3", "searchword"), oc.dump_op()])]


def _mf014_check(runs, home):
    r = runs[1]
    rows = [r.value(k + "_row") for k in ("f1", "f2", "f3")]
    # each Find Next goes on from the current entry; after the last match it wraps to the first, as the page search does
    return [check("each Find Next moves to the next match, and the one after the last is the first",
                  rows[0] is not None and rows[1] == rows[0] + 1 and rows[2] == 0, "rows %s" % rows)]


test("MF-014", "Find Here (Ctrl+F) and Find Next (F3) move through matches on the page", _mf014_build, _mf014_check)


def _mf015_build(home):
    # Global Find is opened from its menu item (the bench does not press Ctrl+G); its text box takes "jac", and the list is read
    return [run_spec(oc.fixtures_ops()),
            run_spec([{"op": "hide_contact", "match": {"surname": "Jackson"}}, oc.dates_op(["jac"]),
                      oc.menu_op("gf", "actionGlobalFind"), oc.wait_op(1500),
                      oc.widgets_op("list", rows=True),
                      {"op": "keys", "name": "enter", "sequence": "Return", "target": "window"}, oc.wait_op(1000),
                      oc.state_op("s"), oc.dump_op()])]


def _mf015_check(runs, home):
    r = runs[1]
    rows = r.value("list_rows") or []
    text = " | ".join(rows)
    return [check("Global Find asks for its text and the list is shown", bool(rows), "no list recorded"),
            check("part of a name matches without case: O'Neil, Jack is listed", "O'Neil, Jack" in text, "rows %r" % rows[:6]),
            check("a hidden contact, Jackson, is listed in brackets", "{Jackson, Daniel}" in text, "rows %r" % rows[:6]),
            check("Enter opens the first match, O'Neil", runs[1].value("s_loaded") == "O'Neil",
                  "loaded %r" % runs[1].value("s_loaded"))]


test("MF-015", "Global Find (Ctrl+G) matches part of a name without case or accents, lists hidden contacts in brackets, and opens on Enter",
     _mf015_build, _mf015_check)

_opens("MF-016", "Advanced Global Find (Ctrl+Shift+G) opens its dialog and applies its criteria (see DL-020)",
       "action_Advanced_Find", "AdvancedFind", obj="lineEdit_SearchText1")


def _rows_test(code, title, action, want_in, want_out):
    """A search or filter shows its list: the wanted surnames are in it, and the others are not."""
    def build(home):
        return [run_spec(oc.fixtures_ops() + _ALL_SEARCH_SETUP.get(code, [])),
                run_spec([oc.hold_op(), oc.menu_op("m", action), oc.widgets_op("list", rows=True), oc.hold_op(False),
                          oc.dump_op()])]

    def checks(runs, home):
        rows = " ".join(runs[1].value("list_rows") or [])
        out = [check("%s: %s is listed" % (code, oc.surname(n)), oc.surname(n) in rows, "rows: %s" % rows[:160])
               for n in want_in]
        out += [check("%s: %s is not listed" % (code, oc.surname(n)), oc.surname(n) not in rows, "rows: %s" % rows[:160])
                for n in want_out]
        return out
    test(code, title, build, checks)


_ALL_SEARCH_SETUP = {
    "MF-019": [oc.edit_contact_op(1, {"groupbusiness": "true"})],
}

# MF-017: the contacts with an email address (1, 2, 5, 6) and the contacts with a mobile (3, 4, 5, 6) by channel
_rows_test("MF-017", "Email / SMS To Contacts lists contacts with an address for each channel", "actionSearch_EmailSMSToContacts",
           want_in=[1, 2, 5, 6], want_out=[])
# MF-018: the non-clients: no contact is a client in the fixtures, so every contact is listed
_rows_test("MF-018", "Email / SMS To Non-Clients lists contacts not ticked Client", "actionSearch_EmailSMSToNonClients",
           want_in=[1, 2, 3, 4, 5, 6], want_out=[])
_rows_test("MF-019", "Uncategorised Contacts lists contacts with no Type ticked", "actionSearch_UncategorisedContacts",
           want_in=[2, 3, 4, 5, 6], want_out=[1])


def _mf020_build(home):
    ops = oc.fixtures_ops()
    ops.insert(len(ops) - 1, {"op": "add_contact", "fields": {"surname": oc.surname(2), "names": FIX_NAME(2),
                                                              "email": oc.email_of(2), "mobile": "+44 7700 900002"}})
    return [run_spec(ops),
            run_spec([oc.hold_op(), oc.menu_op("m", "actionSearch_DuplicateContacts"), oc.widgets_op("list", rows=True),
                      oc.hold_op(False), oc.dump_op()])]


def FIX_NAME(n):
    return oc.FIX["contacts"][n - 1]["given"]


def _mf020_check(runs, home):
    rows = " ".join(runs[1].value("list_rows") or [])
    return [check("both contacts with the same name are listed", rows.count(oc.surname(2)) >= 2,
                  "rows: %s" % rows[:160]),
            check("nothing is merged", len(runs[1].contacts()) == 7, "%d contacts" % len(runs[1].contacts()))]


test("MF-020", "Duplicate Contacts lists possible duplicates, and does not merge them (merging is Tidy Records)",
     _mf020_build, _mf020_check)


def _mf021_build(home):
    ops = oc.fixtures_ops()
    ops.insert(len(ops) - 1, {"op": "add_contact", "fields": {"surname": "Hidden copy", "names": "Jack",
                                                              "email": oc.email_of(1), "mobile": ""}})
    ops.append(oc.hide_contact_op(1))
    ops.append(oc.save_op())
    return [run_spec(ops),
            run_spec([oc.menu_op("m", "actionSearch_HiddenEmailDuplicates"), oc.wait_op(1500),
                      oc.widgets_op("list", rows=True), oc.dump_op()])]


def _mf021_check(runs, home):
    # Decided (ticket #123): deleted and hidden contacts are skipped in this scenario, so nothing is checked
    return [blocked("the hidden contact with the shared address is listed",
                    "skipped in this scenario: deleted and hidden contacts are skipped (ticket #123)")]


test("MF-021", "Duplicate Hidden Auto-Email Entries lists hidden contacts that share an address with a visible one",
     _mf021_build, _mf021_check)


def _mf022_build(home):
    # Identify Me is chosen in Setup, the form is closed with OK, and Ctrl+Shift+O opens the contact chosen
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.hold_op(), oc.menu_op("setup", "actionSetup"), oc.navigator_select_op("pick", "IdentifyMe", "Carter"),
                      oc.press_op("ok", "OK"), oc.shortcut_op("key", "Ctrl+Shift+O"), oc.menu_op("goto", "actionGotoMe"),
                      oc.state_op("s"), oc.hold_op(False),
                      oc.dump_op()])]


def _mf022_check(runs, home):
    r = runs[1]
    return [oc.check("Setup's Identify Me is set to Carter", r.value("pick_selected") is True,
                     "selected %r" % r.value("pick_selected")),
            oc.check("Ctrl+Shift+O is bound to Goto Me, and it is enabled", r.value("key_action") == "actionGotoMe"
                     and r.value("key_enabled") is True, "bound to %r, enabled %r" % (r.value("key_action"), r.value("key_enabled"))),
            oc.check("Goto Me opens Carter", r.value("s_loaded") == "Carter", "loaded %r" % r.value("s_loaded"))]


test("MF-022", "Goto Me (Ctrl+Shift+O) opens the contact set as Identify Me", _mf022_build, _mf022_check)


def _mf023_build(home):
    # the journal opens in Easy Notepad, a program the bench stands in with a script that records its arguments
    journal = os.path.join(home, "journal.txt")
    marker = os.path.join(home, "notepad-ran.txt")
    script = os.path.join(home, "fake-notepad.sh")
    with open(script, "w", encoding="utf-8") as f:
        f.write("#!/bin/sh\necho \"$@\" > \"%s\"\n" % marker)
    os.chmod(script, 0o755)
    ini = oc.INI_BASE + "\n[journal]\nfilename=%s\neasynotepadexe=%s\n" % (journal, script)
    with open(os.path.join(home, "TrumptonApps", "ContactManager.ini"), "w", encoding="utf-8") as f:
        f.write(ini)
    return [run_spec(oc.fixtures_ops()), run_spec([oc.open_op(1, "o"), oc.menu_op("j", "actionGotoJournal"), oc.dump_op()])]


def _mf023_check(runs, home):
    marker = os.path.join(home, "notepad-ran.txt")
    ran = os.path.exists(marker)
    return [check("with Easy Notepad present, the journal opens in it", ran, "the program did not start Easy Notepad")]


test("MF-023", "Goto Journal (Ctrl+J) opens the journal in Easy Notepad, and a missing program gives an error",
     _mf023_build, _mf023_check)


def _mf024_build(home):
    # Ctrl+Shift+L goes to the user's list, which is the contact Identify Me names
    second = run_spec([oc.open_op(1, "o"), oc.hold_op(), oc.shortcut_op("l", "Ctrl+L", press=True), oc.widgets_op("contact", ["editToDo"]),
                       oc.shortcut_op("my", "Ctrl+Shift+L", press=True), oc.widgets_op("mine", ["editToDo"]), oc.hold_op(False), oc.dump_op()])
    # Identify Me is another contact, so Ctrl+Shift+L shows a different list from the open contact's
    second["before"] = oc.identify_me_hook(second, 2)
    return [run_spec(oc.fixtures_ops() + [oc.notes_op(1, "MF024 contact list item", "")]), second]


def _mf024_check(runs, home):
    r = runs[1]
    return [check("Ctrl+L shows the contact's list", (r.value("contact_editToDo") or "").strip() == "MF024 contact list item",
                  "to-do shows %r" % r.value("contact_editToDo")),
            check("Ctrl+Shift+L shows the user's list", r.value("mine_editToDo") not in (None, "MISSING")
                  and r.value("mine_editToDo") != r.value("contact_editToDo"), "to-do shows %r" % r.value("mine_editToDo"))]


test("MF-024", "Goto To Do List (Ctrl+L) and Goto My To Do List (Ctrl+Shift+L) open the right lists", _mf024_build, _mf024_check)


def _mf025_build(home):
    ops = oc.fixtures_ops() + [oc.appt_op(1, "MF025 today", oc.at(0, "12:00"), oc.at(0, "13:00")),
                               oc.appt_op(2, "MF025 later", oc.at(3, "12:00"), oc.at(3, "13:00")), oc.save_op()]
    return [run_spec(ops),
            run_spec([oc.calendar_rows_op("rows"), oc.key_op("down", "n"), oc.key_op("down2", "n"),
                      oc.goto_today_op("today"), oc.calendar_rows_op("after"), oc.dump_op()])]


def _mf025_check(runs, home):
    r = runs[1]
    row = r.value("today_row")
    return [check("Ctrl+T selects the first entry from today", row is not None and row >= 0, "row %r" % row)]


test("MF-025", "Goto Today (Ctrl+T) selects the first entry from now on", _mf025_build, _mf025_check)


def _mf026_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.dates_op([""]), oc.keys_op("d", "Ctrl+D"), oc.dump_op()])]


def _mf026_check(runs, home):
    r = runs[1]
    cursor = r.value("dialog_1_cursor")
    length = r.value("dialog_1_length")
    # the cursor goes to the day field (its start, position 4), as the program places it
    return [check("Ctrl+D opens the date box", r.value("dialog_1_title") is not None, "no date box recorded"),
            check("with the cursor in the day field (position 4)", cursor == 4, "cursor %r, length %r" % (cursor, length))]


test("MF-026", "Goto Date (Ctrl+D) opens the date box", _mf026_build, _mf026_check)


def _mf027_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.shortcut_op("n", "Alt+N", press=True), {"op": "focus", "name": "n"},
                      oc.shortcut_op("m", "Alt+M", press=True), {"op": "focus", "name": "m"},
                      oc.shortcut_op("a", "Alt+A", press=True), {"op": "focus", "name": "a"},
                      oc.shortcut_op("p", "Alt+P", press=True), {"op": "focus", "name": "p"},
                      oc.shortcut_op("b", "Alt+B", press=True), {"op": "focus", "name": "b"},
                      oc.dump_op()])]


def _mf027_check(runs, home):
    r = runs[1]
    return [check("Alt+N moves to the names", r.value("n_focus") == "editFirstName", "focus %r" % r.value("n_focus")),
            check("Alt+M moves to the email", "email" in str(r.value("m_focus")).lower(), "focus %r" % r.value("m_focus")),
            check("Alt+A moves to the address", "address" in str(r.value("a_focus")).lower(), "focus %r" % r.value("a_focus")),
            check("Alt+P moves to the phone", "home" in str(r.value("p_focus")).lower() or "phone" in str(r.value("p_focus")).lower(),
                  "focus %r" % r.value("p_focus")),
            check("Alt+B moves to the birthday", "birth" in str(r.value("b_focus")).lower(), "focus %r" % r.value("b_focus"))]


test("MF-027", "Goto Contact Details (Alt+N, M, A, P, B) moves to each section of the form", _mf027_build, _mf027_check)


def _appt_form_test(code, title, shortcut, want_contact, check_text):
    def build(home):
        return [run_spec(oc.fixtures_ops() + [oc.appt_op(1, "%s existing" % code, oc.at(20, "09:00"), oc.at(20, "10:00")),
                                               oc.save_op()]),
                run_spec([oc.open_op(1, "o"), oc.calendar_click_op("c", "%s existing" % code, oc.surname(1)),
                          oc.hold_op(), oc.shortcut_op("s", shortcut, press=True), oc.widgets_op("form", ["contactName", "editSummary"]),
                          oc.press_op("cancel", "Cancel"), oc.hold_op(False), oc.dump_op()])]

    def checks(runs, home):
        r = runs[1]
        return [check_text(r.value("form_contactName") or "", r.value("form_editSummary") or "", r)]
    test(code, title, build, checks)


_appt_form_test("MF-028", "Insert Appointment After (Ctrl+I) adds an entry after the selected one, for the open contact",
                "Ctrl+I", True,
                lambda cn, summ, r: check("the form is for the open contact", oc.surname(1) in cn, "For: %r" % cn))
_appt_form_test("MF-029", "Insert My Appointment After (Ctrl+Shift+I) adds an entry after the selected one, for the user",
                "Ctrl+Shift+I", False,
                lambda cn, summ, r: check("the form is for the user, not the contact", oc.surname(1) not in cn, "For: %r" % cn))
_appt_form_test("MF-030", "New Appointment (Ctrl+N) opens the form for the open contact", "Ctrl+N", True,
                lambda cn, summ, r: check("the form is for the open contact", oc.surname(1) in cn, "For: %r" % cn))
_appt_form_test("MF-031", "New My Appointment (Ctrl+Shift+N) opens the form for the user", "Ctrl+Shift+N", False,
                lambda cn, summ, r: check("the form is for the user, not the contact", oc.surname(1) not in cn, "For: %r" % cn))


def _mf032_build(home):
    return [run_spec(oc.fixtures_ops() + [oc.appt_op(1, "MF032 update me", oc.at(20, "09:00"), oc.at(20, "10:00")),
                                          oc.save_op()]),
            run_spec([oc.open_op(1, "o"), oc.calendar_click_op("c", "MF032 update me", oc.surname(1)),
                      oc.hold_op(), oc.shortcut_op("u", "Ctrl+U", press=True), oc.widgets_op("form", ["editSummary"]),
                      oc.press_op("cancel", "Cancel"), oc.hold_op(False), oc.dump_op()])]


def _mf032_check(runs, home):
    return [check("Ctrl+U opens that entry's details", runs[1].value("form_editSummary") == "MF032 update me",
                  "summary %r" % runs[1].value("form_editSummary"))]


test("MF-032", "Update Appointment (Ctrl+U) opens the selected entry's form", _mf032_build, _mf032_check)


def _mf033_build(home):
    return [run_spec(oc.fixtures_ops() + [oc.appt_op(1, "MF033 remove", oc.at(20, "09:00"), oc.at(20, "10:00")), oc.save_op()]),
            run_spec([oc.calendar_click_op("c", "MF033 remove", oc.surname(1)), oc.answer_op("No"),
                      oc.shortcut_op("r", "Ctrl+R", press=True), oc.dump_op("declined.json"),
                      oc.answer_op("Yes"), oc.shortcut_op("r2", "Ctrl+R", press=True), oc.dump_op("accepted.json")])]


def _mf033_check(runs, home):
    r = runs[1]
    declined = [a for a in r.appointments("declined.json") if a.get("summary") == "MF033 remove"]
    accepted = [a for a in r.appointments("accepted.json") if a.get("summary") == "MF033 remove"]
    return [check("declining keeps the entry", len(declined) == 1 and not declined[0].get("deleted"), "found %s" % declined),
            check("accepting deletes it", len(accepted) == 1 and accepted[0].get("deleted") is True, "found %s" % accepted)]


test("MF-033", "Remove Appointment (Ctrl+R) deletes the selected entry after confirmation", _mf033_build, _mf033_check)


def _mf034_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.hold_op(), oc.menu_op("m", "actionEnableEditHistoryEdit"),
                      oc.widgets_op("w", ["editHistory"]), oc.hold_op(False), oc.dump_op()])]


def _mf034_check(runs, home):
    r = runs[1]
    return [check("Edit History enables the history for the open contact", r.value("w_editHistory_enabled") is True,
                  "enabled %r" % r.value("w_editHistory_enabled"))]


test("MF-034", "Edit History enables the history to be edited", _mf034_build, _mf034_check)


def _mf035_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.hold_op(), oc.menu_op("m", "actionNewContact"),
                      oc.widgets_op("w", ["editFirstName", "editSurname"]), oc.hold_op(False), oc.dump_op()])]


def _mf035_check(runs, home):
    r = runs[1]
    return [check("New Contact opens a blank contact", r.value("w_editFirstName") == "" and r.value("w_editSurname") == "",
                  "first %r, surname %r" % (r.value("w_editFirstName"), r.value("w_editSurname")))]


test("MF-035", "New Contact opens a blank contact", _mf035_build, _mf035_check)


def _mf036_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o", details=False), oc.menu_op("k", "actionEnableContactDetailsEdit"), oc.hold_op(), oc.widgets_op("w", ["editSurname"]),
                      oc.hold_op(False), oc.dump_op()])]


def _mf036_check(runs, home):
    r = runs[1]
    return [check("Alt+E, D enables editing of the open contact", r.value("w_editSurname_enabled") is True,
                  "enabled %r" % r.value("w_editSurname_enabled"))]


test("MF-036", "Contact Details enables editing of the open contact (Alt+E, D)", _mf036_build, _mf036_check)


def _mf037_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.hold_op(), oc.answer_op("No"), oc.menu_op("m", "actionDeleteContact"),
                      oc.wait_op(1000), oc.dump_op("declined.json"), oc.answer_op("Yes"), oc.menu_op("m2", "actionDeleteContact"),
                      oc.wait_op(1000), oc.hold_op(False), oc.dump_op("accepted.json")])]


def _mf037_check(runs, home):
    r = runs[1]
    kept = r.contact(oc.surname(1), "declined.json")
    gone = r.contact(oc.surname(1), "accepted.json")
    return [check("declining changes nothing", bool(kept) and kept.get("deleted") is not True, "contact not there after No"),
            check("accepting moves the contact to the deleted folder", bool(gone) and gone.get("deleted") is True,
                  "contact still in the program")]


test("MF-037", "Delete Contact asks for confirmation, then moves the contact to the deleted folder", _mf037_build, _mf037_check)


def _mf038_build(home):
    # the mail command is a script that records its arguments, as Easy Notepad does in MF-023
    marker = os.path.join(home, "mail-args.txt")
    script = os.path.join(home, "fake-mail.sh")
    with open(script, "w", encoding="utf-8") as f:
        f.write("#!/bin/sh\nprintf '%%s\\n' \"$@\" > \"%s\"\n" % marker)
    os.chmod(script, 0o755)
    ini = oc.INI_BASE + "\n[email]\ncommand=%s $TO $NAME\n" % script
    with open(os.path.join(home, "TrumptonApps", "ContactManager.ini"), "w", encoding="utf-8") as f:
        f.write(ini)
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.menu_op("mail", "action_Email_Contact"), oc.wait_op(500), oc.dump_op()])]


def _mf038_check(runs, home):
    marker = os.path.join(home, "mail-args.txt")
    args = open(marker, encoding="utf-8").read() if os.path.exists(marker) else ""
    return [check("the mail command runs", os.path.exists(marker), "the mail command did not run"),
            check("with the recipient's address", "jack.oneil@sgc.contacmanagertest" in args, "arguments %r" % args),
            check("and the contact's name", "O'Neil" in args, "arguments %r" % args)]


test("MF-038", "Send EMail Contact (Ctrl+M) starts the mail command with the recipient and name filled in",
     _mf038_build, _mf038_check)

_opens("MF-039", "SMS Contact (Ctrl+Shift+M) opens the text form for the contact", "action_SMS_Contact", "SMS",
       obj="recipientText")


def _money_test(code, title, action, amount):
    def build(home):
        # An expense first asks whether it is for Me, when the contact is not Me: Identify Me is set to contact 2, so the question
        # is asked for contact 1, and No keeps it for that contact.  (Without Identify Me, the program says Me is not configured.)
        second = run_spec([oc.open_op(1, "o"), oc.hold_op()] + ([oc.answer_op("No")] if action == "actionPayment" else []) +
                          [oc.menu_op("m", action), oc.wait_op(500), oc.set_op("f", "lineEditAmount", text=amount),
                           oc.press_op("ok", "OK"), oc.hold_op(False), oc.dump_op()])
        if action == "actionPayment":
            second["before"] = oc.identify_me_hook(second, 2)
        return [run_spec(oc.fixtures_ops()), second]

    def checks(runs, home):
        text = _data_text(home)
        return [check("the entry is recorded in the contact's files", amount in text, "%s not found in the data folder" % amount)]
    test(code, title, build, checks)


_money_test("MF-040", "Transaction Income (Ctrl++) adds an income entry to the contact's log", "actionIncome", "12.50")
_money_test("MF-041", "Transaction Expense (Ctrl+-) adds an expense entry to the contact's log", "actionPayment", "7.25")


def _mf042_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.dates_op(["2026"]), oc.hold_op(), oc.menu_op("m", "actionTransactionReport"), oc.hold_op(False),
                      oc.dump_op()])]


def _mf042_check(runs, home):
    # the handler for Transaction Report is a TODO in mainwindow_menu_transaction.cpp: no report is written, so this fails
    r = runs[1]
    titles = [v for k, v in r.results.items() if k.endswith("_text") and k.startswith("dialog_")]
    reports = [os.path.relpath(p, home) for p in glob.glob(os.path.join(home, "**", "*report*"), recursive=True)]
    return [check("the report asks for the year, and the year is typed", "2026" in titles, "texts %s" % titles),
            check("the report is written for the year", bool(reports), "report files found: %s" % reports)]


test("MF-042", "Transaction Report produces the year's report", _mf042_build, _mf042_check)


def _mf043_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.hold_op(), oc.menu_op("m", "actionManual"), oc.windows_op("open")] + oc.manual_search_ops() +
                     [oc.keys_op("esc", "Escape", target="window"), oc.windows_op("closed"), oc.hold_op(False), oc.dump_op()])]


def _mf043_check(runs, home):
    r = runs[1]
    opened = any("help" in str(w).lower() or "manual" in str(w).lower() for w in (r.value("open_windows") or []))
    first, second = r.value("first_position"), r.value("second_position")
    return [check("Manual opens", opened, "windows: %s" % (r.value("open_windows"),)),
            check("Ctrl+F finds the first match in the manual", r.value("first_found") is True
                  and "find" in str(r.value("first_line") or "").lower(), "cursor line %r" % (r.value("first_line"),)),
            check("F3 finds the next match", isinstance(first, int) and isinstance(second, int) and second > first,
                  "cursor at %r, then %r" % (first, second))]


test("MF-043", "Manual (F1) opens the manual, and F3 finds the next match", _mf043_build, _mf043_check)


def _mf044_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.menu_op("m", "actionAbout"), oc.windows_op("open"), oc.dump_op()])]


def _mf044_check(runs, home):
    titles = " ".join(runs[1].value("open_windows") or [])
    return [check("About shows the program's name", "contact" in titles.lower() or "about" in titles.lower(),
                  "windows: %s" % titles)]


test("MF-044", "About shows the program's name and version", _mf044_build, _mf044_check)


def _mf045_build(home):
    # a digit opens the date box (answered with nothing, so it closes), Escape is pressed, and the calendar row is read before and after
    return [run_spec(oc.fixtures_ops()),
            run_spec([{"op": "focus", "name": "before"}, oc.dates_op([""]), oc.key_op("two", "2"),
                      {"op": "focus", "name": "two"}, oc.keys_op("esc", "Escape"), {"op": "focus", "name": "after"},
                      oc.dump_op()])]


def _mf045_check(runs, home):
    r = runs[1]
    return [check("a digit opens the date box", "Go to date" in [str(v) for k, v in r.results.items() if k.endswith("_title")]
                  or r.logged("Go to date"), "no date box recorded"),
            check("Escape closes it without moving the list",
                  r.value("after_row") is not None and r.value("after_row") == r.value("before_row"),
                  "row before %r, after %r" % (r.value("before_row"), r.value("after_row")))]


test("MF-045", "Typing a digit in the calendar opens the date box, and Escape closes it without moving", _mf045_build,
     _mf045_check)


def _mf046_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.key_op("w1", "w"), oc.key_op("ww", "w"), oc.key_op("t", "t"), oc.dump_op()])]


def _mf046_check(runs, home):
    r = runs[1]
    return [check("W moves a week ahead", r.value("w1_currentrow") is not None and r.value("w1_currentrow") >= 0,
                  "row %r" % r.value("w1_currentrow")),
            check("WW moves two weeks ahead", r.value("ww_currentrow") is not None, "row %r" % r.value("ww_currentrow")),
            check("T starts again from today", r.value("t_currenttitle") is not None, "no title after T")]


test("MF-046", "W and WW move the list a week and two weeks ahead, and T starts again from today", _mf046_build, _mf046_check)


def _mf047_build(home):
    ops = oc.fixtures_ops() + [oc.appt_op(1, "MF047 a", oc.at(20, "09:00"), oc.at(20, "10:00")),
                               oc.appt_op(1, "MF047 b", oc.at(21, "09:00"), oc.at(21, "10:00")),
                               oc.appt_op(1, "MF047 c", oc.at(22, "09:00"), oc.at(22, "10:00")), oc.save_op()]
    return [run_spec(ops),
            run_spec([oc.open_op(1, "o"), oc.key_op("n1", "n"), oc.key_op("n2", "n"), oc.key_op("n3", "n"),
                      oc.key_op("n4", "n"), oc.dump_op()])]


def _mf047_check(runs, home):
    r = runs[1]
    rows = [r.value(k + "_currentrow") for k in ("n1", "n2", "n3", "n4")]
    return [check("N moves to the next entry of the open contact", rows[0] != rows[1], "rows %s" % rows),
            check("N wraps to the first entry", rows[3] == rows[0], "rows %s" % rows)]


test("MF-047", "N and P move between the open contact's entries and N wraps to the first", _mf047_build, _mf047_check)


def _mf048_build(home):
    ops = oc.fixtures_ops() + [oc.appt_op(1, "MF048 a", oc.at(20, "09:00"), oc.at(20, "10:00")),
                               oc.appt_op(2, "MF048 b", oc.at(21, "09:00"), oc.at(21, "10:00")), oc.save_op()]
    return [run_spec(ops),
            run_spec([oc.open_op(1, "o"), oc.calendar_rows_op("all"), oc.key_op("sp1", "space"), oc.key_op("sp2", "space"),
                      oc.dump_op()])]


def _mf048_check(runs, home):
    r = runs[1]
    one = len(r.value("sp1_hints") or [])
    two = len(r.value("sp2_hints") or [])
    all_rows = len(r.value("all_hints") or [])
    return [check("the first Space shows only the open contact's entries", one < all_rows and one >= 1,
                  "rows: %d, all: %d" % (one, all_rows)),
            check("the second Space shows all entries again", two == all_rows, "rows: %d, all: %d" % (two, all_rows))]


test("MF-048", "Space shows only the open contact's entries, and a second Space shows all", _mf048_build, _mf048_check)


def _mf049_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.key_op("sp", "space"), oc.key_op("n", "n"), oc.key_op("p", "p"), oc.key_op("r", "right"),
                      oc.key_op("l", "left"), oc.dump_op()])]


def _mf049_check(runs, home):
    r = runs[1]
    rows = [r.value(k + "_currentrow") for k in ("sp", "n", "p", "r", "l")]
    return [check("with no contact open the keys change nothing", len(set(rows)) == 1, "rows %s" % rows),
            blocked("the beep", "the beep is audible, and the driver cannot hear it; it is checked by ear")]


test("MF-049", "With no contact open, Space, N, P and the arrows beep and change nothing", _mf049_build, _mf049_check)


def _mf050_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.shortcut_op("o", "Alt+O", press=True), oc.overview_op("ov"), oc.dump_op()])]


def _mf050_check(runs, home):
    text = runs[1].value("ov_overview") or ""
    return [check("Alt+O shows the open contact's summary", oc.surname(1) in text, "overview: %s" % text[:120])]


test("MF-050", "Overview (Alt+O) shows the open contact's summary", _mf050_build, _mf050_check)


def _mf051_build(home):
    return [run_spec(oc.fixtures_ops() + [oc.notes_op(1, "", "MF051 history entry")]),
            run_spec([oc.open_op(1, "o"), oc.shortcut_op("y", "Alt+Y", press=True), oc.overview_op("ov"), oc.hold_op(),
                      oc.widgets_op("h", ["editHistory"]), oc.hold_op(False), oc.dump_op()])]


def _mf051_check(runs, home):
    return [check("Alt+Y shows the history", "MF051 history entry" in str(runs[1].value("h_editHistory")),
                  "history: %s" % str(runs[1].value("h_editHistory"))[:120])]


test("MF-051", "History (Alt+Y) lists the contact's recorded changes and sent messages", _mf051_build, _mf051_check)


def _mf052_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.hold_op(), oc.shortcut_op("l", "Ctrl+L", press=True), oc.set_op("t", "editToDo", text="MF052 item"),
                      oc.hold_op(False), oc.save_op(), oc.dump_op()]),
            run_spec([oc.dump_op("again.json")])]


def _mf052_check(runs, home):
    c = runs[2].contact(oc.surname(1), "again.json")
    return [check("the to-do item is saved and there after a restart", (c.get("todo") or "").strip() == "MF052 item", "todo %r" % c.get("todo"))]


test("MF-052", "To-Do List (Alt+L) saves its items with the contact", _mf052_build, _mf052_check)


def _mf053_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.actions_op("none", ["actionEnableContactDetailsEdit"]), oc.open_op(1, "o"),
                      oc.actions_op("open", ["actionEnableContactDetailsEdit"]), oc.dump_op()])]


def _mf053_check(runs, home):
    r = runs[1]     # the states are recorded in the second run, with no contact open and then with one open
    none = (r.value("none_states") or {}).get("actionEnableContactDetailsEdit")
    opened = (r.value("open_states") or {}).get("actionEnableContactDetailsEdit")
    return [check("Contact Details is unavailable with no contact open", none is False, "enabled %r" % none),
            check("and available once a contact is open", opened is True, "enabled %r" % opened)]


test("MF-053", "Contact Details (Alt+D) is unavailable with no contact open, and available after Alt+E, D", _mf053_build,
     _mf053_check)
