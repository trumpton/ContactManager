#ifndef RATEPACER_H
#define RATEPACER_H

#include <QList>
#include <QStringList>

// Pacing of the uploads to Google (issue #94).  Each transaction (an upload, create, change or delete) is counted, and whether
// it needed a retry because Google's rate limit answered.  When 5 of the last 100 transactions needed one, the pacing starts at
// 4 seconds before each transaction.  Every 500 transactions after that it halves, down to 500 ms.  Each change is reported.
class RatePacer
{
public:
    RatePacer() ;

    // Milliseconds to wait before the next transaction (0 until the pacing starts)
    int delayBeforeNext() const ;

    // One transaction has finished.  hadRateLimit is true when Google's rate limit made it retry
    void finished(bool hadRateLimit) ;

    bool adaptive() const { return started ; }

    // The messages for the sync report since the last call
    QStringList takeNotes() ;

private:
    QList<bool> recent ;      // the last 100 transactions: whether each needed a retry for a rate limit
    bool started ;
    int pacingms ;
    int sincestart ;
    QStringList notes ;
} ;

#endif // RATEPACER_H
