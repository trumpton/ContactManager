#include "ratepacer.h"

#include <QtGlobal>

#define PACER_WINDOW 100        // transactions the rate limit is counted over
#define PACER_LIMIT 5           // rate-limited transactions in the window that start the pacing
#define PACER_HALVE_EVERY 500   // transactions between halvings of the pacing
#define PACER_MAX_MS 4000       // the pacing when it starts
#define PACER_MIN_MS 500        // the least pacing

RatePacer::RatePacer() : started(false), pacingms(PACER_MAX_MS), sincestart(0)
{
}

int RatePacer::delayBeforeNext() const
{
    return started ? pacingms : 0 ;
}

void RatePacer::finished(bool hadRateLimit)
{
    recent.append(hadRateLimit) ;
    while (recent.size() > PACER_WINDOW) recent.removeFirst() ;

    if (!started) {
        int limited = 0 ;
        for (bool b : recent) {
            if (b) limited++ ;
        }
        if (limited >= PACER_LIMIT) {
            started = true ;
            pacingms = PACER_MAX_MS ;
            sincestart = 0 ;
            notes << QString("Google is limiting the rate of requests: adaptive pacing started, %1 ms before each upload.").arg(pacingms) ;
        }
        return ;
    }

    sincestart++ ;
    if (sincestart >= PACER_HALVE_EVERY) {
        sincestart = 0 ;
        if (pacingms > PACER_MIN_MS) {
            pacingms = qMax(PACER_MIN_MS, pacingms / 2) ;
            notes << QString("Google is still limiting the rate of requests: adaptive pacing halved to %1 ms before each upload.").arg(pacingms) ;
        }
    }
}

QStringList RatePacer::takeNotes()
{
    QStringList out = notes ;
    notes.clear() ;
    return out ;
}
