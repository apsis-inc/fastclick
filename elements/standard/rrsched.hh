// -*- c-basic-offset: 4 -*-
#ifndef CLICK_RRSCHED_HH
#define CLICK_RRSCHED_HH
#include <click/batchelement.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
 * =c
 * RoundRobinSched
 * =s scheduling
 * pulls from round-robin inputs
 * =io
 * one output, zero or more inputs
 * =d
 * Each time a pull comes in the output, pulls from its inputs
 * in turn until one produces a packet. When the next pull
 * comes in, it starts from the input after the one that
 * last produced a packet. This amounts to a round robin
 * scheduler.
 *
 * The inputs usually come from Queues or other pull schedulers.
 * RoundRobinSched uses notification to avoid pulling from empty inputs.
 *
 * =a PrioSched, StrideSched, DRRSched, RoundRobinSwitch, SimpleRoundRobinSched
 */

class RRSched : public BatchElement {
    public:
        RRSched() CLICK_COLD;

        const char *class_name() const  { return "RoundRobinSched"; }
        const char *port_count() const  { return "-/1"; }
        const char *processing() const  { return PULL; }
        const char *flags() const       { return "S0"; }

        int configure(Vector<String> &conf, ErrorHandler *) CLICK_COLD;
        int initialize(ErrorHandler *) CLICK_COLD;
        void cleanup(CleanupStage) CLICK_COLD;

        Packet *pull(int port);
    #if HAVE_BATCH
        PacketBatch *pull_batch(int port, unsigned max);
    #endif

    protected:
        int _next;
        NotifierSignal *_signals;
        int _max;

};

CLICK_ENDDECLS
#endif
