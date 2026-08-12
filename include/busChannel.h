#pragma once

#include "interfaceClasses.h"

class busChannel : public sc_channel,
    public masterSide_if<16>,
    public slaveSide_if<16>
{
public:
    static const int NUM_MASTERS = 2; 
    static const int NUM_SLAVES  = 3; 

    sc_mutex busBusy;
    int goingToSlave = -1;
    bool requesting[NUM_MASTERS] = { false, false };

    sc_lv<16> addrOut;
    sc_lv<16> writeDataOut;
    sc_lv<16> readDataIn;
    sc_logic readMemOut;
    sc_logic writeMemOut;
    int masterIDOut;

    sc_event slaveOprCompleted;
    sc_event requestMM[NUM_SLAVES];

    busChannel(sc_module_name NAME) : sc_channel(NAME) {}

    void masterMMreq(sc_lv<16> addr, sc_lv<16> writeData, sc_lv<16>& readData, int masterID,
        sc_logic readMem, sc_logic writeMem) override;

    void slaveMMcollection(int slaveID,
        sc_lv<16>& addr,
        sc_lv<16>& data,
        sc_logic& readEnable,
        sc_logic& writeEnable) override;

    void slaveMMresponse(sc_lv<16>& readData) override;

    int decodeSlave(sc_uint<16> addrInt);
    int getPriorityMaster();
};
