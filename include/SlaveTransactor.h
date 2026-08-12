#pragma once

#include <systemc.h>
#include "interfaceClasses.h"

SC_MODULE(SlaveTransactor) {
    sc_port<slaveSide_if<16>> bus_port;

    sc_out<sc_logic> readMem, writeMem, cs;
    sc_in<sc_logic> memReady;
    sc_out<sc_lv<16>> addrBus, writeData;
    sc_in<sc_lv<16>> dataBus;

    sc_lv<16> addr, data;
    sc_logic r_en, w_en;

    sc_event slave_done;

    int slaveID;

    SC_CTOR(SlaveTransactor) {
        slaveID = 0;
        SC_THREAD(slave_process);
        SC_METHOD(on_mem_ready);
        sensitive << memReady;
        dont_initialize();
    }

    void slave_process();
    void on_mem_ready();
};
