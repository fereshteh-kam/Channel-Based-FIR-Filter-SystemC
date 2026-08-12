#include <systemc.h>
#include <iostream>
#include <fstream>
#include <cstdint>
#include "SystemFilterChannel.h"
#include "Config.h"

SC_MODULE(ClockGen)
{
    sc_out<sc_logic> clk;
    void gen()
    {
        clk = sc_logic_0;
        while (true) { wait(5, SC_NS); clk = ~clk.read(); }
    }
    SC_CTOR(ClockGen) { SC_THREAD(gen); }
};

static const char *COEFF_FILE = "data/coefficients.txt";
static const char *AUDIO_FILE = "data/noisy.txt";

void generateMemoryInitFiles()
{
    std::ifstream coefFile(COEFF_FILE);
    if (!coefFile) { std::cerr << "[filter-channel] ERROR: could not open " << COEFF_FILE << std::endl; exit(1); }
    std::ifstream audioFile(AUDIO_FILE);
    if (!audioFile) { std::cerr << "[filter-channel] ERROR: could not open " << AUDIO_FILE << std::endl; exit(1); }

    std::ofstream addrFile("addr.txt");
    std::ofstream dataFile("data.txt");

    int addr = COEFF_BASE, val, count = 0;
    while (count < FIR_TAPS && (coefFile >> val)) { addrFile << addr << "\n"; dataFile << val << "\n"; addr++; count++; }
    if (count != FIR_TAPS)
        std::cerr << "[filter-channel] WARNING: coefficients.txt has " << count << " values, expected " << FIR_TAPS << std::endl;

    addr = AUDIO_BASE; count = 0;
    while (count < NUM_SAMPLES && (audioFile >> val)) { addrFile << addr << "\n"; dataFile << val << "\n"; addr++; count++; }
    while (count < NUM_SAMPLES) { addrFile << addr << "\n"; dataFile << 0 << "\n"; addr++; count++; }

    std::cout << "[filter-channel] Wrote " << FIR_TAPS << " coefficients + " << NUM_SAMPLES << " audio samples." << std::endl;
    addrFile.close();
    dataFile.close();
}

void extractAudio(Memory<16> *mem, int base, int count, const char *filename)
{
    std::ofstream out(filename);
    for (int i = 0; i < count; i++)
    {
        int16_t sample = (int16_t)(uint16_t)mem->mem[base + i].to_uint();
        out << sample << "\n";
    }

    out.close();
    std::cout << "[filter-channel] Wrote " << count << " samples to " << filename << std::endl;
}

int sc_main(int argc, char *argv[])
{
    generateMemoryInitFiles();

    sc_signal<sc_logic> clkSig;
    ClockGen clkgen("clkgen");
    clkgen.clk(clkSig);

    EmbeddedSystemChannel<16> sys("sys");
    sys.clk(clkSig);

    sys.memory->Loading = 0;
    sys.memory->PuttingData = 1;
    sys.memory->DebugON = 1;

    std::cout << "[filter-channel] Starting simulation..." << std::endl;
    sc_start();
    std::cout << "[filter-channel] Simulation finished at " << sc_time_stamp() << std::endl;

    extractAudio(sys.memory, AUDIO_BASE, NUM_SAMPLES, "data/clean.txt");

    std::cout << "[filter-channel] Done." << std::endl;
    return 0;
}
