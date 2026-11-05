"""Builds the home folder and the operations for the timed run that starts at START (issue #89, six-person test)."""
import json, os, shutil, glob, sys
from datetime import datetime, timedelta
import gen

START = datetime(2026, 11, 2, 20, 40)
RUN = os.path.join(os.path.dirname(os.path.abspath(__file__)), "run")
PHONE = gen.PHONE
DEADLINE = datetime(2026, 11, 3, 18, 5)


def iso(t):
    return t.strftime("%Y-%m-%dT%H:%M:%SZ")


def local(t):
    return t.strftime("%Y-%m-%dT%H:%M:%S")


def build_home():
    if os.path.exists(RUN):
        shutil.rmtree(RUN)
    home = os.path.join(RUN, "home")
    os.makedirs(os.path.join(home, ".config", "trumpton.org.uk"))
    os.makedirs(os.path.join(home, "TrumptonApps"))
    os.makedirs(os.path.join(RUN, "box"))
    msgdir = os.path.join(home, "TrumptonApps", "ContactManager", "messages")
    os.makedirs(msgdir)
    for f in glob.glob("/home/user/contactmanager/Samples/messages/*"):
        shutil.copy(f, msgdir)
    for f in glob.glob(os.path.join(msgdir, "*.msg")):
        lines = open(f, encoding="utf-8", errors="replace").read().split("\n")
        if os.path.basename(f).startswith("email"):
            lines[1] = "[TEST] $NAME $SURNAME - " + lines[1].replace("Sogfripp Enterprises", "Vizier Design")
        else:
            lines[1] = "[TEST] $NAME $SURNAME - " + lines[1]
        open(f, "w", encoding="utf-8").write("\n".join(lines))
    open(os.path.join(home, ".config", "trumpton.org.uk", "ContactManager.conf"), "w").write(
        "[General]\ndatabasemaster=0\nenablereminders=yes\n")
    open(os.path.join(home, "TrumptonApps", "ContactManager.ini"), "w").write("""[phone]
localcode=
countrycode=

[reminders]
earliestsend=07:00
latestsend=22:00
remindhoursbefore=24
limitperday=10
limitperhour=5

[message]
emailconfirmation="$CONFIG/ContactManager/messages/emailconfirmation.msg"
emailupdate="$CONFIG/ContactManager/messages/emailupdate.msg"
emaildelete="$CONFIG/ContactManager/messages/emaildelete.msg"
emailreminder="$CONFIG/ContactManager/messages/emailreminder.msg"
emailtoday="$CONFIG/ContactManager/messages/emailtoday.msg"
smstoday="$CONFIG/ContactManager/messages/smstoday.msg"
smsconfirmation="$CONFIG/ContactManager/messages/smsconfirmation.msg"
smsupdate="$CONFIG/ContactManager/messages/smsupdate.msg"
smsdelete="$CONFIG/ContactManager/messages/smsdelete.msg"
smsreminder="$CONFIG/ContactManager/messages/smsreminder.msg"

[sms-reminder]
agent=Esendex
username=EXUK0008877
sender=Vizier Design
debug=yes

[email-reminder]
server=host.vizier.uk
username=martine
port=465
""")


def build_ops():
    appts = gen.generate(START)
    ops = []
    for first, last, chan, email in gen.PEOPLE:
        c = {"names": first, "surname": last, "mobile": PHONE,
             "emailme": "true" if chan in ("email", "both") else "false",
             "textme": "true" if chan in ("sms", "both") else "false"}
        if email:
            c["email"] = email
        ops.append({"op": "add_contact", "fields": c})
    ops.append({"op": "waituntil", "at": local(START)})
    for a in appts:
        ops.append({"op": "add_appointment", "surname": a["last"],
                    "fields": {"summary": a["title"], "from": iso(a["start"]), "to": iso(a["end"]), "location": ""}})
    ops.append({"op": "save"})
    actions = []
    for a in appts:
        if a["action"] == "none":
            continue
        t = a["action_at"]
        if a["action"] in ("back", "fwd"):
            d = timedelta(minutes=-1 if a["action"] == "back" else 1)
            if t < a["start"]:
                fields = {"from": iso(a["start"] + d), "to": iso(a["end"] + d)}
            else:
                fields = {"from": iso(a["start"] + d), "to": iso(a["end"] + d)}   # moved either way; no message after it starts
            actions.append((t, {"op": "edit_appointment", "match": {"summary": a["title"]}, "fields": fields}))
        else:
            actions.append((t, {"op": "delete_appointment", "match": {"summary": a["title"]}}))
    milestones = [(datetime(2026, 11, 2, 22, 5), "dump-2205.json"), (datetime(2026, 11, 3, 7, 10), "dump-0710.json"),
                  (datetime(2026, 11, 3, 11, 5), "dump-1105.json"), (datetime(2026, 11, 3, 18, 0), "dump-1800.json")]
    timeline = sorted([(t, "action", op) for t, op in actions] + [(t, "dump", f) for t, f in milestones], key=lambda x: x[0])
    for t, kind, payload in timeline:
        ops.append({"op": "waituntil", "at": local(t)})
        if kind == "action":
            ops.append(payload)
            ops.append({"op": "save"})
        else:
            ops.append({"op": "dump", "file": payload})
    ops.append({"op": "waituntil", "at": local(DEADLINE)})
    ops.append({"op": "save"})
    ops.append({"op": "exit"})
    return ops


if __name__ == "__main__":
    build_home()
    ops = build_ops()
    json.dump(ops, open(os.path.join(RUN, "ops.json"), "w"), indent=0)
    print("ops", len(ops))
