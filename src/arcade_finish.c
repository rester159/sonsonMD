#include "arcade_finish.h"
void arcade_finish_step(ArcadeFinish *s,u8 clock,ArcadeFinishEvent *event)
{
    /* A807 executes before the final timer decrement, including expiry. */
    event->write=(clock&15)==0;
    event->pattern=(clock&16)?0:1;
    event->results=0;
    if(!(clock&63) && !--s->timer)event->results=1;
    /* Results requests clear A5, set08=4 and clear0B/0E in the source owner.
     * Do not synthesize a scroll release or ordinary section-clear award. */
}
