#!/usr/bin/env python3
#
# Sets up the link between ContactManager's tests and a disposable Google account.  Run it once.
#
#   python3 autotest/utilities/setup_test_account.py              # full set-up
#   python3 autotest/utilities/setup_test_account.py --check      # check the saved account still works
#   python3 autotest/utilities/setup_test_account.py --choose-passwords
#
# It asks for the Google OAuth client, creates the two ContactManager passwords, shows you a code to enter at Google,
# waits for you to authorise, and writes everything to one file outside the repository:
#   ~/.config/contactmanager-test/test-account.json   (permissions 0600)
#
# Environment variables, for a non-interactive set-up: CM_TEST_CLIENT_ID, CM_TEST_CLIENT_SECRET,
# CM_TEST_DB_PASSWORD and CM_TEST_LOGIN_PASSWORD.  The secret and passwords are never printed.
#

import argparse
import datetime
import getpass
import os
import secrets
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import googleclient as gc                     # noqa: E402
import testaccount as ta                      # noqa: E402

GRANT_DEVICE = "http://oauth.net/grant_type/device/1.0"   # the grant the application uses (googleaccess_oauth2.cpp)
MIN_PASSWORD = 12

INSTRUCTIONS = """
ContactManager test set-up
==========================

Use a DISPOSABLE Google account, never a real one.  The clean-up utility deletes test data, and with --all-data
it deletes everything on the account's primary calendar and all of its contacts.

1. Google Cloud console (https://console.cloud.google.com/):
   a. Create a project for the tests.
   b. APIs & Services > Library: enable the Google Calendar API and the Google People API.
   c. APIs & Services > OAuth consent screen: user type External, app name "ContactManager tests".
      While the app is in testing, add the disposable account under Test users.
   d. APIs & Services > Credentials > Create credentials > OAuth client ID.
      Application type: "TVs and Limited Input devices".  Copy the Client ID and the Client secret.

2. Network access for Claude's environment must allow these hosts:
      accounts.google.com, www.googleapis.com, people.googleapis.com
   (In the Claude Code settings for this environment: Network access.)

3. Have the disposable account's password ready. You will sign in to it in step 6.
"""


class SetupError(Exception):
    pass


def ask(prompt, env=None, secret=False, required=True):
    if env and os.environ.get(env):
        return os.environ[env]
    value = getpass.getpass(prompt) if secret else input(prompt)
    value = value.strip()
    if required and not value:
        raise SetupError("A value is needed.")
    return value


def make_passwords(choose):
    """Two passwords: the database (encryption key) password and the login password."""
    if not choose:
        return (os.environ.get("CM_TEST_DB_PASSWORD") or secrets.token_urlsafe(18),
                os.environ.get("CM_TEST_LOGIN_PASSWORD") or secrets.token_urlsafe(18))
    out = []
    for label, env in (("database (encryption key) password", "CM_TEST_DB_PASSWORD"),
                       ("ContactManager login password", "CM_TEST_LOGIN_PASSWORD")):
        while True:
            first = ask(f"Choose the {label} (at least {MIN_PASSWORD} characters): ", env=env, secret=True)
            if len(first) < MIN_PASSWORD:
                print(f"Too short: use at least {MIN_PASSWORD} characters.")
                continue
            again = ask("Type it again: ", secret=True)
            if first != again:
                print("The two entries do not match. Try again.")
                continue
            out.append(first)
            break
    return out[0], out[1]


def start_device_flow(client_id, url=None):
    fields = {"client_id": client_id, "scope": gc.SCOPE}
    return gc.post_form(url or gc.DEVICE_URL, fields)


def poll_for_token(client_id, client_secret, device, sleep=time.sleep, now=time.time, url=None):
    """Wait while the user authorises the code at Google.  Returns the token reply."""
    interval = int(device.get("interval", 5))
    deadline = now() + int(device.get("expires_in", 1800))
    while True:
        if now() > deadline:
            raise SetupError("The code expired before it was authorised. Run the set-up again.")
        sleep(interval)
        try:
            return gc.post_form(url or gc.TOKEN_URL, {
                "client_id": client_id,
                "client_secret": client_secret,
                "code": device["device_code"],
                "grant_type": GRANT_DEVICE,
            }, sleep=sleep)
        except gc.GoogleError as e:
            err = (e.payload or {}).get("error") if isinstance(e.payload, dict) else None
            if err == "authorization_pending":
                continue
            if err == "slow_down":
                interval += 5
                continue
            if err == "access_denied":
                raise SetupError("Access was refused on Google's sign-in page. Nothing was saved.") from None
            if err == "expired_token":
                raise SetupError("The code expired before it was authorised. Run the set-up again.") from None
            raise SetupError(f"Google refused the authorisation: {e}") from None


def account_email(access_token, url=None):
    info = gc.get_json(url or gc.USERINFO_URL, access_token)
    email = (info or {}).get("email", "")
    if not email:
        raise SetupError("Google did not say which account was authorised.")
    return email


def refresh_access_token(client_id, client_secret, refresh_token, url=None):
    reply = gc.post_form(url or gc.TOKEN_URL, {
        "client_id": client_id,
        "client_secret": client_secret,
        "refresh_token": refresh_token,
        "grant_type": "refresh_token",
    })
    return reply["access_token"]


def check_saved_account(path):
    data = ta.load_account(path)
    g = data["google"]
    token = refresh_access_token(g["client_id"], g["client_secret"], g["refresh_token"])
    email = account_email(token)
    if email.lower() != g.get("account_email", "").lower():
        raise SetupError(f"The saved refresh token now belongs to {email}, not {g.get('account_email')}.")
    return email


def run_setup(out_path, choose_passwords):
    print(INSTRUCTIONS)
    client_id = ask("Client ID: ", env="CM_TEST_CLIENT_ID")
    client_secret = ask("Client secret (typed hidden): ", env="CM_TEST_CLIENT_SECRET", secret=True)
    db_password, login_password = make_passwords(choose_passwords)

    device = start_device_flow(client_id)
    url = device.get("verification_url") or device.get("verification_uri")
    print()
    print("Authorise the test account:")
    print(f"  1. Open {url}")
    print(f"  2. Enter this code: {device['user_code']}")
    print("  3. Sign in to the DISPOSABLE account and allow the access requested.")
    print("This waits for you (the code is valid for about "
          f"{int(device.get('expires_in', 1800)) // 60} minutes).")
    token = poll_for_token(client_id, client_secret, device)
    email = account_email(token["access_token"])
    print(f"Authorised: {email}")

    typed = ask("To confirm this is the DISPOSABLE test account, type its address: ")
    if typed.lower() != email.lower():
        raise SetupError("The address does not match the account that was authorised. Nothing was saved.")
    if not token.get("refresh_token"):
        raise SetupError("Google did not return a refresh token. Remove the app's access in the account's "
                         "Google security settings, and run the set-up again.")

    data = {
        "created": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "google": {
            "client_id": client_id,
            "client_secret": client_secret,
            "refresh_token": token["refresh_token"],
            "account_email": email,
            "scope": gc.SCOPE,
        },
        "contactmanager": {
            "database_password": db_password,
            "login_password": login_password,
        },
    }
    path = ta.save_account(data, out_path)
    print()
    print(f"Saved: {path}")
    print("It holds the client secret, the refresh token and both passwords. Keep it private, and keep it outside")
    print("the repository.  The passwords are in the file only: they are not printed here.")
    print("When ContactManager asks for the Encryption Key and the Initial Password in a test, use the two")
    print("passwords from this file (the database password is the key).")
    return path


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--out", default=str(ta.DEFAULT_ACCOUNT_FILE), help="where to save the account file")
    p.add_argument("--check", action="store_true", help="check the saved account, changing nothing")
    p.add_argument("--choose-passwords", action="store_true", help="type the two passwords instead of generating them")
    args = p.parse_args(argv)
    try:
        if args.check:
            email = check_saved_account(args.out)
            print(f"The saved account works: {email}")
        else:
            run_setup(args.out, args.choose_passwords)
    except (SetupError, ta.AccountFileError, gc.GoogleError, KeyboardInterrupt) as e:
        print(f"\nSet-up stopped: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
