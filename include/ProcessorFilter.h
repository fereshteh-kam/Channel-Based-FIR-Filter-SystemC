#ifndef PROCESSOR_FILTER_H
#define PROCESSOR_FILTER_H

#include <systemc.h>
#include <iostream>
#include <cstdint>
// #include "Bus.h"
#include "Config.h"

#define FIR_BASE   0x8000
#define DMA_BASE   0x8040

#define DMA_CTRL    0
#define DMA_FROM    1
#define DMA_TO      2
#define DMA_COUNT   3
#define DMA_STATUS  4

#define DMA_ENABLE        0b001
#define DMA_DIR_MEM2ACCEL 0b010
#define DMA_DIR_ACCEL2MEM 0b100

template <int N>
SC_MODULE(Processor)
{
    sc_in<sc_logic> clk;

    sc_out<sc_lv<16>> i_addr;
    sc_in<sc_lv<16>>  i_in;
    sc_out<sc_lv<16>> i_out;
    sc_out<sc_logic>  i_wr;
    sc_out<sc_logic>  i_rd;
    sc_in<sc_logic>   i_ready;

    sc_in<sc_logic> dmaInterrupt;
    sc_in<sc_logic> firInterrupt;

    SC_CTOR(Processor)
    {
        SC_THREAD(doCPUStuff);
        sensitive << clk;
    }

    void writeToBus(uint16_t address, uint16_t value)
    {
        cout<<"write begin"<<endl;

        i_addr = address;
        i_out  = value;
        i_wr   = sc_logic_1;
        // cout<<"wr=1"<<endl;
        i_rd   = sc_logic_0;

        wait(clk->posedge_event());
        // cout<<"after first wait"<<endl;
        do {
            wait(clk->posedge_event());
        } while (i_ready.read() != sc_logic_1);
        // cout<<"ready received"<<endl;
        i_wr   = sc_logic_0;
        i_rd   = sc_logic_0;
        i_out  = 0;
        i_addr = 0;
        
        wait(SC_ZERO_TIME);
        // std::cout << "[" << sc_time_stamp() << "] [CPU] write released"
        //   << " wr=" << i_wr.read()
        //   << " rd=" << i_rd.read()
        //   << " ready=" << i_ready.read()
        //   << std::endl;

    }

    uint16_t readFromBus(uint16_t address)
    {
        i_addr = address;
        i_rd   = sc_logic_1;
        i_wr   = sc_logic_0;

        wait(clk->posedge_event());
        do {
            wait(clk->posedge_event());
        } while (i_ready.read() != sc_logic_1);

        uint16_t result = i_in.read().to_uint();

        i_rd   = sc_logic_0;
        i_wr   = sc_logic_0;
        i_addr = 0;

        wait(SC_ZERO_TIME);

        return result;
    }

    void waitForInterrupt(sc_in<sc_logic> &line)
    {
        if (line.read() != sc_logic_1)
            wait(line.posedge_event());
    }

    void loadCoefficients()
    {
        for (int i = 0; i < FIR_TAPS; i++)
        {
            uint16_t coeff = readFromBus(COEFF_BASE + i);

            // cout << "[CPU READ coeff[" << i << "]] addr="
            //     << (COEFF_BASE + i)
            //     << " value=" << coeff << endl;

            writeToBus(FIR_BASE + i, coeff);
        }

        writeToBus(FIR_BASE + FIR_TAPS, 0x0001);
        uint16_t ctrl_read = readFromBus(FIR_BASE + FIR_TAPS);
        std::cout << "[DEBUG] FIR Control Readback: " << ctrl_read << std::endl; 

        // std::cout << "[Filter] FIR initialized." << std::endl;
    }   
    
    

    void processChunk(uint16_t addr, uint16_t count)
{
    cout << "CPU: set FROM for input" << endl;
    writeToBus(DMA_BASE + DMA_FROM, addr);

    cout << "CPU: set TO for input" << endl;
    writeToBus(DMA_BASE + DMA_TO, 0);

    cout << "CPU: set COUNT for input" << endl;
    writeToBus(DMA_BASE + DMA_COUNT, count);

    cout << "CPU: START DMA MEM->ACCEL" << endl;

    writeToBus(DMA_BASE + DMA_CTRL, DMA_ENABLE | DMA_DIR_MEM2ACCEL);

    cout << "CPU: waiting input DMA interrupt" << endl;
    waitForInterrupt(dmaInterrupt);
    cout << "CPU: input DMA done" << endl;
    
    writeToBus(DMA_BASE + DMA_STATUS, 0x8000);

    cout << "CPU: START DMA ACCEL->MEM" << endl;
    writeToBus(DMA_BASE + DMA_FROM, 0);
    writeToBus(DMA_BASE + DMA_TO, addr);
    writeToBus(DMA_BASE + DMA_COUNT, count);
    writeToBus(DMA_BASE + DMA_CTRL, DMA_ENABLE | DMA_DIR_ACCEL2MEM);

    cout << "CPU: waiting FIR interrupt" << endl;
    waitForInterrupt(firInterrupt);
    cout << "CPU: FIR interrupt received" << endl;

    readFromBus(FIR_BASE + FIR_TAPS + 1); 

    cout << "CPU: waiting output DMA interrupt" << endl;
    waitForInterrupt(dmaInterrupt);
    cout << "CPU: output DMA done" << endl;
    writeToBus(DMA_BASE + DMA_STATUS, 0x8000);

}
    void doCPUStuff()
    {
        i_rd   = sc_logic_0;
        i_wr   = sc_logic_0;
        i_out  = 0;
        i_addr = 0;

        for (int i = 0; i < 5; i++)
            wait(clk->posedge_event());

        loadCoefficients();

        // std::cout << "[Filter] Filtering " << NUM_SAMPLES
        //           << " samples in chunks of " << FIR_CHUNK
        //           << " (overwriting in place)..." << std::endl;

        for (int offset = 0; offset < NUM_SAMPLES; offset += FIR_CHUNK)
        {
            uint16_t count = FIR_CHUNK;

            if (offset + FIR_CHUNK > NUM_SAMPLES)
                count = NUM_SAMPLES - offset;

            processChunk(AUDIO_BASE + offset, count);
        }

        std::cout << "[Filter] Done. Clean audio is now in place at AUDIO_BASE." << std::endl;

        wait(clk->posedge_event());
        sc_stop();
    }
};

#endif
