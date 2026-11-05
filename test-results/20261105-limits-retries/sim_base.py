"""Runs the send rules over the scenarios one minute at a time (the app checks once a second; at the accelerated speed that
is once a fake minute).  Mirrors decideMessages (messagedecision.cpp) and sendMailSMS (mainwindow_timer_alert.cpp)."""
from datetime import timedelta
from collections import defaultdict
import model2 as M


class Appt:
    def __init__(self, sc):
        self.sc = sc
        self.person = sc["person"]
        self.channel = M.PEOPLE[self.person][1]
        self.emailme = self.channel in ("email", "both")
        self.textme = self.channel in ("sms", "both")
        self.start = sc["occ"]
        self.end = self.start + M.DUR
        self.created = sc["create"]
        self.updated = sc["create"]
        self.deleted = False
        self.messagesent = False
        self.msig = None
        self.reminder_for = None
        self.dayrec = defaultdict(set)   # day -> channels whose on-the-day message has gone (or been given)

    def sig(self):
        return (self.start, self.end)


def decide(a, now, occ):
    """decideMessages: steps as (kind, email, recordreminder)."""
    steps = []
    S = dict(deleted=a.deleted, messagesent=a.messagesent,
             changed=a.messagesent and not a.deleted and a.msig != a.sig(),
             emailme=a.emailme, textme=a.textme, remindersent=a.reminder_for == occ,
             istime=occ < now + M.REMIND, confirmonly=occ < now + M.CONFIRM_ONLY,
             onthisday=occ.date() == now.date(), createdbeforeday=a.created.date() < now.date())
    day = a.dayrec[now.date()]
    emaildone, smsdone = "email" in day, "sms" in day
    latefirstonday = (not S["messagesent"]) and S["onthisday"] and S["createdbeforeday"]
    if S["emailme"] and not S["deleted"] and not S["messagesent"]:
        steps.append(("confirm", True, S["confirmonly"]))
    elif S["textme"] and not S["emailme"] and not S["deleted"] and not S["messagesent"] and not latefirstonday:
        steps.append(("confirm", False, S["confirmonly"]))
    elif S["emailme"] and not S["deleted"] and S["changed"]:
        steps.append(("update", True, S["confirmonly"]))
    elif S["textme"] and not S["emailme"] and not S["deleted"] and S["changed"]:
        steps.append(("update", False, S["confirmonly"]))
    elif S["emailme"] and S["deleted"] and S["messagesent"]:
        steps.append(("cancel", True, False))
    elif S["textme"] and not S["emailme"] and S["deleted"] and S["messagesent"]:
        steps.append(("cancel", False, False))
    elif S["textme"] and not S["emailme"] and not S["deleted"] and S["istime"] and not S["remindersent"] and not S["onthisday"]:
        steps.append(("reminder", False, True))
    elif S["emailme"] and not S["deleted"] and S["istime"] and not S["remindersent"] and not S["onthisday"]:
        steps.append(("reminder", True, True))
    if not steps and not S["deleted"] and S["onthisday"]:
        if S["emailme"] and not emaildone:
            steps.append(("onday", True, False))
        if S["textme"] and not smsdone:
            steps.append(("onday", False, False))
    return steps


def simulate(scenarios, start, end):
    """Returns the expected messages, sorted: dicts with time, person, channel, kind, occ, title, and the appointment's
    edit / delete events as they were applied."""
    appts = {sc["id"]: Appt(sc) for sc in scenarios}
    ops = defaultdict(list)
    for sc in scenarios:
        ops[sc["create"]].append(("add", sc["id"]))
        for ev in sc["events"]:
            ops[ev[0]].append(("edit", sc["id"], ev))
    expected = []
    t = start
    while t <= end:
        for op in ops.get(t, []):
            a = appts[op[1]]
            if op[0] == "add":
                pass   # created at its time; the hold runs from the creation
            else:
                ev = op[2]
                if ev[1] == "move":
                    a.start = ev[2]
                    a.end = ev[2] + M.DUR
                    a.updated = t
                else:
                    a.deleted = True
                    a.updated = t
        if M.in_window(t):
            for a in appts.values():
                if a.created > t:
                    continue
                if a.deleted and not a.messagesent:
                    continue
                occ = a.start if a.start > t else None
                if occ is None:
                    continue
                if not a.deleted and (t - a.updated) < M.HOLD:
                    continue
                steps = decide(a, t, occ)
                if not steps:
                    continue
                if steps[0][0] != "onday":
                    kind, email, recordreminder = steps[0]
                    expected.append(dict(time=t, person=a.person, channel="email" if email else "sms", kind=kind,
                                         occ=occ, id=a.sc["id"]))
                    if kind in ("confirm", "update"):
                        a.messagesent = True
                        a.msig = a.sig()
                        if occ.date() == t.date():
                            a.dayrec[t.date()].add("email" if email else "sms")
                        if recordreminder:
                            a.reminder_for = occ
                    elif kind == "cancel":
                        a.messagesent = False
                        a.msig = None
                        a.reminder_for = None
                    elif kind == "reminder":
                        a.reminder_for = occ
                else:
                    for kind, email, _ in steps:
                        expected.append(dict(time=t, person=a.person, channel="email" if email else "sms", kind=kind,
                                             occ=occ, id=a.sc["id"]))
                        a.dayrec[t.date()].add("email" if email else "sms")
                        if not a.messagesent:
                            a.messagesent = True
                            a.msig = a.sig()
        t += timedelta(minutes=1)
    return expected, appts


if __name__ == "__main__":
    exp, _ = simulate(M.SCENARIOS, M.START, M.END)
    for e in exp:
        print(e["time"].strftime("%a %d %H:%M"), e["id"], e["person"], e["channel"], e["kind"], "occ", e["occ"].strftime("%d %H:%M"))
    print(len(exp), "messages")
