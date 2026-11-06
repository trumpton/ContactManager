# Test utilities

Two utilities set up and clean the disposable Google account that area 1 of the test specification uses. They use only the Python standard library (Python 3).

| File | What it does |
|---|---|
| `setup_test_account.py` | Links the tests to the disposable account, once. Creates the two ContactManager passwords, and writes the account file. |
| `clean_test_data.py` | Deletes the test records from the account (default), or everything on it (`--all-data`). |
| `googleclient.py` | The Google requests the two utilities make, with retries for rate limits and server errors. |
| `testaccount.py` | The account file, and how test records are identified. |
| `test_fixtures.json` | The six test contacts (specification 0.4), and the identifiers. |
| `test_utilities.py` | Tests for the utilities. They use a local mock of Google, and touch no real account. |

The account file is kept **outside the repository**, at `~/.config/contactmanager-test/test-account.json`, with permissions 0600. The utilities refuse to write it anywhere inside the repository.

## 1. Set up the disposable account (once)

Use a disposable Google account. Never a real one.

```sh
python3 autotest/utilities/setup_test_account.py
```

The utility:

1. Shows the Google Cloud steps (enable the Calendar and People APIs, create an OAuth client of the type "TVs and Limited Input devices", and add the account as a test user) and the hosts Claude's environment must reach: `accounts.google.com`, `www.googleapis.com` and `people.googleapis.com`.
2. Asks for the **Client ID** and the **Client secret**. The secret is typed hidden. For a non-interactive run, set `CM_TEST_CLIENT_ID` and `CM_TEST_CLIENT_SECRET`.
3. Creates two passwords: the **database password** (the ContactManager encryption key) and the **login password**. They are generated at random by default. Use `--choose-passwords` to type your own (at least 12 characters). For a non-interactive run, set `CM_TEST_DB_PASSWORD` and `CM_TEST_LOGIN_PASSWORD`.
4. Shows a web address and a code. Open the address, enter the code, and sign in to the disposable account. The utility waits for you.
5. Asks you to type the account's address, to confirm it is the disposable account. Nothing is saved if the address does not match.
6. Writes the account file. The passwords and the secret are never printed.

When ContactManager asks for the **Encryption Key** and the **Initial Password** in a test, use the two passwords from the account file. The database password is the encryption key.

To check the saved account still works, changing nothing:

```sh
python3 autotest/utilities/setup_test_account.py --check
```

## 2. Clean the account

The default removes the test data only:

- contacts with an email address in the test domain (`sgc.contacmanagertest`);
- appointments on the primary calendar whose summary starts with `TEST `.

```sh
python3 autotest/utilities/clean_test_data.py --dry-run   # list what would be deleted; delete nothing
python3 autotest/utilities/clean_test_data.py             # delete the test data, after a question
python3 autotest/utilities/clean_test_data.py --yes       # as above, without the question
```

The default clean-up finds the test data by search (the test domain for contacts, the test prefix for appointments), not by listing the account. A search returns one page of contacts at a time, so the clean-up searches again after each pass, up to 20 passes, and stops at once if a delete fails.

`--all-data` deletes **everything**: every contact in the account's contact list, and every appointment on its primary calendar. Other calendars and "other contacts" are not touched. It needs the account's address typed in, and it cannot be combined with `--yes`:

```sh
python3 autotest/utilities/clean_test_data.py --all-data
```

Before any delete, the utility checks that the token belongs to the account in the account file. If it does not, nothing is deleted.

Exit codes: 0 for success, 1 for a failure (the message says which), 2 for a refused combination of options.

## 3. Run the tests of the utilities

```sh
python3 -m unittest discover -s autotest/utilities -p 'test_*.py' -v
```

The tests run against a local mock of the Google endpoints. They need no network and no account.

## Notes

- The Google scopes are the same as the application's (calendar, contacts, and the account's name and email). The client ID and secret are the same kind of values the application uses (`account-googleaccess.h`), but they are kept in the account file, not in the repository.
- Every request that fails with a rate limit or a server error is retried four times, waiting 0.5 s, 1 s, 2 s and 4 s. Any other error is reported at once, with Google's reason.
