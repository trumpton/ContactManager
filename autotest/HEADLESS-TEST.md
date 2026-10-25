# Headless test environment

How to run ContactManager without a screen, test its Google sync against a Google account, and sign in to that account when a human has to approve it.

The usual setup is an AI agent working in a container with no display and no browser, and a human who has the Google account. The agent does everything it can. The human does only what needs their Google login, and reads the agent's instructions and sends back results.

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

1. In the Google Cloud console, create a project for testing.
2. Enable the **Google People API** and the **Google Calendar API** for that project.
3. Configure the OAuth consent screen. For a personal account choose *External*, and add the throwaway account as a *test user*.
4. Create an OAuth client ID of type **"TVs and Limited Input devices"**. The app uses Google's device sign-in, and other client types fail at that step.
5. Create `account-googleaccess.h` from `account-googleaccess.h.example` (the file is git-ignored) and put in the client ID and secret:

   ```c
   #define CLIENTID "....apps.googleusercontent.com"
   #define SECRET   "GOCSPX-..."
   ```

The secret is a credential. Keep it out of logs, chat transcripts and commits. If it has been pasted anywhere it was not meant to be, rotate it in the console and update the header.

## 3. Local encryption key

The app encrypts its local data, so it asks for a key and a password the first time it runs, and at every login. In a test environment, choose a random secret and keep it in a private file:

```bash
umask 077
openssl rand -base64 18 > /home/user/.cm-test-secret
export CM_TEST_SECRET="$(cat /home/user/.cm-test-secret)"
```

The headless driver enters this secret for the key and login dialogs, and stops at any other key dialog. Never use a real person's key here.

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

Keep the access token private. It lasts about an hour, and the agent should fetch a new one from the refresh token when it needs one. Delete the temporary files that hold the device code and tokens once the settings are saved.

If the human reports an error at step 3, check the client type first (it must be "TVs and Limited Input devices"), then that the test account is listed as a test user.

## 5. Running the app headlessly

```bash
export QT_QPA_PLATFORM=offscreen     # no display needed
cd <an empty working directory>      # the app keeps its local data here, so use a new one per run
bench_driver <outdir> <operations.json>
```

The app keeps its local data in its working directory. Give each run its own empty directory, or one run will load the contacts of an earlier one.

The app shows dialogs even headless. `bench/bench_driver.cpp` answers them: the key and login dialogs with `CM_TEST_SECRET`, the sync dialog by pressing its finish button, and every other dialog with its default button. It records each dialog it meets in `driver.log`.

## 6. Running the Google bench

```bash
export CM_BENCH_DRIVER=/path/to/bench_driver
export CM_TEST_SECRET="$(cat /home/user/.cm-test-secret)"
python3 autotest/google_bench.py run            # all scenarios
python3 autotest/google_bench.py run C02 A03   # some scenarios
```

The bench uses only the throwaway account. It names everything it creates (`Bench ...` contacts, `BENCH ...` appointments, `@test.invalid` addresses), and deletes those names before and after each scenario. Anything else on the account is left alone.

Results are printed, and written to `results.json` in the run directory. A failed check prints its detail.

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
