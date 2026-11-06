#!/usr/bin/env python3
#
# Cleans the test Google account.
#
#   python3 autotest/utilities/clean_test_data.py --dry-run     # show what would be deleted, delete nothing
#   python3 autotest/utilities/clean_test_data.py               # delete the test data (default)
#   python3 autotest/utilities/clean_test_data.py --yes         # as above, without the question
#   python3 autotest/utilities/clean_test_data.py --all-data    # delete EVERYTHING on the account
#
# Test data is what the tests create, and it is identified on any account:
#   contacts whose email address is in the test domain (test_fixtures.json: identifier.email_domain)
#   appointments on the primary calendar whose summary starts with the test prefix (identifier.event_prefix)
# The default clean-up FINDS them by search (the People search for the domain, the Calendar search for the prefix).
# It does not list the account.  Only --all-data lists the whole account.
#
# --all-data deletes every contact in the account's contact list and every appointment on its primary calendar
# (other calendars and "other contacts" are not touched).  It cannot be used with --yes: you must type the
# account's address to confirm.  Nothing is deleted unless the token belongs to the account in the account file.
#

import argparse
import sys
import time
import urllib.parse
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import googleclient as gc                     # noqa: E402
import testaccount as ta                      # noqa: E402
import setup_test_account as sta              # noqa: E402

PACE_SECONDS = 0.2     # a short gap between deletes, so the account is not hit with a burst
MAX_PASSES = 20        # searches for test data, one after another, before the clean-up stops (default clean-up only)


def list_contacts(token, base=None):
    """Every contact in the account's contact list, with its email addresses."""
    out, page = [], None
    while True:
        q = {"personFields": "names,emailAddresses", "pageSize": 1000}
        if page:
            q["pageToken"] = page
        reply = gc.get_json((base or gc.PEOPLE_URL) + "/people/me/connections?" + urllib.parse.urlencode(q), token)
        out.extend((reply or {}).get("connections", []))
        page = (reply or {}).get("nextPageToken")
        if not page:
            return out


def list_events(token, base=None):
    """Every appointment on the primary calendar.  Repeats are listed once, as their series."""
    out, page = [], None
    while True:
        q = {"singleEvents": "false", "showDeleted": "false", "maxResults": 2500}
        if page:
            q["pageToken"] = page
        reply = gc.get_json((base or gc.CALENDAR_URL) + "/events?" + urllib.parse.urlencode(q), token)
        out.extend((reply or {}).get("items", []))
        page = (reply or {}).get("nextPageToken")
        if not page:
            return out


def search_test_contacts(token, fixtures, base=None):
    """Test contacts found by a search for the test domain (not a listing of the account).  The search returns one page
    of matches (at most 30); the caller deletes them and searches again until none are left."""
    domain = fixtures["identifier"]["email_domain"]
    q = urllib.parse.urlencode({"query": domain, "readMask": "names,emailAddresses", "pageSize": 30})
    reply = gc.get_json((base or gc.PEOPLE_URL) + "/people:searchContacts?" + q, token)
    people = [r["person"] for r in (reply or {}).get("results", []) if "person" in r]
    return [p for p in people if ta.is_test_contact(contact_emails(p), fixtures)]


def search_test_events(token, fixtures, base=None):
    """Test appointments found by a search for the test prefix on the primary calendar (not a listing of it)."""
    prefix = fixtures["identifier"]["event_prefix"]
    out, page = [], None
    while True:
        q = {"q": prefix.strip(), "singleEvents": "false", "showDeleted": "false", "maxResults": 250}
        if page:
            q["pageToken"] = page
        reply = gc.get_json((base or gc.CALENDAR_URL) + "/events?" + urllib.parse.urlencode(q), token)
        out += [e for e in (reply or {}).get("items", []) if ta.is_test_event(e.get("summary"), fixtures)]
        page = (reply or {}).get("nextPageToken")
        if not page:
            return out


def contact_emails(person):
    return [e.get("value", "") for e in person.get("emailAddresses", [])]


def choose(contacts, events, fixtures, all_data):
    """The contacts and appointments to delete, from a full listing (used only with --all-data)."""
    if all_data:
        return list(contacts), list(events)
    cs = [c for c in contacts if ta.is_test_contact(contact_emails(c), fixtures)]
    es = [e for e in events if ta.is_test_event(e.get("summary"), fixtures)]
    return cs, es


def contact_label(person):
    names = person.get("names", [])
    name = names[0].get("displayName", "") if names else ""
    return name or (contact_emails(person) or ["(no name)"])[0]


def delete_all(token, contacts, events, base_people=None, base_calendar=None, pace=PACE_SECONDS, sleep=time.sleep):
    """Delete the given records.  Returns (deleted, failed), each a list of descriptions."""
    deleted, failed = [], []
    for p in contacts:
        label = contact_label(p)
        try:
            gc.request("DELETE", (base_people or gc.PEOPLE_URL) + "/" + p["resourceName"] + ":deleteContact",
                       headers={"Authorization": "Bearer " + token})
            deleted.append("contact " + label)
        except gc.GoogleError as e:
            failed.append(f"contact {label}: {e}")
        sleep(pace)
    for e in events:
        label = e.get("summary") or e.get("id", "")
        try:
            gc.delete((base_calendar or gc.CALENDAR_URL) + "/events/" + urllib.parse.quote(e["id"], safe=""), token)
            deleted.append("appointment " + label)
        except gc.GoogleError as err:
            failed.append(f"appointment {label}: {err}")
        sleep(pace)
    return deleted, failed


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--config", default=str(ta.DEFAULT_ACCOUNT_FILE), help="the account file (see setup_test_account.py)")
    p.add_argument("--all-data", action="store_true", help="delete everything on the account, not only the test data")
    p.add_argument("--dry-run", action="store_true", help="list what would be deleted, delete nothing")
    p.add_argument("--yes", action="store_true", help="do not ask (not allowed with --all-data)")
    args = p.parse_args(argv)
    if args.all_data and args.yes:
        print("--all-data needs the account address typed in, so it cannot be used with --yes.", file=sys.stderr)
        return 2
    try:
        account = ta.load_account(args.config)
        g = account["google"]
        token = sta.refresh_access_token(g["client_id"], g["client_secret"], g["refresh_token"])
        email = sta.account_email(token)
        if email.lower() != g.get("account_email", "").lower():
            print(f"The token belongs to {email}, not the account in the account file. Nothing was deleted.",
                  file=sys.stderr)
            return 1
        fixtures = ta.load_fixtures()
        if args.all_data:
            cs, es = choose(list_contacts(token), list_events(token), fixtures, True)
        else:
            cs, es = search_test_contacts(token, fixtures), search_test_events(token, fixtures)
        scope = "EVERYTHING" if args.all_data else "the test data"
        print(f"Account: {email}")
        print(f"Would delete {scope}: {len(cs)} contact(s), {len(es)} appointment(s)")
        for p_ in cs[:20]:
            print("  contact:", contact_label(p_))
        for e in es[:20]:
            print("  appointment:", e.get("summary", e.get("id", "")))
        if len(cs) > 20 or len(es) > 20:
            print("  (first 20 of each shown)")
        if args.dry_run or not (cs or es):
            return 0
        if args.all_data:
            typed = input(f"Type the account address ({email}) to delete everything: ").strip()
            if typed.lower() != email.lower():
                print("The address does not match. Nothing was deleted.")
                return 1
        elif not args.yes:
            if input("Delete the test data? [y/N] ").strip().lower() not in ("y", "yes"):
                print("Nothing was deleted.")
                return 0
        deleted, failed = [], []
        passes = 0
        while True:
            d, f = delete_all(token, cs, es)
            deleted += d
            failed += f
            passes += 1
            # A search returns at most one page of contacts, so the test data is searched for again until none is left.
            # A failure stops the passes, so the same record is not tried again and again.
            if args.all_data or failed or passes >= MAX_PASSES:
                break
            cs, es = search_test_contacts(token, fixtures), search_test_events(token, fixtures)
            if not (cs or es):
                break
        print(f"Deleted {len(deleted)} record(s). Failed: {len(failed)}.")
        for f in failed:
            print("  FAILED", f, file=sys.stderr)
        return 1 if failed else 0
    except (ta.AccountFileError, sta.SetupError, gc.GoogleError, KeyError) as e:
        print(f"Clean-up stopped: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
