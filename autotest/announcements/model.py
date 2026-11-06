#
# The announcement tests (area 2, section 2 of documents/test-specification.md): AN-001 to AN-048.
#
# This file is the plan.  For each test it gives:
#   events   the operations on the program (appointments, edits, deletions, contact changes, shutdowns, checks), by time
#   expect   the messages the test expects, each with its recipient, channel, kind, the time it is due, and the appointment
#            time quoted in it.  A message is on time when it is sent between the due time and one check later (six
#            minutes at 360 times real time, specification 0.7). A confirmation may be up to CONFIRM_LATE (15 minutes) late.
#   groups   counts expected inside a time window, for tests where the messages come in batches and their order is not known
#   refused  the refused sends expected (the stub refuses them, and the program retries them)
#   logs     text the program's log must (or must not) hold at the end of the run
#   dialogs  dialog titles the driver must (or must not) have met, by run and segment
#   history  text in a contact's history at the end of the run, and how many times it appears
#   quiet    times at which the run must send nothing
#
# Expected messages come from the rules in specification section 5 (5.1 to 5.12), worked out for each test's appointment.
# They are not taken from a simulation of the program, so that the program is checked against the rules.  Every message the
# rules produce for a test's appointment is listed, including follow-ups (a reminder, the on-the-day message), so that the
# run as a whole (AN-047) can be checked.  Where the test's own procedure in the specification does not match those rules,
# the test says so in its note (see readme.md, "Where the specification and the rules disagree").
#
# Times are local London time (the program's clock), written as naive datetimes.  A run's events are placed in its segments
# (one process each; a segment that ends with an exit is a shutdown, and the next segment is a restart).
#

import datetime as dt
import os
import sys
import zoneinfo

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
UTIL = os.path.join(REPO, "autotest", "utilities")
sys.path.insert(0, UTIL)
import testaccount as ta                       # noqa: E402

FIX = ta.load_fixtures()
LONDON = zoneinfo.ZoneInfo("Europe/London")
UTC = dt.timezone.utc
SPEED = 360                                    # 10 real seconds = 1 hour
CHECK = dt.timedelta(minutes=6)                # one check of the program at 360 times (specification 0.7)
CONFIRM_LATE = dt.timedelta(minutes=15)        # a confirmation may arrive up to this long after its time (the bench's rule)
H = dt.timedelta(hours=1)
M5 = dt.timedelta(minutes=5)
M6 = dt.timedelta(minutes=6)
DAY = dt.timedelta(days=1)
SENDER_EMAIL = "Bench Clinic <bench@test.invalid>"
SENDER_SMS = "Bench Clinic"
NOV2 = dt.datetime(2026, 11, 2)                # day 0 of the main runs: Monday
DEC30 = dt.datetime(2026, 12, 30)              # for the New Year test
MAR27 = dt.datetime(2027, 3, 27)               # for the clock-change test (the change is at 01:00 on 28 March)

KIND = {"confirm": "Confirmation", "update": "Updated", "cancel": "Cancelled", "reminder": "Reminder", "today": "Today"}


# ------------------------------------------------------------------------------------------------ the contacts

def digits(text):
    return "".join(ch for ch in str(text) if ch.isdigit())


CONTACT = {}
for _c in FIX["contacts"]:
    CONTACT[_c["number"]] = dict(
        names=_c["given"], surname=_c["surname"], email=_c["email"], mobile=_c["phone"],
        emailme=("email" in _c["channels"]), textme=("sms" in _c["channels"]))


def address(n, channel):
    """The address a message to contact n on this channel goes to, as the stub records it."""
    if channel == "email":
        return CONTACT[n]["email"]
    return digits(CONTACT[n]["mobile"])


def contact_fields(n, **change):
    c = CONTACT[n]
    fields = {"names": c["names"], "surname": c["surname"], "email": c["email"], "mobile": c["mobile"],
              "emailme": "true" if c["emailme"] else "false", "textme": "true" if c["textme"] else "false"}
    fields.update(change)
    return fields


# ------------------------------------------------------------------------------------------------ times and operations

def T(base, day, hhmm):
    """A local time: day days after the base (which is midnight), at hhmm."""
    h, m = (int(x) for x in hhmm.split(":"))
    return base + dt.timedelta(days=day, hours=h, minutes=m)


def utc_text(naive_local):
    """The local time as the program's date-time text in UTC (the London clock, in the right zone for the date)."""
    aware = naive_local.replace(tzinfo=LONDON).astimezone(UTC)
    return aware.strftime("%Y-%m-%dT%H:%M:%SZ")


def local_text(naive_local):
    return naive_local.strftime("%Y-%m-%dT%H:%M:%S")


def remind_when(occ):
    """A reminder is due 24 elapsed hours before the appointment (specification 5.1), in local time."""
    aware = occ.replace(tzinfo=LONDON).astimezone(UTC) - dt.timedelta(hours=24)
    return aware.astimezone(LONDON).replace(tzinfo=None)


def on_the_day(occ):
    """The on-the-day message is due from the send window's opening (07:00 local) on the day of the appointment."""
    return dt.datetime.combine(occ.date(), dt.time(7, 0))


def op_add(n, summary, start, end, repeat=None, kind="2"):
    # kind: the repeat type (1 daily, 2 weekly, as appointment.h); repeat: the interval in those units
    fields = {"summary": summary, "from": utc_text(start), "to": utc_text(end), "location": ""}
    if repeat:
        fields["repeat"] = kind
        fields["repeatinterval"] = str(repeat)
    return {"op": "add_appointment", "surname": CONTACT[n]["surname"], "fields": fields}


def op_move(summary, start, end):
    return {"op": "edit_appointment", "match": {"summary": summary},
            "fields": {"from": utc_text(start), "to": utc_text(end)}}


def op_rename(summary, new):
    return {"op": "edit_appointment", "match": {"summary": summary}, "fields": {"summary": new}}


def op_delete(summary):
    return {"op": "delete_appointment", "match": {"summary": summary}}


def op_delete_occurrence(summary, occ):
    """Deletes one occurrence of a repeat: 'Delete just this one' (letter a in the program's choice)."""
    return [{"op": "choose", "letter": "a"},
            {"op": "repeat_delete", "summary": summary, "occurrence": local_text(occ)}]


def op_contact(n, **change):
    return {"op": "edit_contact", "match": {"surname": CONTACT[n]["surname"]}, "fields": contact_fields(n, **change)}


def op_hide(n):
    return {"op": "hide_contact", "match": {"surname": CONTACT[n]["surname"]}}


def op_delete_contact(n):
    return {"op": "delete_contact", "match": {"surname": CONTACT[n]["surname"]}}


def op_setting(key, value):
    return {"op": "setting", "key": key, "value": value}


def contacts_setup(base):
    """The six fixture contacts, at the start of a run."""
    return [(base, {"op": "add_contact", "fields": contact_fields(n)}) for n in CONTACT]


# ------------------------------------------------------------------------------------------------ expectations

def expect(code, channel, n, kind, when, occ, must_not=None):
    return dict(code=code, channel=channel, to=address(n, channel), kind=KIND[kind], when=when, occ=occ,
                must_not=must_not)


def conf(code, channel, n, when, occ, **kw):
    return expect(code, channel, n, "confirm", when, occ, **kw)


def upd(code, channel, n, when, occ, **kw):
    return expect(code, channel, n, "update", when, occ, **kw)


def canc(code, channel, n, when, occ, **kw):
    return expect(code, channel, n, "cancel", when, occ, **kw)


def rem(code, channel, n, occ, **kw):
    return expect(code, channel, n, "reminder", remind_when(occ), occ, **kw)


def tod(code, channel, n, occ, **kw):
    return expect(code, channel, n, "today", on_the_day(occ), occ, **kw)


def group(code, channel, kind, start, end, count):
    return dict(code=code, channel=channel, kind=KIND[kind], start=start, end=end, count=count)


def test(test_id, title, run, build, **extra):
    """A test: its title, the run it is in, and the function that gives its events and expectations (given the run's base)."""
    return dict(id=test_id, title=title, run=run, build=build, **extra)


# ------------------------------------------------------------------------------------------------ the tests: run A (plain tests)

def an001(B):
    occ, cr = T(B, 3, "10:00"), T(B, 0, "10:00")
    return dict(events=[(cr, op_add(1, "AN-001 Jack confirm", occ, occ + H))],
                expect=[conf("AN-001", "email", 1, cr + M5, occ), rem("AN-001", "email", 1, occ),
                        tod("AN-001", "email", 1, occ)])


def an002(B):
    occ, cr = T(B, 3, "10:00"), T(B, 0, "10:00")
    return dict(events=[(cr, op_add(3, "AN-002 Daniel text", occ, occ + H))],
                expect=[conf("AN-002", "sms", 3, cr + M5, occ), rem("AN-002", "sms", 3, occ),
                        tod("AN-002", "sms", 3, occ)])


def an003(B):
    # Both channels: the confirmation is by email only.  The on-the-day messages go to each channel (5.1, 5.3), so the
    # text on the day is expected; the test's "no text" is about the confirmation (see the note).
    occ, cr = T(B, 3, "10:00"), T(B, 0, "10:00")
    return dict(events=[(cr, op_add(5, "AN-003 George email", occ, occ + H))],
                expect=[conf("AN-003", "email", 5, cr + M5, occ), rem("AN-003", "email", 5, occ),
                        tod("AN-003", "email", 5, occ), tod("AN-003", "sms", 5, occ)],
                note="AN-003 says 'no text'. The rules send the on-the-day text to a contact with both channels (5.3), so "
                     "that text is expected here; the confirmation is by email only.")


def an004(B):
    occ, cr = T(B, 2, "12:00"), T(B, 1, "10:00")
    return dict(events=[(cr, op_add(2, "AN-004 Samantha on the day", occ, occ + H))],
                expect=[conf("AN-004", "email", 2, cr + M5, occ), tod("AN-004", "email", 2, occ)])


def an005(B):
    occ, cr = T(B, 2, "12:00"), T(B, 1, "10:00")
    return dict(events=[(cr, op_add(4, "AN-005 Tealc on the day", occ, occ + H))],
                expect=[conf("AN-005", "sms", 4, cr + M5, occ), tod("AN-005", "sms", 4, occ)])


def an006(B):
    occ, cr = T(B, 2, "12:00"), T(B, 1, "10:00")
    return dict(events=[(cr, op_add(6, "AN-006 Vala both", occ, occ + H))],
                expect=[conf("AN-006", "email", 6, cr + M5, occ), tod("AN-006", "email", 6, occ),
                        tod("AN-006", "sms", 6, occ)])


def an007(B):
    occ, cr = T(B, 3, "15:00"), T(B, 0, "10:00")
    return dict(events=[(cr, op_add(2, "AN-007 Samantha reminder", occ, occ + H))],
                expect=[conf("AN-007", "email", 2, cr + M5, occ), rem("AN-007", "email", 2, occ),
                        tod("AN-007", "email", 2, occ)])


def an008(B):
    occ, cr = T(B, 3, "15:00"), T(B, 2, "09:00")
    return dict(events=[(cr, op_add(4, "AN-008 Tealc confirm only", occ, occ + H))],
                expect=[conf("AN-008", "sms", 4, cr + M5, occ), tod("AN-008", "sms", 4, occ)],
                note="Confirm-only (5.6): no reminder. The on-the-day text on day 3 is still due (5.1).")


def an009(B):
    occ, cr = T(B, 3, "10:00"), T(B, 0, "10:00")
    final = T(B, 3, "11:00")
    return dict(events=[(cr, op_add(1, "AN-009 Jack edits", occ, occ + H)),
                        (T(B, 0, "10:02"), op_move("AN-009 Jack edits", T(B, 3, "10:30"), T(B, 3, "11:30"))),
                        (T(B, 0, "10:04"), op_move("AN-009 Jack edits", final, final + H))],
                expect=[conf("AN-009", "email", 1, T(B, 0, "10:09"), final),
                        rem("AN-009", "email", 1, final), tod("AN-009", "email", 1, final)])


def an010(B):
    occ, cr = T(B, 3, "10:00"), T(B, 0, "10:00")
    return dict(events=[(cr, op_add(1, "AN-010 Jack revert", occ, occ + H)),
                        (T(B, 0, "10:02"), op_move("AN-010 Jack revert", T(B, 3, "11:00"), T(B, 3, "12:00"))),
                        (T(B, 0, "10:04"), op_move("AN-010 Jack revert", occ, occ + H))],
                expect=[conf("AN-010", "email", 1, T(B, 0, "10:09"), occ),
                        rem("AN-010", "email", 1, occ), tod("AN-010", "email", 1, occ)])


def an011(B):
    occ, cr = T(B, 3, "10:00"), T(B, 0, "10:00")
    final = T(B, 3, "12:00")
    return dict(events=[(cr, op_add(1, "AN-011 Jack update", occ, occ + H)),
                        (T(B, 0, "11:00"), op_move("AN-011 Jack update", T(B, 3, "11:00"), T(B, 3, "12:00"))),
                        (T(B, 0, "11:02"), op_move("AN-011 Jack update", final, final + H))],
                expect=[conf("AN-011", "email", 1, cr + M5, occ), upd("AN-011", "email", 1, T(B, 0, "11:07"), final),
                        rem("AN-011", "email", 1, final), tod("AN-011", "email", 1, final)])


def an012(B):
    occ, cr = T(B, 3, "10:00"), T(B, 0, "10:00")
    return dict(events=[(cr, op_add(1, "AN-012 Jack cancel", occ, occ + H)),
                        (T(B, 0, "11:00"), op_delete("AN-012 Jack cancel"))],
                expect=[conf("AN-012", "email", 1, cr + M5, occ), canc("AN-012", "email", 1, T(B, 0, "11:00"), occ)])


def an013(B):
    occ, cr = T(B, 3, "10:00"), T(B, 0, "10:00")
    return dict(events=[(cr, op_add(1, "AN-013 Jack none", occ, occ + H)),
                        (T(B, 0, "10:01"), op_delete("AN-013 Jack none"))],
                expect=[])


def an014(B):
    occ, cr = T(B, 3, "10:00"), T(B, 0, "10:00")
    return dict(events=[(cr, op_add(1, "AN-014 Jack title", occ, occ + H)),
                        (T(B, 0, "11:00"), op_rename("AN-014 Jack title", "AN-014 Jack title changed"))],
                expect=[conf("AN-014", "email", 1, cr + M5, occ), rem("AN-014", "email", 1, occ),
                        tod("AN-014", "email", 1, occ)])


def an015(B):
    first, cr = T(B, 2, "10:00"), T(B, 0, "10:00")
    return dict(events=[(cr, op_add(2, "AN-015 Samantha weekly", first, first + H, repeat=1))]
                        + [(T(B, 0, "11:00"), op) for op in op_delete_occurrence("AN-015 Samantha weekly", first)],
                expect=[conf("AN-015", "email", 2, cr + M5, first), canc("AN-015", "email", 2, T(B, 0, "11:00"), first)],
                note="Only the first occurrence is in the run; the later ones are after its end. The reminder and the "
                     "on-the-day message for the removed date are not sent.")


def an017(B):
    occ, cr = T(B, 1, "12:00"), T(B, 0, "21:30")
    return dict(events=[(cr, op_add(1, "AN-017 Jack late", occ, occ + H))],
                expect=[conf("AN-017", "email", 1, cr + M5, occ), tod("AN-017", "email", 1, occ)],
                note="The specification says the confirmation is sent at 07:00. Created at 21:30 with the five-minute hold, "
                     "the confirmation is due at 21:35, inside the send window (5.5), so the rules send it then. The "
                     "specification row needs correcting: the expectation here follows the rules.")


def an018(B):
    occ, cr = T(B, 2, "23:00"), T(B, 0, "10:00")
    return dict(events=[(cr, op_add(1, "AN-018 Jack silence", occ, occ + dt.timedelta(minutes=30)))],
                expect=[conf("AN-018", "email", 1, cr + M5, occ), tod("AN-018", "email", 1, occ)],
                note="The reminder is due at 23:00 on day 1, in the silent period. At 07:00 on day 2 the appointment is "
                     "on that day, and a reminder is not sent on the day (5.1), so the message at 07:00 is the on-the-day "
                     "message. The specification says 'reminder'; the rules say 'on the day'.")


def an019(B):
    cr = T(B, 0, "19:55")
    occ = T(B, 3, "10:00")
    return dict(events=[(cr, op_add(1, "AN-019 Jack overnight", occ, occ + H)),
                        (T(B, 0, "21:00"), op_move("AN-019 Jack overnight", T(B, 3, "10:30"), T(B, 3, "11:30"))),
                        (T(B, 0, "22:30"), op_move("AN-019 Jack overnight", T(B, 3, "10:45"), T(B, 3, "11:45"))),
                        (T(B, 0, "23:30"), op_move("AN-019 Jack overnight", T(B, 3, "11:00"), T(B, 3, "12:00")))],
                expect=[conf("AN-019", "email", 1, T(B, 0, "20:00"), occ),
                        upd("AN-019", "email", 1, T(B, 1, "07:00"), T(B, 3, "11:00")),
                        rem("AN-019", "email", 1, T(B, 3, "11:00")), tod("AN-019", "email", 1, T(B, 3, "11:00"))])


def an020(B):
    occ, cr = T(B, 3, "10:00"), T(B, 0, "21:55")
    return dict(events=[(cr, op_add(1, "AN-020 Jack window edge", occ, occ + H))],
                expect=[conf("AN-020", "email", 1, T(B, 1, "07:00"), occ), rem("AN-020", "email", 1, occ),
                        tod("AN-020", "email", 1, occ)])


def an021(B):
    occ, cr = T(B, 3, "10:00"), T(B, 0, "06:55")
    return dict(events=[(cr, op_add(1, "AN-021 Jack window open", occ, occ + H))],
                expect=[conf("AN-021", "email", 1, T(B, 0, "07:00"), occ), rem("AN-021", "email", 1, occ),
                        tod("AN-021", "email", 1, occ)])


def an024(B):
    occ, cr = T(B, 1, "10:00"), T(B, 1, "10:30")
    return dict(events=[(cr, op_add(1, "AN-024 Jack started", occ, occ + H))], expect=[])


def an025(B):
    occ = T(B, -1, "10:00")
    return dict(events=[(B, op_add(1, "AN-025 Jack ended", occ, occ + H))], expect=[])


# ------------------------------------------------------------------------------------------------ run B: contact state

def an026(B):
    cr, occ = T(B, 0, "09:00"), T(B, 3, "10:00")
    return dict(events=[(cr, op_add(3, "AN-026 Daniel deleted", occ, occ + H)),
                        (T(B, 0, "09:02"), op_delete_contact(3))], expect=[])


def an027(B):
    occ = T(B, 3, "10:00")
    return dict(events=[(T(B, 0, "09:00"), op_hide(4)),
                        (T(B, 0, "10:00"), op_add(4, "AN-027 Tealc hidden", occ, occ + H))], expect=[])


def an028(B):
    occ, cr = T(B, 3, "10:00"), T(B, 0, "10:00")
    return dict(events=[(cr, op_add(5, "AN-028 George channel", occ, occ + H)),
                        (T(B, 0, "11:00"), op_contact(5, emailme="false", textme="true"))],
                expect=[conf("AN-028", "email", 5, cr + M5, occ), rem("AN-028", "sms", 5, occ),
                        tod("AN-028", "sms", 5, occ)])


def an029(B):
    occ = T(B, 3, "10:00")
    return dict(events=[(T(B, 0, "09:00"), op_contact(6, emailme="false", textme="false")),
                        (T(B, 0, "10:00"), op_add(6, "AN-029 Vala neither", occ, occ + H))], expect=[])


def an030(B):
    occ, cr = T(B, 3, "10:00"), T(B, 0, "10:00")
    return dict(events=[(T(B, 0, "09:00"), op_contact(1, email="", textme="true")),
                        (cr, op_add(1, "AN-030 Jack fallback", occ, occ + H))],
                expect=[conf("AN-030", "sms", 1, cr + M5, occ), rem("AN-030", "sms", 1, occ),
                        tod("AN-030", "sms", 1, occ)])


# ------------------------------------------------------------------------------------------------ runs C1 to C5: restarts

def an031(B):
    occ, cr = T(B, 3, "10:00"), T(B, 0, "10:00")
    return dict(events=[(cr, op_add(1, "AN-031 Jack restart", occ, occ + H))],
                expect=[conf("AN-031", "email", 1, T(B, 0, "10:20"), occ), rem("AN-031", "email", 1, occ),
                        tod("AN-031", "email", 1, occ)],
                note="The shutdown is at 10:02 and the restart at 10:20 (segments of run C1). The confirmation is due "
                     "at the restart, once the hold has passed.")


def an032(B):
    occ, cr = T(B, 1, "15:00"), T(B, 0, "10:00")
    return dict(events=[(cr, op_add(3, "AN-032 Daniel shutdown", occ, occ + H))],
                expect=[conf("AN-032", "sms", 3, cr + M5, occ), tod("AN-032", "sms", 3, occ)],
                note="The program is off from 11:00 on day 0 to 06:00 on day 1 (segments of run C2). No reminder is sent "
                     "for the missed reminder time (5.8).")


def an033(B):
    occ, cr = T(B, 1, "15:00"), T(B, 0, "10:00")
    return dict(events=[(cr, op_add(6, "AN-033 Vala restart", occ, occ + H))],
                expect=[conf("AN-033", "email", 6, cr + M5, occ), tod("AN-033", "email", 6, occ),
                        tod("AN-033", "sms", 6, occ)],
                note="The on-the-day messages are sent at 07:00 on day 1, before the restart at 12:00 (run C3). None is "
                     "sent again after the restart.")


def an034(B):
    cr = T(B, 0, "10:00")
    events, expected = [], []
    for i in range(10):
        occ0 = T(B, 5, "%02d:00" % (9 + i))
        events.append((cr, op_add(2, "AN-034 Samantha %02d" % (i + 1), occ0, occ0 + H)))
    # the edits are made while the send window is closed (before 07:00 on day 3), when no message can be sent
    t = T(B, 3, "00:00")
    minute = 0
    for i in range(10):
        summary = "AN-034 Samantha %02d" % (i + 1)
        for k in range(19):                                # nineteen changes, each a new time on day 5
            minute += 1
            events.append((t + dt.timedelta(minutes=minute),
                           op_move(summary, T(B, 5, "%02d:%02d" % (9 + i, k % 60)),
                                   T(B, 5, "%02d:%02d" % (9 + i, k % 60)) + H)))
        final = T(B, 4, "%02d:00" % (8 + i))            # the twentieth change is the final time
        minute += 1
        events.append((t + dt.timedelta(minutes=minute), op_move(summary, final, final + H)))
        expected.append(conf("AN-034", "email", 2, T(B, 3, "07:00"), final))
        expected.append(tod("AN-034", "email", 2, final))
    return dict(events=events, expect=expected,
                note="Twenty changes to each of ten appointments, made before the send window opens on day 3 (the "
                     "program's silent period), then the program runs. One confirmation per appointment, as 5.8 requires.")


def an043(B):
    return dict(events=[], expect=[],
                dialogs=[(("C5", 0), "SMS Key Expiring", True), (("C5", 1), "SMS Key Expiring", False)],
                note="Run C5 has two starts: the key expires in 29 days (the warning shows), then in 31 days (it does not).")


# ------------------------------------------------------------------------------------------------ runs L1 and L2: limits

def an022(B):
    cr = T(B, 0, "23:00")
    events = [(cr, op_add(6, "AN-022 Vala %02d" % (i + 1), T(B, 3, "%02d:00" % (10 + i)), T(B, 3, "%02d:00" % (10 + i)) + H))
              for i in range(12)]
    return dict(events=events, expect=[],
                groups=[group("AN-022", "email", "confirm", T(B, 1, "07:00"), T(B, 1, "07:06"), 5),
                        group("AN-022", "email", "confirm", T(B, 1, "08:00"), T(B, 1, "08:06"), 5),
                        group("AN-022", "email", "confirm", T(B, 2, "07:00"), T(B, 2, "07:06"), 2)],
                note="Default limits: five an hour, ten a day (5.7). Twelve confirmations for one contact: five at 07:00, "
                     "five at 08:00 on day 1, and the last two once the first batch leaves the 24-hour count.")


def an023(B):
    events = []
    for i in range(5):
        events.append((T(B, 1, "06:55"), op_add(2, "AN-023 Samantha A%d" % (i + 1), T(B, 3, "%02d:00" % (10 + i)),
                                               T(B, 3, "%02d:00" % (10 + i)) + H)))
    for i in range(5):
        events.append((T(B, 1, "08:25"), op_add(2, "AN-023 Samantha B%d" % (i + 1), T(B, 3, "%02d:00" % (15 + i)),
                                               T(B, 3, "%02d:00" % (15 + i)) + H)))
    occ = T(B, 3, "20:00")
    events.append((T(B, 1, "12:30"), op_add(2, "AN-023 Samantha C1", occ, occ + H)))
    return dict(events=events, expect=[],
                groups=[group("AN-023", "email", "confirm", T(B, 1, "07:00"), T(B, 1, "07:06"), 5),
                        group("AN-023", "email", "confirm", T(B, 1, "08:30"), T(B, 1, "08:36"), 5),
                        group("AN-023", "email", "confirm", T(B, 2, "07:00"), T(B, 2, "07:06"), 1)],
                note="Default limits. The program restarts at 12:00 on day 1 (segments of run L2). The eleventh message "
                     "waits until the first batch leaves the 24-hour count, at 07:00 on day 2.")


# ------------------------------------------------------------------------------------------------ runs D1 and D2: dates

def an045(B):
    occ = dt.datetime(2027, 3, 28, 12, 0)             # 12:00 British Summer Time, the day after the change
    cr = dt.datetime(2027, 3, 27, 10, 0)
    return dict(events=[(cr, op_add(1, "AN-045 Jack clock", occ, occ + H))],
                expect=[conf("AN-045", "email", 1, cr + M5, occ), rem("AN-045", "email", 1, occ),
                        tod("AN-045", "email", 1, occ)],
                note="The reminder is 24 elapsed hours before: 11:00 on 27 March, one hour earlier than wall-clock 24 "
                     "hours, because the clock goes forward at 01:00 on 28 March.")


def an046(B):
    occ = dt.datetime(2027, 1, 1, 10, 0)
    cr = dt.datetime(2026, 12, 30, 10, 0)
    return dict(events=[(cr, op_add(4, "AN-046 Tealc new year", occ, occ + H))],
                expect=[conf("AN-046", "sms", 4, cr + M5, occ), rem("AN-046", "sms", 4, occ),
                        tod("AN-046", "sms", 4, occ)])


# ------------------------------------------------------------------------------------------------ runs G1 to G4: settings

def an036(B):
    occ, cr = T(B, 3, "10:00"), T(B, 0, "10:00")
    return dict(events=[(cr, op_add(1, "AN-036 Jack no sender", occ, occ + H))], expect=[],
                logs=[("sender", True)], history=[(1, "Sending confirmation", 0)],
                note="The email sender is blank in the ini file (run G1a).")


def an037(B):
    occ, cr = T(B, 3, "10:00"), T(B, 0, "10:00")
    return dict(events=[(cr, op_add(3, "AN-037 Daniel no sender", occ, occ + H))], expect=[],
                logs=[("sender", True)], history=[(3, "Sending confirmation", 0)],
                note="The text sender is blank in the ini file (run G1b).")


def an038(B):
    occ, cr = T(B, 1, "15:00"), T(B, 0, "10:00")
    return dict(events=[(cr, op_add(1, "AN-038 Jack fallback template", occ, occ + H))],
                expect=[conf("AN-038", "email", 1, cr + M5, occ),
                        expect("AN-038", "email", 1, "reminder", on_the_day(occ), occ)],
                note="emailtoday is blank in the ini file, so the reminder template is used on the day (run G2a). The "
                     "message's kind word is therefore 'Reminder'.")


def an039(B):
    occ, cr = T(B, 3, "10:00"), T(B, 0, "10:00")
    return dict(events=[(cr, op_add(1, "AN-039 Jack missing file", occ, occ + H))],
                expect=[rem("AN-039", "email", 1, occ), tod("AN-039", "email", 1, occ)],
                logs=[("could not load", True)],
                note="emailconfirmation points at a file that does not exist (run G2b). The confirmation is not sent; the "
                     "reminder and the on-the-day message use their own templates.")


def an040(B):
    occ, cr = T(B, 3, "10:00"), T(B, 0, "10:00")
    return dict(events=[(cr, op_add(1, "AN-040 Jack retry", occ, occ + H))],
                expect=[conf("AN-040", "email", 1, T(B, 0, "10:15"), occ), rem("AN-040", "email", 1, occ),
                        tod("AN-040", "email", 1, occ)],
                refused=[("AN-040", "Confirmation", 2)],
                history=[(1, "Sending confirmation", 1)],
                note="The stub refuses the first two sends to this address (run G3a). Retries are five minutes apart, "
                     "so with six-minute checks they fall at the next check after each wait.")


def an041(B):
    occ, cr = T(B, 3, "10:00"), T(B, 0, "10:00")
    return dict(events=[(cr, op_add(2, "AN-041 Samantha gives up", occ, occ + H))], expect=[],
                refused=[("AN-041", "Confirmation", 10), ("AN-041", "Reminder", 10), ("AN-041", "Today", 10)],
                logs=[("given up", True)],
                note="The stub refuses every send to this address (run G3b). Each message is tried ten times and then given "
                     "up. The log says 'Given up on ... after 10 tries' (mainwindow_timer_alert.cpp).")


def an042(B):
    occ, cr = T(B, 3, "10:00"), T(B, 0, "10:00")
    return dict(events=[(cr, op_add(3, "AN-042 Daniel refused", occ, occ + H))],
                expect=[conf("AN-042", "sms", 3, T(B, 0, "10:12"), occ), rem("AN-042", "sms", 3, occ),
                        tod("AN-042", "sms", 3, occ)],
                refused=[("AN-042", "Confirmation", 1)],
                logs=[("test reason", True)],
                history=[(3, "Sending confirmation", 1)],
                note="The stub refuses the first text, with a reason (run G3c). The reason must be in the log.")


def an044(B):
    occ, cr = T(B, 3, "10:00"), T(B, 0, "10:00")
    return dict(events=[(cr, op_add(3, "AN-044 Daniel two lines", occ, occ + H))],
                expect=[conf("AN-044", "sms", 3, cr + M5, occ),
                        rem("AN-044", "sms", 3, occ, must_not="Second line"),
                        tod("AN-044", "sms", 3, occ)],
                note="The text reminder template has two lines (run G2c). Only the first line is sent.")


def an048(B):
    off_occ, on_cr = T(B, 2, "12:00"), T(B, 3, "10:01")
    on_occ = T(B, 4, "12:00")
    return dict(events=[(T(B, 0, "10:00"), op_add(1, "AN-048 Jack off", off_occ, off_occ + H)),
                        (T(B, 3, "10:00"), op_setting("enablereminders", "yes")),
                        (on_cr, op_add(1, "AN-048 Jack on", on_occ, on_occ + H))],
                expect=[conf("AN-048", "email", 1, on_cr + M5, on_occ), tod("AN-048", "email", 1, on_occ)],
                note="Reminders are off for run G4 until day 3 at 10:00. The first appointment (day 2) is past by then.")


# ------------------------------------------------------------------------------------------------ run N: a second copy

def an035(B):
    return dict(events=[], expect=[],
                dialogs=[(("N", 0), "Messages Not Sent", True)],
                quiet=[("A", T(B, 1, "09:00"), T(B, 1, "10:00"))],
                note="A second copy runs on run A's data folder from 09:00 to 10:00 on day 1 (run N). It must show the "
                     "warning and send nothing: run A's messages in that hour must be none.")


# ------------------------------------------------------------------------------------------------ AN-016: a daily repeat

def an016(B):
    # Created at 09:00 on day 0 as a daily repeat, first on day 1 at 10:00, with occurrences on days 1, 2 and 3.
    # The last message is the on-the-day message at 07:00 on day 3, inside run A's three days.
    cr, first = T(B, 0, "09:00"), T(B, 1, "10:00")
    expected = [conf("AN-016", "email", 5, cr + M5, first)]
    # The first occurrence is made 25 hours ahead, so it is confirmed less than 36 hours before its start and gets no
    # reminder (the reminder rule, section 5.1). Its reminder is therefore not expected. The later two are reminded.
    for k in (1, 2, 3):
        occ = T(B, k, "10:00")
        if k > 1:
            expected.append(rem("AN-016", "email", 5, occ))
        expected.append(tod("AN-016", "email", 5, occ))
        expected.append(tod("AN-016", "sms", 5, occ))
    return dict(events=[(cr, op_add(5, "AN-016 George daily", first, first + H, repeat=1, kind="1"))],
                expect=expected,
                note="A daily repeat of three occurrences, in run A. The first occurrence is confirmed less than 36 hours "
                     "before its start, so it gets no reminder (5.1); the second and third are reminded once each. It was a "
                     "weekly repeat over three weeks, which needs sixteen virtual days, so it is shortened to fit run A.")


# ------------------------------------------------------------------------------------------------ the tests, and the runs

ALL = [
    # run A: plain tests (they share no state that another test relies on)
    test("AN-001", "An appointment made for contact 1 at 10:00 is confirmed by email at 10:05", "A", an001),
    test("AN-002", "An appointment made for contact 3 is confirmed by text at 10:05", "A", an002),
    test("AN-003", "An appointment for contact 5 (both channels) is confirmed by email only", "A", an003),
    test("AN-004", "On the day, an email-only contact gets one email from 07:00", "A", an004),
    test("AN-005", "On the day, a text-only contact gets one text from 07:00", "A", an005),
    test("AN-006", "On the day, a contact with both channels gets an email, then a text", "A", an006),
    test("AN-007", "A reminder is sent 24 hours before, once", "A", an007),
    test("AN-008", "An appointment made 30 hours before its start gets its confirmation only, with no reminder", "A", an008),
    test("AN-009", "Edits within the hold give one confirmation with the final details", "A", an009),
    test("AN-010", "An edit and its reversal within the hold send nothing", "A", an010),
    test("AN-011", "An edit after confirmation sends one update with the final details, after the hold", "A", an011),
    test("AN-012", "A cancellation after confirmation is sent at once, not held", "A", an012),
    test("AN-013", "An appointment created and deleted before confirmation sends nothing", "A", an013),
    test("AN-014", "A title or location change alone sends no update", "A", an014),
    test("AN-015", "A repeat occurrence removed after confirmation sends a cancellation for that date only", "A", an015),
    test("AN-017", "An appointment made at 21:30 is confirmed at 07:00, when the window opens", "A", an017),
    test("AN-018", "A reminder due in the silent period is sent once, at 07:00", "A", an018),
    test("AN-019", "Edits made overnight send one update at 07:00, with the final times", "A", an019),
    test("AN-020", "A send due at 22:00 waits until 07:00", "A", an020),
    test("AN-021", "A message due at 07:00 is sent at 07:00", "A", an021),
    test("AN-024", "An appointment already started gets no reminder", "A", an024),
    test("AN-025", "No message is sent for a past appointment", "A", an025),
    test("AN-047", "Run A, as a whole: every expected message of the run is sent once, on time, and nothing unexpected is sent",
         "A", None, aggregate=True),
    # run B: contact state (each test uses its own contact)
    test("AN-026", "A deleted contact's appointments send no messages", "B", an026),
    test("AN-027", "A hidden contact's appointments send no messages", "B", an027),
    test("AN-028", "A channel changed after confirmation sends only what is pending on the new channel, and nothing again",
         "B", an028),
    test("AN-029", "A contact with neither channel ticked is sent nothing", "B", an029),
    test("AN-030", "A confirmation falls back to text when email is ticked but the contact has no email address", "B", an030),
    # restarts: one run each
    test("AN-031", "A shutdown during a hold sends the message after restart, with the current details", "C1", an031),
    test("AN-032", "A reminder missed while the program was off is not sent late once the appointment is on that day", "C2",
         an032),
    test("AN-033", "A restart in the middle of a day does not repeat the on-the-day messages", "C3", an033),
    test("AN-034", "Three days off with many edits send one message per appointment, not one per change", "C4", an034),
    test("AN-043", "The SMS key expiry warning appears at start-up within 30 days of expiry, and not before", "C5", an043),
    # limits
    test("AN-022", "Twelve appointments for contact 6 created overnight send five at 07:00, five at 08:00, and the rest once "
                   "the daily limit clears", "L1", an022),
    test("AN-023", "The limits hold across a restart", "L2", an023),
    # dates
    test("AN-045", "A clock change in March keeps the reminder at 24 elapsed hours, and sends in local time", "D1", an045),
    test("AN-046", "A reminder and on-the-day message across New Year are sent on the right dates", "D2", an046),
    # settings and senders, templates, sending failures: one run for each change to the ini file, the templates or the stub
    test("AN-036", "A blank email sender sends no email, logs 'sender missing', and adds nothing to history", "G1a", an036),
    test("AN-037", "A blank text sender sends no text, logs 'sender missing', and adds nothing to history", "G1b", an037),
    test("AN-038", "With no on-the-day template, the reminder template is used on the day", "G2a", an038),
    test("AN-039", "A template file that cannot be read sends nothing, and the log says why", "G2b", an039),
    test("AN-044", "A multi-line text template sends only its first line", "G2c", an044),
    test("AN-040", "A send refused by the transport is retried after 5 minutes, and recorded once when it succeeds", "G3a",
         an040),
    test("AN-041", "A send that always fails is given up after 10 tries, and the log says so", "G3b", an041),
    test("AN-042", "A text refused by the transport is not recorded as sent, and the reason is logged", "G3c", an042),
    test("AN-048", "Setup's 'Enable SMS and Email Reminders' off stops all automatic messages", "G4", an048),
    # a second copy, alongside run A
    test("AN-035", "A second copy on the same data folder sends no messages and shows the warning", "N", an035),
    # a daily repeat over three days, in run A
    test("AN-016", "Each occurrence of a repeat is reminded once", "A", an016),
]


def _run(rid, base, end, tests, **opts):
    return dict(id=rid, base=base, end=end, tests=tests, **opts)


# Each run: its base (midnight of day 0), its end, the tests in it, its settings, and its segments (one process each).
# A segment is (name, start, end, conf overrides).  The shutdown at a segment's end is an exit; the next segment starts
# with a new process and its clock at the segment's start.  Events are placed in the segment they fall in.
RUNS = [
    _run("A", NOV2, T(NOV2, 3, "09:00"), ["AN-001", "AN-002", "AN-003", "AN-004", "AN-005", "AN-006", "AN-007", "AN-008",
                                           "AN-009", "AN-010", "AN-011", "AN-012", "AN-013", "AN-014", "AN-015", "AN-016", "AN-017",
                                           "AN-018", "AN-019", "AN-020", "AN-021", "AN-024", "AN-025", "AN-047"],
         limits="raised", segments=[("A", NOV2, T(NOV2, 3, "09:00"), {})]),
    _run("B", NOV2, T(NOV2, 3, "09:00"), ["AN-026", "AN-027", "AN-028", "AN-029", "AN-030"], limits="raised",
         segments=[("B", NOV2, T(NOV2, 3, "09:00"), {})]),
    _run("C1", NOV2, T(NOV2, 3, "09:00"), ["AN-031"], limits="raised",
         segments=[("C1a", NOV2, T(NOV2, 0, "10:02"), {}), ("C1b", T(NOV2, 0, "10:20"), T(NOV2, 3, "09:00"), {})]),
    _run("C2", NOV2, T(NOV2, 1, "09:00"), ["AN-032"], limits="raised",
         segments=[("C2a", NOV2, T(NOV2, 0, "11:00"), {}), ("C2b", T(NOV2, 1, "06:00"), T(NOV2, 1, "09:00"), {})]),
    _run("C3", NOV2, T(NOV2, 1, "13:00"), ["AN-033"], limits="raised",
         segments=[("C3a", NOV2, T(NOV2, 1, "12:00"), {}), ("C3b", T(NOV2, 1, "12:00"), T(NOV2, 1, "13:00"), {})]),
    _run("C4", NOV2, T(NOV2, 4, "09:00"), ["AN-034"], limits="raised",
         segments=[("C4a", NOV2, T(NOV2, 0, "10:02"), {}), ("C4b", T(NOV2, 3, "00:00"), T(NOV2, 4, "09:00"), {})]),
    _run("C5", NOV2, T(NOV2, 0, "12:15"), ["AN-043"], limits="raised",
         segments=[("C5a", T(NOV2, 0, "12:00"), T(NOV2, 0, "12:05"), {"smsapikeyexpires": "01/12/2026"}),
                   ("C5b", T(NOV2, 0, "12:10"), T(NOV2, 0, "12:15"), {"smsapikeyexpires": "03/12/2026"})]),
    _run("L1", NOV2, T(NOV2, 2, "08:10"), ["AN-022"], limits="default",
         segments=[("L1", NOV2, T(NOV2, 2, "08:10"), {})]),
    _run("L2", NOV2, T(NOV2, 2, "09:00"), ["AN-023"], limits="default",
         segments=[("L2a", NOV2, T(NOV2, 1, "12:00"), {}), ("L2b", T(NOV2, 1, "12:00"), T(NOV2, 2, "09:00"), {})]),
    _run("D1", MAR27, T(MAR27, 1, "13:00"), ["AN-045"], limits="raised",
         segments=[("D1", MAR27, T(MAR27, 1, "13:00"), {})]),
    _run("D2", DEC30, T(DEC30, 2, "09:00"), ["AN-046"], limits="raised",
         segments=[("D2", DEC30, T(DEC30, 2, "09:00"), {})]),
    _run("G1a", NOV2, T(NOV2, 0, "12:00"), ["AN-036"], limits="raised", email_sender="",
         segments=[("G1a", NOV2, T(NOV2, 0, "12:00"), {})]),
    _run("G1b", NOV2, T(NOV2, 0, "12:00"), ["AN-037"], limits="raised", sms_sender="",
         segments=[("G1b", NOV2, T(NOV2, 0, "12:00"), {})]),
    _run("G2a", NOV2, T(NOV2, 1, "09:00"), ["AN-038"], limits="raised", blank=("emailtoday",),
         segments=[("G2a", NOV2, T(NOV2, 1, "09:00"), {})]),
    _run("G2b", NOV2, T(NOV2, 3, "09:00"), ["AN-039"], limits="raised", missing=("emailconfirmation",),
         segments=[("G2b", NOV2, T(NOV2, 3, "09:00"), {})]),
    _run("G2c", NOV2, T(NOV2, 3, "09:00"), ["AN-044"], limits="raised", two_line=("smsreminder",),
         segments=[("G2c", NOV2, T(NOV2, 3, "09:00"), {})]),
    _run("G3a", NOV2, T(NOV2, 3, "09:00"), ["AN-040"], limits="raised",
         fail_email="jack.oneil@sgc.contacmanagertest=2",
         segments=[("G3a", NOV2, T(NOV2, 3, "09:00"), {})]),
    _run("G3b", NOV2, T(NOV2, 3, "09:00"), ["AN-041"], limits="raised",
         fail_email="samantha.carter@sgc.contacmanagertest=999",
         segments=[("G3b", NOV2, T(NOV2, 3, "09:00"), {})]),
    _run("G3c", NOV2, T(NOV2, 3, "09:00"), ["AN-042"], limits="raised",
         fail_sms="%s=1:Esendex refused the text (test reason)" % digits(CONTACT[3]["mobile"]),
         segments=[("G3c", NOV2, T(NOV2, 3, "09:00"), {})]),
    _run("G4", NOV2, T(NOV2, 4, "09:00"), ["AN-048"], limits="raised",
         segments=[("G4", NOV2, T(NOV2, 4, "09:00"), {"enablereminders": "no"})]),
    _run("N", NOV2, T(NOV2, 1, "10:00"), ["AN-035"], limits="raised", shares_home="A",
         segments=[("N", T(NOV2, 1, "09:00"), T(NOV2, 1, "10:00"), {})]),
]
