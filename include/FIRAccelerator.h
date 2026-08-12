#include <systemc.h>
#include <vector>
#include <cstdint>
// #include "Bus.h"   

#ifndef FIR_TAPS
#define FIR_TAPS 31
#endif

#ifndef FIR_CHUNK_SIZE
#define FIR_CHUNK_SIZE 512
#endif

SC_MODULE(FIRAccelerator)
{
    static const int N      = 16;
    static const int TAPS   = FIR_TAPS;
    static const int CHUNK  = FIR_CHUNK_SIZE;

    
    sc_in<sc_logic> clk;
    sc_in<sc_logic>      t_cs;
    sc_in<sc_lv<16>>     t_addr;
    sc_in<sc_lv<16>>     t_in;
    sc_out<sc_lv<16>>    t_out;
    sc_in<sc_logic>      t_wr;
    sc_in<sc_logic>      t_rd;
    sc_out<sc_logic>     t_ready;

    
    sc_fifo_in<sc_lv<16>>   in_fifo;    
    sc_fifo_out<sc_lv<16>>  out_fifo;   
    sc_out<sc_logic> interrupt;

    sc_lv<16> coeffReg[TAPS];
    sc_lv<16> controlReg;
    sc_lv<16> statusReg;
    bool coeffsValid;

    std::vector<int16_t> history; 

    SC_CTOR(FIRAccelerator) : coeffsValid(false), history(TAPS - 1, 0)
    {
        for (int i = 0; i < TAPS; i++) coeffReg[i] = 0;
        controlReg = 0;
        statusReg  = 0;

        SC_THREAD(evalConfig);
        sensitive << clk;
        SC_THREAD(evalProcessing);
        sensitive << clk;
    }

    void evalConfig()
    {
        t_ready = sc_logic_1;
        t_out   = 0;

        while (true)
        {
            do
            {
                wait(clk->posedge_event());
            } while (t_cs != '1');
            
            t_ready = sc_logic_0;
            t_out   = 0;

            if (t_wr == '1')
            {
                wait(clk->posedge_event());
                unsigned addr = t_addr.read().to_uint();
                if (addr < (unsigned)TAPS)
                {
                    coeffReg[addr] = t_in.read();
                }
                else if (addr == (unsigned)TAPS) 
                {
                    controlReg = t_in.read();
                    // std::cout << "[FIRAccelerator] Config Write! Address=" << addr 
                    // << " Data=" << controlReg.to_uint() 
                    // << " StartBit=" << controlReg[0] << std::endl;

                    if (controlReg[0] == '1')
                    {
                        coeffsValid = true;                      
                    }
                }
            }
            else if (t_rd == '1')
            {
                unsigned addr = t_addr.read().to_uint();
                if (addr < (unsigned)TAPS)
                {
                    t_out = coeffReg[addr];
                }
                else if (addr == (unsigned)TAPS)
                {
                    t_out = controlReg;
                }
                else if (addr == (unsigned)TAPS + 1)
                {
                    t_out = statusReg;
                        
                    if (statusReg[0] == '1')
                    {
                        statusReg[0] = sc_logic_0;
                        interrupt    = sc_logic_0;
                    }
                }
            }

            t_ready = sc_logic_1;
        }
    }

    
    static int16_t toSigned(const sc_lv<16> &v)
    {
        return (int16_t)(uint16_t)v.to_uint();
    }
    static sc_lv<16> fromSigned(int16_t v)
    {
        return sc_lv<16>((uint16_t)v);
    }

    void evalProcessing()
    {
        
        cout << "[FIRAccelerator] eval processing started"<< endl;

        interrupt = sc_logic_0;
        statusReg[0] = sc_logic_0;
        
        while (!coeffsValid)
            wait(clk->posedge_event());
        
        // cout << "[FIRAccelerator] coeffs valid check"<< endl;

        while (true)
        {
            
            std::vector<int16_t> chunk(CHUNK);
            for (int i = 0; i < CHUNK; i++)
            {
                sc_lv<16> v = in_fifo.read();   
                if (in_fifo.num_available() > 0) {
                    // std::cout << "[FIRAccelerator] FIFO has data! Count=" << in_fifo.num_available() << std::endl;
                } else {
                    
                    // std::cout << "[FIRAccelerator] FIFO is empty, waiting..." << std::endl; 
                }
                chunk[i] = toSigned(v);
            }

            
            std::vector<int16_t> ext(TAPS - 1 + CHUNK);
            for (int i = 0; i < TAPS - 1; i++) ext[i] = history[i];
            for (int i = 0; i < CHUNK; i++)    ext[TAPS - 1 + i] = chunk[i];

            
            std::vector<int16_t> outChunk(CHUNK);
            for (int n = 0; n < CHUNK; n++)
            {
                int idx = n + (TAPS - 1); 
                int64_t acc = 0;
                for (int k = 0; k < TAPS; k++)
                {
                    acc += (int32_t)ext[idx - k] * (int32_t)toSigned(coeffReg[k]);
                }
                // if (n < 10) {
                //     std::cout << "FIR i=" << n
                //             << " sum=" << acc
                //             << " out=" << (acc >> 15)
                //             << std::endl;
                // }

                acc >>= 15;
                if (acc > 32767)  acc = 32767;
                if (acc < -32768) acc = -32768;
                outChunk[n] = (int16_t)acc;
            }

            
            for (int i = 0; i < TAPS - 1; i++)
                history[i] = ext[CHUNK + i]; 

            
            for (int i = 0; i < CHUNK; i++)
                out_fifo.write(fromSigned(outChunk[i]));

            statusReg[0] = sc_logic_1; 
            interrupt = sc_logic_1;

            cout << "[FIRAccelerator] chunk processed and interrupt issued @ "
                 << sc_time_stamp() << endl;
        }
    }
};
