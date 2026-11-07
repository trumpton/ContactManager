#
# Minimal Google API client for the test utilities.  Standard library only, so the utilities run on a clean machine.
#
# Requests are retried for rate limits (429) and server errors (5xx): the wait is 0.5 s, doubling up to 4 s, four
# retries, as the application does.  Any other failure raises GoogleError with the HTTP status, Google's reason and
# message, and the parsed body in .payload (OAuth device errors such as authorization_pending are read from it).
#

import json
import time
import urllib.error
import urllib.parse
import urllib.request

TOKEN_URL = "https://www.googleapis.com/oauth2/v4/token"
DEVICE_URL = "https://accounts.google.com/o/oauth2/device/code"
USERINFO_URL = "https://www.googleapis.com/oauth2/v3/userinfo"
PEOPLE_URL = "https://people.googleapis.com/v1"
CALENDAR_URL = "https://www.googleapis.com/calendar/v3/calendars/primary"

# The same scopes as the application (googleaccess_oauth2.cpp, account-googleaccess.h)
SCOPE = ("https://www.googleapis.com/auth/calendar https://www.googleapis.com/auth/contacts "
         "https://www.google.com/m8/feeds/ profile email")

RETRY_STATUS = {429, 500, 502, 503, 504}


class GoogleError(Exception):
    def __init__(self, status, reason, message, payload=None):
        super().__init__(f"HTTP {status} {reason}: {message}".rstrip(": "))
        self.status = status
        self.reason = reason
        self.message = message
        self.payload = payload


def _parse(body):
    if not body:
        return None
    try:
        return json.loads(body.decode("utf-8"))
    except (ValueError, UnicodeDecodeError):
        return None


def _error_fields(payload):
    """Reason and message from a Google error body, in either the API or the OAuth shape."""
    if not isinstance(payload, dict):
        return "", ""
    err = payload.get("error")
    if isinstance(err, dict):
        reasons = err.get("errors") or [{}]
        return reasons[0].get("reason", err.get("status", "")), err.get("message", "")
    if isinstance(err, str):
        return err, payload.get("error_description", "")
    return "", ""


def request(method, url, *, data=None, headers=None, sleep=time.sleep, retries=4, timeout=60):
    """Return the parsed JSON reply (None for an empty reply).  Raises GoogleError on failure."""
    attempt = 0
    while True:
        req = urllib.request.Request(url, data=data, method=method, headers=headers or {})
        try:
            with urllib.request.urlopen(req, timeout=timeout) as resp:
                return _parse(resp.read())
        except urllib.error.HTTPError as e:
            payload = _parse(e.read())
            if e.code in RETRY_STATUS and attempt < retries:
                sleep(min(0.5 * (2 ** attempt), 4.0))
                attempt += 1
                continue
            reason, message = _error_fields(payload)
            raise GoogleError(e.code, reason, message, payload) from None


def post_form(url, fields, **kw):
    data = urllib.parse.urlencode(fields).encode("ascii")
    return request("POST", url, data=data,
                   headers={"Content-Type": "application/x-www-form-urlencoded"}, **kw)


def get_json(url, token, **kw):
    return request("GET", url, headers={"Authorization": "Bearer " + token}, **kw)


def delete(url, token, **kw):
    return request("DELETE", url, headers={"Authorization": "Bearer " + token}, **kw)
