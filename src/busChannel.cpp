#include "busChannel.h"

int busChannel::getPriorityMaster() {
    if (requesting[0]) return 0;
    if (requesting[1]) return 1;
    return -1;
}

void busChannel::masterMMreq(sc_lv<16> addr, sc_lv<16> writeData, sc_lv<16>& readData, int masterID,
    sc_logic readMem, sc_logic writeMem) {
    // cout << "[bus] Master " << masterID
    //  << " entered masterMMreq" << endl;
    requesting[masterID] = true;

    while (getPriorityMaster() != masterID) {
        // cout << "[bus] waiting slave response..." << endl;
        wait(slaveOprCompleted);
        // cout << "[bus] slave responded" << endl;
    }

    busBusy.lock();
    requesting[masterID] = false;

    addrOut = addr;
    writeDataOut = writeData;
    readMemOut = readMem;
    writeMemOut = writeMem;
    masterIDOut = masterID;

    goingToSlave = decodeSlave(addr.to_uint());

    if (goingToSlave >= 0 && goingToSlave < NUM_SLAVES) {
        // std::cout << "[busChannel] master " << masterID << " -> slave " << goingToSlave
        //           << " addr=" << addr << std::endl;
        requestMM[goingToSlave].notify(SC_ZERO_TIME);

        wait(slaveOprCompleted);
        readData = readDataIn;
    }
    else {
        std::cout << "[busChannel] ERROR: address " << addr << " does not map to any slave!" << std::endl;
    }

    busBusy.unlock();
}


void busChannel::slaveMMcollection(int slaveID,
    sc_lv<16>& addr,
    sc_lv<16>& data,
    sc_logic& readEnable,
    sc_logic& writeEnable) {
    if (slaveID != goingToSlave) {
        wait(requestMM[slaveID]);
    }
    addr = addrOut;
    data = writeDataOut;
    readEnable = readMemOut;
    writeEnable = writeMemOut;
    goingToSlave = -1;
}

int busChannel::decodeSlave(sc_uint<16> addrInt) {
    const unsigned MEM_BASE = 0x0000, MEM_END = 0x7FFF;
    const unsigned FIR_BASE_ADDR = 0x8000, FIR_END = 0x803F;
    const unsigned DMA_BASE_ADDR = 0x8040, DMA_END = 0x8047;

    unsigned a = addrInt.to_uint();

    if (a >= MEM_BASE && a <= MEM_END) {
        return 0; 
    }
    else if (a >= FIR_BASE_ADDR && a <= FIR_END) {
        return 1; 
    }
    else if (a >= DMA_BASE_ADDR && a <= DMA_END) {
        return 2; 
    }

    std::cout << "[decodeSlave] address 0x" << std::hex << a
              << " does not match any target!" << std::dec << std::endl;
    return -1;
}

void busChannel::slaveMMresponse(sc_lv<16>& readData) {
    readDataIn = readData;
    slaveOprCompleted.notify(SC_ZERO_TIME);
}
