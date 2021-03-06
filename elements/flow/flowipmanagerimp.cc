/*
 * flowipmanagerimp.{cc,hh}
 */

#include <click/config.h>
#include <click/glue.hh>
#include <click/args.hh>
#include <click/ipflowid.hh>
#include <click/routervisitor.hh>
#include <click/error.hh>
#include "flowipmanagerimp.hh"
#include <rte_hash.h>
#include <click/dpdk_glue.hh>
#include <rte_ethdev.h>

CLICK_DECLS

FlowIPManagerIMP::FlowIPManagerIMP() : _verbose(1), _flags(0), _timer(this), _task(this), _tables(0) {
}

FlowIPManagerIMP::~FlowIPManagerIMP()
{
}

int
FlowIPManagerIMP::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
        .read_or_set_p("CAPACITY", _table_size, 65536)
        .read_or_set("RESERVE",_reserve, 0)
        .read_or_set("TIMEOUT", _timeout, -1)
        .complete() < 0)
        return -1;

    if (_timeout > 0) {
        return errh->error("Timeout unsupported!");
    }

    errh->warning("This element does not support timeout");

    find_children(_verbose);

    router()->get_root_init_future()->postOnce(&_fcb_builded_init_future);

    _fcb_builded_init_future.post(this);

    return 0;
}

int FlowIPManagerIMP::solve_initialize(ErrorHandler *errh)
{
    struct rte_hash_parameters hash_params = {0};
    char buf[32];
    hash_params.name = buf;
    auto passing = get_passing_threads();
    _table_size = next_pow2(_table_size/passing.weight());
    click_chatter("Real capacity for each table will be %d", _table_size);
    hash_params.entries = _table_size;
    hash_params.key_len = sizeof(IPFlow5ID);
    hash_params.hash_func = ipv4_hash_crc;
    hash_params.hash_func_init_val = 0;
    hash_params.extra_flag = _flags;

    _flow_state_size_full = sizeof(FlowControlBlock) + _reserve;

    _tables = CLICK_ALIGNED_NEW(gtable, passing.size());
    CLICK_ASSERT_ALIGNED(_tables);

    for (int i = 0; i < passing.size(); i++) {
        if (!passing[i])
            continue;
        sprintf(buf, "flowipmanager%d", i);
        _tables[i].hash = rte_hash_create(&hash_params);
        if (!_tables[i].hash)
            return errh->error("Could not init flow table %d!", i);

        _tables[i].fcbs =  (FlowControlBlock*)CLICK_ALIGNED_ALLOC(_flow_state_size_full * _table_size);
        CLICK_ASSERT_ALIGNED(_tables[i].fcbs);
        bzero(_tables[i].fcbs,_flow_state_size_full * _table_size);
        if (!_tables[i].fcbs)
            return errh->error("Could not init data table %d!", i);
    }

    if (_timeout > 0) {
        _timer_wheel.initialize(_timeout);
    }

    /*
    _timer.initialize(this);
    _timer.schedule_after(Timestamp::make_sec(1));*/
    _task.initialize(this, false);

    return 0;
}


const auto setter = [](FlowControlBlock* prev, FlowControlBlock* next)
{
        *((FlowControlBlock**)&prev->data_32[2]) = next;
};

bool FlowIPManagerIMP::run_task(Task* t)
{
    /*
     Not working : the timerwheel must be per-thread too
     Timestamp recent = Timestamp::recent_steady();
    _timer_wheel.run_timers([this,recent](FlowControlBlock* prev) -> FlowControlBlock*{
        FlowControlBlock* next = *((FlowControlBlock**)&prev->data_32[2]);
        int old = (recent - prev->lastseen).sec();
        if (old > _timeout) {
            //click_chatter("Release %p as it is expired since %d", prev, old);
            //expire
            rte_hash_free_key_with_position(vhash[click_current_cpu_id()], prev->data_32[0]);//depreciated
        } else {
            //click_chatter("Cascade %p", prev);
            //No need for lock as we'll be the only one to enqueue there
            _timer_wheel.schedule_after(prev, _timeout - (recent - prev->lastseen).sec(),setter);
        }
        return next;
    });
    return true;*/
}

void FlowIPManagerIMP::run_timer(Timer* t)
{
    //_task.reschedule();
   // t->reschedule_after(Timestamp::make_sec(1));
}

void FlowIPManagerIMP::cleanup(CleanupStage stage)
{
    click_chatter("Cleanup the table");
    for(int i =0; i<click_max_cpu_ids(); i++) {
       if (_tables[i].hash)
           rte_hash_free(_tables[i].hash);

       if (_tables[i].fcbs)
            delete _tables[i].fcbs;
    }

    delete _tables;
}

void FlowIPManagerIMP::process(Packet* p, BatchBuilder& b, const Timestamp& recent)
{
    IPFlow5ID fid = IPFlow5ID(p);

    auto& tab = _tables[click_current_cpu_id()];
    rte_hash* table = tab.hash;

    FlowControlBlock* fcb;

    int ret = rte_hash_lookup(table, &fid);
    if (ret < 0) { //new flow
        ret = rte_hash_add_key(table, &fid);
        if (ret < 0) {
                    if (unlikely(_verbose > 0)) {
                        click_chatter("Cannot add key (have %d items. Error %d)!", rte_hash_count(table), ret);
            }
            p->kill();
            return;
        }
        fcb = (FlowControlBlock*)((unsigned char*)tab.fcbs + (_flow_state_size_full * ret));
        fcb->data_32[0] = ret;
        if (_timeout > 0) {
            if (_flags) {
                _timer_wheel.schedule_after_mp(fcb, _timeout, setter);
            } else {
                _timer_wheel.schedule_after(fcb, _timeout, setter);
            }
        }
    } else {
        fcb = (FlowControlBlock*)((unsigned char*)tab.fcbs + (_flow_state_size_full * ret));
    }

    if (b.last == ret) {
        b.append(p);
    } else {
        PacketBatch* batch;
        batch = b.finish();
        if (batch) {
            fcb_stack->lastseen = recent;
            output_push_batch(0, batch);
        }
        fcb_stack = fcb;
        b.init();
        b.append(p);
    }
}

void FlowIPManagerIMP::push_batch(int, PacketBatch* batch)
{
    BatchBuilder b;
    Timestamp recent = Timestamp::recent_steady();
    FOR_EACH_PACKET_SAFE(batch, p) {
        process(p, b, recent);
    }

    batch = b.finish();
    if (batch) {
        fcb_stack->lastseen = recent;
        output_push_batch(0, batch);
    }
}

enum {h_count};
String FlowIPManagerIMP::read_handler(Element* e, void* thunk)
{
    FlowIPManagerIMP* fc = static_cast<FlowIPManagerIMP*>(e);
    click_chatter("ENTERED in the read_handler function");
    rte_hash* table = fc->_tables[click_current_cpu_id()].hash;
    switch ((intptr_t)thunk) {
    case h_count:
        return String(rte_hash_count(table));
    default:
        return "<error>";
    }
};

void FlowIPManagerIMP::add_handlers()
{

}

CLICK_ENDDECLS

ELEMENT_REQUIRES(dpdk dpdk19)
EXPORT_ELEMENT(FlowIPManagerIMP)
ELEMENT_MT_SAFE(FlowIPManagerIMP)
