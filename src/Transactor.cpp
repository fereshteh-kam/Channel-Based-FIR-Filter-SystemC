#include "Transactor.h"

void Transactor::transactor_process() {
    dataBus = 0;
    while (true) {
        wait();
        memReady.write(SC_LOGIC_0);

        sc_lv<16> addr = addrBus.read();
        sc_lv<16> writeData = dataBusOut.read();
        sc_lv<16> readValue;

        if (writeMem.read() == SC_LOGIC_1 || readMem.read() == SC_LOGIC_1) {
            bus->masterMMreq(addr, writeData, readValue,
                            masterID,
                            readMem.read(),
                            writeMem.read());

            dataBus.write(readValue);
            wait(SC_ZERO_TIME);

            memReady.write(SC_LOGIC_1);
        }
    }
}
