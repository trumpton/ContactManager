"""Accelerated test model 2 for issue #110 (limits and retries).  The same rules as model.py, with the limits at their defaults
(5 messages an hour and 10 a day for each contact) and sends that fail.  The failures are set by the test email (CM_TEST_FAIL_EMAIL):
the first N sends to an address fail.  Times are local UK time (GMT on these dates)."""
from datetime import datetime, timedelta

HOLD = timedelta(minutes=5)          # waitbeforesendmins
REMIND = timedelta(hours=24)         # remindhoursbefore
CONFIRM_ONLY = timedelta(hours=36)   # confirmonlyhours
WIN_START, WIN_END = (7, 0), (22, 0)  # earliestsend, latestsend
DUR = timedelta(minutes=30)
LIMIT_HOUR, LIMIT_DAY = 5, 10        # limitperhour, limitperday
SENDRETRY = timedelta(minutes=5)     # SENDRETRYMINS
MAXRETRIES = 10                      # MAXMESSAGERETRIES

START = datetime(2026, 11, 3, 8, 50)
SETUP = datetime(2026, 11, 3, 8, 40)   # the clock starts here; the contacts are added before START
END = datetime(2026, 11, 4, 12, 0)     # the last message is due before this

PEOPLE = {  # first, surname: channel, email
    "Limit": ("Hourly", "email", "limit.hourly@vizier.uk"),
    "Daily": ("Daily", "email", "limit.daily@vizier.uk"),
    "Retry": ("Retry", "email", "retry.twice@vizier.uk"),
    "Giveup": ("Giveup", "email", "retry.never@vizier.uk"),
}

# The sends to each address that fail first (the test email substitute reads the same plan from CM_TEST_FAIL_EMAIL)
FAILS = {"retry.twice@vizier.uk": 2, "retry.never@vizier.uk": 10}


def d(day, hh, mm=0):
    return datetime(2026, 11, day, hh, mm)


def in_window(t):
    return WIN_START <= (t.hour, t.minute) < WIN_END


SCENARIOS = []
# Seven confirmations for one contact, made within seven minutes: five go within the hour, the sixth and seventh wait for it to clear
for i in range(7):
    SCENARIOS.append(dict(id="L%02d" % (i + 1), person="Limit", create=d(3, 9, i), occ=d(6, 10, 0) + i * timedelta(minutes=40),
                          events=[], corner="One of seven confirmations for an hour limit of five: the sixth and seventh wait"))
# Eleven confirmations for one contact, two hours apart: the eleventh waits until the first has left the last 24 hours
for i in range(11):
    SCENARIOS.append(dict(id="D%02d" % (i + 1), person="Daily", create=d(3, 10, 0) + i * timedelta(hours=2),
                          occ=d(6, 9, 0) + i * timedelta(hours=1), events=[],
                          corner="One of eleven confirmations for a day limit of ten: the eleventh waits for a day to clear"))
SCENARIOS.append(dict(id="R01", person="Retry", create=d(3, 14, 0), occ=d(6, 10, 0), events=[],
                      corner="Two sends fail: retried five minutes after each failure, and sent on the third try at 14:15"))
SCENARIOS.append(dict(id="G01", person="Giveup", create=d(3, 15, 0), occ=d(6, 11, 0), events=[],
                      corner="Ten sends fail: given up after the tenth failure, so nothing is sent"))
