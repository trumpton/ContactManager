#
# Tests for the test utilities.  They use a local mock of the Google endpoints, so no real account is touched.
#
#   python3 -m unittest discover -s autotest/utilities -p 'test_*.py' -v
#

import http.server
import io
import json
import os
import contextlib
import shutil
import stat
import tempfile
import threading
import unittest
import urllib.parse
from pathlib import Path

HERE = Path(__file__).resolve().parent
import sys                                    # noqa: E402
sys.path.insert(0, str(HERE))

# The mock is on 127.0.0.1: never send those requests through a proxy
os.environ["NO_PROXY"] = "127.0.0.1,localhost"
os.environ["no_proxy"] = "127.0.0.1,localhost"

import googleclient as gc                     # noqa: E402
import testaccount as ta                      # noqa: E402
import setup_test_account as sta              # noqa: E402
import clean_test_data as clean               # noqa: E402

FIX = ta.load_fixtures()


class MockGoogle:
    """A local HTTP server that answers from a list of scripted replies, and records every request."""

    def __init__(self):
        self.replies = []        # (method, path-prefix, status, body) served in order, first match wins
        self.requests = []       # (method, path, query dict, body text, Authorization header)
        mock = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def _handle(self, method):
                length = int(self.headers.get("Content-Length") or 0)
                body = self.rfile.read(length).decode("utf-8") if length else ""
                parsed = urllib.parse.urlparse(self.path)
                mock.requests.append((method, parsed.path, dict(urllib.parse.parse_qsl(parsed.query)),
                                      body, self.headers.get("Authorization", "")))
                for i, (m, prefix, status, reply) in enumerate(mock.replies):
                    if m == method and parsed.path.startswith(prefix):
                        del mock.replies[i]
                        data = b"" if reply is None else json.dumps(reply).encode("utf-8")
                        self.send_response(status)
                        self.send_header("Content-Type", "application/json")
                        self.send_header("Content-Length", str(len(data)))
                        self.end_headers()
                        self.wfile.write(data)
                        return
                self.send_response(599)
                self.send_header("Content-Length", "0")
                self.end_headers()

            def do_GET(self):
                self._handle("GET")

            def do_POST(self):
                self._handle("POST")

            def do_DELETE(self):
                self._handle("DELETE")

            def log_message(self, *args):
                pass

        self.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.base = f"http://127.0.0.1:{self.server.server_address[1]}"
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def close(self):
        self.server.shutdown()
        self.server.server_close()

    def serve(self, method, prefix, status, reply=None):
        self.replies.append((method, prefix, status, reply))


class MockTestCase(unittest.TestCase):
    def setUp(self):
        self.mock = MockGoogle()
        self.saved = {n: getattr(gc, n) for n in ("TOKEN_URL", "DEVICE_URL", "USERINFO_URL", "PEOPLE_URL", "CALENDAR_URL")}
        gc.TOKEN_URL = self.mock.base + "/token"
        gc.DEVICE_URL = self.mock.base + "/device/code"
        gc.USERINFO_URL = self.mock.base + "/userinfo"
        gc.PEOPLE_URL = self.mock.base + "/v1"
        gc.CALENDAR_URL = self.mock.base + "/calendar/primary"

    def tearDown(self):
        for n, v in self.saved.items():
            setattr(gc, n, v)
        self.mock.close()


class FixtureTests(unittest.TestCase):
    def test_six_contacts_with_the_agreed_channels_and_birthdays(self):
        cs = FIX["contacts"]
        self.assertEqual(len(cs), 6)
        self.assertEqual([c["channels"] for c in cs],
                         [["email"], ["email"], ["sms"], ["sms"], ["email", "sms"], ["email", "sms"]])
        self.assertEqual([c["birthday"] is not None for c in cs], [True, False, True, False, True, False])

    def test_phones_are_in_the_agreed_range(self):
        for c in FIX["contacts"]:
            digits = c["phone"].replace(" ", "")
            self.assertTrue(digits.startswith("+447700900"), c["phone"])
            self.assertTrue(900000 <= int(digits[-6:]) <= 900999, c["phone"])

    def test_test_contact_is_recognised_by_domain_in_any_case(self):
        self.assertTrue(ta.is_test_contact(["Jack.ONeil@SGC.contacmanagertest"], FIX))
        self.assertFalse(ta.is_test_contact(["someone@example.com"], FIX))
        self.assertFalse(ta.is_test_contact([], FIX))

    def test_test_event_is_recognised_by_prefix_only(self):
        self.assertTrue(ta.is_test_event("TEST Jack O'Neil: appointment", FIX))
        self.assertFalse(ta.is_test_event("Dentist", FIX))
        self.assertFalse(ta.is_test_event("test lower case", FIX))
        self.assertFalse(ta.is_test_event(None, FIX))


class AccountFileTests(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix="cm-acct-test-")

    def tearDown(self):
        shutil.rmtree(self.dir, ignore_errors=True)

    def test_refuses_a_path_inside_the_repository(self):
        inside = ta.REPO / "account-test-should-not-exist.json"
        with self.assertRaises(ta.AccountFileError):
            ta.save_account({"x": 1}, inside)
        self.assertFalse(inside.exists())

    def test_saves_outside_the_repository_with_owner_only_permissions(self):
        path = Path(self.dir) / "sub" / "test-account.json"
        ta.save_account({"google": {}, "contactmanager": {}}, path)
        self.assertEqual(stat.S_IMODE(os.stat(path).st_mode), 0o600)
        self.assertEqual(stat.S_IMODE(os.stat(path.parent).st_mode), 0o700)
        self.assertEqual(ta.load_account(path), {"google": {}, "contactmanager": {}})

    def test_missing_file_is_reported_with_the_next_step(self):
        with self.assertRaises(ta.AccountFileError) as cm:
            ta.load_account(Path(self.dir) / "none.json")
        self.assertIn("setup_test_account.py", str(cm.exception))

    def test_file_without_a_google_section_is_refused(self):
        path = Path(self.dir) / "bad.json"
        path.write_text(json.dumps({"contactmanager": {}}))
        with self.assertRaises(ta.AccountFileError):
            ta.load_account(path)


class RequestTests(MockTestCase):
    def test_rate_limit_is_retried_with_doubling_waits_then_succeeds(self):
        self.mock.serve("GET", "/people", 429, {"error": {"errors": [{"reason": "rateLimitExceeded"}]}})
        self.mock.serve("GET", "/people", 503, None)
        self.mock.serve("GET", "/people", 200, {"ok": True})
        waits = []
        reply = gc.request("GET", self.mock.base + "/people", sleep=waits.append)
        self.assertEqual(reply, {"ok": True})
        self.assertEqual(waits, [0.5, 1.0])

    def test_a_bad_request_is_not_retried_and_keeps_google_reason(self):
        self.mock.serve("GET", "/people", 400, {"error": {"errors": [{"reason": "badRequest"}], "message": "bad"}})
        waits = []
        with self.assertRaises(gc.GoogleError) as cm:
            gc.request("GET", self.mock.base + "/people", sleep=waits.append)
        self.assertEqual(waits, [])
        self.assertEqual((cm.exception.status, cm.exception.reason, cm.exception.message), (400, "badRequest", "bad"))

    def test_rate_limit_that_keeps_coming_back_is_given_up_after_four_retries(self):
        for _ in range(5):
            self.mock.serve("GET", "/people", 429, {"error": {"errors": [{"reason": "userRateLimitExceeded"}]}})
        waits = []
        with self.assertRaises(gc.GoogleError):
            gc.request("GET", self.mock.base + "/people", sleep=waits.append)
        self.assertEqual(waits, [0.5, 1.0, 2.0, 4.0])


class SetupTests(MockTestCase):
    def test_device_flow_waits_through_pending_and_slow_down_then_returns_the_token(self):
        self.mock.serve("POST", "/token", 428, {"error": "authorization_pending"})
        self.mock.serve("POST", "/token", 400, {"error": "slow_down"})
        self.mock.serve("POST", "/token", 200, {"access_token": "AT", "refresh_token": "RT"})
        sleeps = []
        token = sta.poll_for_token("cid", "secret", {"device_code": "DC", "interval": 5, "expires_in": 600},
                                   sleep=sleeps.append, now=lambda: 0)
        self.assertEqual(token["refresh_token"], "RT")
        self.assertEqual(sleeps, [5, 5, 10])               # slow_down adds 5 seconds to the interval
        grant = [r for r in self.mock.requests if r[1] == "/token"][-1][3]
        self.assertIn("grant_type=http%3A%2F%2Foauth.net%2Fgrant_type%2Fdevice%2F1.0", grant)
        self.assertIn("code=DC", grant)

    def test_access_denied_stops_with_a_clear_message_and_no_retry(self):
        self.mock.serve("POST", "/token", 403, {"error": "access_denied"})
        with self.assertRaises(sta.SetupError) as cm:
            sta.poll_for_token("cid", "s", {"device_code": "DC", "interval": 1, "expires_in": 600},
                               sleep=lambda s: None, now=lambda: 0)
        self.assertIn("refused", str(cm.exception))

    def test_expired_code_is_reported(self):
        clock = iter([0, 1000])
        with self.assertRaises(sta.SetupError) as cm:
            sta.poll_for_token("cid", "s", {"device_code": "DC", "interval": 1, "expires_in": 600},
                               sleep=lambda s: None, now=lambda: next(clock))
        self.assertIn("expired", str(cm.exception))

    def test_start_device_flow_returns_the_user_code_and_url(self):
        self.mock.serve("POST", "/device/code", 200,
                        {"device_code": "DC", "user_code": "ABCD-EFGH", "verification_url": "https://example/verify",
                         "interval": 5, "expires_in": 1800})
        d = sta.start_device_flow("cid")
        self.assertEqual(d["user_code"], "ABCD-EFGH")
        self.assertIn("scope=", [r for r in self.mock.requests if r[1] == "/device/code"][-1][3])

    def test_passwords_are_generated_and_differ(self):
        a, b = sta.make_passwords(choose=False)
        self.assertGreaterEqual(len(a), 20)
        self.assertNotEqual(a, b)


class CleanTests(MockTestCase):
    def setUp(self):
        super().setUp()
        self.dir = tempfile.mkdtemp(prefix="cm-clean-test-")
        self.config = Path(self.dir) / "test-account.json"
        ta.save_account({"google": {"client_id": "c", "client_secret": "s", "refresh_token": "r",
                                    "account_email": "disposable@example.com"},
                         "contactmanager": {}}, self.config)

    def tearDown(self):
        super().tearDown()
        shutil.rmtree(self.dir, ignore_errors=True)

    def _token_for(self, email):
        self.mock.serve("POST", "/token", 200, {"access_token": "AT"})
        self.mock.serve("GET", "/userinfo", 200, {"email": email})

    def test_lists_every_page_of_contacts(self):
        self.mock.serve("GET", "/v1/people/me/connections", 200,
                        {"connections": [{"resourceName": "people/c1"}], "nextPageToken": "P2"})
        self.mock.serve("GET", "/v1/people/me/connections", 200, {"connections": [{"resourceName": "people/c2"}]})
        got = clean.list_contacts("AT", base=self.mock.base + "/v1")
        self.assertEqual([p["resourceName"] for p in got], ["people/c1", "people/c2"])
        self.assertEqual(self.mock.requests[-1][2].get("pageToken"), "P2")

    def test_default_run_lists_and_deletes_only_test_data(self):
        self._token_for("disposable@example.com")
        self.mock.serve("GET", "/v1/people/me/connections", 200, {"connections": [
            {"resourceName": "people/c1", "names": [{"displayName": "Jack O'Neil"}],
             "emailAddresses": [{"value": "jack.oneil@sgc.contacmanagertest"}]},
            {"resourceName": "people/c9", "names": [{"displayName": "Real Person"}],
             "emailAddresses": [{"value": "real@example.com"}]}]})
        self.mock.serve("GET", "/calendar/primary/events", 200, {"items": [
            {"id": "e1", "summary": "TEST Jack O'Neil: confirm"},
            {"id": "e2", "summary": "Dentist"}]})
        self.mock.serve("DELETE", "/v1/people/c1:deleteContact", 200, None)
        self.mock.serve("DELETE", "/calendar/primary/events/e1", 204, None)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = clean.main(["--config", str(self.config), "--yes"])
        self.assertEqual(code, 0, out.getvalue())
        deletes = [r[1] for r in self.mock.requests if r[0] == "DELETE"]
        self.assertEqual(deletes, ["/v1/people/c1:deleteContact", "/calendar/primary/events/e1"])

    def test_dry_run_deletes_nothing(self):
        self._token_for("disposable@example.com")
        self.mock.serve("GET", "/v1/people/me/connections", 200, {"connections": []})
        self.mock.serve("GET", "/calendar/primary/events", 200, {"items": [{"id": "e1", "summary": "TEST x"}]})
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = clean.main(["--config", str(self.config), "--dry-run"])
        self.assertEqual(code, 0)
        self.assertIn("1 appointment(s)", out.getvalue())
        self.assertFalse([r for r in self.mock.requests if r[0] == "DELETE"])

    def test_all_data_cannot_be_combined_with_yes(self):
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            code = clean.main(["--config", str(self.config), "--all-data", "--yes"])
        self.assertEqual(code, 2)
        self.assertFalse(self.mock.requests)

    def test_wrong_account_stops_before_any_delete(self):
        self._token_for("someone.else@example.com")
        err = io.StringIO()
        with contextlib.redirect_stderr(err), contextlib.redirect_stdout(io.StringIO()):
            code = clean.main(["--config", str(self.config), "--yes"])
        self.assertEqual(code, 1)
        self.assertIn("Nothing was deleted", err.getvalue())
        self.assertFalse([r for r in self.mock.requests if r[0] == "DELETE"])

    def test_all_data_selects_everything_on_the_account(self):
        contacts = [{"resourceName": "people/c1", "emailAddresses": [{"value": "real@example.com"}]}]
        events = [{"id": "e1", "summary": "Dentist"}]
        cs, es = clean.choose(contacts, events, FIX, all_data=True)
        self.assertEqual((len(cs), len(es)), (1, 1))
        cs, es = clean.choose(contacts, events, FIX, all_data=False)
        self.assertEqual((len(cs), len(es)), (0, 0))

    def test_a_failed_delete_is_reported_and_the_others_still_run(self):
        self.mock.serve("DELETE", "/calendar/primary/events/e1", 404, {"error": {"errors": [{"reason": "notFound"}],
                                                                              "message": "Not Found"}})
        self.mock.serve("DELETE", "/calendar/primary/events/e2", 204, None)
        deleted, failed = clean.delete_all("AT", [], [{"id": "e1", "summary": "TEST a"},
                                                      {"id": "e2", "summary": "TEST b"}],
                                           base_calendar=self.mock.base + "/calendar/primary",
                                           sleep=lambda s: None)
        self.assertEqual(deleted, ["appointment TEST b"])
        self.assertEqual(len(failed), 1)
        self.assertIn("notFound", failed[0])


if __name__ == "__main__":
    unittest.main()
