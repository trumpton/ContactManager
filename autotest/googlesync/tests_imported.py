#
# Imported Google sync tests (area 1): the scenarios of the archive's Google bench that the tests in section 1 do not cover.
# Each is registered as IG-nnn (the register is documents/imported-tests.md, and the procedure of each is in section 1.9 of
# documents/test-specification.md).  They use the same driver, helpers (gs_common) and checks as the other area 1 tests, and
# they are registered with the runner in googlesync_tests.py.
#
# Records are named so that the clean-up finds them: contacts have an address in the test domain, and events start with the
# test prefix.  Each test uses its own contacts and events, so tests do not depend on one another.
#

import re

import gs_common as g

CM = "CM"            # ContactManager is the master
G = "G"              # Google is the master

TESTS = []

EMAIL_KEYS = ["email", "email2", "email3", "email4"]
EMAIL_PHONE_KEYS = EMAIL_KEYS + ["phone", "mobile", "work", "phone2", "phone3", "phone4"]
PHONE_KEYS = ["phone", "mobile", "work", "phone2", "phone3", "phone4", "voip"]


def gs(test_id, title, configs=(CM,)):
    """Registers an imported test.  configs is CM, G, or both."""
    def wrap(fn):
        TESTS.append((test_id, title, tuple(configs), fn))
        return fn
    return wrap


# ------------------------------------------------------------------------------------------------ helpers

def _mail(key, n=1):
    """A test-domain address for a test, so the clean-up finds the contact."""
    return "%s.%d@%s" % (key.lower().replace("-", ""), n, g.DOMAIN)


def _ev(test_id, label):
    """A test event's summary: it starts with the test prefix, so it is found and removed."""
    return g.PREFIX + test_id + " " + label


def _add(tag, **fields):
    """An add_contact operation for the program: a contact with this surname (the tag) and the given fields."""
    f = {"surname": tag, "names": "Bench"}
    f.update(fields)
    return {"op": "add_contact", "fields": f}


def _body(tag, emails=(), phones=(), urls=(), nicknames=(), biographies=(), addresses=(), given="Bench"):
    """A Google person with this surname (the tag) and the given details."""
    body = {"names": [{"givenName": given, "familyName": tag}]}
    if emails:
        body["emailAddresses"] = [{"value": e, "type": "work"} for e in emails]
    if phones:
        body["phoneNumbers"] = [{"value": p, "type": "mobile"} for p in phones]
    if urls:
        body["urls"] = [{"value": u} for u in urls]
    if nicknames:
        body["nicknames"] = [{"value": n} for n in nicknames]
    if biographies:
        body["biographies"] = [{"value": b} for b in biographies]
    if addresses:
        body["addresses"] = [{"formattedValue": a} for a in addresses]
    return body


def _create(tag, **details):
    """Creates a contact on Google and returns its resource name."""
    return g.create_google_contact(_body(tag, **details))


def _info(person):
    names = person.get("names", [])
    return {"tag": names[0].get("familyName", "") if names else "",
            "etag": person.get("etag"),
            "emails": [e.get("value", "") for e in person.get("emailAddresses", [])],
            "phones": [p.get("value", "") for p in person.get("phoneNumbers", [])],
            "urls": [u.get("value", "") for u in person.get("urls", [])],
            "nicknames": [n.get("value", "") for n in person.get("nicknames", [])],
            "biographies": [b.get("value", "") for b in person.get("biographies", [])],
            "addresses": [a.get("formattedValue", "") for a in person.get("addresses", [])]}


def _google_tagged(tag):
    """The Google contacts with this surname, among those in the test domain."""
    return [p for p in g.fixture_people() if _info(p)["tag"] == tag]


def _google_prefixed(prefix):
    return [p for p in g.fixture_people() if _info(p)["tag"].startswith(prefix)]


def _app(run, tag):
    """The program's live contacts with this surname, from the run's dump."""
    return [c for c in run.contacts if c.get("surname") == tag and c.get("deleted") != "true"]


def _app_appts(run, summary):
    return [a for a in run.appointments if a.get("summary") == summary and not a.get("deleted")]


def _google_events(summary):
    return [e for e in g.test_events() if e.get("summary") == summary]


def _digits(text):
    return re.sub(r"\D", "", str(text or ""))


def _norm(text):
    return " ".join(str(text or "").split())


def _now(days, hhmm="09:00"):
    return g.days_from_now(days, hhmm)


# ------------------------------------------------------------------------------------------------ 1.1 contacts

@gs("IG-001", "Add a contact locally, no sync: Google unchanged")
def ig001(t):
    tag = "IG-001"
    home = t.home()
    r = t.run(home, "add locally", [_add(tag, email=_mail(tag)), g.dump("d1.json")])
    t.check("no copy of the contact is on Google", not _google_tagged(tag), "%d copies" % len(_google_tagged(tag)))
    t.check("the app holds the contact", len(_app(r, tag)) == 1, "%d contacts" % len(_app(r, tag)))


@gs("IG-003", "Add then delete locally before any sync: nothing reaches Google")
def ig003(t):
    tag = "IG-003"
    home = t.home()
    r = t.run(home, "add, delete, sync", [_add(tag, email=_mail(tag)),
                                          {"op": "delete_contact", "match": {"names": "Bench", "surname": tag}},
                                          g.sync(), g.dump("d1.json")])
    t.check("no copy of the contact is on Google", not _google_tagged(tag), "%d copies" % len(_google_tagged(tag)))
    t.check("the app no longer holds it as live", not _app(r, tag), "%d live" % len(_app(r, tag)))


@gs("IG-007", "Same name on both sides before sync: both kept")
def ig007(t):
    tag = "IG-007"
    home = t.home()
    _create(tag, emails=[_mail(tag, 1)], phones=["01111 111111"])
    r = t.run(home, "add the same name in the app, sync",
              [_add(tag, phone="02222 222222", email=_mail(tag, 2)), g.sync(), g.dump("d1.json")])
    copies = _google_tagged(tag)
    phones = [_digits(p) for c in copies for p in _info(c)["phones"]]
    t.check("Google holds two contacts with the name", len(copies) == 2, "found %d" % len(copies))
    t.check("both phone numbers are on Google", any("111111" in p for p in phones) and any("222222" in p for p in phones),
            "phones %s" % phones)
    t.check("the app holds two contacts with the name", len(_app(r, tag)) == 2, "found %d" % len(_app(r, tag)))


@gs("IG-010", "Out-of-sequence phones and emails: each value kept in some slot")
def ig010(t):
    tag = "IG-010"
    home = t.home()
    emails = [_mail(tag, 1), _mail(tag, 2), _mail(tag, 3)]
    phones = ["0101 010101", "0202 020202", "0303 030303", "0404 040404"]
    _create(tag, emails=emails, phones=phones)
    r1 = t.run(home, "first sync", [g.sync(), g.dump("d1.json")])
    t.run(home, "second sync", [g.sync(), g.dump("d2.json")])
    local = _app(r1, tag)
    values = [str(local[0].get(k, "")) for k in EMAIL_PHONE_KEYS] + [str(local[0].get("comments", ""))] if local else []
    for m in emails:
        t.check("the app holds %s" % m, any(m in v for v in values), "")
    for p in phones:
        t.check("the app holds phone %s" % p, any(_digits(p) in _digits(v) for v in values), "")
    copies = _google_tagged(tag)
    t.check("Google still holds all three emails after the second sync",
            bool(copies) and len(_info(copies[0])["emails"]) == 3,
            "emails %s" % (_info(copies[0])["emails"] if copies else None))


@gs("IG-011", "Stray spaces and newlines in Google data: no spurious upload")
def ig011(t):
    tag = "IG-011"
    home = t.home()
    resource = _create(tag, emails=["  " + _mail(tag) + " \n"], addresses=["12 High St\n  Town  "])
    before = g.google_person(resource).get("etag")
    r = t.run(home, "sync", [g.sync(), g.dump("d1.json")])
    after = g.google_person(resource).get("etag")
    t.check("Google is not written by the sync (etag unchanged)", before == after, "etag changed")
    local = _app(r, tag)
    t.check("the app holds the trimmed email",
            bool(local) and any(_norm(local[0].get(k)) == _mail(tag) for k in EMAIL_KEYS), "")


@gs("IG-012", "Large upload: 40 local contacts reach Google once")
def ig012(t):
    prefix = "IG-012-"
    home = t.home()
    ops = [_add("%s%02d" % (prefix, i), email=_mail("%s%02d" % (prefix, i))) for i in range(40)]
    t.run(home, "upload 40 contacts", ops + [g.sync(), g.dump("d1.json")])
    copies = _google_prefixed(prefix)
    tags = sorted(_info(c)["tag"] for c in copies)
    t.check("Google holds 40 contacts of the upload", len(copies) == 40, "found %d" % len(copies))
    t.check("no duplicates", len(set(tags)) == len(tags), "%d distinct of %d" % (len(set(tags)), len(tags)))


@gs("IG-013", "Unicode and apostrophes in names round-trip exactly")
def ig013(t):
    tag = "Ørsted-O'Brien Ångström"
    home = t.home()
    t.run(home, "upload", [_add(tag, email=_mail("IG-013")), g.sync(), g.dump("d1.json")])
    t.check("Google holds the name exactly", len(_google_tagged(tag)) == 1, "copies %d" % len(_google_tagged(tag)))


@gs("IG-014", "Email-only contact (no name) is uploaded with its email")
def ig014(t):
    address = _mail("IG-014")
    home = t.home()
    t.run(home, "upload", [{"op": "add_contact", "fields": {"email": address}}, g.sync(), g.dump("d1.json")])
    found = [p for p in g.fixture_people() if address in _info(p)["emails"]]
    t.check("Google holds a contact with that email", len(found) == 1, "found %d" % len(found))


@gs("IG-015", "Repeated syncs with no changes do not write to Google")
def ig015(t):
    tag = "IG-015"
    home = t.home()
    resource = _create(tag, emails=[_mail(tag)])
    t.run(home, "first sync", [g.sync()])
    etag1 = g.google_person(resource).get("etag")
    t.run(home, "two more syncs", [g.sync(), g.sync()])
    etag2 = g.google_person(resource).get("etag")
    t.check("the etag is unchanged after two more syncs", etag1 == etag2, "etag changed")


@gs("IG-016", "Edit keeps URLs the app does not hold (option 1 regression)")
def ig016(t):
    tag = "IG-016"
    home = t.home()
    urls = ["https://example.test/16-a", "https://example.test/16-b"]
    resource = _create(tag, emails=[_mail(tag)], urls=urls, nicknames=["Nick IG-016"], biographies=["Bio IG-016"])
    t.run(home, "sync", [g.sync()])
    t.run(home, "edit the surname in the app, sync",
          [{"op": "edit_contact", "match": {"surname": tag}, "fields": {"surname": tag + " edited"}},
           g.sync(), g.dump("d2.json")])
    info = _info(g.google_person(resource))
    t.check("both URLs are kept", sorted(info["urls"]) == sorted(urls), "urls %s" % info["urls"])
    t.check("the nickname and the biography are kept",
            info["nicknames"] == ["Nick IG-016"] and info["biographies"] == ["Bio IG-016"],
            "nicknames %s, biographies %s" % (info["nicknames"], info["biographies"]))
    t.check("the edit reached Google", info["tag"] == tag + " edited", "surname %s" % info["tag"])


# ------------------------------------------------------------------------------------------------ 1.2 appointments

@gs("IG-017", "Add an appointment locally, no sync: Google unchanged")
def ig017(t):
    summary = _ev("IG-017", "local")
    home = t.home()
    r = t.run(home, "add locally", [{"op": "add_appointment", "fields": {"summary": summary, "from": _now(2),
                                                                      "to": _now(2, "10:00")}}, g.dump("d1.json")])
    t.check("no copy of the appointment is on Google", not _google_events(summary), "%d copies" % len(_google_events(summary)))
    t.check("the app holds it", len(_app_appts(r, summary)) == 1, "found %d" % len(_app_appts(r, summary)))


@gs("IG-019", "Identical events on Google (duplicates): both kept, no growth on re-sync")
def ig019(t):
    summary = _ev("IG-019", "duplicate")
    home = t.home()
    g.create_event(summary, _now(4), _now(4, "10:00"))
    g.create_event(summary, _now(4), _now(4, "10:00"))
    r1 = t.run(home, "first sync", [g.sync(), g.dump("d1.json")])
    r2 = t.run(home, "second sync", [g.sync(), g.dump("d2.json")])
    t.check("the app holds both after the first sync", len(_app_appts(r1, summary)) == 2, "found %d" % len(_app_appts(r1, summary)))
    t.check("still two after the second sync", len(_app_appts(r2, summary)) == 2, "found %d" % len(_app_appts(r2, summary)))
    t.check("Google still holds two", len(_google_events(summary)) == 2, "found %d" % len(_google_events(summary)))


@gs("IG-020", "Same event added locally and on Google before sync: both kept")
def ig020(t):
    summary = _ev("IG-020", "same")
    home = t.home()
    g.create_event(summary, _now(5), _now(5, "10:00"))
    t.run(home, "add the same event in the app, sync",
          [{"op": "add_appointment", "fields": {"summary": summary, "from": _now(5), "to": _now(5, "10:00")}},
           g.sync(), g.dump("d1.json")])
    t.check("Google holds both", len(_google_events(summary)) == 2, "found %d" % len(_google_events(summary)))


@gs("IG-026", "Large upload: 40 local appointments reach Google once")
def ig026(t):
    key = _ev("IG-026", "")
    home = t.home()
    ops = [{"op": "add_appointment", "fields": {"summary": _ev("IG-026", "%02d" % i), "from": _now(2 + i),
                                              "to": _now(2 + i, "10:00")}} for i in range(40)]
    t.run(home, "upload 40 appointments", ops + [g.sync(), g.dump("d1.json")])
    events = [e for e in g.test_events() if e.get("summary", "").startswith(key)]
    names = sorted(e.get("summary") for e in events)
    t.check("Google holds 40 appointments of the upload", len(events) == 40, "found %d" % len(events))
    t.check("no duplicates", len(set(names)) == len(names), "%d distinct of %d" % (len(set(names)), len(names)))


@gs("IG-028", "Stray spaces and newlines in a location: no spurious upload")
def ig028(t):
    summary = _ev("IG-028", "location")
    home = t.home()
    eid = g.create_event(summary, _now(13), _now(13, "10:00"), location="  Room 12 \n  ")
    before = g.get_event(eid).get("etag")
    r = t.run(home, "sync", [g.sync(), g.dump("d1.json")])
    after = g.get_event(eid).get("etag")
    t.check("Google is not written by the sync (etag unchanged)", before == after, "etag changed")
    local = _app_appts(r, summary)
    t.check("the app holds the location", len(local) == 1 and _norm(local[0].get("location")) == "Room 12",
            "location %r" % (local[0].get("location") if local else None))


@gs("IG-029", "Local ID of a Google event is the same across syncs")
def ig029(t):
    summary = _ev("IG-029", "stable")
    home = t.home()
    g.create_event(summary, _now(14), _now(14, "10:00"))
    r1 = t.run(home, "first sync", [g.sync(), g.dump("d1.json")])
    r2 = t.run(home, "second sync", [g.sync(), g.dump("d2.json")])
    a, b = _app_appts(r1, summary), _app_appts(r2, summary)
    t.check("one entry each sync", len(a) == 1 and len(b) == 1, "found %d and %d" % (len(a), len(b)))
    t.check("the same local ID both times", len(a) == 1 and len(b) == 1 and a[0].get("id") == b[0].get("id"), "")


# ------------------------------------------------------------------------------------------------ 1.3 edge cases

@gs("IG-038", "Blank and whitespace-only name fields: no crash, no duplicate")
def ig038(t):
    tag = "IG-038"
    home = t.home()
    t.run(home, "upload",
          [{"op": "add_contact", "fields": {"names": "Bench", "surname": "   ", "email": _mail(tag, 1)}},
           {"op": "add_contact", "fields": {"names": "  ", "email": _mail(tag, 2)}},
           g.sync(), g.dump("d1.json")])
    copies = [p for p in g.fixture_people() if _mail(tag, 2) in _info(p)["emails"]]
    t.check("no more than one Google copy of the nameless contact", len(copies) <= 1, "found %d" % len(copies))


@gs("IG-039", "Very long comment (6000 characters) round-trips intact")
def ig039(t):
    tag = "IG-039"
    home = t.home()
    long_text = ("Lorem ipsum dolor sit amet, consectetur. " * 150)[:6000]
    resource = _create(tag, emails=[_mail(tag)], biographies=[long_text])
    r = t.run(home, "sync", [g.sync(), g.dump("d1.json")])
    notes = " ".join(_info(g.google_person(resource))["biographies"])
    t.check("the Google text is unchanged", _norm(notes) == _norm(long_text), "length %d" % len(_norm(notes)))
    local = _app(r, tag)
    kept = _norm(local[0].get("comments", "")) if local else ""
    t.check("the app holds the full text", kept == _norm(long_text), "length %d" % len(kept))


@gs("IG-040", "Quotes, backslashes, ampersands, angle brackets, emoji and right-to-left text round-trip")
def ig040(t):
    tag = 'O\'Neil & "Co" <b>\\ é中文 \U0001F600 שלום'
    home = t.home()
    r = t.run(home, "upload", [_add(tag, email=_mail("IG-040"), comments='a "quoted" \\ & <tag>'),
                               g.sync(), g.dump("d1.json")])
    t.check("Google holds the name exactly", len(_google_tagged(tag)) == 1, "copies %d" % len(_google_tagged(tag)))
    t.check("the app holds the name exactly", len(_app(r, tag)) == 1, "found %d" % len(_app(r, tag)))


@gs("IG-041", "Phone number formats (international, brackets, extensions) keep their digits")
def ig041(t):
    tag = "IG-041"
    home = t.home()
    phones = [("+44 (0)20 7946 0000", "work"), ("07700-900123", "mobile"), ("+1 555 0100 ext 5", "home")]
    _create(tag, emails=[_mail(tag)], phones=[p for p, _ in phones])
    r = t.run(home, "sync", [g.sync(), g.dump("d1.json")])
    copies = _google_tagged(tag)
    google_digits = sorted(_digits(p) for c in copies for p in _info(c)["phones"])
    want = sorted(_digits(p) for p, _ in phones)
    t.check("the Google digits are unchanged", google_digits == want, "got %s" % google_digits)
    local = _app(r, tag)
    app_digits = sorted(_digits(local[0].get(k, "")) for k in PHONE_KEYS if local and local[0].get(k)) if local else []
    # The program drops the (0) after a country code, so it holds the number without that trunk zero
    want_app = sorted(_digits(p.replace("(0)", "")) for p, _ in phones)
    t.check("the app holds all three numbers, without the (0)", app_digits == want_app, "got %s" % app_digits)
