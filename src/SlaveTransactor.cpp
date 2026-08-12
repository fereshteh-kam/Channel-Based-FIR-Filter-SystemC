#include "SlaveTransactor.h"
#include <iostream>
#include <iomanip>

static const unsigned FIR_BASE_ADDR = 0x8000;
static const unsigned DMA_BASE_ADDR = 0x8040;

static sc_lv<16> convert_to_local_address(
    int slave_id,
    const sc_lv<16>& global_addr)
{
    const unsigned global = global_addr.to_uint();
    unsigned local = global;

    switch (slave_id)
    {
        case 0: 
            local = global;
            break;
        case 1:
            if (global >= FIR_BASE_ADDR)
                local = global - FIR_BASE_ADDR;
            break;

        case 2:
            
            if (global >= DMA_BASE_ADDR)
                local = global - DMA_BASE_ADDR;
            break;

        default:
            std::cerr << sc_time_stamp()
                      << " [SlaveTransactor " << slave_id
                      << "] WARNING: unknown slave ID; address is not localized"
                      << std::endl;
            break;
    }

    return sc_lv<16>(local);
}

void SlaveTransactor::slave_process()
{
    while (true)
    {
        bus_port->slaveMMcollection(
            slaveID,
            addr,
            data,
            r_en,
            w_en
        );

        const sc_lv<16> global_addr = addr;
        const sc_lv<16> local_addr = convert_to_local_address(slaveID, global_addr);

        // std::cout << sc_time_stamp()
        //           << " [SlaveTransactor " << slaveID
        //           << "] got request:"
        //           << " global_addr=0x"
        //           << std::hex << global_addr.to_uint()
        //           << " local_addr=0x"
        //           << local_addr.to_uint()
        //           << " data=0x"
        //           << data.to_uint()
        //           << std::dec
        //           << " r_en=" << r_en
        //           << " w_en=" << w_en
        //           << std::endl;
        
        addrBus.write(local_addr);
        writeData.write(data);
        readMem.write(r_en);
        writeMem.write(w_en);
        cs.write(SC_LOGIC_1);

        // std::cout << sc_time_stamp()
        //           << " [SlaveTransactor " << slaveID
        //           << "] signals driven:"
        //           << " addrBus=" << local_addr.to_uint()
        //           << ", waiting one delta..."
        //           << std::endl;

        
        wait(SC_ZERO_TIME);

        if (r_en == SC_LOGIC_1 && slaveID == 1 && memReady.read() == SC_LOGIC_1)
        {
            sc_lv<16> read_value = dataBus.read();

            // std::cout << sc_time_stamp()
            //         << " [SlaveTransactor " << slaveID
            //         << "] fast-path response, read_value="
            //         << read_value << std::endl;

            bus_port->slaveMMresponse(read_value);
        }
        else
        {
            // std::cout << sc_time_stamp()
            //         << " [SlaveTransactor " << slaveID
            //         << "] waiting for slave_done..."
            //         << std::endl;

            wait(slave_done);
        }


        // std::cout << sc_time_stamp()
        //           << " [SlaveTransactor " << slaveID
        //           << "] slave_done received, clearing control signals"
        //           << std::endl;

        cs.write(SC_LOGIC_0);
        readMem.write(SC_LOGIC_0);
        writeMem.write(SC_LOGIC_0);

        
        wait(SC_ZERO_TIME);
    }
}

void SlaveTransactor::on_mem_ready() {
    // std::cout << sc_time_stamp()
    //           << " [SlaveTransactor " << slaveID
    //           << "] on_mem_ready called, memReady="
    //           << memReady.read()
    //           << " dataBus=" << dataBus.read()
    //           << std::endl;

    if (memReady.read() == SC_LOGIC_1) {
        sc_lv<16> read_value = dataBus.read();
        bus_port->slaveMMresponse(read_value);
        slave_done.notify(SC_ZERO_TIME);
    }
}
