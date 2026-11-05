"""Builds the accelerated run (issue #89): the driver's operations, the home folder, the expected messages and the table."""
import csv, json, os, sys
from datetime import datetime, timedelta
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import model2 as M
import sim2 as sim
import build_run2 as build_run

HERE = os.path.dirname(os.path.abspath(__file__))
RUN = os.path.join(HERE, "run")
PHONE = "+447718997189"


def iso(t):
    return t.strftime("%Y-%m-%dT%H:%M:%SZ")


def local(t):
    return t.strftime("%Y-%m-%dT%H:%M:%S")


def rel(t):
    days = (t.date() - M.START.date()).days
    day = {0: "today", 1: "tomorrow", 2: "day after tomorrow"}.get(days, "in %d days" % days)
    return "%s %s" % (day, t.strftime("%H:%M"))


def title(sc):
    last, chan = M.PEOPLE[sc["person"]][0], M.PEOPLE[sc["person"]][1]
    return "%s %s %s (%s)" % (sc["id"], sc["person"], last, chan)


def main():
    expected, appts = sim.simulate(M.SCENARIOS, M.START, M.END)

    # the operations, in time order: each change is waited for, then saved
    ops = [{"op": "waituntil", "at": local(M.SETUP)}]   # the contacts are added before the start, so the setup takes no time from it
    for first, (last, chan, email) in M.PEOPLE.items():
        c = {"names": first, "surname": last, "mobile": PHONE,
             "emailme": "true" if chan in ("email", "both") else "false",
             "textme": "true" if chan in ("sms", "both") else "false"}
        if email:
            c["email"] = email
        ops.append({"op": "add_contact", "fields": c})
    ops.append({"op": "waituntil", "at": local(M.START)})
    timeline = []
    for sc in M.SCENARIOS:
        timeline.append((sc["create"], 0, {"op": "add_appointment", "surname": M.PEOPLE[sc["person"]][0],
                                           "fields": {"summary": title(sc), "from": iso(sc["occ"]),
                                                      "to": iso(sc["occ"] + M.DUR), "location": ""}}))
        for ev in sc["events"]:
            if ev[1] == "move":
                new = ev[2]
                timeline.append((ev[0], 1, {"op": "edit_appointment", "match": {"summary": title(sc)},
                                            "fields": {"from": iso(new), "to": iso(new + M.DUR)}}))
            else:
                timeline.append((ev[0], 1, {"op": "delete_appointment", "match": {"summary": title(sc)}}))
    timeline.sort(key=lambda x: (x[0], x[1]))
    for t, _, op in timeline:
        ops.append({"op": "waituntil", "at": local(t)})
        ops.append(op)
        ops.append({"op": "save"})
    ops.append({"op": "waituntil", "at": local(M.END + timedelta(minutes=2))})
    ops.append({"op": "save"})
    ops.append({"op": "exit"})

    # the home folder (the same build as the timed run: templates, settings and the accounts)
    build_run.RUN = RUN
    build_run.build_home()
    json.dump(ops, open(os.path.join(RUN, "ops.json"), "w"), indent=0)

    # the expected messages, for comparison
    json.dump([dict(time=local(e["time"]), person=e["person"], channel=e["channel"], kind=e["kind"],
                    occ=local(e["occ"]), id=e["id"]) for e in expected],
              open(os.path.join(HERE, "expected.json"), "w"), indent=0)

    # the table for the user: every change and expected message, in time order, with the corner case it tests
    rows = []
    for sc in M.SCENARIOS:
        user = sc["person"] + " " + M.PEOPLE[sc["person"]][0]
        base = dict(time=sc["create"], user=user, channel=M.PEOPLE[sc["person"]][1], scenario=sc["id"],
                    title=title(sc), corner=sc["corner"])
        rows.append(dict(base, event="APPOINTMENT SET", detail="starts %s" % rel(sc["occ"])))
        for ev in sc["events"]:
            if ev[1] == "move":
                rows.append(dict(base, time=ev[0], event="CHANGE START",
                                 detail="now starts %s" % rel(ev[2])))
            else:
                rows.append(dict(base, time=ev[0], event="DELETE", detail="appointment deleted"))
    for e in expected:
        sc = next(s for s in M.SCENARIOS if s["id"] == e["id"])
        what = {"confirm": "confirmation", "update": "update", "cancel": "cancellation", "reminder": "reminder",
                "onday": "on-the-day message"}[e["kind"]]
        rows.append(dict(time=e["time"], user=e["person"] + " " + M.PEOPLE[e["person"]][0], channel=e["channel"],
                         scenario=e["id"], title=title(sc), event="%s EXPECTED" % ("EMAIL" if e["channel"] == "email" else "SMS"),
                         detail=what + " (appointment at %s)" % e["occ"].strftime("%H:%M") if False else
                         "%s for appointment %s" % (what, rel(e["occ"])), corner=""))
    rows.sort(key=lambda r: (r["time"], r["scenario"], r["event"]))
    with open(os.path.join(HERE, "sequence-1-accelerated.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["time", "user", "channel", "scenario", "title", "event", "detail", "corner case tested"])
        for r in rows:
            w.writerow([rel(r["time"]), r["user"], r["channel"], r["scenario"], r["title"], r["event"], r["detail"],
                        r["corner"]])
    print("ops", len(ops), "expected messages", len(expected), "table rows", len(rows))


if __name__ == "__main__":
    main()
