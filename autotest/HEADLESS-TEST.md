# Headless test environment

How to run ContactManager without a screen, test its Google sync against a Google account, and sign in to that account when a human has to approve it.

The usual setup is an AI agent working in a container with no display and no browser, and a human who has the Google account. The agent does everything it can. The human does only what needs their Google login, and reads the agent's instructions and sends back results.

## 0. Use a scratch Google account

**The bench must run against a scratch Google account, made for testing and used for nothing else.** It creates, changes and deletes contacts and calendar events on that account, many times in one run. Do not point it at a personal, work or family account, or at any account that holds real contacts, calendars or mail.

- Create a new Google account for the bench. Do not link it to any real person's data, and do not add it to a shared calendar or contact list.
- Its contacts and events are fictitious. The bench uses clearly fake names (`Bench ...`, `BENCH ...`) and `@test.invalid` addresses, and deletes them before and after each scenario. Anything else on the account is left alone, but the bench still changes what is there, so keep nothing on it you want to keep.
- Its secrets can be reset at any time, and resets are expected (section 9). Plan for one.
- Google expires refresh tokens for an app left in *Testing* status after about seven days. Expect to repeat section 4 about once a week.

## 1. Build environment

Ubuntu 24.04 (other releases need the matching Qt 6 packages):

```bash
sudo apt-get install -y qt6-base-dev qt6-base-dev-tools qt6-multimedia-dev qt6-5compat-dev \
    build-essential libssl-dev python3
```

Clone the app and its `Lib` dependency side by side. The project file reads `../Lib`:

```bash
git clone https://github.com/trumpton/contactmanager /home/user/contactmanager
git clone https://github.com/trumpton/Lib           /home/user/Lib
```

Build out of the source tree, so no build files end up in the repository:

```bash
mkdir -p /home/user/build-contactmanager && cd /home/user/build-contactmanager
qmake6 /home/user/contactmanager/ContactManager.pro && make -j"$(nproc)"
```

## 2. Google API access (keys and credentials)

The app signs in with an OAuth client. Only the client ID and secret are needed, and they must never be committed.

1. Sign in to the Google Cloud console with the **scratch account's owner** login, and create a project for testing. Use a project that exists only for the bench.
2. Enable the **Google People API** and the **Google Calendar API** for that project.
3. Configure the OAuth consent screen. For a personal account choose *External*, and add the scratch account as a *test user*.
4. Create an OAuth client ID of type **"TVs and Limited Input devices"**. The app uses Google's device sign-in, and other client types fail at that step.
5. Create `account-googleaccess.h` from `account-googleaccess.h.example` (the file is git-ignored) and put in the client ID and secret:

   ```c
   #define CLIENTID "....apps.googleusercontent.com"
   #define SECRET   "GOCSPX-..."
   ```

   Keep the file's permissions private (`chmod 600`). The bench reads it through `CM_HEADER`.

The client secret is a credential. Keep it out of logs, chat transcripts, issues, pull requests and commits. Share it as described in section 9. If it has been pasted anywhere it was not meant to be, rotate it (section 9).

## 3. Local encryption key

The app encrypts its local data, so it asks for a key and a password the first time it runs, and at every login. The test uses one random secret, which the driver enters for both the key and the password (and their confirmations). Generate it on the test machine, once, and keep it in a private file:

```bash
umask 077
openssl rand -base64 18 > /home/user/.cm-test-secret
export CM_TEST_SECRET="$(cat /home/user/.cm-test-secret)"
```

`openssl rand -base64 18` gives 24 characters. The driver enters this secret for the key and login dialogs, and stops at any other key dialog. It is a test value only. Never use a real person's key here.

The app also writes its own encryption keys to its settings file (`TextFileEncryption.conf`). Those are created by the app, are protected by this secret, and must not be shared. Every new working directory creates new ones.

## 4. Signing in to Google from a headless environment

The agent cannot open a browser, so the sign-in is a device flow with a human in the loop:

1. **Agent** asks Google for a device code (the same request the app makes):

   ```bash
   curl -sS -X POST https://accounts.google.com/o/oauth2/device/code \
        --data-urlencode "client_id=$CID" --data-urlencode "scope=$SCOPE"
   ```

   Google returns a `user_code` (about eight characters), a `verification_url` (normally `https://www.google.com/device`), and a `device_code`, which the agent keeps to itself.
2. **Agent** gives the human the verification URL and the user code, and says how long the code lasts (`expires_in`, normally 30 minutes).
3. **Human** opens the URL in any browser, signs in to the test account, types the code, and selects **Allow**.
4. **Agent** polls the token endpoint every `interval` seconds until Google returns tokens. A `authorization_pending` reply means the human has not finished yet:

   ```bash
   curl -sS -X POST https://oauth2.googleapis.com/token \
        --data-urlencode "client_id=$CID" --data-urlencode "client_secret=$SECRET" \
        --data-urlencode "device_code=$DEVICE_CODE" \
        --data-urlencode "grant_type=urn:ietf:params:oauth:grant-type:device_code"
   ```

5. **Agent** saves the refresh token and the account name in the app's settings, as the app does after a sign-in. The app's settings are in `~/.config/trumpton.org.uk/ContactManager.conf` under `googlerefreshtoken` (the refresh token and the account name, separated by a space) and `googleusername`.
6. **Agent** confirms the account it signed in with (userinfo) and tells the human which address it is, so the human can check it is the right account.

The refresh token is a credential, as good as a password for the scratch account. The app keeps it in `ContactManager.conf`, which must be `chmod 600`. Keep the access token private. It lasts about an hour, and the agent should fetch a new one from the refresh token when it needs one. Delete the temporary files that hold the device code and tokens once the settings are saved.

If the human reports an error at step 3, check the client type first (it must be "TVs and Limited Input devices"), then that the test account is listed as a test user.

## 4a. The app's settings file

The app reads `~/TrumptonApps/ContactManager.ini` (or `~/ContactManager/ContactManager.ini`). Without it, the
app's settings reader returns the text `ERROR` for every setting, and the phone code then uses that text as the
dialling codes. Numbers come out as `error202020202`. This was found by the bench (scenarios M02, M06, X04).

For the bench, create the file with the phone codes blank, so numbers are stored as typed:

```ini
[phone]
localcode=
countrycode=
```

The sample in `Samples/ContactManager.ini` sets real codes (`localcode=01234`, `countrycode=0044`). With those, the
app rewrites phone numbers when it stores them, so use them only to test that rewriting on purpose.

## 5. Running the app headlessly

```bash
export QT_QPA_PLATFORM=offscreen     # no display needed
cd <an empty working directory>      # the app keeps its local data here, so use a new one per run
bench_driver <outdir> <operations.json>
```

The app keeps its local data in its working directory. Give each run its own empty directory, or one run will load the contacts of an earlier one.

The app shows dialogs even headless. `bench/bench_driver.cpp` answers them: the key and login dialogs with `CM_TEST_SECRET`, the sync dialog by pressing its finish button, and every other dialog with its default button. It records each dialog it meets in `driver.log`.

## 5a. Messages and programs outside the bench

**"No audio device detected" and "PulseAudioService: pa_context_connect() failed".** The container has no sound device, so
Qt Multimedia reports it when the app's sound effects are set up. Every bench run prints about eleven of these lines
in its `stdout.log`. They are harmless and are not a failure. The bench driver calls `initSound(NULL)`, which creates
the sound effects without a device. A test program that uses the app's dialogs or alerts must call `initSound(NULL)`
too (`alertsound.h` in the Lib folder).

**Test programs that link against the app.** These are for checks the bench cannot make, such as loading a file
written by an older version. Build one from the app's objects, not from the app's `main`:

- Build the app first (section 1). Then compile the program with the same `DEFINES` and `INCPATH` as the Makefile, and
  link it with every `.o` in the build folder except `main.o`, plus the Qt libraries. Add `-I/home/user/Lib`, because
  `encryption.h` and `alertsound.h` are in the Lib folder.
- Include the Qt and standard headers before any `#define private public`. Defining `private` first breaks the
  standard library headers.
- Call `initSound(NULL)`, then create `new Configuration(nullptr)` and give it an `Encryption` object. Contact loading
  needs both. The app keeps contacts as plain text unless the settings say `encrypted=true`, which is the default.
- The encryption object shows a key dialog. Answer it with `CM_TEST_SECRET`, as the driver does (its "Enter Key and
  Password" fields are `lineEdit0` to `lineEdit3`, "Login" uses `lineEdit2`), or the program waits for ever.
- Give the program its own HOME with `TrumptonApps/ContactManager.ini` (section 4a) and an empty
  `.config/trumpton.org.uk` folder, the same as a bench run. Without the ini file, phone numbers are stored with the
  dialling code `ERROR`.
- Run it with `QT_QPA_PLATFORM=offscreen`, and in the background with a time limit, so a wait on a dialog does not
  hold up the session.

**The old-file test (`autotest/oldfile/old_contact_test.cpp`).** It loads a contact file written before the label fields
existed and checks the meaning of its emails and phones, then saves and reloads it. Build it in the build folder the same
way as above, with `-I../contactmanager`, and link it with the app's objects (not `main.o`). Run it in an empty folder
with the two fixture files copied in, using its own HOME with the settings from section 4a, and with
`CM_TEST_SECRET` set:

    cp autotest/oldfile/*.contact /some/empty/folder/
    CM_TEST_SECRET="$(cat /home/user/.cm-test-secret)" QT_QPA_PLATFORM=offscreen HOME=/some/home ./old_contact_test /some/empty/folder

It prints PASS or FAIL for each check and exits with 1 if any check fails.

**Long builds.** A build can take longer than the two-minute limit of the tool that runs commands. Start it in the
background with a log file, then wait for the log to show it has finished. This matters most after a change to a
`.pro` file, which needs `qmake6` run again before `make`.

**One run at a time.** Two bench runs, or a bench run and a test program, on the same account delete each other's
test contacts. The bench's cleanup removes every contact named `Bench`. Stop any background run before starting another.

## 6. Running the bench

The bench has three groups (ticket #105). Each scenario is in one group, and `google_bench.py groups` lists them with a typical duration.

| Group | What it covers | Time |
|---|---|---|
| 1, uploads (`U01`) | Purges the account, then uploads 250 contacts and 1,000 appointments, half through the Google API and half through ContactManager (its sync). Aaron Aesop is among the contacts. The records are left in place. | slow: about 20 minutes or more |
| 2, updates | Every other test that syncs with Google. Each puts on Google the one or two details it needs first, rather than relying on the records in group 1. Google changes outside ContactManager are made with the API. | about 45 seconds each |
| 3, local | Tests that need no Google sync. The account is not touched, so there is no cleanup on it. | a few seconds each |

Selections (`google_bench.py run <name>`):

| Name | Runs | Use |
|---|---|---|
| `full` | 1, 2, 3 | everything |
| `quick` | 2, 3 | no uploads |
| `google` | 2 | every Google sync test |
| `google-quick` | a subset of 2 (`GOOGLE_QUICK` in the script) | a few minutes |
| `local` | 3 | no Google at all |

Scenario codes can be given instead: `google_bench.py run R01 L02`.

Build the driver after the application (it links the application's objects, so rebuild it after each change to the app). It is built in a folder of its own, not in the build folder:

```bash
sh autotest/bench/build_driver.sh /home/user/build-contactmanager    # writes sync_driver into that folder
```

`sync_driver` (`autotest/bench/sync_driver.cpp`) is a superset of `bench_driver`. It adds `listdir` (a calendar sub-folder, such as `archive`), `dumplog`, `paths`, `setting`, `wait` and `key`.

```bash
export CM_BENCH_DRIVER=/home/user/build-contactmanager/sync_driver
export CM_BUILD=/home/user/build-contactmanager          # for the messages tests (L01)
export CM_TEST_SECRET="$(cat /home/user/.cm-test-secret)"
python3 autotest/google_bench.py run local
python3 autotest/google_bench.py run google-quick
python3 autotest/google_bench.py run full
```

### Purging the account

```bash
python3 autotest/google_bench.py purge contacts|appointments|both --yes
```

It removes **everything** of that kind on the account, not only the test entries, so run it only on the scratch account. Without `--yes` it does nothing. Google's contact-delete limit is per minute, so a purge of a full account takes a long time. The upload group purges the account itself before it uploads.

### Changes on Google, and the test prefix

Changes made on Google outside ContactManager go through `google_bench.py gchange` (or the same helpers in Python), which **refuses any event or contact without the test prefix**. Only `BENCH ...` events, and contacts named `Bench` or with a `@test.invalid` address, can be changed. The upload group's records are named `Upload ...` and are left in place at the end of the group, which is why its tests do not depend on them.

### How a group is chosen

The updates and local groups are decided by reading each scenario's source: a scenario that calls the sync, or a Google helper, is in group 2; the rest are in group 3. `google_bench.py groups` shows the result, so check it after adding a scenario.

### Typical durations

The durations are in `TYPICAL_SECONDS` in `google_bench.py`, and each run records the time it took in `results.json`. Measured on the scratch account (about 3,300 appointments and 500 contacts on it):

| Test | Typical seconds |
|---|---|
| Group 3, most tests | 4 to 11 |
| `K24` | about 20 |
| `R01` to `R05` | 42 to 47 (two or three runs each) |
| `AR01` | about 80 (four runs) |
| `U01` | about 1,080 for the earlier all-API version; the mixed version is not yet timed |

Group 2 has 95 tests. Most have not been timed yet, so the group's total is not known until a full run has been done.

### Known test results

- Group 3: 24 of 24 pass (`L01` runs the 77 messages unit tests, which all pass).
- `R02` and `R03` fail: under Google master, an edit made in ContactManager is kept by the app at the next sync, where the ticket's matrix says Google's copy should win. This is a behaviour question for the owner, not a test fault, and has not been changed in the app.

## 7. Working with a human go-between

- The agent prepares, the human approves. The agent should say exactly what it is about to change on the account before it does.
- Keep the human to the steps only they can do: signing in, and answering questions about the account.
- Ask for a screen-reader check or a visual check only when the test needs one. Most of the bench needs no human.
- When the human sends back a code, the agent uses it straight away, because device codes expire.

## 8. Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `invalid_client` at the device step | Client type is not "TVs and Limited Input devices". |
| `access_denied` after the human signs in | The account is not a test user on the consent screen. |
| `429 Too Many Requests` | Google's quota. The bench backs off and retries; reduce the size of large scenarios if it persists. |
| App stops at an unexpected key dialog | `CM_TEST_SECRET` is unset, or the local data is from a different secret. Use a new working directory. |
| Contacts from an earlier run appear | The working directory was reused. Start from an empty directory. |
| 404 on `createContact` | The path must be `/v1/people:createContact`. |
| `400` "Cannot have more than one person.birthdays per source" | Google allows one birthday per contact. Put each birthday on its own contact. |
| `invalid_grant` on the token refresh | The refresh token has expired or been revoked (Testing status, or access removed). Repeat section 4, or reset (section 9). |
| The bench finds a real contact or event | Stop. The account is not a scratch account. Section 0 applies. Do not run the bench again until the account is replaced. |

## 9. Secrets: where they live, how to share and reset them

The bench needs four secrets. None of them is committed, and none goes in an issue, pull request, commit message or doc.

| Secret | Where it lives on the test machine | How it is made | Shared? |
|---|---|---|---|
| OAuth client ID and secret | `account-googleaccess.h` (git-ignored, `chmod 600`) | Section 2 | Yes, to the agent |
| Refresh token and account name | `~/.config/trumpton.org.uk/ContactManager.conf`, key `googlerefreshtoken` (`chmod 600`) | Section 4 | Yes, to the agent, if the agent cannot sign in itself |
| Local key and password | `/home/user/.cm-test-secret`, used as `CM_TEST_SECRET` | Section 3 | Yes, to the agent |
| Access token | Never stored. Minted from the refresh token for each run. | Google | No |

The app's own `TextFileEncryption.conf` keys are not in this table. They are not needed for the bench and must not be shared.

### Sharing

1. **The scratch account owner** creates the Google Cloud project and the OAuth client (section 2), signs the scratch account in once (section 4), and creates the local key (section 3).
2. **Hand the values to whoever runs the bench** through a channel the bench does not record: a password manager share, or a direct message. Put them straight into the files above on the test machine, with `umask 077` or `chmod 600`.
3. **Never** paste them into a chat that is kept, an issue, a pull request or its comments, a commit, a doc, or a log. If a bench log prints a secret, delete the log and rotate the secret.
4. **Check before a commit.** The repository should contain none of these values. For example, `git grep -n -e 'GOCSPX-' -e '1//' -e 'ya29\.'` should match only placeholders such as `GOCSPX-...`.
5. **Delete what is not needed.** Remove temporary device codes and access tokens once the refresh token is saved.

### Resetting

Resets are expected. Do one when a secret may have leaked, when the scratch account is replaced, or when Google expires the refresh token.

- **Client secret:** in the Google Cloud console, open the OAuth client and reset its secret. Update `account-googleaccess.h`. The old secret stops working at once.
- **Refresh token:** remove the app's access at `https://myaccount.google.com/permissions` from the scratch account. Delete the `googlerefreshtoken` line from `ContactManager.conf`, then repeat section 4.
- **Local key:** write a new `.cm-test-secret` (section 3). Use new working directories, because local data made with the old key cannot be opened with the new one.
- **Scratch account replaced:** create the new account, repeat sections 2 to 4, and check that nothing from the old account is still in the bench's settings.
