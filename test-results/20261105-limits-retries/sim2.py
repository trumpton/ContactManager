"""Runs the send rules over the scenarios one minute at a time, as sim.py does, with the per-contact limits and the retries
(issue #110).  Mirrors sendMailSMS and maySend in mainwindow_timer_alert.cpp, and contactLimitReason."""
from datetime import timedelta
from collections import defaultdict
import model2 as M
import sim_base as base     # decide() and Appt: the send rules for one appointment, as in sim.py


def simulate(scenarios, start, end):
    appts = {sc["id"]: base.Appt(sc) for sc in scenarios}
    ops = defaultdict(list)
    for sc in scenarios:
        ops[sc["create"]].append(("add", sc["id"]))
    expected = []
    sent = defaultdict(list)            # person -> the times of their sent messages (the last 24 hours are kept)
    fails_left = dict(M.FAILS)          # address -> failures still to come
    failures = defaultdict(int)         # (appointment, kind) -> failed tries
    retry_after = {}                    # (appointment, kind) -> the time the next try may be made
    attempts = []
    t = start
    while t <= end:
        if M.in_window(t):
            for a in appts.values():
                if a.created > t:
                    continue
                occ = a.start if a.start > t else None
                if occ is None:
                    continue
                if (t - a.updated) < M.HOLD:
                    continue
                steps = base.decide(a, t, occ)
                if not steps or steps[0][0] == "onday":
                    continue
                kind, email, recordreminder = steps[0]
                key = (a.sc["id"], kind)
                # the limits: a contact may have five messages in the last hour and ten in the last 24 hours
                times = [s for s in sent[a.person] if s > t - timedelta(hours=24)]
                sent[a.person] = times
                hour = sum(1 for s in times if s > t - timedelta(hours=1))
                if hour >= M.LIMIT_HOUR or len(times) >= M.LIMIT_DAY:
                    continue
                # the retries: not after maxretries failures, and not before the wait after the last failure is over
                if failures[key] >= M.MAXRETRIES:
                    continue
                if key in retry_after and t < retry_after[key]:
                    continue
                address = M.PEOPLE[a.person][2]
                if fails_left.get(address, 0) > 0:
                    fails_left[address] -= 1
                    failures[key] += 1
                    retry_after[key] = t + M.SENDRETRY
                    attempts.append(dict(time=t, id=a.sc["id"], person=a.person, kind=kind, result="failed"))
                    continue
                # a send that succeeds
                expected.append(dict(time=t, person=a.person, channel="email", kind=kind, occ=occ, id=a.sc["id"]))
                sent[a.person].append(t)
                failures.pop(key, None)
                retry_after.pop(key, None)
                if kind in ("confirm", "update"):
                    a.messagesent = True
                    a.msig = a.sig()
                    if occ.date() == t.date():
                        a.dayrec[t.date()].add("email")
                    if recordreminder:
                        a.reminder_for = occ
                elif kind == "cancel":
                    a.messagesent = False
                    a.msig = None
                    a.reminder_for = None
                elif kind == "reminder":
                    a.reminder_for = occ
        t += timedelta(minutes=1)
    simulate.attempts = attempts
    return expected, appts


if __name__ == "__main__":
    exp, _ = simulate(M.SCENARIOS, M.START, M.END)
    for e in exp:
        print(e["time"].strftime("%a %d %H:%M"), e["id"], e["person"], e["kind"], "occ", e["occ"].strftime("%d %H:%M"))
    print(len(exp), "messages;", len(simulate.attempts), "failed attempts")
    for a in simulate.attempts:
        print("  failed", a["time"].strftime("%a %d %H:%M"), a["id"], a["kind"])
