Message templates for the announcements bench.

Each template's first line is the subject (email) or the text (SMS).  The subject carries the test code ($TESTTAG, which the
program replaces with the appointment's test code when CM_TEST_MESSAGE_TAGS is set), the kind of message (Confirmation,
Updated, Cancelled, Reminder, Today), and the appointment time and date ($TIME, $DATE, $MONTH).  The checker reads these
words, so keep the layout: test code, kind, time, day, month.

The runner copies these into each run's home folder, and changes them only where a test needs it (AN-039, AN-044).
