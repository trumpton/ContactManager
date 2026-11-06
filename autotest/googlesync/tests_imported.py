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


# ------------------------------------------------------------------------------------------------ batch 2: edge cases and labels

def _types_by_email(resource):
    """The Google type (lower case) of each email on a contact, keyed by the address."""
    person = g.google_person(resource)
    return {e.get("value", ""): (e.get("type") or "").lower() for e in person.get("emailAddresses", [])}


def _types_by_digits(resource):
    """The Google type (lower case) of each phone on a contact, keyed by its digits."""
    person = g.google_person(resource)
    return {_digits(p.get("value", "")): (p.get("type") or "").lower() for p in person.get("phoneNumbers", [])}


def _date(days):
    import datetime
    return (datetime.datetime.now(datetime.timezone.utc).date() + datetime.timedelta(days=days)).isoformat()


@gs("IG-042", "Same email on two different contacts: both kept, not merged")
def ig042(t):
    tag_a, tag_b = "IG-042A", "IG-042B"
    address = _mail("IG-042")
    home = t.home()
    _create(tag_a, emails=[address])
    r = t.run(home, "add the second contact with the same email, sync", [_add(tag_b, email=address), g.sync(), g.dump("d1.json")])
    t.check("Google keeps both", len(_google_tagged(tag_a)) == 1 and len(_google_tagged(tag_b)) == 1,
            "copies %d and %d" % (len(_google_tagged(tag_a)), len(_google_tagged(tag_b))))
    t.check("the app keeps both", len(_app(r, tag_a)) == 1 and len(_app(r, tag_b)) == 1,
            "found %d and %d" % (len(_app(r, tag_a)), len(_app(r, tag_b))))


@gs("IG-043", "Six emails on Google: the app holds four, and the rest go to Comments")
def ig043(t):
    tag = "IG-043"
    home = t.home()
    emails = [_mail(tag, i) for i in range(1, 7)]
    resource = _create(tag, emails=emails)
    r1 = t.run(home, "first sync", [g.sync(), g.dump("d1.json")])
    t.run(home, "second sync", [g.sync(), g.dump("d2.json")])
    local = _app(r1, tag)
    held = sum(1 for k in EMAIL_KEYS if local and local[0].get(k))
    t.check("the app holds four emails", held == 4, "held %d" % held)
    comments = str(local[0].get("comments", "")) if local else ""
    t.check("the other two are kept in Comments", emails[4] in comments and emails[5] in comments, "comments %r" % comments[:80])
    t.check("Google still holds all six emails after a second sync", len(_info(g.google_person(resource))["emails"]) == 6,
            "Google holds %d" % len(_info(g.google_person(resource))["emails"]))


@gs("IG-044", "Birthdays: leap day and no-year birthday are not changed by the sync")
def ig044(t):
    home = t.home()
    leap = g.create_google_contact({"names": [{"givenName": "Bench", "familyName": "IG-044 leap"}],
                                    "emailAddresses": [{"value": _mail("IG-044", 1)}],
                                    "birthdays": [{"date": {"year": 1980, "month": 2, "day": 29}}]})
    noyear = g.create_google_contact({"names": [{"givenName": "Bench", "familyName": "IG-044 no year"}],
                                      "emailAddresses": [{"value": _mail("IG-044", 2)}],
                                      "birthdays": [{"date": {"month": 12, "day": 25}}]})
    before = {r: g.google_person(r).get("etag") for r in (leap, noyear)}
    t.run(home, "two syncs", [g.sync(), g.sync(), g.dump("d1.json")])
    after = {r: g.google_person(r).get("etag") for r in (leap, noyear)}
    t.check("Google not written by the syncs (leap day)", before[leap] == after[leap], "etag changed")
    t.check("Google not written by the syncs (no year)", before[noyear] == after[noyear], "etag changed")


@gs("IG-045", "Multi-line address round-trips with formatting ignored")
def ig045(t):
    tag = "IG-045"
    home = t.home()
    address = "12 High Street\n  London\n\tSW1A 1AA"
    resource = _create(tag, emails=[_mail(tag)], addresses=[address])
    r = t.run(home, "sync", [g.sync(), g.dump("d1.json")])
    gaddr = [_norm(a) for a in _info(g.google_person(resource))["addresses"]]
    t.check("the Google address is unchanged (formatting ignored)", gaddr == [_norm(address)], "got %s" % gaddr)
    local = _app(r, tag)
    held = bool(local) and any(_norm(local[0].get(k, "")) == _norm(address) for k in ("address", "address2", "address3"))
    t.check("the app holds the address", held, "")


@gs("IG-046", "Hidden, then shown again: the contact is back on Google")
def ig046(t):
    tag = "IG-046"
    home = t.home()
    _create(tag, emails=[_mail(tag)])
    t.run(home, "sync", [g.sync()])
    t.run(home, "hide in the app, sync", [{"op": "hide_contact", "match": {"surname": tag}}, g.sync()])
    r = t.run(home, "show again in the app, sync",
              [{"op": "edit_contact", "match": {"surname": tag}, "fields": {"hidden": "false"}}, g.sync(), g.dump("d1.json")])
    t.check("Google copy back after unhiding", len(_google_tagged(tag)) == 1, "copies %d" % len(_google_tagged(tag)))
    shown = [c for c in _app(r, tag) if c.get("hidden") not in (True, "true")]
    t.check("the app shows it as not hidden", len(shown) == 1, "found %d" % len(shown))


@gs("IG-047", "Edit then delete between syncs: the stale edit is not uploaded")
def ig047(t):
    tag = "IG-047"
    home = t.home()
    _create(tag, emails=[_mail(tag)])
    t.run(home, "sync", [g.sync()])
    t.run(home, "edit, delete in the app, sync",
          [{"op": "edit_contact", "match": {"surname": tag}, "fields": {"surname": tag + " stale edit"}},
           {"op": "delete_contact", "match": {"surname": tag + " stale edit"}}, g.sync(), g.dump("d2.json")])
    t.check("Google no longer holds the contact in any form",
            not _google_tagged(tag) and not _google_tagged(tag + " stale edit"), "copies remain")


@gs("IG-048", "All-day event on Google: read without crashing, not rewritten")
def ig048(t):
    summary = _ev("IG-048", "all day")
    home = t.home()
    eid = g.api("POST", g.CALENDAR + "/events", {"summary": summary, "start": {"date": _date(21)},
                                                  "end": {"date": _date(22)}})["id"]
    before = g.get_event(eid).get("etag")
    r1 = t.run(home, "first sync", [g.sync(), g.dump("d1.json")])
    t.run(home, "second sync", [g.sync(), g.dump("d2.json")])
    after = g.get_event(eid).get("etag")
    t.check("the Google event is not rewritten", before == after, "etag changed")
    t.check("the app holds one entry", len(_app_appts(r1, summary)) == 1, "found %d" % len(_app_appts(r1, summary)))


@gs("IG-049", "Event crossing midnight with a timezone offset keeps the same instant")
def ig049(t):
    summary = _ev("IG-049", "midnight")
    home = t.home()
    g.create_event(summary, _date(22) + "T23:30:00+01:00", _date(23) + "T01:00:00+01:00")
    r1 = t.run(home, "first sync", [g.sync(), g.dump("d1.json")])
    r2 = t.run(home, "second sync", [g.sync(), g.dump("d2.json")])
    a, b = _app_appts(r1, summary), _app_appts(r2, summary)
    t.check("one entry after two syncs", len(a) == 1 and len(b) == 1, "found %d and %d" % (len(a), len(b)))
    t.check("the same local ID after two syncs", len(a) == 1 and len(b) == 1 and a[0].get("id") == b[0].get("id"), "")


@gs("IG-051", "Unicode summary and multi-line description round-trip")
def ig051(t):
    summary = _ev("IG-051", "Café meeting – \U0001F4C5")
    description = "Line one\nLine two é\n\nLine four"
    home = t.home()
    g.api("POST", g.CALENDAR + "/events", {"summary": summary, "description": description,
                                            "start": {"dateTime": _now(24)}, "end": {"dateTime": _now(24, "10:00")}})
    r = t.run(home, "sync", [g.sync(), g.dump("d1.json")])
    t.check("the Google summary is unchanged", len(_google_events(summary)) == 1, "found %d" % len(_google_events(summary)))
    local = _app_appts(r, summary)
    t.check("the description is intact in the app",
            bool(local) and _norm(local[0].get("description")) == _norm(description), "")


@gs("IG-053", "Appointment deleted on both sides before sync: no error, nothing left")
def ig053(t):
    summary = _ev("IG-053", "both sides")
    home = t.home()
    eid = g.create_event(summary, _now(25), _now(25, "10:00"))
    t.run(home, "first sync", [g.sync()])
    t.run(home, "delete in the app", [{"op": "delete_appointment", "match": {"summary": summary}}])
    g.delete_event(eid)
    r = t.run(home, "sync", [g.sync(), g.dump("d1.json")])
    t.check("Google holds none", not _google_events(summary), "found %d" % len(_google_events(summary)))
    t.check("the app holds no live copy", not _app_appts(r, summary), "found %d" % len(_app_appts(r, summary)))


@gs("IG-055", "Five back-to-back syncs with no change: no writes after the first")
def ig055(t):
    tag = "IG-055"
    home = t.home()
    resource = _create(tag, emails=[_mail(tag)])
    t.run(home, "first sync", [g.sync()])
    etag = g.google_person(resource).get("etag")
    t.run(home, "five syncs", [g.sync(), g.sync(), g.sync(), g.sync(), g.sync()])
    t.check("the etag is unchanged after five syncs", g.google_person(resource).get("etag") == etag, "etag changed")


@gs("IG-056", "Twenty contacts made in ContactManager master, then edits on Google under Google master", configs=(CM,))
def ig056(t):
    home = t.home()
    tags = ["IG-056-%02d" % i for i in range(20)]
    t.run(home, "twenty contacts in the app, sync", [_add(tg, email=_mail(tg)) for tg in tags] + [g.sync()])
    home.set_master(True)
    people = _google_prefixed("IG-056-")
    for p in people[:5]:
        tag = _info(p)["tag"]
        g.update_google_contact(p["resourceName"], "names", [{"givenName": "Bench", "familyName": tag + " edited"}])
    t.run(home, "sync under Google master", [g.sync()])
    home.set_master(False)
    t.run(home, "sync back under ContactManager master", [g.sync(), g.dump("d1.json")])
    people = _google_prefixed("IG-056-")
    t.check("twenty contacts on Google, no duplicates", len(people) == 20, "found %d" % len(people))
    edited = [p for p in people if _info(p)["tag"].endswith(" edited")]
    t.check("the five edits from Google survive the switch back", len(edited) == 5, "found %d" % len(edited))


@gs("IG-057", "Four emails from Google (home, other, work, Partner): each keeps its slot and label, no write")
def ig057(t):
    tag = "IG-057"
    home = t.home()
    resource = g.create_google_contact({"names": [{"givenName": "Bench", "familyName": tag}],
                                        "emailAddresses": [{"value": _mail(tag, 1), "type": "home"},
                                                           {"value": _mail(tag, 2), "type": "other"},
                                                           {"value": _mail(tag, 3), "type": "work"},
                                                           {"value": _mail(tag, 4), "type": "Partner"}]})
    before = g.google_person(resource).get("etag")
    r = t.run(home, "sync", [g.sync(), g.dump("d1.json")])
    t.check("Google is not written by the sync", before == g.google_person(resource).get("etag"), "etag changed")
    local = _app(r, tag)
    c = local[0] if local else {}
    got = (c.get("email"), c.get("email2"), c.get("email3"), c.get("email4"))
    t.check("the app holds the four addresses in their slots", got == tuple(_mail(tag, i) for i in range(1, 5)), "got %s" % (got,))
    labels = (c.get("emailtitle"), c.get("email2title"), c.get("email3title"), c.get("email4title"))
    t.check("the app holds the labels (home, other, work, Partner)", tuple(str(x).lower() for x in labels) ==
            ("home", "other", "work", "partner"), "labels %s" % (labels,))


@gs("IG-058", "A label changed in the app is sent to Google as that email's type")
def ig058(t):
    tag = "IG-058"
    home = t.home()
    resource = g.create_google_contact({"names": [{"givenName": "Bench", "familyName": tag}],
                                        "emailAddresses": [{"value": _mail(tag, 1), "type": "home"},
                                                           {"value": _mail(tag, 2), "type": "other"}]})
    t.run(home, "sync", [g.sync()])
    t.run(home, "label the second email Mobile in the app, sync",
          [{"op": "edit_contact", "match": {"surname": tag}, "fields": {"email2title": "Mobile"}}, g.sync(),
           g.dump("d2.json")])
    types = _types_by_email(resource)
    t.check("the second email is sent as Mobile", types.get(_mail(tag, 2)) == "mobile", "Google types %s" % types)
    t.check("the first email keeps its home type", types.get(_mail(tag, 1)) == "home", "Google types %s" % types)


@gs("IG-059", "Emails with no labels set are sent with the labels they always had")
def ig059(t):
    tag = "IG-059"
    home = t.home()
    t.run(home, "add four emails, sync",
          [_add(tag, email=_mail(tag, 1), email2=_mail(tag, 2), email3=_mail(tag, 3), email4=_mail(tag, 4)),
           g.sync(), g.dump("d1.json")])
    copies = _google_tagged(tag)
    t.check("one copy on Google", len(copies) == 1, "copies %d" % len(copies))
    types = _types_by_email(copies[0]["resourceName"]) if copies else {}
    want = ("home", "other", "other", "other")
    got = tuple(types.get(_mail(tag, i)) for i in range(1, 5))
    t.check("Home, Other, Other, Other as Google types", got == want, "Google types %s" % (got,))


@gs("IG-060", "An address with no type is held in the first address slot")
def ig060(t):
    tag = "IG-060"
    home = t.home()
    _create(tag, emails=[_mail(tag)], addresses=["1 Test Road London"])
    r = t.run(home, "sync", [g.sync(), g.dump("d1.json")])
    local = _app(r, tag)
    t.check("the app holds the address in the first slot",
            bool(local) and _norm(local[0].get("address", "")) == _norm("1 Test Road London"),
            "address slots %s" % ({k: v for k, v in (local[0].items() if local else []) if "address" in k},))


@gs("IG-061", "Four phones from Google (home, work, mobile, Pager): each keeps its slot and label, no write")
def ig061(t):
    tag = "IG-061"
    home = t.home()
    resource = g.create_google_contact({"names": [{"givenName": "Bench", "familyName": tag}],
                                        "emailAddresses": [{"value": _mail(tag)}],
                                        "phoneNumbers": [{"value": "+44 20 7946 0101", "type": "home"},
                                                         {"value": "+44 20 7946 0102", "type": "work"},
                                                         {"value": "+44 20 7946 0103", "type": "mobile"},
                                                         {"value": "+44 20 7946 0104", "type": "Pager"}]})
    before = g.google_person(resource).get("etag")
    r = t.run(home, "sync", [g.sync(), g.dump("d1.json")])
    t.check("Google is not written by the sync", before == g.google_person(resource).get("etag"), "etag changed")
    local = _app(r, tag)
    c = local[0] if local else {}
    got = tuple(_digits(c.get(k, "")) for k in ("phone", "work", "mobile", "phone2"))
    t.check("the app holds the four numbers in their slots", got == ("442079460101", "442079460102", "442079460103",
                                                                      "442079460104"), "got %s" % (got,))
    labels = tuple(str(c.get(k, "")).lower() for k in ("phonetitle", "worktitle", "mobiletitle", "phone2title"))
    t.check("the app holds the labels", labels == ("home", "work", "mobile", "pager"), "labels %s" % (labels,))


@gs("IG-062", "A phone label changed in the app is sent to Google as that number's type")
def ig062(t):
    tag = "IG-062"
    home = t.home()
    resource = g.create_google_contact({"names": [{"givenName": "Bench", "familyName": tag}],
                                        "emailAddresses": [{"value": _mail(tag)}],
                                        "phoneNumbers": [{"value": "+44 20 7946 0201", "type": "home"},
                                                         {"value": "+44 20 7946 0202", "type": "other"}]})
    t.run(home, "sync", [g.sync()])
    t.run(home, "label the second number Fax in the app, sync, twice",
          [{"op": "edit_contact", "match": {"surname": tag}, "fields": {"phone2title": "Fax"}}, g.sync(), g.sync(),
           g.dump("d2.json")])
    types = _types_by_digits(resource)
    t.check("the second number is sent as Fax", types.get(_digits("+44 20 7946 0202")) == "fax", "Google types %s" % types)
    t.check("the first number keeps its home type", types.get(_digits("+44 20 7946 0201")) == "home", "Google types %s" % types)


@gs("IG-063", "Phones with no labels set are sent with the labels they always had")
def ig063(t):
    tag = "IG-063"
    home = t.home()
    t.run(home, "add six phones, sync",
          [_add(tag, phone="+44 20 7946 0301", work="+44 20 7946 0302", mobile="+44 20 7946 0303",
                phone2="+44 20 7946 0304", phone3="+44 20 7946 0305", phone4="+44 20 7946 0306"),
           g.sync(), g.dump("d1.json")])
    copies = _google_tagged(tag)
    t.check("one copy on Google", len(copies) == 1, "copies %d" % len(copies))
    types = _types_by_digits(copies[0]["resourceName"]) if copies else {}
    want = [("+44 20 7946 030%d" % i, ty) for i, ty in enumerate(("home", "work", "mobile", "other", "other", "other"), 1)]
    got = [types.get(_digits(v)) for v, _ in want]
    t.check("Home, Work, Mobile, Other, Other, Other as Google types", got == [ty for _, ty in want], "Google types %s" % got)


@gs("IG-064", "Seven phones on Google: the app holds six, the seventh goes to Comments")
def ig064(t):
    tag = "IG-064"
    home = t.home()
    types = ("home", "work", "mobile", "other", "other", "other", "other")
    phones = ["+44 20 7946 04%02d" % i for i in range(1, 8)]
    g.create_google_contact({"names": [{"givenName": "Bench", "familyName": tag}],
                             "emailAddresses": [{"value": _mail(tag)}],
                             "phoneNumbers": [{"value": v, "type": ty} for v, ty in zip(phones, types)]})
    r1 = t.run(home, "first sync", [g.sync(), g.dump("d1.json")])
    t.run(home, "second sync", [g.sync(), g.dump("d2.json")])
    local = _app(r1, tag)
    held = sum(1 for k in PHONE_KEYS if local and local[0].get(k))
    t.check("the app holds six phones", held == 6, "held %d" % held)
    t.check("the seventh number is kept in Comments",
            bool(local) and _digits(phones[6]) in _digits(local[0].get("comments", "")), "")
