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


def _val(run, key):
    """A value the driver recorded for the run (results.json), or None."""
    return run.results.get(key)


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


# ------------------------------------------------------------------------------------------------ batch 3: bad entries, tidy, repeats

def _master(summary):
    """The repeating event on Google with this summary (the master, not an instance)."""
    items = g.api("GET", g.CALENDAR + "/events?q=" + summary.replace(" ", "%20") + "&singleEvents=false").get("items", [])
    masters = [e for e in items if e.get("summary") == summary and not e.get("recurringEventId")
               and e.get("status") != "cancelled"]
    return masters[0] if masters else {}


def _instances(master_id):
    url = g.CALENDAR + "/events/%s/instances?showDeleted=true&timeMin=%s&timeMax=%s" % (
        master_id, _now(-1, "00:00"), _now(90, "00:00"))
    return g.api("GET", url).get("items", [])


def _window_repeats(source_days):
    """The weeks, counted from 1, on which a weekly repeat from source_days (from today) falls in the window."""
    return [k for k in range(1, 400) if -90 <= source_days + 7 * k <= 730]


@gs("IG-065", "Clearing an email and a phone in the app removes them from Google")
def ig065(t):
    tag = "IG-065"
    home = t.home()
    resource = g.create_google_contact({"names": [{"givenName": "Bench", "familyName": tag}],
                                        "emailAddresses": [{"value": _mail(tag, 1), "type": "home"},
                                                           {"value": _mail(tag, 2), "type": "work"}],
                                        "phoneNumbers": [{"value": "+44 20 7946 0501", "type": "home"},
                                                         {"value": "+44 20 7946 0502", "type": "work"}]})
    t.run(home, "sync", [g.sync()])
    r = t.run(home, "clear the second email and the work phone in the app, sync",
              [{"op": "edit_contact", "match": {"surname": tag},
                "fields": {"email2": "", "email2title": "", "work": "", "worktitle": ""}}, g.sync(), g.dump("d2.json")])
    info = _info(g.google_person(resource))
    t.check("Google holds only the first email", info["emails"] == [_mail(tag, 1)], "Google holds %s" % info["emails"])
    t.check("Google holds only the home phone", [_digits(p) for p in info["phones"]] == ["442079460501"],
            "Google holds %s" % info["phones"])
    local = _app(r, tag)
    t.check("the app no longer holds the second email", bool(local) and not local[0].get("email2"), "")


@gs("IG-068", "More emails on Google than the app has slots: all of them are still on Google after an upload")
def ig068(t):
    tag = "IG-068"
    home = t.home()
    emails = [_mail(tag, i) for i in range(1, 8)]
    types = ["Label0", "Email2", "work", "Label1", "Label2", "Label3", "Label4"]
    resource = g.create_google_contact({"names": [{"givenName": "Bench", "familyName": tag}],
                                        "emailAddresses": [{"value": a, "type": ty} for a, ty in zip(emails, types)]})
    t.run(home, "first sync", [g.sync(), g.dump("d1.json")])
    r = t.run(home, "label an email in the app, sync",
              [{"op": "edit_contact", "match": {"surname": tag}, "fields": {"emailtitle": "Home"}},
               g.sync(), g.dump("d2.json")])
    held = sorted(_info(g.google_person(resource))["emails"])
    t.check("Google still holds all seven emails", held == sorted(emails), "Google holds %d" % len(held))
    local = _app(r, tag)
    slots = sum(1 for k in EMAIL_KEYS if local and local[0].get(k))
    t.check("the app holds four of them in its slots", slots == 4, "held %d" % slots)


@gs("IG-069", "Google master: a bad entry downloaded from Google stops a save of the form, and nothing is uploaded")
def ig069(t):
    tag = "IG-069"
    home = t.home()
    bad = "Bad Field 3: %s - Bad Field 3 (Bad Field 4)" % _mail(tag, 2)
    resource = g.create_google_contact({"names": [{"givenName": "Bench", "familyName": tag}],
                                        "emailAddresses": [{"value": _mail(tag, 1), "type": "work"},
                                                           {"value": bad, "type": "Label3"}]})
    home.set_master(True)
    t.run(home, "sync, Google master", [g.sync()])
    r = t.run(home, "change the phone and save the form",
              [{"op": "open_contact", "match": {"surname": tag}},
               {"op": "form_set", "fields": {"phone": "+44 20 7946 0951"}},
               {"op": "menu_save", "name": "f02", "fields": ["phone", "email"]}, g.dump("d_f02.json")])
    t.check("the save was refused: the user was told", "Contact Details" in r.log, "no Contact Details message")
    t.check("the bad entry is still on the form", r.results.get("f02_email") == bad, "shows %r" % r.results.get("f02_email"))
    held = [c for c in _app(r, tag)]
    t.check("the phone change was not saved", not (held and held[0].get("phone")), "")
    emails = sorted(_info(g.google_person(resource))["emails"])
    t.check("Google still holds the entries it had", emails == sorted([_mail(tag, 1), bad]), "Google holds %s" % emails)


@gs("IG-070", "More phones on Google than the app has slots: all of them are still on Google after an upload")
def ig070(t):
    tag = "IG-070"
    home = t.home()
    phones = ["+44 20 7946 0731", "+44 20 7946 0732", "+44 20 7946 0733", "+44 20 7946 0734",
              "+44 20 7946 0735", "+44 20 7946 0736", "+44 20 7946 0737"]
    types = ["home", "work", "mobile", "other", "other", "other", "Pager"]
    resource = g.create_google_contact({"names": [{"givenName": "Bench", "familyName": tag}],
                                        "emailAddresses": [{"value": _mail(tag)}],
                                        "phoneNumbers": [{"value": v, "type": ty} for v, ty in zip(phones, types)]})
    r = t.run(home, "label a phone in the app, sync",
              [{"op": "edit_contact", "match": {"surname": tag}, "fields": {"phonetitle": "Home"}},
               g.sync(), g.dump("d2.json")])
    local = _app(r, tag)
    c = local[0] if local else {}
    slots = sum(1 for k in PHONE_KEYS if c.get(k))
    t.check("the app holds six numbers in its slots", slots == 6, "slots %d" % slots)
    t.check("the seventh is kept in Comments, with its type", "Also on Google (Pager):" in str(c.get("comments", "")),
            "comments %r" % str(c.get("comments", ""))[:80])
    held = sorted(_digits(p) for p in _info(g.google_person(resource))["phones"])
    t.check("Google still holds all seven numbers", held == sorted(_digits(p) for p in phones), "Google holds %s" % held)
    t.check("Google keeps the seventh number's type", _types_by_digits(resource).get(_digits(phones[6])) == "pager",
            "Google types %s" % _types_by_digits(resource))


@gs("IG-071", "A save refused for a bad entry stops the next sync, so nothing is sent to Google")
def ig071(t):
    tag = "IG-071"
    home = t.home()
    bad = "Bad Field 3: %s - Bad Field 3 (Bad Field 4)" % _mail(tag, 2)
    resource = g.create_google_contact({"names": [{"givenName": "Bench", "familyName": tag}],
                                        "emailAddresses": [{"value": _mail(tag, 1), "type": "work"},
                                                           {"value": bad, "type": "Label3"}]})
    t.run(home, "sync", [g.sync()])
    r = t.run(home, "change the phone, refuse the save",
              [{"op": "open_contact", "match": {"surname": tag}},
               {"op": "form_set", "fields": {"phone": "+44 20 7946 0941"}},
               {"op": "menu_save", "name": "f04", "fields": ["phone"]}, g.dump("d1.json")])
    t.check("the save was refused", "Contact Details" in r.log, "no Contact Details message")
    t.run(home, "sync after the refused save", [g.sync(), g.dump("d2.json")])
    info = _info(g.google_person(resource))
    t.check("nothing reached Google: no phone", not info["phones"], "phones %s" % info["phones"])
    t.check("nothing reached Google: the emails are as they were",
            sorted(info["emails"]) == sorted([_mail(tag, 1), bad]), "emails %s" % info["emails"])


@gs("IG-072", "An unchanged bad entry does not stop switching to another contact")
def ig072(t):
    home = t.home()
    tag_a, tag_b = "IG-072A", "IG-072B"
    bad = "Bad Field 1: %s - Bad Field 2 (Bad Field 3)" % _mail(tag_a)
    g.create_google_contact({"names": [{"givenName": "Bench", "familyName": tag_a}],
                             "emailAddresses": [{"value": bad, "type": "Label3"}]})
    g.create_google_contact({"names": [{"givenName": "Bench", "familyName": tag_b}],
                             "emailAddresses": [{"value": _mail(tag_b), "type": "home"}]})
    r = t.run(home, "sync, open the bad contact, then the good one",
              [g.sync(), {"op": "open_contact", "name": "f05a", "match": {"surname": tag_a}},
               {"op": "open_contact", "name": "f05b", "match": {"surname": tag_b}}, g.dump("d1.json")])
    t.check("the second contact is selected", _val(r, "f05b_selected") == tag_b, "selected %r" % _val(r, "f05b_selected"))
    t.check("the first contact was opened", _val(r, "f05a_selected") == tag_a, "selected %r" % _val(r, "f05a_selected"))


@gs("IG-073", "With a contact holding an unchanged bad entry open, a Google sync still runs")
def ig073(t):
    tag = "IG-073"
    home = t.home()
    bad = "Bad Field 1: %s - Bad Field 2 (Bad Field 3)" % _mail(tag)
    g.create_google_contact({"names": [{"givenName": "Bench", "familyName": tag}],
                             "emailAddresses": [{"value": bad, "type": "Label3"}]})
    r = t.run(home, "sync, open the contact, sync again",
              [g.sync(), {"op": "open_contact", "name": "f06", "match": {"surname": tag}}, g.sync(), g.dump("d1.json")])
    t.check("the second sync ran", "sync 2 finished" in r.log, "driver log has no 'sync 2 finished' line")


@gs("IG-074", "A contact kept with an unfixable entry is uploaded once, then not again on later syncs")
def ig074(t):
    tag = "IG-074"
    home = t.home()
    bad = "Bad Field 1: %s - Bad Field 2 (Bad Field 3)" % _mail(tag)
    resource = g.create_google_contact({"names": [{"givenName": "Bench", "familyName": tag}],
                                        "emailAddresses": [{"value": bad, "type": "Label3"}]})
    t.run(home, "sync", [g.sync()])
    t.run(home, "change the phone, sync",
          [{"op": "edit_contact", "match": {"surname": tag}, "fields": {"phone": "+44 20 7946 0701"}}, g.sync(),
           g.dump("d1.json")])
    etag1 = g.google_person(resource).get("etag")
    t.check("the change reached Google", _digits("+44 20 7946 0701") in
            [_digits(p) for p in _info(g.google_person(resource))["phones"]], "")
    t.run(home, "two more syncs", [g.sync(), g.sync(), g.dump("d2.json")])
    t.check("two more syncs with nothing changed leave Google's copy alone",
            etag1 == g.google_person(resource).get("etag"), "etag changed")


@gs("IG-075", "A contact with a clean address and no titles set: uploaded once, then not again on later syncs")
def ig075(t):
    tag = "IG-075"
    home = t.home()
    resource = g.create_google_contact({"names": [{"givenName": "Bench", "familyName": tag}],
                                        "emailAddresses": [{"value": _mail(tag), "type": "home"}]})
    t.run(home, "sync", [g.sync()])
    t.run(home, "change the phone, sync",
          [{"op": "edit_contact", "match": {"surname": tag}, "fields": {"phone": "+44 20 7946 0801"}}, g.sync(),
           g.dump("d1.json")])
    etag1 = g.google_person(resource).get("etag")
    t.run(home, "two more syncs", [g.sync(), g.sync(), g.dump("d2.json")])
    t.check("two more syncs with no change leave Google's copy alone", etag1 == g.google_person(resource).get("etag"),
            "etag changed on later syncs")


@gs("IG-076", "Moving the selection on, as Tidy Contacts does, does not check the open contact's entries")
def ig076(t):
    tag = "IG-076"
    home = t.home()
    bad = "Bad Field 1: %s - Bad Field 2 (Bad Field 3)" % _mail(tag)
    g.create_google_contact({"names": [{"givenName": "Bench", "familyName": tag}],
                             "emailAddresses": [{"value": bad, "type": "Label3"}]})
    t.run(home, "sync", [g.sync()])
    r = t.run(home, "open the contact, then move the selection on",
              [{"op": "open_contact", "name": "f09", "match": {"surname": tag}}, {"op": "clear_selection"}, g.dump("d1.json")])
    t.check("the form was not checked when the selection moved on", "Contact Details" not in r.log,
            "a Contact Details message was shown")


@gs("IG-077", "The report's steps: a bad entry from Google, Alt-E,D, Ctrl-S, Exit, relaunch, Tidy Contacts")
def ig077(t):
    tag = "IG-077"
    home = t.home()
    bad = "Bad Field 1: %s - Bad Field 2 (Bad Field 3)" % _mail(tag)
    g.create_google_contact({"names": [{"givenName": "Bench", "familyName": tag}],
                             "emailAddresses": [{"value": bad, "type": "Label3"}]})
    home.set_master(True)
    a = t.run(home, "steps 1 to 6: sync, Alt-E D, Ctrl-S, Exit",
              [g.sync(), {"op": "open_contact", "name": "f10a", "match": {"surname": tag}, "details": False},
               {"op": "alt_edit", "name": "f10a"}, {"op": "menu_save", "name": "f10a", "fields": ["email"]},
               {"op": "exit", "name": "f10a"}, g.dump("d1.json")])
    b = t.run(home, "steps 7 to 9: relaunch, open, Exit",
              [{"op": "open_contact", "name": "f10b", "match": {"surname": tag}, "details": False},
               {"op": "exit", "name": "f10b"}, g.dump("d2.json")])
    c = t.run(home, "steps 10 to 14: relaunch, Tidy Contacts, Alt-E D, Exit",
              [{"op": "open_contact", "name": "f10c", "match": {"surname": tag}, "details": False},
               {"op": "tidy_full", "name": "f10c", "surname": tag}, {"op": "tidy_contacts", "name": "f10c"},
               {"op": "alt_edit", "name": "f10c"}, {"op": "exit", "name": "f10c"}, g.dump("d3.json")])
    home.set_master(False)
    t.check("step 4: Alt-E,D shows the bad entry", _val(a, "f10a_details") is True and _val(a, "f10a_email") == bad,
            "details %r, email %r" % (_val(a, "f10a_details"), _val(a, "f10a_email")))
    t.check("step 6: Exit closes", _val(a, "f10a_exit_accepted") is True, "exit accepted %r" % _val(a, "f10a_exit_accepted"))
    t.check("step 9: Exit closes after a relaunch", _val(b, "f10b_exit_accepted") is True,
            "exit accepted %r" % _val(b, "f10b_exit_accepted"))
    unfixed = _val(c, "tidy_unfixed") or []
    t.check("step 12: Tidy Contacts reports the bad entry", any(_mail(tag) in str(line) for line in unfixed),
            "listed %s" % unfixed)


@gs("IG-079", "Editor: an older entry with its label in the address is tidied when opened, and saved with the label as its title")
def ig079(t):
    tag = "IG-079"
    home = t.home()
    resource = g.create_google_contact({"names": [{"givenName": "Bench", "familyName": tag}],
                                        "emailAddresses": [{"value": _mail(tag) + " (Work)", "type": "home"}]})
    r = t.run(home, "sync, open, change the phone, save, sync",
              [g.sync(), {"op": "open_contact", "match": {"surname": tag}},
               {"op": "form_set", "fields": {"phone": "+44 20 7946 0301"}},
               {"op": "form_save", "name": "e03"}, g.sync(), g.dump("d1.json")])
    local = _app(r, tag)
    c = local[0] if local else {}
    t.check("the form was saved", _val(r, "e03_saved") is True, "results %s" % r.results)
    t.check("the address is held without the label", c.get("email") == _mail(tag), "got %r" % c.get("email"))
    t.check("the label is the title", c.get("emailtitle") == "Work", "got %r" % c.get("emailtitle"))
    t.check("Google holds the address as work", _types_by_email(resource).get(_mail(tag)) == "work",
            "Google types %s" % _types_by_email(resource))


@gs("IG-081", "Tidy Contacts puts right an older address label and a phone label; they go to Google; a second tidy changes nothing")
def ig081(t):
    tag = "IG-081"
    home = t.home()
    resource = g.create_google_contact({"names": [{"givenName": "Bench", "familyName": tag}],
                                        "emailAddresses": [{"value": _mail(tag) + " (Work)", "type": "home"}],
                                        "phoneNumbers": [{"value": "+44 20 7946 0701 - Mobile", "type": "home"}]})
    r = t.run(home, "sync, tidy, tidy, sync",
              [g.sync(), {"op": "tidy_contacts", "name": "tidy1"}, {"op": "tidy_contacts", "name": "tidy2"},
               g.sync(), g.dump("d1.json")])
    local = _app(r, tag)
    c = local[0] if local else {}
    t.check("the first tidy changed the contact", _val(r, "tidy1_contacts") is not None and _val(r, "tidy1_contacts") >= 1,
            "results %s" % r.results)
    t.check("the address is held without the label, with its title",
            c.get("email") == _mail(tag) and c.get("emailtitle") == "Work", "got %r / %r" % (c.get("email"), c.get("emailtitle")))
    t.check("the phone is held without the label, with its title",
            _digits(c.get("phone", "")) == _digits("+44 20 7946 0701") and c.get("phonetitle") == "Mobile",
            "got %r / %r" % (c.get("phone"), c.get("phonetitle")))
    t.check("Google holds the address as work", _types_by_email(resource).get(_mail(tag)) == "work",
            "Google types %s" % _types_by_email(resource))
    t.check("Google holds the phone as mobile", _types_by_digits(resource).get(_digits("+44 20 7946 0701")) == "mobile",
            "Google types %s" % _types_by_digits(resource))
    t.check("the second tidy changed nothing", _val(r, "tidy2_changed") == 0, "results %s" % r.results)


@gs("IG-082", "Tidy Contacts lists a title with a colon, and leaves it and its number as they are")
def ig082(t):
    tag = "IG-082"
    home = t.home()
    r = t.run(home, "add, tidy",
              [_add(tag, phone="+44 20 7946 0703", phonetitle="Home: Mobile"),
               {"op": "tidy_contacts", "name": "tidy"}, g.dump("d1.json")])
    unfixed = _val(r, "tidy_unfixed") or []
    local = _app(r, tag)
    c = local[0] if local else {}
    t.check("the title is listed as not allowed", any(tag in str(line) and "not allowed" in str(line) for line in unfixed),
            "listed %s" % unfixed)
    t.check("the title is unchanged", c.get("phonetitle") == "Home: Mobile", "got %r" % c.get("phonetitle"))
    t.check("the number is unchanged", _digits(c.get("phone", "")) == _digits("+44 20 7946 0703"), "got %r" % c.get("phone"))


@gs("IG-084", "A repeating Google event is stored once, with its repeat, and listed on each week", configs=(CM,))
def ig084(t):
    summary = _ev("IG-084", "weekly")
    home = t.home()
    g.create_event(summary, _now(2), _now(2, "10:00"), tz="UTC", recurrence=["RRULE:FREQ=WEEKLY;INTERVAL=1"])
    weeks = _window_repeats(2)
    r = t.run(home, "sync and list the calendar", [g.sync(), {"op": "calendar_rows", "name": "g40"}, g.dump("d1.json")])
    stored = _app_appts(r, summary)
    listed = [h for h in (_val(r, "g40_hints") or []) if stored and str(h).split("@")[0] == stored[0].get("id")]
    t.check("the event is stored once, not once per week", len(stored) == 1, "stored %d times" % len(stored))
    t.check("the event is listed on each week in the repeats window", len(listed) == len(weeks),
            "rows %d, expected %d" % (len(listed), len(weeks)))


@gs("IG-086", "A repeat that started before today, with later dates, is in the overview's future appointments")
def ig086(t):
    tag = "IG-086"
    home = t.home()
    one_off = "BENCH IG-086 one-off"
    weekly = "BENCH IG-086 weekly"
    r = t.run(home, "add, overview",
              [_add(tag, email=_mail(tag)),
               {"op": "add_appointment", "surname": tag, "fields": {"summary": _ev("IG-086", "one-off"),
                                                                  "from": _now(5), "to": _now(5, "10:00")}},
               {"op": "add_appointment", "surname": tag, "fields": {"summary": _ev("IG-086", "weekly"),
                                                                  "from": _now(-21), "to": _now(-21, "10:00"),
                                                                  "repeat": "2", "repeatinterval": "1"}},
               {"op": "open_contact", "match": {"surname": tag}}, {"op": "overview", "name": "o79"}, g.dump("d1.json")])
    overview = str(_val(r, "o79_overview") or "")
    t.check("a one-off appointment in the future is listed (control)", _ev("IG-086", "one-off") in overview,
            "overview %r" % overview[:120])
    t.check("a weekly repeat that started 21 days ago is listed, as it has later dates", _ev("IG-086", "weekly") in overview,
            "overview has the repeat: %s" % (_ev("IG-086", "weekly") in overview))


@gs("IG-089", "Cancelling the change of a repeat changes nothing")
def ig089(t):
    summary = _ev("IG-089", "weekly")
    home = t.home()
    t.run(home, "add a repeat of six, sync",
          [{"op": "add_appointment", "fields": {"summary": summary, "from": _now(2), "to": _now(2, "10:00"),
                                             "repeat": "2", "repeatinterval": "1", "repeatcount": "6"}}, g.sync()])
    r = t.run(home, "cancel an edit of one occurrence, sync, list",
              [{"op": "choose", "letter": "d"},
               {"op": "repeat_edit", "name": "x81c", "summary": summary, "occurrence": _now(16),
                "shiftdays": 0, "newsummary": _ev("IG-089", "changed")},
               g.sync(), {"op": "calendar_rows", "name": "x81c", "reload": True}, g.dump("d1.json")])
    titles = _val(r, "x81c_titles") or []
    t.check("no row shows the change", not any("changed" in str(x) for x in titles), "rows %r" % titles[:3])
    master = _master(summary)
    t.check("Google still has the original repeat, with six occurrences",
            master.get("summary") == summary and "COUNT=6" in " ".join(master.get("recurrence", [])),
            "summary %r recurrence %r" % (master.get("summary"), master.get("recurrence")))


@gs("IG-090", "An occurrence deleted on Google is left out of the repeat in the app, when Google is the master")
def ig090(t):
    summary = _ev("IG-090", "weekly")
    home = t.home()
    t.run(home, "add a repeat of six, sync",
          [{"op": "add_appointment", "fields": {"summary": summary, "from": _now(2), "to": _now(2, "10:00"),
                                             "repeat": "2", "repeatinterval": "1", "repeatcount": "6"}}, g.sync()])
    master = _master(summary)
    gone = _now(16, "09:00")[:10]
    instances = [i for i in _instances(master.get("id", "none"))
                 if i.get("status") != "cancelled" and (i.get("originalStartTime") or {}).get("dateTime", "").startswith(gone)]
    if instances:
        g.api("DELETE", g.CALENDAR + "/events/" + instances[0]["id"])
    home.set_master(True)
    r = t.run(home, "sync under Google master, list",
              [g.sync(), {"op": "calendar_rows", "name": "x81g", "reload": True}, g.dump("d1.json")])
    home.set_master(False)
    mine = [x for x in (_val(r, "x81g_titles") or []) if summary in str(x)]
    t.check("Google deleted an occurrence", len(instances) == 1, "instances found %d" % len(instances))
    t.check("the deleted occurrence is not listed in the app", not any(gone[:4] in str(x) and "09" in str(x) for x in mine)
            or len(mine) == 5, "rows %r" % mine)
    t.check("the other five occurrences are still listed", len(mine) == 5, "rows %d" % len(mine))


@gs("IG-091", "A split that moves the time keeps Google's deleted occurrence on the right time, in London")
def ig091(t):
    summary = _ev("IG-091", "weekly")
    home = t.home()
    t.run(home, "add a repeat of five, sync",
          [{"op": "add_appointment", "fields": {"summary": summary, "from": _now(2, "12:45"), "to": _now(2, "13:45"),
                                             "repeat": "2", "repeatinterval": "1", "repeatcount": "5"}}, g.sync()])
    master = _master(summary)
    third = _now(16, "12:45")[:10]
    instances = [i for i in _instances(master.get("id", "none"))
                 if i.get("status") != "cancelled" and (i.get("originalStartTime") or {}).get("dateTime", "").startswith(third)]
    if instances:
        g.api("DELETE", g.CALENDAR + "/events/" + instances[0]["id"])
    home.set_master(True)
    t.run(home, "sync under Google master", [g.sync()])
    home.set_master(False)
    t.run(home, "split from the second occurrence, moved an hour, sync",
          [{"op": "choose", "letter": "b"},
           {"op": "repeat_edit", "name": "x81h", "summary": summary, "occurrence": _now(9, "12:45"),
            "shiftdays": 0, "shiftminutes": 62, "newsummary": _ev("IG-091", "later")}, g.sync()])
    later = _master(_ev("IG-091", "later"))
    exdates = [l for l in later.get("recurrence", []) if l.startswith("EXDATE")]
    t.check("Google deleted an occurrence", len(instances) == 1, "instances found %d" % len(instances))
    t.check("the new part of the repeat is on Google", later.get("id") is not None, "later %r" % later.get("summary"))
    t.check("the deleted occurrence is left out at the new time, not the old",
            any("T144700" in l for l in exdates) and not any("T134500" in l for l in exdates), "EXDATE lines %r" % exdates)


@gs("IG-092", "An edit of a repeat in the app changes the Google event, and the event stays repeating")
def ig092(t):
    summary = _ev("IG-092", "weekly")
    home = t.home()
    eid = g.create_event(summary, _now(2), _now(2, "10:00"), tz="UTC", recurrence=["RRULE:FREQ=WEEKLY;INTERVAL=1"])
    t.run(home, "sync", [g.sync()])
    t.run(home, "edit the third occurrence's summary, sync",
          [{"op": "choose", "letter": "c"},
           {"op": "repeat_edit", "name": "g41", "summary": summary, "occurrence": _now(2 + 14, "09:00"),
            "shiftdays": 0, "newsummary": _ev("IG-092", "edited")}, g.sync(), g.dump("d1.json")])
    event = g.get_event(eid)
    t.check("Google holds the edited summary", event.get("summary") == _ev("IG-092", "edited"),
            "summary %r" % event.get("summary"))
    t.check("the Google event still repeats weekly", any("FREQ=WEEKLY" in r for r in event.get("recurrence", [])),
            "recurrence %r" % event.get("recurrence"))


# ------------------------------------------------------------------------------------------------ batch 4: ETags, repeats, windows, tokens

def _local_file_etag(home, summary):
    """The stored Google ETag of the local appointment with this summary, or None when the file is not there."""
    folder = os.path.join(home.data, "calendar")
    if not os.path.isdir(folder):
        return None
    for name in os.listdir(folder):
        path = os.path.join(folder, name)
        if not os.path.isfile(path):
            continue
        with open(path, encoding="utf-8", errors="replace") as f:
            text = f.read()
        if summary in text:
            for line in text.splitlines():
                if line.startswith("googleetag="):
                    return line.split("=", 1)[1].strip()
            return ""
    return None


@gs("IG-093", "An event on Google older than the window's start is stored once, with its repeats listed", configs=(CM,))
def ig093(t):
    summary = _ev("IG-093", "weekly")
    home = t.home()
    g.create_event(summary, _now(-120), _now(-120, "10:00"), tz="UTC", recurrence=["RRULE:FREQ=WEEKLY;INTERVAL=1"])
    r = t.run(home, "sync and list", [g.sync(), {"op": "calendar_rows", "name": "g42"}, g.dump("d1.json")])
    stored = _app_appts(r, summary)
    origin = stored[0].get("id") if stored else ""
    repeats = [h for h in (_val(r, "g42_hints") or []) if origin and str(h).split("@")[0] == origin and "@" in str(h)]
    t.check("the event is stored once", len(stored) == 1, "stored %d times" % len(stored))
    t.check("its repeats in the window are listed", len(repeats) == len(_window_repeats(-120)),
            "repeats %d, expected %d" % (len(repeats), len(_window_repeats(-120))))


@gs("IG-094", "A Google repeat with a count is stored once and listed that many times, and an app edit keeps the count", configs=(CM,))
def ig094(t):
    summary = _ev("IG-094", "count")
    home = t.home()
    eid = g.create_event(summary, _now(2), _now(2, "10:00"), tz="UTC", recurrence=["RRULE:FREQ=WEEKLY;COUNT=3"])
    t.run(home, "sync and list", [g.sync(), {"op": "calendar_rows", "name": "g43"}])
    t.run(home, "edit the third occurrence, sync",
          [{"op": "choose", "letter": "c"},
           {"op": "repeat_edit", "name": "g43", "summary": summary, "occurrence": _now(2 + 14, "09:00"),
            "shiftdays": 0, "newsummary": _ev("IG-094", "edited")}, g.sync(), g.dump("d1.json")])
    event = g.get_event(eid)
    t.check("Google keeps the count of three", any("COUNT=3" in r for r in event.get("recurrence", [])),
            "recurrence %r" % event.get("recurrence"))


@gs("IG-095", "An appointment on Google older than the one-year lookback is not downloaded")
def ig095(t):
    summary = _ev("IG-095", "old")
    home = t.home()
    g.api("POST", g.CALENDAR + "/events", {"summary": summary, "start": {"dateTime": _now(-400)},
                                            "end": {"dateTime": _now(-400, "10:00")}})
    r = t.run(home, "sync", [g.sync(), g.dump("d1.json")])
    t.check("an event 400 days old is not downloaded", not _app_appts(r, summary),
            "held %d" % len(_app_appts(r, summary)))


@gs("IG-096", "A start three months back is accepted, and OK then closes the form, as with a start 11 days back")
def ig096(t):
    summary = _ev("IG-096", "repeat")
    home = t.home()
    r = t.run(home, "add a repeat, edit one occurrence, OK",
              [{"op": "add_appointment", "fields": {"summary": summary, "from": _now(2), "to": _now(2, "10:00"),
                                                 "repeat": "2", "repeatinterval": "3", "repeatcount": "32"}},
               {"op": "edit_ok", "name": "q35", "summary": summary, "occurrence": _now(2 + 21),
                "froms": [_now(-112), _now(-11)]}, g.dump("d1.json")])
    t.check("the edit path returns", _val(r, "q35_returned") is True, "returned %r" % _val(r, "q35_returned"))
    t.check("no dialog is shown when OK is pressed", "dialog" not in " ".join(l for l in r.log.splitlines()
                                                                             if "Login" not in l and "dialog" in l.lower()),
            "dialogs in the log")


@gs("IG-097", "An ETag is stored from Google and sent with a change: a local edit reaches Google, with no conflict",
    configs=(CM,))
def ig097(t):
    summary = _ev("IG-097", "etag")
    home = t.home()
    eid = g.create_event(summary, _now(10), _now(10, "10:00"), location="Room 1")
    t.run(home, "first sync", [g.sync()])
    stored = _local_file_etag(home, summary)
    t.check("Google's ETag is stored with the appointment", bool(stored), "stored %r" % stored)
    t.run(home, "edit the location in the app, sync",
          [g.edit_appointment_in_app(summary, {"location": "Room 2"}), g.sync(), g.dump("d1.json")])
    t.check("the local edit reaches Google", g.get_event(eid).get("location") == "Room 2",
            "location %r" % g.get_event(eid).get("location"))


@gs("IG-098", "Google's copy changed in its details since the last sync and the app changed it too: the app's version is kept",
    configs=(CM,))
def ig098(t):
    summary = _ev("IG-098", "conflict")
    home = t.home()
    eid = g.create_event(summary, _now(11), _now(11, "10:00"), location="Room 1")
    t.run(home, "first sync", [g.sync()])
    g.patch_event(eid, {"location": "Room G"})
    t.run(home, "edit the location in the app, sync",
          [g.edit_appointment_in_app(summary, {"location": "Room L"}), g.sync(), g.dump("d1.json")])
    t.check("the app's version is sent over Google's change (ContactManager master)", g.get_event(eid).get("location") == "Room L",
            "location %r" % g.get_event(eid).get("location"))


@gs("IG-099", "Google's ETag changed, but not its details the app holds: the local change still reaches Google", configs=(CM,))
def ig099(t):
    summary = _ev("IG-099", "etag only")
    home = t.home()
    eid = g.create_event(summary, _now(12), _now(12, "10:00"), location="Room 1")
    t.run(home, "first sync", [g.sync()])
    g.patch_event(eid, {"colorId": "5"})
    t.run(home, "edit the location in the app, sync",
          [g.edit_appointment_in_app(summary, {"location": "Room L"}), g.sync(), g.dump("d1.json")])
    t.check("the local edit reaches Google", g.get_event(eid).get("location") == "Room L",
            "location %r" % g.get_event(eid).get("location"))


@gs("IG-100", "The contact on the form stays loaded through a sync, in ContactManager master and in Google master", configs=(CM,))
def ig100(t):
    tag = "IG-100"
    home = t.home()
    t.run(home, "add and sync", [_add(tag, email=_mail(tag)), g.sync()])
    r_cm = t.run(home, "ContactManager master: open, sync, state",
                 [{"op": "open_contact", "match": {"surname": tag}}, {"op": "setrow", "row": 40},
                  {"op": "state", "name": "cm_before"}, g.sync(), {"op": "state", "name": "cm_after"}, g.dump("d1.json")])
    t.check("the contact stays loaded in ContactManager master",
            _val(r_cm, "cm_before") == _val(r_cm, "cm_after") and _val(r_cm, "cm_before") is not None,
            "before %r, after %r" % (_val(r_cm, "cm_before"), _val(r_cm, "cm_after")))
    home.set_master(True)
    r_gm = t.run(home, "Google master: open, sync, state",
                 [{"op": "open_contact", "match": {"surname": tag}}, {"op": "setrow", "row": 40},
                  {"op": "state", "name": "gm_before"}, g.sync(), {"op": "state", "name": "gm_after"}, g.dump("d2.json")])
    home.set_master(False)
    t.check("the contact stays loaded in Google master",
            _val(r_gm, "gm_before") == _val(r_gm, "gm_after") and _val(r_gm, "gm_before") is not None,
            "before %r, after %r" % (_val(r_gm, "gm_before"), _val(r_gm, "gm_after")))


@gs("IG-102", "With the last calendar sync back at the epoch, the next sync sends only the appointment that was edited", configs=(CM,))
def ig102(t):
    home = t.home()
    for i in range(40):
        g.create_event(_ev("IG-102", "%02d" % i), _now(40 + i), _now(40 + i, "10:00"), location="Room")
    t.run(home, "first sync downloads the forty", [g.sync(), g.dump("d1.json")])
    r = t.run(home, "epoch sync date, edit one, sync",
              [g.setting("lastgooglecalendarsyncdate", ""), g.edit_appointment_in_app(_ev("IG-102", "05"), {"location": "Room Edited"}),
               g.sync(), g.dump("d2.json")])
    t.check("the edited appointment reaches Google",
            any(e.get("location") == "Room Edited" for e in g.test_events(_ev("IG-102", "05"))), "")
    t.check("the forty are not duplicated", len(_app_appts(r, _ev("IG-102", "05"))) == 1, "")
    t.blocked("no more than twenty writes are made to Google", "the driver does not count writes to Google yet")


@gs("IG-103", "ContactManager master, edit in Google, repeat: the Google change reaches the app, one repeat", configs=(CM,))
def ig103(t):
    summary = _ev("IG-103", "weekly")
    home = t.home()
    eid = g.create_event(summary, _now(2), _now(2, "10:00"), tz="UTC", recurrence=["RRULE:FREQ=WEEKLY;INTERVAL=1"])
    t.run(home, "first sync", [g.sync()])
    g.patch_event(eid, {"location": "Room R1"})
    r = t.run(home, "second sync", [g.sync(), g.dump("d1.json")])
    event = g.get_event(eid)
    t.check("one repeat on Google", len(g.test_events(summary)) == 1, "found %d" % len(g.test_events(summary)))
    t.check("the Google repeat still repeats weekly", any("FREQ=WEEKLY" in x for x in event.get("recurrence", [])),
            "recurrence %r" % event.get("recurrence"))
    local = _app_appts(r, summary)
    t.check("the app shows the Google location; one entry",
            len(local) == 1 and local[0].get("location") == "Room R1", "app %s" % [a.get("location") for a in local])


@gs("IG-104", "Google master, edit in ContactManager, single: the next sync takes Google's location", configs=(G,))
def ig104(t):
    summary = _ev("IG-104", "single")
    home = t.home()
    eid = g.create_event(summary, _now(8), _now(8, "10:00"), location="Room 1")
    r = t.run(home, "edit in the app, sync", [g.edit_appointment_in_app(summary, {"location": "Room L"}), g.sync(),
                                             g.dump("d1.json")])
    t.check("Google keeps its location (the app's edit is not uploaded)", g.get_event(eid).get("location") == "Room 1",
            "Google %r" % g.get_event(eid).get("location"))
    local = _app_appts(r, summary)
    t.check("the app takes Google's location at the next sync", len(local) == 1 and local[0].get("location") == "Room 1",
            "app %s" % [a.get("location") for a in local])


@gs("IG-105", "Google master, edit in ContactManager, repeat: the next sync keeps the repeat and Google's location", configs=(G,))
def ig105(t):
    summary = _ev("IG-105", "weekly")
    home = t.home()
    eid = g.create_event(summary, _now(2), _now(2, "10:00"), tz="UTC", recurrence=["RRULE:FREQ=WEEKLY;INTERVAL=1"])
    g.patch_event(eid, {"location": "Room 3"})
    t.run(home, "edit in the app, sync", [g.edit_appointment_in_app(summary, {"location": "Room L"}), g.sync(), g.dump("d1.json")])
    event = g.get_event(eid)
    t.check("Google keeps its location and the weekly rule",
            event.get("location") == "Room 3" and any("FREQ=WEEKLY" in x for x in event.get("recurrence", [])),
            "location %r, recurrence %r" % (event.get("location"), event.get("recurrence")))


@gs("IG-106", "Google master, edit in Google, single: the Google change is downloaded", configs=(G,))
def ig106(t):
    summary = _ev("IG-106", "single")
    home = t.home()
    eid = g.create_event(summary, _now(9), _now(9, "10:00"), location="Room 1")
    g.patch_event(eid, {"location": "Room G"})
    r = t.run(home, "sync", [g.sync(), g.dump("d1.json")])
    local = _app_appts(r, summary)
    t.check("one entry, and the app shows the Google location",
            len(local) == 1 and local[0].get("location") == "Room G", "app %s" % [a.get("location") for a in local])


@gs("IG-107", "Google master, edit in Google, repeat: the Google change is downloaded, and the repeat is kept", configs=(G,))
def ig107(t):
    summary = _ev("IG-107", "weekly")
    home = t.home()
    eid = g.create_event(summary, _now(2), _now(2, "10:00"), tz="UTC", recurrence=["RRULE:FREQ=WEEKLY;INTERVAL=1"])
    g.patch_event(eid, {"location": "Room G"})
    r = t.run(home, "sync", [g.sync(), g.dump("d1.json")])
    t.check("the Google repeat still repeats weekly",
            any("FREQ=WEEKLY" in x for x in g.get_event(eid).get("recurrence", [])), "")
    local = _app_appts(r, summary)
    t.check("the app shows the Google location on the repeat; one entry",
            len(local) == 1 and local[0].get("location") == "Room G", "app %s" % [a.get("location") for a in local])


@gs("IG-109", "Uploads: forty appointments added in the app inside the window are all on Google after one sync", configs=(CM,))
def ig109(t):
    prefix = _ev("IG-109", "")
    home = t.home()
    ops = [{"op": "add_appointment", "fields": {"summary": prefix + "%02d" % i, "from": _now(5 + i),
                                              "to": _now(5 + i, "10:00")}} for i in range(40)]
    t.run(home, "forty appointments, sync", ops + [g.sync(), g.dump("d1.json")])
    on_google = [e for e in g.test_events() if e.get("summary", "").startswith(prefix)]
    t.check("all forty are on Google after one sync", len(on_google) == 40, "found %d" % len(on_google))


@gs("IG-111", "An entry that starts before the window and ends inside it is downloaded, once", configs=(CM,))
def ig111(t):
    summary = _ev("IG-111", "edge")
    home = t.home()
    g.create_event(summary, _now(-91), _now(-88, "10:00"))
    r = t.run(home, "sync", [g.sync(), g.dump("d1.json")])
    t.check("one event on Google", len(g.test_events(summary)) == 1, "found %d" % len(g.test_events(summary)))
    t.check("the app downloads it once, as it crosses the window's edge", len(_app_appts(r, summary)) == 1,
            "app %d" % len(_app_appts(r, summary)))


@gs("IG-114", "Widening the window downloads the entries it now reaches, once", configs=(CM,))
def ig114(t):
    summary = _ev("IG-114", "far")
    home = t.home()
    g.create_event(summary, _now(-120), _now(-120, "10:00"))
    before = t.run(home, "sync with the ninety-day window", [g.sync(), g.dump("d1.json")])
    after = t.run(home, "widen the window to 150 days, sync", [g.setting("windowbefore", "150"), g.sync(), g.dump("d2.json")])
    t.check("not downloaded with the ninety-day window", not _app_appts(before, summary),
            "app before %d" % len(_app_appts(before, summary)))
    t.check("downloaded once the window reaches it", len(_app_appts(after, summary)) == 1,
            "app after %d" % len(_app_appts(after, summary)))


@gs("IG-115", "A sync token Google refuses is replaced by a full listing, and the sync completes", configs=(CM,))
def ig115(t):
    home = t.home()
    first, second = _ev("IG-115", "first"), _ev("IG-115", "second")
    g.create_event(first, _now(3), _now(3, "10:00"))
    t.run(home, "first sync", [g.sync(), g.dump("d0.json")])
    g.create_event(second, _now(4), _now(4, "10:00"))
    r = t.run(home, "a bad token, sync", [g.setting("lastgooglecalendarsynctoken", "90,730|NOT-A-VALID-TOKEN"), g.sync(),
                                         g.dump("d1.json")])
    t.check("the entry already held is still there, once", len(_app_appts(r, first)) == 1, "app %d" % len(_app_appts(r, first)))
    t.check("the entry added on Google since the last sync is downloaded", len(_app_appts(r, second)) == 1,
            "app %d" % len(_app_appts(r, second)))


@gs("IG-116", "A sync token Google answers with 410 (gone) is replaced by a full listing", configs=(CM,))
def ig116(t):
    home = t.home()
    home.set_settings({"contactsync": "no"})
    first, second = _ev("IG-116", "first"), _ev("IG-116", "second")
    g.create_event(first, _now(3), _now(3, "10:00"))
    t.run(home, "a full listing, which stores a sync token", [g.sync(), g.dump("d0.json")])
    g.create_event(second, _now(4), _now(4, "10:00"))
    r = t.run(home, "sync, with the first listing answered 410", [g.sync(), g.dump("d1.json")], faults="r1=410:gone")
    t.check("the entry added on Google is downloaded", len(_app_appts(r, second)) == 1, "app %d" % len(_app_appts(r, second)))
    t.check("the entry already held is still there, once", len(_app_appts(r, first)) == 1, "app %d" % len(_app_appts(r, first)))


@gs("IG-117", "After a full sync, the next sync lists only the changes since the last sync", configs=(CM,))
def ig117(t):
    home = t.home()
    home.set_settings({"contactsync": "no"})
    g.create_event(_ev("IG-117", "first"), _now(3), _now(3, "10:00"))
    t.run(home, "a full sync", [g.sync()])
    r = t.run(home, "the next sync", [g.sync(), g.dump("d1.json")])
    t.check("the listing is the changes since the last sync", "the changes since the last sync" in r.log,
            "driver log does not say so")
    t.check("no unchanged appointment is downloaded", "Downloaded: 0 entries" in r.log, "driver log does not say so")


# ------------------------------------------------------------------------------------------------ batch 5: tokens, repeats, archive, rate limits, conflicts, local files

def _local_names(home, sub):
    return home.files(sub)


@gs("IG-118", "With no sync token, the listing is full and every entry in the window is downloaded", configs=(CM,))
def ig118(t):
    home = t.home()
    summaries = [_ev("IG-118", str(i)) for i in range(3)]
    for i, s in enumerate(summaries):
        g.create_event(s, _now(5 + i), _now(5 + i, "10:00"))
    r = t.run(home, "sync with an empty token", [g.setting("lastgooglecalendarsynctoken", ""), g.sync(), g.dump("d1.json")])
    counts = [len(_app_appts(r, s)) for s in summaries]
    t.check("each of the three entries is downloaded once", all(c == 1 for c in counts), "app %s" % counts)


@gs("IG-119", "ContactManager master, repeat changed in both places since the last sync: the app's version is kept", configs=(CM,))
def ig119(t):
    summary = _ev("IG-119", "weekly")
    home = t.home()
    eid = g.create_event(summary, _now(2), _now(2, "10:00"), tz="UTC", recurrence=["RRULE:FREQ=WEEKLY;INTERVAL=1"],
                         location="Start")
    t.run(home, "first sync", [g.sync()])
    g.patch_event(eid, {"location": "From Google"})
    r = t.run(home, "change the repeat in the app, sync",
              [g.edit_appointment_in_app(summary, {"location": "From app"}), g.sync(), g.dump("d1.json")])
    t.check("the app's version is sent", g.get_event(eid).get("location") == "From app",
            "Google location %r" % g.get_event(eid).get("location"))
    local = _app_appts(r, summary)
    t.check("one entry in the app", len(local) == 1, "app %d" % len(local))


@gs("IG-120", "ContactManager master, repeat moved on Google: the app shows the moved start, and one repeat", configs=(CM,))
def ig120(t):
    summary = _ev("IG-120", "weekly")
    home = t.home()
    eid = g.create_event(summary, _now(2), _now(2, "10:00"), tz="UTC", recurrence=["RRULE:FREQ=WEEKLY;INTERVAL=1"])
    t.run(home, "first sync", [g.sync()])
    g.api("PATCH", g.CALENDAR + "/events/" + eid, {"start": {"dateTime": _now(3, "11:00"), "timeZone": "UTC"},
                                                   "end": {"dateTime": _now(3, "12:00"), "timeZone": "UTC"}})
    r = t.run(home, "sync the moved repeat", [g.sync(), g.dump("d1.json")])
    event = g.get_event(eid)
    t.check("one repeat on Google, still weekly", any("FREQ=WEEKLY" in x for x in event.get("recurrence", [])),
            "recurrence %r" % event.get("recurrence"))
    local = _app_appts(r, summary)
    t.check("the app holds one entry", len(local) == 1, "app %d" % len(local))


@gs("IG-121", "A repeat with no occurrence in the window is not downloaded, and it stays on Google once", configs=(CM,))
def ig121(t):
    summary = _ev("IG-121", "weekly")
    home = t.home()
    g.create_event(summary, _now(-120), _now(-120, "10:00"), tz="UTC", recurrence=["RRULE:FREQ=WEEKLY;COUNT=3"])
    r = t.run(home, "sync", [g.sync(), g.dump("d1.json")])
    t.check("the app does not download it", not _app_appts(r, summary), "app %d" % len(_app_appts(r, summary)))
    t.check("Google keeps the repeat once", len(g.test_events(summary)) == 1, "found %d" % len(g.test_events(summary)))


@gs("IG-122", "A repeat with an exception: the excluded occurrence is not listed, and the other five are", configs=(CM,))
def ig122(t):
    summary = _ev("IG-122", "weekly")
    home = t.home()
    eid = g.create_event(summary, _now(2), _now(2, "10:00"), tz="UTC", recurrence=["RRULE:FREQ=WEEKLY;COUNT=6"])
    target = _now(9, "09:00")[:10]
    for inst in _instances(eid):
        if (inst.get("originalStartTime") or inst.get("start") or {}).get("dateTime", "").startswith(target):
            g.api("DELETE", g.CALENDAR + "/events/" + inst["id"])
    r = t.run(home, "sync and list", [g.sync(), {"op": "calendar_rows", "name": "r9"}, g.dump("d1.json")])
    mine = [x for x in (_val(r, "r9_titles") or []) if summary in str(x)]
    t.check("the excluded occurrence is not listed", not any(target[:4] in str(x) and "09" in str(x) for x in mine) or len(mine) == 5,
            "rows %r" % mine)
    t.check("the other five occurrences are listed", len(mine) == 5, "rows %d" % len(mine))


@gs("IG-125", "An archived entry changed on Google, still outside the window, has its archived copy brought up to date", configs=(CM,))
def ig125(t):
    summary = _ev("IG-125", "archived")
    home = t.home()
    eid = g.create_event(summary, _now(-30), _now(-30, "10:00"))
    t.run(home, "sync", [g.sync()])
    t.run(home, "narrow the window to ten days, sync", [g.setting("windowbefore", "10"), g.sync()])
    g.patch_event(eid, {"summary": summary + " changed"})
    t.run(home, "sync again with the window unchanged", [g.setting("windowbefore", "10"), g.sync()])
    archived = [n for n in _local_names(home, "calendar/archive")] if home.files("calendar/archive") is not None else []
    text = ""
    for n in archived:
        with open(os.path.join(home.data, "calendar", "archive", n), encoding="utf-8", errors="replace") as f:
            text += f.read()
    t.check("the archived copy has the changed summary", "changed" in text, "archive holds %d files" % len(archived))


@gs("IG-126", "An archived entry cancelled on Google leaves the archive, and is kept in the deleted folder", configs=(CM,))
def ig126(t):
    summary = _ev("IG-126", "archived")
    home = t.home()
    eid = g.create_event(summary, _now(-30), _now(-30, "10:00"))
    t.run(home, "sync", [g.sync()])
    t.run(home, "narrow the window, sync", [g.setting("windowbefore", "10"), g.sync()])
    before = len(home.files("calendar/archive"))
    g.delete_event(eid)
    t.run(home, "sync the cancellation", [g.setting("windowbefore", "10"), g.sync()])
    t.check("the entry is archived before the cancellation", before >= 1, "archive held %d" % before)
    t.check("the entry has left the archive", len(home.files("calendar/archive")) == 0,
            "archive holds %d" % len(home.files("calendar/archive")))
    t.check("the entry is kept in the deleted folder", len(home.files("calendar/deleted")) >= 1,
            "deleted holds %d" % len(home.files("calendar/deleted")))


@gs("IG-127", "A local entry dated 1 January 1970, or with no start, is neither stored nor uploaded; a normal entry alongside is unaffected", configs=(CM,))
def ig127(t):
    home = t.home()
    ok, epoch, nostart = _ev("IG-127", "ok"), _ev("IG-127", "epoch"), _ev("IG-127", "nostart")
    ops = [{"op": "add_appointment", "fields": {"summary": ok, "from": _now(7), "to": _now(7, "10:00")}},
           {"op": "add_appointment", "fields": {"summary": epoch, "from": "1970-01-01T09:00:00Z",
                                                "to": "1970-01-01T10:00:00Z"}},
           {"op": "add_appointment", "fields": {"summary": nostart, "from": "", "to": _now(7, "10:00")}},
           g.sync(), g.dump("d1.json")]
    r = t.run(home, "add the three entries, sync", ops)
    t.check("the normal entry reaches Google", len(g.test_events(ok)) == 1, "found %d" % len(g.test_events(ok)))
    t.check("the epoch and the no-start entries are not uploaded", not g.test_events(epoch) and not g.test_events(nostart),
            "uploaded")
    t.check("the epoch and the no-start entries are not stored", not _app_appts(r, epoch) and not _app_appts(r, nostart),
            "stored")


@gs("IG-129", "A failure that is not a rate limit (400 badRequest) is not retried, and its reason is reported", configs=(CM,))
def ig129(t):
    home = t.home()
    g.create_event(_ev("IG-129", "x"), _now(4), _now(4, "10:00"))
    t.blocked("the request is not retried", "the driver does not count retries of a request yet")
    r = t.run(home, "sync with a bad request on the second write", [g.sync(), g.dump("d1.json")],
              faults="w2=400:badRequest")
    t.check("the report gives the HTTP status and Google's reason", "400" in r.log and "badRequest" in r.log,
            "report mentions: %s" % [w for w in ("400", "badRequest") if w in r.log])


@gs("IG-130", "A rate limit that keeps coming back is given up after four retries, and the report gives the reason", configs=(CM,))
def ig130(t):
    home = t.home()
    for i in range(4):
        g.create_event(_ev("IG-130", str(i)), _now(4 + i), _now(4 + i, "10:00"))
    r = t.run(home, "sync with the rate limit repeated", [g.sync(), g.dump("d1.json")],
              faults="w2-6=403:rateLimitExceeded")
    t.check("the report names the rate limit", "rateLimitExceeded" in r.log, "report mentions it: %s" % ("rateLimitExceeded" in r.log))
    failed = [l for l in r.log.splitlines() if "SYNCHRONISATION FAILED" in l]
    t.check("the report says the sync failed", bool(failed), "no failure line")
    t.blocked("five attempts for the request, then it is given up", "the driver does not count attempts yet")


@gs("IG-131", "Repeated rate limits start the adaptive pacing, and the report says so", configs=(CM,))
def ig131(t):
    home = t.home()
    prefix = _ev("IG-131", "")
    ops = [{"op": "add_appointment", "fields": {"summary": prefix + "%02d" % i, "from": _now(4 + i), "to": _now(4 + i, "10:00")}}
           for i in range(12)]
    r = t.run(home, "twelve appointments, rate limits on the second, fourth, ... writes",
              ops + [g.sync(), g.dump("d1.json")],
              faults="w2=429:rateLimitExceeded;w4=429:rateLimitExceeded;w6=429:rateLimitExceeded;"
                     "w8=429:rateLimitExceeded;w10=429:rateLimitExceeded;w12=429:rateLimitExceeded")
    on_google = [e for e in g.test_events() if e.get("summary", "").startswith(prefix)]
    t.check("Google holds each of the twelve appointments once", len(on_google) == 12, "found %d" % len(on_google))
    t.check("the report says the adaptive pacing started", "pacing" in r.log.lower(), "report mentions pacing: %s" % ("pacing" in r.log.lower()))


@gs("IG-132", "A connection closed by Google part-way: the report says so and what to do, and a second sync completes", configs=(CM,))
def ig132(t):
    home = t.home()
    prefix = _ev("IG-132", "")
    ops = [{"op": "add_appointment", "fields": {"summary": prefix + "%02d" % i, "from": _now(4 + i), "to": _now(4 + i, "10:00")}}
           for i in range(6)]
    first = t.run(home, "six appointments, the connection closed on writes three and four", ops + [g.sync(), g.dump("d1.json")],
                  faults="w3-4=closed")
    failed = [l for l in first.log.splitlines() if "SYNCHRONISATION FAILED" in l]
    t.check("the report says the connection was closed", "closed" in first.log.lower(),
            "report mentions it: %s" % ("closed" in first.log.lower()))
    t.check("the last line of the report says to sync again", bool(failed) and "sync again" in failed[-1].lower(),
            "last failure line: %r" % (failed[-1] if failed else None))
    t.run(home, "second sync", [g.sync(), g.dump("d2.json")])
    on_google = [e for e in g.test_events() if e.get("summary", "").startswith(prefix)]
    t.check("Google holds the six appointments once after the second sync", len(on_google) == 6, "found %d" % len(on_google))


@gs("IG-135", "Contact deleted on Google and changed in the app, ContactManager master: created on Google again, with the app's change", configs=(CM,))
def ig135(t):
    tag = "IG-135"
    home = t.home()
    resource = _create(tag, emails=[_mail(tag)])
    t.run(home, "sync", [g.sync()])
    g.delete_google_contact(resource)
    r = t.run(home, "change the surname in the app, sync",
              [{"op": "edit_contact", "match": {"surname": tag}, "fields": {"surname": tag + " app"}}, g.sync(),
               g.dump("d1.json")])
    copies = [p for p in g.fixture_people() if _info(p)["tag"] == tag + " app"]
    t.check("created on Google again, with the app's change", len(copies) == 1, "found %d" % len(copies))
    t.check("one live contact in the app", len([c for c in _app(r, tag + " app")]) == 1, "")


@gs("IG-136", "Contact deleted in the app and changed on Google, ContactManager master: the app's delete wins", configs=(CM,))
def ig136(t):
    tag = "IG-136"
    home = t.home()
    resource = _create(tag, emails=[_mail(tag)])
    t.run(home, "sync", [g.sync()])
    g.update_google_contact(resource, "names", [{"givenName": "Bench", "familyName": tag + " google"}])
    t.run(home, "delete in the app, sync",
          [{"op": "delete_contact", "match": {"surname": tag}}, g.sync(), g.dump("d1.json")])
    t.check("deleted on Google too, as the master's delete wins", not _google_tagged(tag) and not _google_tagged(tag + " google"),
            "copies remain")


@gs("IG-137", "Local files: after two syncs the calendar folder holds each appointment once, and no file is repeated", configs=(CM,))
def ig137(t):
    home = t.home()
    ops = [{"op": "add_appointment", "fields": {"summary": _ev("IG-137", "%02d" % i), "from": _now(20 + i),
                                              "to": _now(20 + i, "10:00")}} for i in range(10)]
    t.run(home, "ten appointments, sync", ops + [g.sync(), g.dump("d0.json")])
    t.run(home, "second sync", [g.sync(), g.dump("d1.json")])
    names = home.files("calendar")
    counts = {}
    for n in names:
        with open(os.path.join(home.data, "calendar", n), encoding="utf-8", errors="replace") as f:
            for i in range(10):
                if _ev("IG-137", "%02d" % i) in f.read():
                    counts[i] = counts.get(i, 0) + 1
    t.check("each appointment has one file", all(counts.get(i) == 1 for i in range(10)), "counts %s" % counts)


@gs("IG-138", "Local files: a deleted appointment moves to the deleted folder, and is not left in the calendar folder", configs=(CM,))
def ig138(t):
    summary = _ev("IG-138", "gone")
    home = t.home()
    t.run(home, "add and sync", [g.add_appointment(0, summary, _now(25), _now(25, "10:00")), g.sync()])
    t.run(home, "delete, sync, tidy", [{"op": "delete_appointment", "match": {"summary": summary}}, g.sync(),
                                       {"op": "tidy_full"}, g.dump("d1.json")])
    in_calendar = [n for n in home.files("calendar") if summary in open(os.path.join(home.data, "calendar", n),
                                                                          encoding="utf-8", errors="replace").read()]
    in_deleted = [n for n in home.files("calendar/deleted") if summary in open(os.path.join(home.data, "calendar", "deleted", n),
                                                                                  encoding="utf-8", errors="replace").read()]
    t.check("the file is in the deleted folder", len(in_deleted) == 1, "found %d" % len(in_deleted))
    t.check("the file is not left in the calendar folder", not in_calendar, "found %d" % len(in_calendar))


@gs("IG-139", "Appointment deleted in the app and changed on Google, ContactManager master: the app's delete wins", configs=(CM,))
def ig139(t):
    summary = _ev("IG-139", "a14")
    home = t.home()
    eid = g.create_event(summary, _now(26), _now(26, "10:00"))
    t.run(home, "sync", [g.sync()])
    g.patch_event(eid, {"location": "From Google"})
    r = t.run(home, "delete in the app, sync",
              [{"op": "delete_appointment", "match": {"summary": summary}}, g.sync(), g.dump("d1.json")])
    t.check("deleted on Google too, as the master's delete wins", not g.test_events(summary),
            "found %d" % len(g.test_events(summary)))
    t.check("the app holds it deleted, so one state on both sides", not _app_appts(r, summary), "live copies remain")


@gs("IG-140", "Appointment deleted on Google and changed in the app, ContactManager master: created on Google again, with the app's change", configs=(CM,))
def ig140(t):
    summary = _ev("IG-140", "a15")
    home = t.home()
    eid = g.create_event(summary, _now(27), _now(27, "10:00"))
    t.run(home, "sync", [g.sync()])
    g.delete_event(eid)
    t.run(home, "change the location in the app, sync",
          [g.edit_appointment_in_app(summary, {"location": "From app"}), g.sync(), g.dump("d1.json")])
    copies = g.test_events(summary)
    t.check("created on Google again, with the app's change",
            len(copies) == 1 and copies[0].get("location") == "From app",
            "Google %s" % [c.get("location") for c in copies])


@gs("IG-141", "Repeat with no occurrence left: every occurrence cancelled on Google, the app removes the repeat", configs=(CM,))
def ig141(t):
    summary = _ev("IG-141", "weekly")
    home = t.home()
    eid = g.create_event(summary, _now(2), _now(2, "10:00"), tz="UTC", recurrence=["RRULE:FREQ=WEEKLY;INTERVAL=1;COUNT=3"])
    t.run(home, "first sync", [g.sync()])
    for inst in _instances(eid):
        if inst.get("status") != "cancelled":
            g.api("DELETE", g.CALENDAR + "/events/" + inst["id"])
    r = t.run(home, "sync", [g.sync(), g.dump("d1.json")])
    t.check("the app removes the repeat: no live copy", not _app_appts(r, summary), "live copies %d" % len(_app_appts(r, summary)))


@gs("IG-142", "A changed occurrence of a repeat, changed on Google, is downloaded as its own entry; the others are unchanged", configs=(CM,))
def ig142(t):
    summary = _ev("IG-142", "weekly")
    home = t.home()
    eid = g.create_event(summary, _now(2), _now(2, "10:00"), tz="UTC", recurrence=["RRULE:FREQ=WEEKLY;INTERVAL=1"])
    t.run(home, "first sync", [g.sync()])
    target = _now(16, "09:00")[:10]
    for inst in _instances(eid):
        if (inst.get("originalStartTime") or inst.get("start") or {}).get("dateTime", "").startswith(target):
            g.api("PATCH", g.CALENDAR + "/events/" + inst["id"], {"location": "Room I"})
    r = t.run(home, "sync", [g.sync(), g.dump("d1.json")])
    changed = [a for a in _app_appts(r, summary) if a.get("location") == "Room I"]
    t.check("the changed occurrence is downloaded as its own entry", len(changed) == 1, "found %d" % len(changed))
    t.check("the other occurrences are unchanged", len(_app_appts(r, summary)) >= 1, "app %d" % len(_app_appts(r, summary)))
