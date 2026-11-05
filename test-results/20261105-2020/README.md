# #89 accelerated messaging test, after the messaging refactor (5 November)

Commit 0379c0a moved the limit, retry, channel, signature and template rules into `messagedecision`. This run checks the send loop with those rules.
The run used the accelerated clock at 60 times real time, the 25 scenarios in `accel/model.py`, and the same expected messages as run 1.

Result: all 60 expected messages sent and matched, each within one check of its expected minute, none missing or unexpected, and every appointment time quoted correct.
The run's report is `report-final.md`; the messages are in `box/`.
