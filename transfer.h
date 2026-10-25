#ifndef TRANSFER_H
#define TRANSFER_H

#include "transferfile.h"

class ContactDatabase ;
class Calendar ;

// Contacts and appointments as export records.  Deleted contacts are not exported.  Appointments that
// come from Google as read-only entries, and birthdays, are not exported, as they are not the user's own
// entries.  Google's links and the sync flags are not exported either.
QList<TransferRecord> contactTransferRecords(ContactDatabase &db) ;
QList<TransferRecord> appointmentTransferRecords(Calendar &cal) ;

// True if a record with the same ID is already held
bool contactExists(ContactDatabase &db, const TransferRecord &rec) ;
bool appointmentExists(Calendar &cal, const TransferRecord &rec) ;

// Stores an imported record.  With overwrite set, an existing record is updated in place, so that its
// Google link is kept.  Otherwise a new record is added.  The caller decides whether to overwrite.
void importContact(ContactDatabase &db, const TransferRecord &rec, bool overwrite) ;
void importAppointment(Calendar &cal, const TransferRecord &rec, bool overwrite) ;

// Describes a record for the user, for example in a question about an existing record
QString transferDescription(const TransferRecord &rec) ;

#endif // TRANSFER_H
