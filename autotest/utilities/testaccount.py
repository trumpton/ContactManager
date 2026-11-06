#
# The test account file and the test identifiers, shared by setup_test_account.py and clean_test_data.py.
#
# The account file holds the Google client secret, the refresh token and the two ContactManager passwords.  It is kept
# outside the repository (default ~/.config/contactmanager-test/test-account.json), with permissions 0600, and these
# functions refuse any path inside the repository.
#

import json
import os
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent                     # autotest/utilities is two folders below the repository root
FIXTURES = HERE / "test_fixtures.json"
DEFAULT_ACCOUNT_FILE = Path.home() / ".config" / "contactmanager-test" / "test-account.json"


class AccountFileError(Exception):
    pass


def load_fixtures(path=FIXTURES):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def is_inside_repo(path):
    p = Path(os.path.realpath(path))
    r = Path(os.path.realpath(REPO))
    return p == r or r in p.parents


def check_outside_repo(path):
    if is_inside_repo(path):
        raise AccountFileError(
            f"{path} is inside the repository. The account file holds secrets and must be kept outside it, "
            f"for example in {DEFAULT_ACCOUNT_FILE}.")


def save_account(data, path=DEFAULT_ACCOUNT_FILE):
    """Write the account file with owner-only permissions.  Refuses paths inside the repository."""
    path = Path(path)
    check_outside_repo(path)
    path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    os.chmod(path.parent, 0o700)
    tmp = path.with_suffix(".tmp")
    # Created with 0600 from the start, so the secrets are never readable by others, even briefly
    fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
        f.write("\n")
    os.replace(tmp, path)
    os.chmod(path, 0o600)
    return path


def load_account(path=DEFAULT_ACCOUNT_FILE):
    path = Path(path)
    if not path.exists():
        raise AccountFileError(f"No account file at {path}. Run setup_test_account.py first.")
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    for key in ("google", "contactmanager"):
        if key not in data:
            raise AccountFileError(f"{path} has no '{key}' section. Run setup_test_account.py again.")
    return data


# ---- identifying test records

def is_test_contact(email_values, fixtures):
    """A contact is a test record when any of its email addresses is in the test domain."""
    domain = "@" + fixtures["identifier"]["email_domain"].lower()
    return any(str(v).strip().lower().endswith(domain) for v in email_values)


def is_test_event(summary, fixtures):
    """An appointment is a test record when its summary starts with the test prefix."""
    return str(summary or "").startswith(fixtures["identifier"]["event_prefix"])
