#
# Start-up and shutdown (test specification, section 3a): SS-001 to SS-015.  Each test is in fresh HOME folder; its runs are
# restarts of the same program, and the checks read what the runs recorded and what they left in the data folder.
#

import glob
import os
import shutil
import sys
import time

import ops_common as oc
from ops_common import check, blocked, run_spec, test


def _key_file(home):
    return any(f == "TextFileEncryption.conf" for _, _, fs in os.walk(home) for f in fs)


def _ss001_runs(home):
    # no data folder: the first start makes it, and sets up the key
    return [run_spec([oc.dump_op()])]


def _ss001_check(runs, home):
    r = runs[0]
    folder = os.path.isdir(oc.data_folder(home))
    return [check("the data folder is made", folder, "no folder"),
            check("the key is set up (its settings file is written)", _key_file(home), "no TextFileEncryption.conf"),
            check("the program starts", r.logged("main window shown"), "no main window in the log")]


test("SS-001", "A first start with no data folder creates the folder and asks for the key to be set",
     _ss001_runs, _ss001_check)


def _ss002_runs(home):
    # the first start sets the key and stores six contacts; the second start logs in with the password
    return [run_spec(oc.fixtures_ops()), run_spec([oc.dump_op()])]


def _ss002_check(runs, home):
    r = runs[1]
    return [check("the second start logs in", r.logged("main window shown") and not r.logged("failed to login"),
                  "login failed or the window did not show"),
            check("the six contacts load", len(r.contacts()) == 6, "%d contacts" % len(r.contacts()))]


test("SS-002", "A correct login password lets the program start the first time", _ss002_runs, _ss002_check)


def _ss003_runs(home):
    # three wrong passwords: the program offers set-up or close; the test chooses Close (CM_TEST_LOGIN_ANSWER).  The data
    # folder is snapshotted before the wrong-password start, and compared with it afterwards.
    def before(h):
        _ss003_before.clear()
        _ss003_before.update(oc.snapshot(oc.data_folder(h)))
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.dump_op()], secret="a-wrong-password-for-test", setkey=False,
                     env={"CM_TEST_LOGIN_ANSWER": "Close"}, before=before)]


def _ss003_check(runs, home):
    r = runs[1]
    # the program draws its window before it aborts, so the test looks for the program quitting before its operation runs
    return [check("the program quits before the data is opened", not r.logged("dumped") and not r.logged("op 1"),
                  "the operation ran after the wrong passwords"),
            check("the Close choice is made", r.logged("login question answered: Close") or r.logged('answered "Close"'),
                  "the Close choice was not made"),
            check("the data is unchanged", oc.snapshot(oc.data_folder(home)) == dict(_ss003_before),
                  "the data folder changed")]


_ss003_before = {}


test("SS-003", "Three wrong passwords offer set-up or close, and close exits without changing data",
     _ss003_runs, _ss003_check)


def _ss004_runs(home):
    # run 1: a copy that holds the data folder (it waits); run 2: a second copy, which must show the warning and work
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.wait_op(40000)], bg=True, settle=10),
            run_spec([oc.open_op(1, "b"), oc.windows_op("w"), oc.dump_op()])]


def _ss004_check(runs, home):
    r = runs[2]
    return [check("the second copy shows the warning", r.logged("Messages Not Sent") or "Messages Not Sent" in r.dialog_titles(),
                  "no 'Messages Not Sent' in the second copy's log"),
            check("the second copy starts", r.logged("main window shown"), "no main window"),
            check("the second copy still works (a contact opens)", r.value("b_selected") == oc.surname(1),
                  "selected %r" % r.value("b_selected"))]


test("SS-004", "A second copy on the same folder shows the warning and still works for everything but messages",
     _ss004_runs, _ss004_check)


def _ss005_runs(home):
    # two overlapping appointments, saved; a restart lists the overlap and changes nothing
    ops = oc.fixtures_ops() + [oc.appt_op(1, "SS005 A", oc.at(20, "10:00"), oc.at(20, "11:00")),
                               oc.appt_op(2, "SS005 B", oc.at(20, "10:30"), oc.at(20, "11:30")), oc.save_op()]
    return [run_spec(ops), run_spec([oc.dump_op()])]


def _ss005_check(runs, home):
    r = runs[1]
    summaries = sorted(a.get("summary", "") for a in r.appointments())
    return [check("the overlap is listed", r.logged("These appointments overlap") or "Appointment Clashes" in r.dialog_titles(),
                   "no clash report in the log"),
            check("both appointments are kept, unchanged", summaries == ["SS005 A", "SS005 B"], "found %s" % summaries)]


test("SS-005", "Start-up lists overlapping appointments and changes nothing", _ss005_runs, _ss005_check)


def _ss006_runs(home):
    # the key expires in 20 days (warning expected), then the setting is moved to 40 days (no warning)
    return [run_spec(oc.fixtures_ops() + [oc.setting_op("smsapikeyexpires", oc.dates_ahead(20))]),
            run_spec([oc.dump_op()]),
            run_spec([oc.setting_op("smsapikeyexpires", oc.dates_ahead(40))]),
            run_spec([oc.dump_op()])]


def _ss006_check(runs, home):
    soon, later = runs[1], runs[3]
    return [check("the warning shows with the key expiring in 20 days", "SMS Key Expiring" in soon.dialog_titles()
                  or soon.logged("SMS Key Expiring"), "no warning"),
            check("no warning with the key 40 days away", "SMS Key Expiring" not in later.dialog_titles()
                  and not later.logged("SMS Key Expiring"), "a warning showed")]


test("SS-006", "The SMS key expiry warning appears at start-up within 30 days of expiry, and not before",
     _ss006_runs, _ss006_check)


def _ss007_runs(home):
    # the base ini has no sender for email or SMS, so each is warned about at start-up, in the log window; the log is saved after
    return [run_spec(oc.fixtures_ops()), run_spec([oc.dumplog_op("log.txt"), oc.dump_op()])]


def _ss007_check(runs, home):
    path = os.path.join(runs[1].outdir, "log.txt")
    text = open(path, encoding="utf-8", errors="replace").read() if os.path.exists(path) else ""
    lines = [l for l in text.splitlines() if "sender" in l.lower() and "warning" in l.lower()]
    return [check("a warning for each blank sender is logged", len(lines) >= 2, "%d sender warning lines in the log" % len(lines)),
            check("the email warning names the blank section", any("[email-reminder]" in l for l in lines), "lines: %s" % lines[:2]),
            check("the SMS warning names the blank section", any("[sms-reminder]" in l for l in lines), "lines: %s" % lines[:2])]


test("SS-007", "Start-up warns for each blank sender and logs it", _ss007_runs, _ss007_check)


def _backup_dir(home):
    return os.path.join(oc.data_folder(home), "backup")


def _zips(home):
    return sorted(os.path.basename(p) for p in glob.glob(os.path.join(_backup_dir(home), "*-backup.zip")))


def _ss008_runs(home):
    # run 2 is the first start of the day (one backup), run 3 the second (none).  Then the backup date is set back, so
    # each later start makes a backup: run 5 and run 7 make two more, and only the newest two are kept.
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.dump_op()]),
            run_spec([oc.dump_op()]),
            run_spec([oc.setting_op("backupdate", "2000-01-01"), oc.dump_op("d4.json")]),
            run_spec([oc.dump_op()]),
            run_spec([oc.setting_op("backupdate", "2000-01-02"), oc.dump_op("d6.json")]),
            run_spec([oc.dump_op()])]


_ss008_counts = {}


def _ss008_check(runs, home):
    zips = _zips(home)
    return [check("one backup on the first start of the day, and none on the second",
                  _ss008_counts.get("after_second", -1) == 1, "zips after the second start: %s" % _ss008_counts),
            check("the newest two backups are kept after a third backup", len(zips) == 2, "zips: %s" % zips)]


def _ss008_runs_wrap(home):
    specs = _ss008_runs(home)
    _ss008_counts.clear()

    def before_run4(h):
        _ss008_counts["after_second"] = len(_zips(h))   # run 3 has finished: the zips it left
    specs[3]["before"] = before_run4
    return specs


test("SS-008", "The first start on a day makes one backup zip, and a second start that day makes none",
     _ss008_runs_wrap, _ss008_check)


def _ss009_runs(home):
    ops = oc.fixtures_ops() + [oc.appt_op(3, "SS009 appointment", oc.at(20, "09:00"), oc.at(20, "10:00")),
                               oc.notes_op(3, "SS009 to-do", "SS009 history entry"), oc.save_op()]
    return [run_spec(ops), run_spec([oc.dump_op()])]


def _ss009_check(runs, home):
    r = runs[1]
    c3 = r.contact(oc.surname(3))
    return [check("six contacts after the restart", len(r.contacts()) == 6, "%d" % len(r.contacts())),
            check("the appointment after the restart", len(r.appointments()) == 1, "%d" % len(r.appointments())),
            check("the to-do item after the restart", (c3.get("todo") or "").strip() == "SS009 to-do", repr(c3.get("todo"))),
            check("the history entry after the restart", "SS009 history entry" in c3.get("history", ""),
                  repr(c3.get("history")))]


test("SS-009", "Saved data is there after a restart", _ss009_runs, _ss009_check)


def _ss010_runs(home):
    # unsaved edit; closing asks; the choice is Cancel, so the program stays open and the dump still runs
    return [run_spec(oc.fixtures_ops()),
            run_spec([oc.open_op(1, "o"), oc.form_set_op({"email": "unsaved.oneil@sgc.contacmanagertest"}),
                      oc.answer_op("Cancel"), oc.exit_op("ss010"), oc.dump_op()])]


def _ss010_check(runs, home):
    r = runs[1]
    return [check("the close was refused", r.value("ss010_exit_accepted") is False,
                  "exit accepted: %r" % r.value("ss010_exit_accepted")),
            check("the Cancel choice was made", r.logged('answered "Cancel"'), "no Cancel in the log"),
            check("the program is still running at the dump", r.dump().get("contacts") is not None, "no dump")]


test("SS-010", "Closing with unsaved edits asks Save, Discard or Cancel, and Cancel keeps the program open",
     _ss010_runs, _ss010_check)


def _ss011_runs(home):
    # one contact file is damaged after the first start; the second start must load the other five, report the damage,
    # and leave the damaged file as it was.  The state is kept in _holder, which the callback fills in.
    def corrupt(h):
        folder = os.path.join(oc.data_folder(h), "contact")
        name = sorted(f for f in os.listdir(folder) if f.endswith(".contact"))[0]
        path = os.path.join(folder, name)
        with open(path, "w", encoding="utf-8") as f:
            f.write("this is not a contact file\n")
        _holder["name"] = name
        _holder["path"] = path
        _holder["bytes"] = open(path, "rb").read()
    return [run_spec(oc.fixtures_ops()), run_spec([oc.dump_op()], before=corrupt)]


def _ss011_check(runs, home):
    r = runs[1]
    name = _holder.get("name", "")
    now = open(_holder["path"], "rb").read() if _holder.get("path") and os.path.exists(_holder["path"]) else None
    # the damaged file loads as an empty entry marked deleted, so only the live contacts are counted
    live = [c for c in r.contacts() if c.get("deleted") not in (True, "true")]
    return [check("the other five contacts load", len(live) == 5, "%d live contacts (%d entries)" % (len(live), len(r.contacts()))),
            # the program's own load(...) lines name every file it loads, damaged or not, so they do not count as a report
            check("the damaged file is reported (its name, or a contact-file message, is in the log)",
                  any(not ("load(" in l) and ((name and (name in l or os.path.splitext(name)[0] in l))
                                              or "contact file" in l.lower() or "could not" in l.lower())
                      for l in r.log.splitlines()), "no report of the damaged file"),
            check("the damaged file is not overwritten", now is not None and now == _holder.get("bytes"),
                  "the file changed or is gone")]


_holder = {}


test("SS-011", "An unreadable contact file is reported; the other contacts load and the file is not overwritten",
     _ss011_runs, _ss011_check)


def _ss012_runs(home):
    # no ini file at all: the program starts with its defaults, and the log says the file was not found
    def remove_ini(h):
        path = os.path.join(h, "TrumptonApps", "ContactManager.ini")
        if os.path.exists(path):
            os.remove(path)
    return [run_spec([oc.dump_op()], before=remove_ini)]


def _ss012_check(runs, home):
    r = runs[0]
    return [check("the program starts", r.logged("main window shown") and r.rc == 0, "exit %s" % r.rc),
            # the program reports it in an error dialog ("Unable to find ContactManager.ini"), and goes on with its defaults
            check("the program reports that the ini file was not found",
                  "Contact Manager Error" in r.dialog_titles() and r.logged("Unable to find ContactManager.ini"),
                  "dialogs: %s" % r.dialog_titles())]


test("SS-012", "Start with no ini file uses the defaults and reports that the file is missing", _ss012_runs, _ss012_check)


def _ss013_runs(home):
    # run 1 saves the fixtures, then the data folder is made read-only; run 2 starts on it, edits a contact and saves;
    # run 3 makes the folder writable again and reads the data back
    data = os.path.join(home, "TrumptonApps", "ContactManager")
    return [run_spec(oc.fixtures_ops() + [oc.exec_op("/bin/chmod", ["-R", "a-w", data])]),
            run_spec([oc.edit_contact_op(1, {"names": "Jack ReadOnly"}), oc.save_op(), oc.dump_op("after-save.json")]),
            run_spec([oc.exec_op("/bin/chmod", ["-R", "u+w", data]), oc.dump_op()])]


def _ss013_check(runs, home):
    r = runs[2]
    return [check("the program starts on the read-only folder and shows its data",
                  r.contact(oc.surname(1)).get("names") is not None, "no contact read back"),
            check("the edit saved to the read-only folder is not stored",
                  r.contact(oc.surname(1)).get("names") != "Jack ReadOnly",
                  "names read back: %r" % r.contact(oc.surname(1)).get("names")),
            check("the stored contacts are still there", len(r.contacts()) == 6,
                  "%d contacts read back" % len(r.contacts())),
            check("the failed save is reported to the user",
                  any("dialog" in l.lower() and ("save" in l.lower() or "read" in l.lower() or "error" in l.lower())
                      for l in runs[1].log.splitlines()),
                  "no error dialog about the save in run 2")]


test("SS-013", "Start with a read-only data folder reports the error and loses no data", _ss013_runs, _ss013_check)


def _ss014_runs(home):
    # run 1: a contact is saved and another is edited without saving; run 2 is killed while it runs; run 3 restarts
    edits = [oc.edit_contact_op(1, {"names": "Jack Saved"}), oc.save_op(),
             oc.edit_contact_op(2, {"email": "unsaved.carter@sgc.contacmanagertest"})]
    return [run_spec(oc.fixtures_ops()),
            run_spec(edits + [oc.wait_op(60000)], kill=8),
            run_spec([oc.dump_op()])]


def _ss014_check(runs, home):
    r = runs[2]
    return [check("the saved edit is there after the kill", r.contact(oc.surname(1)).get("names") == "Jack Saved",
                  "names %r" % r.contact(oc.surname(1)).get("names")),
            check("the unsaved edit is not", r.contact(oc.surname(2)).get("email") != "unsaved.carter@sgc.contacmanagertest",
                  "the unsaved email is there")]


test("SS-014", "Data saved before a crash is there after it; unsaved edits are not", _ss014_runs, _ss014_check)


def _ss015_runs(home):
    # the sync fails at its first read (the fault plan closes the connection), which is what a network loss looks like
    ops = [{"op": "add_contact", "fields": {"surname": "Network", "names": "Test"}},
           oc.save_op(), {"op": "sync"}, oc.dump_op()]
    return [run_spec(oc.fixtures_ops()), run_spec(ops, env={"CM_TEST_FAULTS": "r1=closed"})]


def _ss015_check(runs, home):
    r = runs[1]
    return [check("the program starts", r.logged("main window shown"), "no main window"),
            check("the contact is added", len(r.contacts()) == 7, "%d contacts" % len(r.contacts())),
            check("the sync reports the error", r.logged("closed") or r.logged("google") or r.logged("error"),
                  "no sync error in the log")]


test("SS-015", "With no network, the program starts and works, and a sync reports the error", _ss015_runs, _ss015_check)
