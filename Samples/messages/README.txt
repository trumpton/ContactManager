This folder contains the template messages in the following files:

    emailconfirmation.msg
    emaildelete.msg
    emailupdate.msg
    emailreminder.msg
    smsconfirmation.msg
    smsdelete.msg
    smsupdate.msg
    smsreminder.msg

SMS messages should be on a single line.
Email messages can be multi-line.

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


