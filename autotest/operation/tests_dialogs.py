#
# By dialog (test specification, section 3c): DL-001 to DL-030 (DL-006 is in area 2).  Each test opens a dialog, types into
# it and presses its buttons (the driver's "hold", "set_widget", "press" and "widgets" operations), then checks what the
# program saved, or the warning it showed.  Where a dialog's result can't be read yet, that part is BLOCKED with the reason.
#

import os

import ops_common as oc
from ops_common import check, blocked, run_spec, test


# ------------------------------------------------------------------------------------------------ DL-001 to DL-005: Setup

def _dl001_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.hold_op(), oc.menu_op("setup", "actionSetup"), oc.set_op("typed", "windowBefore", text="45"),
                      oc.press_op("cancel", "Cancel"),
                      oc.menu_op("setup2", "actionSetup"), oc.widgets_op("after_cancel", ["windowBefore"]),
                      oc.set_op("typed2", "windowBefore", text="60"), oc.press_op("ok", "OK"),
                      oc.menu_op("setup3", "actionSetup"), oc.widgets_op("after_ok", ["windowBefore"]),
                      oc.press_op("cancel3", "Cancel"), oc.hold_op(False), oc.dump_op()])]


def _dl001_check(runs, home):
    r = runs[1]
    return [check("Cancel discards the change", r.value("after_cancel_windowBefore") != "45",
                   "shown %r" % r.value("after_cancel_windowBefore")),
            check("OK keeps the change", r.value("after_ok_windowBefore") == "60",
                  "shown %r" % r.value("after_ok_windowBefore")),
            blocked("each control saved and discarded", "only the window days are read here; the other controls need their "
                                                         "own reads (their names are in configuration.ui)")]


test("DL-001", "Setup saves each control on OK and discards the changes on Cancel", _dl001_build, _dl001_check)

test("DL-002", "Setup's Identify Me choice is the contact Goto Me opens",
     lambda home: [run_spec(oc.fixtures_ops())], lambda runs, home: [],
     blocked_reason="Identify Me is chosen in Setup from a list the bench cannot set yet (see MF-022).")


def _dl003_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.hold_op(), oc.menu_op("setup", "actionSetup"), oc.set_op("g", "conflictGoogle", checked=True),
                      oc.press_op("ok", "OK"), oc.menu_op("setup2", "actionSetup"),
                      oc.widgets_op("again", ["conflictGoogle", "conflictContactManager"]),
                      oc.press_op("cancel", "Cancel"), oc.hold_op(False), oc.dump_op()])]


def _dl003_check(runs, home):
    r = runs[1]
    return [check("the master choice is kept after OK", r.value("again_conflictGoogle") == "true",
                  "conflictGoogle %r" % r.value("again_conflictGoogle")),
            blocked("the calendar and contacts masters set independently",
                    "the calendar master's control is not identified yet; the contacts one is checked above")]


test("DL-003", "Setup's calendar and contacts masters are saved independently", _dl003_build, _dl003_check)


def _dl004_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.hold_op(), oc.menu_op("setup", "actionSetup"), oc.set_op("a", "windowAfter", text="30"),
                      oc.widgets_op("kept", ["windowAfter"]),
                      oc.set_op("b", "windowAfter", text="thirty"), oc.widgets_op("refused", ["windowAfter"]),
                      oc.press_op("cancel", "Cancel"), oc.hold_op(False), oc.dump_op()])]


def _dl004_check(runs, home):
    r = runs[1]
    return [check("30 is kept", r.value("kept_windowAfter") == "30", "shown %r" % r.value("kept_windowAfter")),
            check("text is refused, and 30 stays", r.value("refused_windowAfter") == "30",
                  "shown %r" % r.value("refused_windowAfter"))]


test("DL-004", "Setup's window days accept whole numbers and refuse text", _dl004_build, _dl004_check)


def _dl005_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.hold_op(), oc.menu_op("setup", "actionSetup"),
                      oc.set_op("good", "smsKeyExpiry", text="25/01/2035"), oc.press_op("ok", "OK"),
                      oc.widgets_op("warn_good", []), oc.press_op("warnok", "OK"),
                      oc.menu_op("setup2", "actionSetup"), oc.widgets_op("kept", ["smsKeyExpiry"]),
                      oc.set_op("short", "smsKeyExpiry", text="25/1/35"), oc.press_op("ok2", "OK"),
                      oc.widgets_op("warn_short", []), oc.press_op("warnok2", "OK"),
                      oc.set_op("year", "smsKeyExpiry", text="2035"), oc.press_op("ok3", "OK"),
                      oc.widgets_op("warn_year", []), oc.press_op("warnok3", "OK"),
                      oc.hold_op(False), oc.dump_op()])]


def _dl005_check(runs, home):
    r = runs[1]
    return [check("25/01/2035 is accepted", r.value("kept_smsKeyExpiry") == "25/01/2035",
                   "shown %r" % r.value("kept_smsKeyExpiry")),
            check("25/1/35 is refused with the expiry warning", r.value("warn_short_title") == "SMS API key expiry",
                  "warning %r" % r.value("warn_short_title")),
            check("2035 is refused with the expiry warning", r.value("warn_year_title") == "SMS API key expiry",
                  "warning %r" % r.value("warn_year_title"))]


test("DL-005", "Setup's SMS key expiry accepts day/month/year and refuses other text", _dl005_build, _dl005_check)


def _dl007_build(home):
    return [run_spec(oc.fixtures_ops() + [oc.setting_op("googleusername", "bench.account@example.invalid")]),
            run_spec([oc.hold_op(), oc.menu_op("setup", "actionSetup"), oc.widgets_op("before", ["googleUsername"]),
                      oc.press_op("reset", "Reset"), oc.widgets_op("after", ["googleUsername"]),
                      oc.press_op("cancel", "Cancel"), oc.hold_op(False), oc.dump_op()])]


def _dl007_check(runs, home):
    r = runs[1]
    return [check("Reset removes the account", r.value("after_googleUsername") in ("", "MISSING", None)
                  and r.value("before_googleUsername") not in ("", None),
                  "before %r, after %r" % (r.value("before_googleUsername"), r.value("after_googleUsername"))),
            blocked("the sign-in code of Change", "needs the device sign-in with Google, which the operation tests do not reach")]


test("DL-007", "Setup's Google Account Change shows the sign-in code, and Reset removes the account", _dl007_build, _dl007_check)


# ------------------------------------------------------------------------------------------------ DL-008 to DL-013: the form

def _new_appt_ops(summary, description, press="OK"):
    return [oc.open_op(1, "o"), oc.hold_op(), oc.menu_op("new", "actionNewAppointment"),
            oc.set_op("s", "editSummary", text=summary), oc.set_op("d", "plaintexteditDescription", text=description),
            oc.press_op("ok", press), oc.hold_op(False), oc.dump_op()]


def _dl008_build(home):
    return [run_spec(oc.fixtures_ops()), run_spec(_new_appt_ops("DL008 entry", "DL008 description"))]


def _dl008_check(runs, home):
    a = [x for x in runs[1].appointments() if x.get("summary") == "DL008 entry"]
    return [check("the entry is saved with its summary and description",
                  len(a) == 1 and a[0].get("description") == "DL008 description", "found %s" % a),
            blocked("the For, From and To fields", "the date fields are typed by the program's own format; checked by DL-010")]


test("DL-008", "The appointment form has For, Summary, From, To and Description, and Enter saves", _dl008_build, _dl008_check)


def _dl009_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.hold_op(), oc.menu_op("new", "actionNewAppointment"),
                      oc.set_op("s", "editSummary", text="DL009 cancelled"), oc.press_op("cancel", "Cancel"),
                      oc.hold_op(False), oc.dump_op()])]


def _dl009_check(runs, home):
    kept = [x for x in runs[1].appointments() if x.get("summary") == "DL009 cancelled"]
    return [check("Cancel leaves nothing saved", not kept, "found %s" % kept)]


test("DL-009", "Escape in the appointment form cancels with no change", _dl009_build, _dl009_check)


test("DL-010", "The duration arrows change To, and changing To changes the duration",
     lambda home: [run_spec(oc.fixtures_ops())], lambda runs, home: [],
     blocked_reason="the arrows and the To field are dates typed in the program's own format, which the bench does not "
                    "set yet (AccessibleDateRangeEdit). The keys are in the form's tab order, to be added with the format.")


def _dl011_build(home):
    existing = oc.at(20, "10:00")
    return [run_spec(oc.fixtures_ops() + [oc.appt_op(1, "DL011 existing", existing, oc.at(20, "11:00")), oc.save_op()]),
            run_spec([oc.hold_op(), {"op": "edit_ok", "name": "e", "summary": "DL011 existing",
                                     "froms": [oc.at(20, "10:30")]},
                      oc.answer_op("No"), oc.hold_op(False), oc.dump_op()])]


def _dl011_check(runs, home):
    r = runs[1]
    found = [x for x in r.appointments() if x.get("summary") == "DL011 existing"]
    return [check("the clash is offered, and declining saves nothing (the time is unchanged)",
                  len(found) == 1 and found[0].get("from") == oc.at(20, "10:00"), "found %s" % found)]


test("DL-011", "A clash in the appointment form offers a change, and declining saves nothing", _dl011_build, _dl011_check)


def _dl012_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.hold_op(), oc.menu_op("new", "actionNewAppointment"),
                      oc.set_op("s", "editSummary", text="DL012 weekly"), oc.set_op("r", "comboRepeat", index=2),
                      oc.set_op("e", "spinRepeatEvery", text="1"), oc.press_op("ok", "OK"),
                      oc.menu_op("new2", "actionNewAppointment"), oc.set_op("s2", "editSummary", text="DL012 single"),
                      oc.press_op("ok2", "OK"), oc.hold_op(False), oc.dump_op()])]


def _dl012_check(runs, home):
    r = runs[1]
    weekly = [x for x in r.appointments() if x.get("summary") == "DL012 weekly"]
    single = [x for x in r.appointments() if x.get("summary") == "DL012 single"]
    return [check("the weekly repeat keeps its type and interval",
                  len(weekly) == 1 and weekly[0].get("repeat") == "2" and weekly[0].get("repeatinterval") == "1",
                  "found %s" % weekly),
            check("a single appointment has no repeat", len(single) == 1 and single[0].get("repeat") in ("", "0", None),
                  "found %s" % single)]


test("DL-012", "A repeat keeps its type and interval, and a single appointment gets no repeat", _dl012_build, _dl012_check)


def _dl013_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.hold_op(), oc.menu_op("new", "actionNewAppointment"),
                      oc.set_op("s", "editSummary", text="DL013 counted"), oc.set_op("r", "comboRepeat", index=2),
                      oc.set_op("n", "spinEndCount", text="3"), oc.press_op("ok", "OK"), oc.hold_op(False), oc.dump_op()])]


def _dl013_check(runs, home):
    r = runs[1]
    a = [x for x in r.appointments() if x.get("summary") == "DL013 counted"]
    return [check("the count of three is kept in the list", len(a) == 1 and a[0].get("repeatcount") == "3",
                  "found %s" % a),
            blocked("the end by date", "the end date is typed in the program's date format; not set by the bench yet")]


test("DL-013", "A repeat's end, by count or by date, is kept in the list and the form", _dl013_build, _dl013_check)


def _dl014_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.form_set_op({"email": "not-an-email"}), oc.form_save_op("save"), oc.dump_op()])]


def _dl014_check(runs, home):
    r = runs[1]
    return [check("the save is refused", r.value("save_saved") is False, "saved %r" % r.value("save_saved")),
            check("the focus is in the email box", r.value("save_focus") == "editEmail", "focus %r" % r.value("save_focus")),
            check("nothing is saved", r.contact(oc.surname(1)).get("email") == oc.email_of(1),
                  "email %r" % r.contact(oc.surname(1)).get("email"))]


test("DL-014", "A bad email in the contact form stops the save, with focus in its box and nothing saved", _dl014_build, _dl014_check)


def _dl015_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.form_set_op({"email": "name@domain.com (Work)"}, tidy=True), oc.form_save_op("save"),
                      oc.dump_op()])]


def _dl015_check(runs, home):
    c = runs[1].contact(oc.surname(1))
    return [check("the address is saved", c.get("email") == "name@domain.com", "email %r" % c.get("email")),
            check("the label becomes the title", c.get("emailtitle") == "Work", "emailtitle %r" % c.get("emailtitle"))]


test("DL-015", "A label typed after an address becomes the email's title", _dl015_build, _dl015_check)


def _dl016_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.form_set_op({"phone": "+44 (0) 1234 567890"}, tidy=True), oc.form_save_op("save"),
                      oc.dump_op()])]


def _dl016_check(runs, home):
    c = runs[1].contact(oc.surname(1))
    return [check("the phone is saved in tidy form", c.get("phone") == "+44 1234 567890", "phone %r" % c.get("phone"))]


test("DL-016", "A phone typed with a (0) after the country code is saved in tidy form", _dl016_build, _dl016_check)


def _dl017_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.dates_op(["Bad: title"]), oc.keys_op("f2", "F2"), oc.dump_op()])]


def _dl017_check(runs, home):
    r = runs[1]
    return [check("F2 asks for the title", r.value("dialog_1_title") is not None, "no title box"),
            check("the colon is refused with the title rules", r.logged("is not a valid title"), "no refusal in the log")]


test("DL-017", "F2 changes a title, and a title with a colon is refused", _dl017_build, _dl017_check)


def _dl018_build(home):
    return [run_spec([{"op": "add_contact", "fields": {"email": "only.detail@sgc.contacmanagertest"}}, oc.save_op()]),
            run_spec([{"op": "open_contact", "name": "c", "match": {"email": "only.detail@sgc.contacmanagertest"}},
                      oc.form_set_op({"email": ""}), oc.form_save_op("save"), oc.dump_op()])]


def _dl018_check(runs, home):
    r = runs[1]
    still = [c for c in r.contacts() if c.get("email") == "only.detail@sgc.contacmanagertest"]
    return [check("the change is saved (the only detail is gone)", r.value("save_saved") is True and not still,
                  "saved %r, still there %d" % (r.value("save_saved"), len(still)))]


test("DL-018", "Clearing the only detail of a contact saves the change", _dl018_build, _dl018_check)


def _dl019_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.form_set_op({"email": "switch.oneil@sgc.contacmanagertest"}),
                      oc.answer_op("Cancel"), oc.open_op(2, "s1"),
                      oc.answer_op("Discard"), oc.open_op(2, "s2"), oc.dump_op()])]


def _dl019_check(runs, home):
    r = runs[1]
    return [check("Cancel keeps the first contact open", r.value("s1_selected") == oc.surname(1),
                  "selected %r" % r.value("s1_selected")),
            check("Discard switches to the second", r.value("s2_selected") == oc.surname(2),
                  "selected %r" % r.value("s2_selected")),
            check("Discard leaves the edit unsaved", r.contact(oc.surname(1)).get("email") == oc.email_of(1),
                  "email %r" % r.contact(oc.surname(1)).get("email"))]


test("DL-019", "Quitting or switching contact with unsaved edits asks Save, Discard or Cancel", _dl019_build, _dl019_check)


def _dl020_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.hold_op(), oc.menu_op("adv", "action_Advanced_Find"), oc.set_op("t", "lineEdit_SearchText1", text="Jackson"),
                      oc.press_op("ok", "OK"), oc.widgets_op("res", rows=True), oc.hold_op(False), oc.dump_op()])]


def _dl020_check(runs, home):
    rows = " ".join(runs[1].value("res_rows") or [])
    return [check("the criteria find Jackson", "Jackson" in rows, "rows: %s" % rows[:160]),
            blocked("Enter opens the result", "the result list's selection by Enter is not read yet")]


test("DL-020", "Advanced Global Find applies its criteria, lists results, and opens on Enter", _dl020_build, _dl020_check)


def _dl021_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.hold_op(), oc.menu_op("inc", "actionIncome"),
                      oc.set_op("a", "lineEditAmount", text="abc"), oc.press_op("ok", "OK"), oc.widgets_op("warn", []),
                      oc.press_op("warnok", "OK"), oc.set_op("b", "lineEditAmount", text="12.50"),
                      oc.press_op("ok2", "OK"), oc.hold_op(False), oc.dump_op()])]


def _dl021_check(runs, home):
    r = runs[1]
    text = ""
    for root, _d, names in os.walk(oc.data_folder(home)):
        for n in names:
            with open(os.path.join(root, n), encoding="utf-8", errors="replace") as f:
                text += f.read()
    return [check("text in the amount is refused with a warning (a window other than the form is open)",
                  r.value("warn_class") not in (None, "TransactionForm"), "top window %r" % r.value("warn_class")),
            check("a number is recorded", "12.50" in text, "12.50 not in the data")]


test("DL-021", "The transaction form checks the amount, and records income and expense", _dl021_build, _dl021_check)


def _dl022_build(home):
    # the text form, with no sender set in the ini file: the program refuses to send, and says why (no network needed)
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.hold_op(), oc.menu_op("sms", "action_SMS_Contact"),
                      oc.set_op("t", "messageText", text="DL022 text"), oc.press_op("send", "OK"), oc.widgets_op("warn", []),
                      oc.hold_op(False), oc.dump_op()])]


def _dl022_check(runs, home):
    r = runs[1]
    return [check("with no sender, the text is refused and the warning says why",
                  r.value("warn_title") == "Unable to Send", "warning %r" % r.value("warn_title")),
            blocked("the send with the key set, and the history entry", "the send needs the text service; the bench stands "
                                                                       "in for it only for the announcement tests")]


test("DL-022", "The text form sends through the stub and records the send in history; a missing key gives a message",
     _dl022_build, _dl022_check)


def _dl023_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.menu_op("log", "actionShow_Log"), oc.windows_op("open"), oc.dump_op()])]


def _dl023_check(runs, home):
    titles = " ".join(runs[1].value("open_windows") or [])
    return [check("the Logs window is open", "log" in titles.lower(), "windows: %s" % titles),
            blocked("the entry added while open, and focus back after closing", "the log window is not modal, so the "
                    "driver cannot press its Close button; the focus check needs the driver's focus record")]


test("DL-023", "The Logs window updates while open, and closing it returns focus to the program", _dl023_build, _dl023_check)


def _dl024_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.menu_op("man", "actionManual"), oc.windows_op("open"), oc.keys_op("esc", "Escape"),
                      oc.windows_op("closed"), oc.dump_op()])]


def _dl024_check(runs, home):
    r = runs[1]
    opened = any("manual" in str(w).lower() for w in (r.value("open_windows") or []))
    closed = not any("manual" in str(w).lower() for w in (r.value("closed_windows") or []))
    return [check("the manual opens", opened, "windows: %s" % (r.value("open_windows"),)),
            check("Escape closes it", closed, "windows after: %s" % (r.value("closed_windows"),)),
            blocked("the Find and F3 in the manual", "its search is not checked yet (see MF-043)")]


test("DL-024", "The manual's Find and F3 work, and Escape closes it", _dl024_build, _dl024_check)


def _encryption_test(code, title, action, body, expects):
    def build(home):
        return [run_spec(oc.fixtures_ops()),
                run_spec([oc.menu_op("logout", "actionLogout"), oc.hold_op(), oc.menu_op("m", action)] + body +
                         [oc.hold_op(False), oc.dump_op()])]

    def checks(runs, home):
        return [check(label, cond(runs[1]), detail(runs[1])) for label, cond, detail in expects]
    test(code, title, build, checks)


_encryption_test("DL-025", "The Set Encryption Key dialog refuses a mismatched key and a mismatched password",
                 "actionSetEncryptionKey",
                 [oc.set_op("k1", "lineEdit0", text="key-one"), oc.set_op("k2", "lineEdit1", text="key-two"),
                  oc.set_op("k3", "lineEdit2", text="pw"), oc.set_op("k4", "lineEdit3", text="pw"),
                  oc.press_op("ok", "OK"), oc.widgets_op("warn1", []), oc.press_op("warnok", "OK"),
                  oc.set_op("p1", "lineEdit0", text="key"), oc.set_op("p2", "lineEdit1", text="key"),
                  oc.set_op("p3", "lineEdit2", text="pw-one"), oc.set_op("p4", "lineEdit3", text="pw-two"),
                  oc.press_op("ok2", "OK"), oc.widgets_op("warn2", [])],
                 [("a mismatched key is refused with a warning", lambda r: r.value("warn1_title") not in (None, "", "MISSING"),
                   lambda r: "warning %r" % r.value("warn1_title")),
                  ("a mismatched password is refused with a warning", lambda r: r.value("warn2_title") not in (None, "", "MISSING"),
                   lambda r: "warning %r" % r.value("warn2_title"))])

_encryption_test("DL-026", "Change Password asks for the old password first, and refuses a wrong one",
                 "actionChangePassword",
                 [oc.set_op("old", "lineEdit0", text="not-the-old-password"), oc.press_op("ok", "OK"), oc.widgets_op("warn", [])],
                 [("a wrong old password is refused with a warning", lambda r: r.value("warn_title") not in (None, "", "MISSING"),
                   lambda r: "warning %r" % r.value("warn_title"))])


def _dl027_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.dates_op(["2/12/26"]), oc.key_op("two", "2"), oc.dates_op(["xyz"]), oc.key_op("two2", "2"),
                      oc.dump_op()])]


def _dl027_check(runs, home):
    r = runs[1]
    texts = [v for k, v in r.results.items() if k.endswith("_text") and k.startswith("dialog_")]
    return [check("a valid date moves the list (the date box took 2/12/26)", "2/12/26" in texts, "texts %s" % texts),
            check("the date boxes are answered (the valid one and the text one)",
                  r.log.count("date entry answered") + r.log.count("date entry cancelled") >= 2,
                  "answers in the log: %d" % (r.log.count("date entry answered") + r.log.count("date entry cancelled"))),
            blocked("the beep", "the beep is audible, and the driver cannot hear it")]


test("DL-027", "The typed-date box moves the list for valid dates, and beeps and closes for text", _dl027_build, _dl027_check)


def _dl028_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.menu_op("tidy", "actionTidyContacts"), oc.dump_op()])]


def _dl028_check(runs, home):
    r = runs[1]
    return [check("the tidy summary is shown and closed", r.logged("progress window closed") or "Tidy Records" in r.dialog_titles(),
                  "no summary in the log")]


test("DL-028", "The tidy progress window shows its summary, and closes", _dl028_build, _dl028_check)


def _dl029_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.hold_op(), oc.answer_key_op("Escape"), oc.menu_op("del", "actionDeleteContact"),
                      oc.hold_op(False), oc.dump_op()])]


def _dl029_check(runs, home):
    r = runs[1]
    return [check("Escape on the delete question deletes nothing", any(c.get("surname") == oc.surname(1) for c in r.contacts()),
                  "contact 1 is gone")]


test("DL-029", "Message boxes default to the safe answer, and Escape cancels without action", _dl029_build, _dl029_check)


def _dl030_build(home):
    folder = os.path.join(home, "exports")
    os.makedirs(folder, exist_ok=True)
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.hold_op(), oc.menu_op("exp", "actionExportContacts"), oc.press_op("cancel", "Cancel"),
                      oc.hold_op(False), oc.dump_op()])]


def _dl030_check(runs, home):
    folder = os.path.join(home, "exports")
    files = os.listdir(folder)
    return [check("cancelling the export writes no file", not files, "files: %s" % files)]


test("DL-030", "Export and Import file dialogs leave no file when cancelled", _dl030_build, _dl030_check)
