# #110 accelerated test: per-contact limits and retries (5 November)

Set-up
- Clock: accelerated, 60 times real time, from 08:40 on Tuesday 3 November 2026 to 12:00 on Wednesday 4 November.
- Limits at their defaults: 5 messages an hour and 10 a day for each contact (`[reminders]` limitperhour=5, limitperday=10).
- Send window 07:00 to 22:00. Hold 5 minutes. Retry wait 5 minutes. Maximum 10 tries.
- Failures: `CM_TEST_FAIL_EMAIL="retry.twice@vizier.uk=2;retry.never@vizier.uk=10"`. The test email substitute (`email_test.cpp`) fails
  the first N sends to each address and writes nothing for them.

Contacts and appointments (`model2.py`)
- Hourly (limit.hourly@vizier.uk): L01 to L07, created 09:00 to 09:06 on 3 November, one confirmation each.
- Daily (limit.daily@vizier.uk): D01 to D11, created every 2 hours from 10:00 on 3 November, one confirmation each.
- Retry (retry.twice@vizier.uk): R01, created 14:00 on 3 November. The first two sends fail.
- Giveup (retry.never@vizier.uk): G01, created 15:00 on 3 November. All ten sends fail.

Result: 19 expected, 18 matched within one minute, 1 late (R01: sent 14:17:34, expected 14:15). Unexpected 1 (the same R01 message, because it
is outside its expected minute). See `report-final.md`.

Data
- `box/`: every message the run wrote (19 files, one per sent message).
- `expected.json`: the model's expected messages, with their times.
- `model2.py`, `sim2.py`, `sim_base.py`: the model and the simulator. `build_accel2.py` and `build_run2.py`: the builders.
- `email_test.cpp`: the test email substitute, with the failure hook.
- `supervisor.log` and `ops.json`: how the run was driven.

Not yet known: the times of the failed tries (the application does not log them to a file the test reads).
