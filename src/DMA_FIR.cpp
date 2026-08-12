#include <systemc.h>
#include <iostream>
// #include "Bus.h"

SC_MODULE(DMA_FIR)
{
    sc_lv<16> tempReg;

    sc_lv<16> fromAddress; 
    sc_lv<16> byteCount;   
    sc_lv<16> toAddress;   
    sc_lv<16> controlReg;  
    sc_lv<16> statusReg;   
    bool      transferBusy = false;
    sc_in<sc_logic> clk;

    
    sc_out<sc_lv<16>> i_addr;
    sc_in<sc_lv<16>>  i_in;
    sc_out<sc_lv<16>> i_out;
    sc_out<sc_logic>  i_wr;
    sc_out<sc_logic>  i_rd;
    sc_in<sc_logic>   i_ready;

    
    sc_in<sc_logic>   t_cs;
    sc_in<sc_lv<16>>  t_addr;
    sc_in<sc_lv<16>>  t_in;
    sc_out<sc_lv<16>> t_out;
    sc_in<sc_logic>   t_wr;
    sc_in<sc_logic>   t_rd;
    sc_out<sc_logic>  t_ready;

    
    sc_fifo_out<sc_lv<16>> accelOut; 
    sc_fifo_in<sc_lv<16>>  accelIn;  

    
    sc_out<sc_logic> interrupt;

    SC_CTOR(DMA_FIR)
    {
        SC_THREAD(eval);
        sensitive << clk;
        SC_THREAD(evalConf);
        sensitive << clk;
    }


    void evalConf()
    {
        while (true)
        {
            t_ready = sc_logic_1;
            do
            {
                wait(clk->posedge_event());
            } while (t_cs != '1');
            // cout << "[DMA evalConf] t_cs active"
            //     << " addr=" << t_addr.read().to_uint()
            //     << " wr=" << t_wr.read()
            //     << endl;
            t_ready = sc_logic_0;
            t_out = 0;
            if (t_wr == '1')
            {   
                unsigned waddr = t_addr.read().to_uint();

                if (transferBusy && waddr <= 3)     
                {
                    cout << "[DMA] WARNING: config write to addr=" << waddr
                        << " ignored, transfer still in progress!" << endl;
                }
                else
                {
                    // cout << "[DMA CONFIG] addr=" << waddr
                    //     << " value=" << t_in.read().to_uint() << endl;
                    wait(clk->posedge_event());

                    if (waddr == 0)      { controlReg  = t_in; }
                    else if (waddr == 1) { fromAddress = t_in; }
                    else if (waddr == 2) { toAddress   = t_in; }
                    else if (waddr == 3) { byteCount   = t_in; }
                    else if (waddr == 4) { statusReg   = t_in; }
                }
            }
            else if (t_rd == '1')
            {
                if (t_addr.read().to_uint() == 0)      t_out = controlReg;
                else if (t_addr.read().to_uint() == 1) t_out = fromAddress;
                else if (t_addr.read().to_uint() == 2) t_out = toAddress;
                else if (t_addr.read().to_uint() == 3) t_out = byteCount;
                else if (t_addr.read().to_uint() == 4) t_out = statusReg;
            }
            t_ready = sc_logic_1;
        }
    }

    void eval()
    {
        controlReg = 0;
        statusReg = 0;
        interrupt = sc_logic_0;
        i_addr = 0;
        i_out = 0;
        i_rd = sc_logic_0;
        i_wr = sc_logic_0;

        while (true)
        {
            
            do
            {
                wait(clk->posedge_event());
            } while (controlReg[0] == '0');
            cout << "DMA START" << endl;
            controlReg[0] = sc_logic_0;
            transferBusy = true;   
            statusReg = 0;
            interrupt = sc_logic_0;
            i_addr = 0;
            i_out = 0;
            i_rd = sc_logic_0;
            i_wr = sc_logic_0;

            for (int addrOffset = 0; addrOffset < byteCount.to_uint(); addrOffset++)
            {
                if (controlReg[1] == '1') 
                {
                    
                    // cout << "DMA READ addr="
                    // << fromAddress.to_uint() + addrOffset
                    // << endl;
                    i_addr = fromAddress.to_uint() + addrOffset;
                    i_wr = sc_logic_0;
                    i_rd = sc_logic_1;

                    wait(clk->posedge_event());
                    do
                    {
                        wait(clk->posedge_event());
                    } while (i_ready != '1');
                    // cout << "DMA MEMORY READ OK" << endl;
                    i_wr = sc_logic_0;
                    i_rd = sc_logic_0;
                    tempReg = i_in;
                    
                    // cout << "DMA FIFO WRITE" << endl;
                    accelOut.write(tempReg);                             
   
                    wait(clk->posedge_event());
                }
                else if (controlReg[2] == '1') 
                {
                    
                    tempReg = accelIn.read();  
                    i_addr = toAddress.to_uint() + addrOffset;
                    i_wr = sc_logic_1;
                    i_rd = sc_logic_0;
                    i_out = tempReg;

                    wait(clk->posedge_event());
                    do
                    {
                        wait(clk->posedge_event());
                    } while (i_ready != '1');
                    i_wr = sc_logic_0;
                    i_rd = sc_logic_0;                    
                    wait(clk->posedge_event());
                }
            }

            if (controlReg[2] == '0')
                statusReg = 1; 
            else
                statusReg = 2;
    
            interrupt = sc_logic_1;

            while (statusReg[15] == sc_logic_0) {
                wait(clk->posedge_event());
            }

            interrupt = sc_logic_0;
            transferBusy = false; 
            statusReg[15] = sc_logic_0;
        }
    }
};