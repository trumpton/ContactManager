This folder contains the template messages in the following files:

    emailconfirmation.msg
    emaildelete.msg
    emailupdate.msg
    emailreminder.msg
    smsconfirmation.msg
    smsdelete.msg
    smsupdate.msg
    smsreminder.msg

The first line is the sender:
    Email:  the From address, written as "Name <address>", e.g.
            My Company Name <email@domain.com>
    SMS:    the name shown to the recipient, e.g. My Company Name
The sender in the ini file overrides it, when set: sender= under
[email-reminder] for email, and sender= under [sms-reminder] for SMS.
The templates have a placeholder first line, "override with sender=",
so set the sender in ContactManager.ini before sending.

The second line is the subject for email, or the message text for SMS.
SMS messages should be on a single line (the second line only).
Email messages can be multi-line, from the third line on.

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


