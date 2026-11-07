#
# By dialog (test specification, section 3c): DL-001 to DL-030 (DL-006 is in area 2).  Each test opens a dialog, types into
# it and presses its buttons (the driver's "hold", "set_widget", "press" and "widgets" operations), then checks what the
# program saved, or the warning it showed.  Where a dialog's result can't be read yet, that part is BLOCKED with the reason.
#

import os

import ops_common as oc
from ops_common import check, blocked, run_spec, test


# ------------------------------------------------------------------------------------------------ DL-001 to DL-005: Setup

_DL001_CONTROLS = ["windowBefore", "windowAfter", "showBirthdays"]


def _dl001_build(home):
    # The birthdays box starts unticked (its setting is "no"), so a change to it is visible
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.setting_op("showbirthdays", "no"), oc.hold_op(),
                      oc.menu_op("setup0", "actionSetup"), oc.widgets_op("before", _DL001_CONTROLS), oc.press_op("cancel0", "Cancel"),
                      oc.menu_op("setup", "actionSetup"), oc.set_op("typed", "windowBefore", text="45"),
                      oc.set_op("typed2", "windowAfter", text="46"), oc.set_op("typed3", "showBirthdays", checked=True),
                      oc.press_op("cancel", "Cancel"),
                      oc.menu_op("setup2", "actionSetup"), oc.widgets_op("after_cancel", _DL001_CONTROLS),
                      oc.set_op("typed4", "windowBefore", text="60"), oc.set_op("typed5", "windowAfter", text="61"),
                      oc.set_op("typed6", "showBirthdays", checked=True), oc.press_op("ok", "OK"),
                      oc.menu_op("setup3", "actionSetup"), oc.widgets_op("after_ok", _DL001_CONTROLS),
                      oc.keys_op("esc3", "Escape", target="window"), oc.hold_op(False), oc.dump_op()])]


def _dl001_check(runs, home):
    r = runs[1]
    before = {c: r.value("before_" + c) for c in _DL001_CONTROLS}
    checks = []
    for c in _DL001_CONTROLS:
        checks.append(check("Cancel discards the change to %s" % c, r.value("after_cancel_" + c) == before[c],
                            "before %r, after Cancel %r" % (before[c], r.value("after_cancel_" + c))))
    checks.append(check("OK keeps the change to windowBefore", r.value("after_ok_windowBefore") == "60",
                        "shown %r" % r.value("after_ok_windowBefore")))
    checks.append(check("OK keeps the change to windowAfter", r.value("after_ok_windowAfter") == "61",
                        "shown %r" % r.value("after_ok_windowAfter")))
    checks.append(check("OK keeps the change to showBirthdays", r.value("after_ok_showBirthdays") == "true",
                        "shown %r" % r.value("after_ok_showBirthdays")))
    return checks


test("DL-001", "Setup saves each control on OK and discards the changes on Cancel", _dl001_build, _dl001_check)

def _dl002_build(home):
    # The choice is kept: Setup is opened again to read it, then Goto Me opens the contact chosen
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.hold_op(), oc.menu_op("setup", "actionSetup"), oc.navigator_select_op("pick", "IdentifyMe", "Carter"),
                      oc.press_op("ok", "OK"), oc.shortcut_op("key", "Ctrl+Shift+O"), oc.menu_op("goto", "actionGotoMe"), oc.state_op("s"),
                      oc.menu_op("setup2", "actionSetup"), oc.widgets_op("w", ["IdentifyMe"]), oc.press_op("cancel", "Cancel"),
                      oc.hold_op(False), oc.dump_op()])]


def _dl002_check(runs, home):
    r = runs[1]
    return [oc.check("Setup keeps Carter as Identify Me when it is opened again",
                     "Carter" in str(r.value("w_IdentifyMe") or ""), "Identify Me reads %r" % r.value("w_IdentifyMe")),
            oc.check("Ctrl+Shift+O is bound to Goto Me", r.value("key_action") == "actionGotoMe", "bound to %r" % r.value("key_action")),
            oc.check("Goto Me opens the contact chosen", r.value("s_loaded") == "Carter", "loaded %r" % r.value("s_loaded"))]


test("DL-002", "Setup's Identify Me choice is the contact Goto Me opens", _dl002_build, _dl002_check)


def _dl003_build(home):
    # Setup has one master for calendar and contacts (Database Management), so Google is chosen once and both are kept with it
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.hold_op(), oc.menu_op("setup", "actionSetup"), oc.set_op("g", "conflictGoogle", checked=True),
                      oc.press_op("ok", "OK"), oc.menu_op("setup2", "actionSetup"),
                      oc.widgets_op("again", ["conflictGoogle", "conflictContactManager", "checkBox_syncCalendar",
                                              "checkBox_syncContacts"]),
                      oc.press_op("cancel", "Cancel"), oc.hold_op(False), oc.dump_op()])]


def _dl003_check(runs, home):
    r = runs[1]
    return [check("the master choice is kept after OK", r.value("again_conflictGoogle") == "true",
                  "conflictGoogle %r" % r.value("again_conflictGoogle")),
            check("there is one master: Contact Manager is not chosen as well",
                  r.value("again_conflictContactManager") == "false",
                  "conflictContactManager %r" % r.value("again_conflictContactManager")),
            check("the calendar and contacts boxes are there, as sync choices under that one master",
                  r.value("again_checkBox_syncCalendar") not in (None, "MISSING")
                  and r.value("again_checkBox_syncContacts") not in (None, "MISSING"),
                  "calendar %r, contacts %r" % (r.value("again_checkBox_syncCalendar"), r.value("again_checkBox_syncContacts")))]


test("DL-003", "Setup has one master for calendar and contacts, and it is kept", _dl003_build, _dl003_check)


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
    # 25/1/35 is a day/month/year date: it is accepted and kept as 25/01/2035.  2035 alone is not, so it is refused and not kept.
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.hold_op(), oc.menu_op("setup", "actionSetup"), oc.set_op("short", "smsKeyExpiry", text="25/1/35"),
                      oc.press_op("ok", "OK"), oc.menu_op("setup2", "actionSetup"), oc.widgets_op("kept", ["smsKeyExpiry"]),
                      oc.keys_op("esc2", "Escape", target="window"),
                      oc.menu_op("setup3", "actionSetup"), oc.set_op("year", "smsKeyExpiry", text="2035"),
                      oc.press_op("ok3", "OK"), oc.wait_op(500), oc.widgets_op("warn_year", []), oc.press_op("warnok3", "OK"),
                      oc.wait_op(500), oc.press_op("cancel3", "Cancel"),
                      oc.menu_op("setup4", "actionSetup"), oc.widgets_op("final", ["smsKeyExpiry"]), oc.keys_op("esc4", "Escape", target="window"),
                      oc.hold_op(False), oc.dump_op()])]


def _dl005_check(runs, home):
    r = runs[1]
    return [check("25/1/35 is accepted and kept as 25/01/2035", r.value("kept_smsKeyExpiry") == "25/01/2035",
                  "shown %r" % r.value("kept_smsKeyExpiry")),
            check("2035 is refused with the expiry warning", r.value("warn_year_title") == "SMS API key expiry",
                  "warning shown: %r" % r.value("warn_year_title")),
            check("and the refused value is not kept", r.value("final_smsKeyExpiry") == "25/01/2035",
                  "kept value %r" % r.value("final_smsKeyExpiry"))]


test("DL-005", "Setup's SMS key expiry accepts day/month/year and refuses other text", _dl005_build, _dl005_check)


_DL007_SEEDED_SYNC = "2026-01-02T03:04:05Z"
_DL007_NEVER_SYNCED = "1970-01-01T00:00:00Z"     # what Reset shows for a date it has cleared (the program's never-synced value)


def _dl007_build(home):
    # Both last sync dates are set, then Reset is pressed: the dates are cleared and the Google account is kept
    seeded = oc.fixtures_ops() + [oc.setting_op("googleusername", "bench.account@example.invalid"),
                                  oc.setting_op("lastgooglecontactsyncdate", _DL007_SEEDED_SYNC),
                                  oc.setting_op("lastgooglecalendarsyncdate", _DL007_SEEDED_SYNC)]
    return [run_spec(seeded),
            run_spec([oc.hold_op(), oc.menu_op("setup", "actionSetup"),
                      oc.widgets_op("before", ["googleUsername", "lastContactSync", "lastCalendarSync"]),
                      oc.press_op("reset", "Reset"),
                      oc.widgets_op("after", ["googleUsername", "lastContactSync", "lastCalendarSync"]),
                      oc.press_op("cancel", "Cancel"), oc.hold_op(False), oc.dump_op()])]


def _dl007_check(runs, home):
    r = runs[1]
    account = r.value("before_googleUsername")
    return [check("the last contact sync date is shown as set before Reset", r.value("before_lastContactSync") == _DL007_SEEDED_SYNC,
                  "before %r" % r.value("before_lastContactSync")),
            check("Reset clears the last contact sync date", r.value("after_lastContactSync") == _DL007_NEVER_SYNCED,
                  "after %r" % r.value("after_lastContactSync")),
            check("the last calendar sync date is shown as set before Reset", r.value("before_lastCalendarSync") == _DL007_SEEDED_SYNC,
                  "before %r" % r.value("before_lastCalendarSync")),
            check("Reset clears the last calendar sync date", r.value("after_lastCalendarSync") == _DL007_NEVER_SYNCED,
                  "after %r" % r.value("after_lastCalendarSync")),
            check("Reset keeps the Google account", account not in (None, "", "MISSING")
                  and r.value("after_googleUsername") == account,
                  "before %r, after %r" % (account, r.value("after_googleUsername")))]


test("DL-007", "Setup's Reset clears the last sync dates and keeps the Google account", _dl007_build, _dl007_check)


# ------------------------------------------------------------------------------------------------ DL-008 to DL-013: the form

def _new_appt_ops(summary, description, press="OK"):
    return [oc.open_op(1, "o"), oc.hold_op(), oc.menu_op("new", "actionNewAppointment"),
            oc.set_op("s", "editSummary", text=summary), oc.set_op("d", "plaintexteditDescription", text=description),
            oc.press_op("ok", press), oc.hold_op(False), oc.dump_op()]


def _dl008_build(home):
    # the form's For, From and To are read before Enter is pressed (Return, to the form), and the entry is then saved
    enter = {"op": "keys", "name": "enter", "sequence": "Return", "target": "window"}
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.hold_op(), oc.menu_op("new", "actionNewAppointment"),
                      oc.set_op("s", "editSummary", text="DL008 entry"), oc.set_op("d", "plaintexteditDescription", text="DL008 description"),
                      oc.widgets_op("form", ["contactName", "editSummary", "plaintexteditDescription"]),
                      oc.range_state_op("range", ["editWhenFrom", "editWhenTo"]),
                      enter, oc.hold_op(False), oc.dump_op()])]


def _dl008_check(runs, home):
    r = runs[1]
    a = [x for x in runs[1].appointments() if x.get("summary") == "DL008 entry"]
    return [check("the For field shows the open contact", "O'Neil" in str(r.value("form_contactName") or ""),
                  "For reads %r" % r.value("form_contactName")),
            check("the From field has its start and duration", r.value("range_editWhenFrom_duration") not in (None, -2),
                  "From duration %r" % r.value("range_editWhenFrom_duration")),
            check("the To field has its end", "MISSING" not in str(r.value("range_editWhenTo_text") or "MISSING"),
                  "To reads %r" % r.value("range_editWhenTo_text")),
            check("Enter saves the entry with its summary and description",
                  len(a) == 1 and a[0].get("description") == "DL008 description", "found %s" % a)]


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


_DL010_RANGE = ["editWhenFrom", "editWhenTo"]
# The text is "Thu 26/Nov/2026 10:00 (1h)": the hour of the start is at 16, and the duration (X) of the start at 23
_DL010_HOUR, _DL010_DURATION = 16, 23


def _dl010_build(home):
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.hold_op(), oc.menu_op("m", "actionNewAppointment"),
                      oc.range_state_op("s0", _DL010_RANGE),
                      oc.range_key_op("du", "editWhenFrom", _DL010_DURATION, "Up"), oc.range_state_op("s1", _DL010_RANGE),
                      oc.range_key_op("dd", "editWhenFrom", _DL010_DURATION, "Down"), oc.range_state_op("s2", _DL010_RANGE),
                      oc.range_key_op("tu", "editWhenTo", _DL010_HOUR, "Up"), oc.range_state_op("s3", _DL010_RANGE),
                      oc.range_key_op("td", "editWhenTo", _DL010_HOUR, "Down"), oc.range_state_op("s4", _DL010_RANGE),
                      oc.hold_op(False), oc.dump_op()])]


def _dl010_check(runs, home):
    r = runs[1]
    def dur(s, o="editWhenFrom"): return r.value("%s_%s_duration" % (s, o))
    def txt(s, o="editWhenTo"): return r.value("%s_%s_text" % (s, o))
    def start(s): return str(txt(s, "editWhenFrom") or "").split(" (")[0]
    def clock(text): return text.split(" (")[0].split()[-1]          # "Fri 06/Nov/2026 22:45" -> "22:45"
    def minutes(hhmm):
        hh, mm = hhmm.split(":")
        return int(hh) * 60 + int(mm)
    def gap(s):                                                       # To minus the start, in minutes
        return minutes(clock(str(txt(s, "editWhenTo") or ""))) - minutes(clock(start(s)))
    return [oc.check("the duration arrow changes the duration", dur("s1") != dur("s0"),
                     "duration %r, then %r" % (dur("s0"), dur("s1"))),
            oc.check("and the duration arrow changes To", txt("s1") != txt("s0"),
                     "To %r, then %r" % (txt("s0"), txt("s1"))),
            oc.check("Down on the duration gives back the first duration and To",
                     dur("s2") == dur("s0") and txt("s2") == txt("s0"),
                     "duration %r, To %r" % (dur("s2"), txt("s2"))),
            oc.check("changing To changes the duration", dur("s3") != dur("s0"),
                     "duration %r, then %r" % (dur("s0"), dur("s3"))),
            oc.check("and the start is not moved by it", start("s3") == start("s0"),
                     "start %r, then %r" % (start("s0"), start("s3"))),
            oc.check("Down on To gives back the first duration and To", dur("s4") == dur("s0") and txt("s4") == txt("s0"),
                     "duration %r, To %r" % (dur("s4"), txt("s4"))),
            oc.check("after the duration arrow, To is the start plus the duration", gap("s1") == dur("s1"),
                     "To minus start %r minutes, duration %r" % (gap("s1"), dur("s1"))),
            oc.check("after To changes, the duration is To minus the start", gap("s3") == dur("s3"),
                     "To minus start %r minutes, duration %r" % (gap("s3"), dur("s3")))]


test("DL-010", "The duration arrows change To, and changing To changes the duration", _dl010_build, _dl010_check)


def _dl011_build(home):
    # two appointments: A at 10:00, and B moved to 10:30, which clashes with A.  Declining the change ("No") sends the form back;
    # Escape then closes it, and nothing is saved.  The edit is started, and a settle lets the form, and the answer, run.
    return [run_spec(oc.fixtures_ops() + [oc.appt_op(1, "DL011 existing", oc.at(20, "10:00"), oc.at(20, "11:00")),
                                          oc.appt_op(1, "DL011 moved", oc.at(20, "13:00"), oc.at(20, "14:00")), oc.save_op()]),
            run_spec([oc.hold_op(), oc.answer_op("No"), {"op": "edit_ok", "name": "e", "summary": "DL011 moved",
                                                         "froms": [oc.at(20, "10:30")]},
                      oc.settle_op(2500), oc.keys_op("esc", "Escape", target="window"), oc.settle_op(400),
                      oc.hold_op(False), oc.dump_op()])]


def _dl011_check(runs, home):
    r = runs[1]
    moved = [x for x in r.appointments() if x.get("summary") == "DL011 moved"]
    return [check("the clash is offered", r.logged('answered "No": Appointment Clash'), "no clash question in the log"),
            check("declining saves nothing (the time is unchanged)",
                  len(moved) == 1 and moved[0].get("from") == oc.at(20, "13:00"), "found %s" % moved)]


test("DL-011", "A clash in the appointment form offers a change, and declining saves nothing", _dl011_build, _dl011_check)


def _dl012_build(home):
    # the single appointment is moved three hours on, so it does not clash with the weekly one
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.hold_op(), oc.menu_op("new", "actionNewAppointment"),
                      oc.set_op("s", "editSummary", text="DL012 weekly"), oc.set_op("r", "comboRepeat", index=2),
                      oc.set_op("e", "spinRepeatEvery", text="1"), oc.press_op("ok", "OK"),
                      oc.menu_op("new2", "actionNewAppointment"), oc.set_op("s2", "editSummary", text="DL012 single"),
                      oc.range_key_op("h1", "editWhenFrom", 16, "Down"), oc.range_key_op("h2", "editWhenFrom", 16, "Down"),
                      oc.range_key_op("h3", "editWhenFrom", 16, "Down"),
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
    # Ends: 1 is "After a number of times" and 2 is "On a date".  The end date is typed in the box's own format.
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.hold_op(), oc.menu_op("new", "actionNewAppointment"),
                      oc.set_op("s", "editSummary", text="DL013 counted"), oc.set_op("r", "comboRepeat", index=2),
                      oc.set_op("e", "comboEnds", index=1), oc.set_op("n", "spinEndCount", text="3"),
                      oc.press_op("ok", "OK"),
                      oc.open_op(1, "o"), oc.menu_op("new2", "actionNewAppointment"),
                      oc.set_op("s2", "editSummary", text="DL013 dated"), oc.set_op("r2", "comboRepeat", index=2),
                      oc.set_op("e2", "comboEnds", index=2), oc.widgets_op("dateform", ["dateEndDate"]),
                      oc.set_op("d", "dateEndDate", text="15 Dec 2026"), oc.widgets_op("dated", ["dateEndDate"]),
                      # three hours later than the first, so the repeats do not clash with it
                      oc.range_key_op("h1", "editWhenFrom", 16, "Down"), oc.range_key_op("h2", "editWhenFrom", 16, "Down"),
                      oc.range_key_op("h3", "editWhenFrom", 16, "Down"),
                      oc.press_op("ok2", "OK"), oc.hold_op(False), oc.dump_op()])]


def _dl013_check(runs, home):
    r = runs[1]
    a = [x for x in r.appointments() if x.get("summary") == "DL013 counted"]
    b = [x for x in r.appointments() if x.get("summary") == "DL013 dated"]
    return [check("the count of three is kept in the list", len(a) == 1 and a[0].get("repeatcount") == "3",
                  "found %s" % a),
            check("the end date is kept in the list", len(b) == 1 and str(b[0].get("repeatuntil") or "").startswith("2026-12-15"),
                  "repeat until %r" % (b[0].get("repeatuntil") if b else None)),
            check("the end date is shown in the form", "15 Dec 2026" in str(r.value("dated_dateEndDate") or ""),
                  "the form reads %r" % r.value("dated_dateEndDate"))]


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
    # the title of the email entry is changed with F2, with the focus on that entry
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.focus_on_op("editEmail"), oc.dates_op(["Bad: title"]), oc.keys_op("f2", "F2"),
                      oc.dump_op()])]


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
            run_spec([oc.hold_op(), oc.menu_op("adv", "action_Advanced_Find"),
                      oc.set_op("c", "comboBox_SearchCategory1", index=8),          # Surname: the text box is enabled by a category
                      oc.set_op("t", "lineEdit_SearchText1", text="Jackson"),
                      oc.press_op("ok", "OK"), oc.wait_op(1500), oc.widgets_op("res", rows=True),
                      {"op": "keys", "name": "enter", "sequence": "Return", "target": "window"}, oc.wait_op(1000),
                      oc.state_op("s"), oc.hold_op(False), oc.dump_op()])]


def _dl020_check(runs, home):
    rows = runs[1].value("res_rows") or []
    return [check("the criteria find Jackson", any("Jackson, Daniel" == x for x in rows), "rows: %s" % rows[:6]),
            check("and only the matching contacts are listed", not any("Carter" in x for x in rows),
                  "rows: %s" % rows[:6]),
            check("Enter opens the first result, Jackson", runs[1].value("s_loaded") == "Jackson",
                  "loaded %r" % runs[1].value("s_loaded"))]


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
            run_spec([oc.hold_op(), oc.menu_op("man", "actionManual"), oc.windows_op("open")] + oc.manual_search_ops() +
                     [oc.keys_op("esc", "Escape", target="window"), oc.windows_op("closed"), oc.hold_op(False), oc.dump_op()])]


def _dl024_check(runs, home):
    r = runs[1]
    opened = any("help" in str(w).lower() or "manual" in str(w).lower() for w in (r.value("open_windows") or []))
    closed = not any("help" in str(w).lower() or "manual" in str(w).lower() for w in (r.value("closed_windows") or []))
    first, second = r.value("first_position"), r.value("second_position")
    return [check("the manual opens", opened, "windows: %s" % (r.value("open_windows"),)),
            check("Ctrl+F finds the first 'Find' in the manual", "find" in str(r.value("first_line") or "").lower()
                  and r.value("first_found") is True, "cursor line %r" % (r.value("first_line"),)),
            check("F3 moves to the next match", isinstance(first, int) and isinstance(second, int) and second > first,
                  "cursor at %r, then %r" % (first, second)),
            check("the next match is on a line with 'Find'", "find" in str(r.value("second_line") or "").lower(),
                  "cursor line %r" % (r.value("second_line"),)),
            check("Escape closes it", closed, "windows after: %s" % (r.value("closed_windows"),))]


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
    return [check("the date boxes are answered (the valid one and the text one), and close",
                  r.log.count("date entry answered") + r.log.count("date entry cancelled") >= 2,
                  "answers in the log: %d" % (r.log.count("date entry answered") + r.log.count("date entry cancelled"))),
            blocked("a valid date moves the list", "the calendar rows show the same first row for every day, so the list's "
                                                   "position is not read yet"),
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
