This folder contains the template messages in the following files:

    emailconfirmation.msg
    emaildelete.msg
    emailupdate.msg
    emailreminder.msg
    smsconfirmation.msg
    smsdelete.msg
    smsupdate.msg
    smsreminder.msg
    emailtoday.msg   (the on-the-day email; the reminder template is used if not set)
    smstoday.msg    (the on-the-day text; the reminder template is used if not set)

The sender is not in these files. It is set in ContactManager.ini:
    Email:  sender= under [email-reminder], the From address, written as
            "Name <address>", e.g. My Company Name <email@domain.com>
    SMS:    sender= under [sms-reminder], the name shown to the recipient,
            e.g. MyCompany (letters and digits, 3 to 11 of them)
Every message on a channel uses that channel's sender. If it is blank, no
message on that channel is sent, the log says "sender missing", and nothing
is added to the contact's history. A blank sender is also logged at startup.

The first line is the subject for email, or the message text for SMS.
SMS messages should be on a single line (the first line only).
Email messages can be multi-line, from the second line on.

Files written for ContactManager before this change have a sender as their
first line. They are not compatible: remove that line from each file.

Message variable substitutions are as follows:

    $NAME  - First Names
    $TIME  - Start Time
    $DOW   - Day of Week
    $DATE  - Day of Month
    $MONTH - Month
    $YEAR  - Year
    $FDATE - Full Date: dd-MMM-yyyy
    $APPOINTMENTS - The contact's next appointments (up to 10), soonest first,
                    in the form "10:00 on Friday 30 October".  One per line in
                    an email body.  On one line, separated by "; ", in the
                    subject and in SMS messages.


