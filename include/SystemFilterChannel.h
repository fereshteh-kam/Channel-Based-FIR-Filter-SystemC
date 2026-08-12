#include <systemc.h>
#include <iostream>
        
#include "Memory.h"
#include "FIRAccelerator.h"
#include "DMA_FIR.cpp"
#include "ProcessorFilter.h"

#include "busChannel.h"
#include "Transactor.h"
#include "SlaveTransactor.h"

template <int N>
SC_MODULE(EmbeddedSystemChannel)
{
public:
    sc_in<sc_logic> clk;

    busChannel      *bus;
    Memory<N>       *memory;
    FIRAccelerator  *fir;
    DMA_FIR         *dma;
    Processor<N>    *cpu;

    Transactor      *cpuTransactor;
    Transactor      *dmaMasterTransactor;
    SlaveTransactor *memTransactor;
    SlaveTransactor *firTransactor;
    SlaveTransactor *dmaSlaveTransactor;

    
    
    sc_signal<sc_lv<N>> i_addr[2];
    sc_signal<sc_lv<N>> i_in[2];
    sc_signal<sc_lv<N>> i_out[2];
    sc_signal<sc_logic> i_wr[2];
    sc_signal<sc_logic> i_rd[2];
    sc_signal<sc_logic> i_ready[2];

    
    
    sc_signal<sc_logic> t_cs[3];
    sc_signal<sc_lv<N>> t_addr[3];
    sc_signal<sc_lv<N>> t_in[3];
    sc_signal<sc_lv<N>> t_out[3];
    sc_signal<sc_logic> t_wr[3];
    sc_signal<sc_logic> t_rd[3];
    sc_signal<sc_logic> t_ready[3];

    
    
    sc_fifo<sc_lv<16>> dmaToFir;
    sc_fifo<sc_lv<16>> firToDma;

    sc_signal<sc_logic> dmaInterruptSig;
    sc_signal<sc_logic> firInterruptSig;

    SC_CTOR(EmbeddedSystemChannel) : dmaToFir(512), firToDma(512)
    {
        bus = new busChannel("bus_channel");

        
        cpu = new Processor<N>("CPU");
        cpu->clk(clk);
        cpu->i_addr(i_addr[0]); cpu->i_in(i_in[0]); cpu->i_out(i_out[0]);
        cpu->i_wr(i_wr[0]); cpu->i_rd(i_rd[0]); cpu->i_ready(i_ready[0]);
        cpu->dmaInterrupt(dmaInterruptSig);
        cpu->firInterrupt(firInterruptSig);

        cpuTransactor = new Transactor("cpuTransactor");
        cpuTransactor->bus(*bus);
        cpuTransactor->addrBus(i_addr[0]);
        cpuTransactor->dataBusOut(i_out[0]);
        cpuTransactor->dataBus(i_in[0]);
        cpuTransactor->readMem(i_rd[0]);
        cpuTransactor->writeMem(i_wr[0]);
        cpuTransactor->memReady(i_ready[0]);
        cpuTransactor->masterID = 0;

        
        memory = new Memory<N>("mem");
        memory->clk(clk);
        memory->t_cs(t_cs[0]); memory->t_addr(t_addr[0]); memory->t_in(t_in[0]);
        memory->t_out(t_out[0]); memory->t_wr(t_wr[0]); memory->t_rd(t_rd[0]);
        memory->t_ready(t_ready[0]);

        memTransactor = new SlaveTransactor("memTransactor");
        memTransactor->bus_port(*bus);
        memTransactor->cs(t_cs[0]);
        memTransactor->addrBus(t_addr[0]);
        memTransactor->writeData(t_in[0]);
        memTransactor->readMem(t_rd[0]);
        memTransactor->writeMem(t_wr[0]);
        memTransactor->dataBus(t_out[0]);
        memTransactor->memReady(t_ready[0]);
        memTransactor->slaveID = 0;

        
        fir = new FIRAccelerator("fir");
        fir->clk(clk);
        fir->t_cs(t_cs[1]); fir->t_addr(t_addr[1]); fir->t_in(t_in[1]);
        fir->t_out(t_out[1]); fir->t_wr(t_wr[1]); fir->t_rd(t_rd[1]);
        fir->t_ready(t_ready[1]);
        fir->in_fifo(dmaToFir); fir->out_fifo(firToDma);
        fir->interrupt(firInterruptSig);

        firTransactor = new SlaveTransactor("firTransactor");
        firTransactor->bus_port(*bus);
        firTransactor->cs(t_cs[1]);
        firTransactor->addrBus(t_addr[1]);
        firTransactor->writeData(t_in[1]);
        firTransactor->readMem(t_rd[1]);
        firTransactor->writeMem(t_wr[1]);
        firTransactor->dataBus(t_out[1]);
        firTransactor->memReady(t_ready[1]);
        firTransactor->slaveID = 1;

        
        dma = new DMA_FIR("dma");
        dma->clk(clk);
        dma->t_cs(t_cs[2]); dma->t_addr(t_addr[2]); dma->t_in(t_in[2]);
        dma->t_out(t_out[2]); dma->t_wr(t_wr[2]); dma->t_rd(t_rd[2]);
        dma->t_ready(t_ready[2]);
        dma->i_addr(i_addr[1]); dma->i_in(i_in[1]); dma->i_out(i_out[1]);
        dma->i_wr(i_wr[1]); dma->i_rd(i_rd[1]); dma->i_ready(i_ready[1]);
        dma->accelOut(dmaToFir); dma->accelIn(firToDma);
        dma->interrupt(dmaInterruptSig);

        dmaSlaveTransactor = new SlaveTransactor("dmaSlaveTransactor");
        dmaSlaveTransactor->bus_port(*bus);
        dmaSlaveTransactor->cs(t_cs[2]);
        dmaSlaveTransactor->addrBus(t_addr[2]);
        dmaSlaveTransactor->writeData(t_in[2]);
        dmaSlaveTransactor->readMem(t_rd[2]);
        dmaSlaveTransactor->writeMem(t_wr[2]);
        dmaSlaveTransactor->dataBus(t_out[2]);
        dmaSlaveTransactor->memReady(t_ready[2]);
        dmaSlaveTransactor->slaveID = 2;

        dmaMasterTransactor = new Transactor("dmaMasterTransactor");
        dmaMasterTransactor->bus(*bus);
        dmaMasterTransactor->addrBus(i_addr[1]);
        dmaMasterTransactor->dataBusOut(i_out[1]);
        dmaMasterTransactor->dataBus(i_in[1]);
        dmaMasterTransactor->readMem(i_rd[1]);
        dmaMasterTransactor->writeMem(i_wr[1]);
        dmaMasterTransactor->memReady(i_ready[1]);
        dmaMasterTransactor->masterID = 1;
    }
};
