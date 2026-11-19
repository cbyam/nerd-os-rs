/* The former board logic, verbatim (extracted by a script from the commit
 * before board step B1): Board::validateVoltage/validateFrequency/
 * getMaxChipTemp and the clamping of loadSettings() (main/boards/board.cpp),
 * NerdQaxePlus::getFault() (nerdqaxeplus.cpp), on a minimal Board. */
#include <algorithm>
#include <cstdint>

#define ESP_LOGE(tag, ...) ((void) (tag))
static const char *TAG = "board";

namespace Config {
static uint16_t s_freq, s_volt;
inline uint16_t getAsicFrequency(uint16_t) { return s_freq; }
inline uint16_t getAsicVoltage(uint16_t) { return s_volt; }
} // namespace Config

struct StatusSource {
    uint8_t regs[5];
    uint8_t get_status_byte() { return regs[0]; }
    uint8_t get_status_iout() { return regs[1]; }
    uint8_t get_status_vout() { return regs[2]; }
    uint8_t get_status_input() { return regs[3]; }
    uint8_t get_status_temp() { return regs[4]; }
};

class Board {
  public:
    enum Error { NONE, TEMP_FAULT, VREG_TEMP_FAULT, PSU_FAULT, IOUT_OC_FAULT, VOUT_FAULT, COINBASE_VERIFY_FAULT };
    int m_absMaxAsicFrequency = 0, m_absMinAsicVoltageMillis = 0, m_absMaxAsicVoltageMillis = 0;
    int m_asicFrequency = 0, m_asicVoltageMillis = 0;
    int m_asicCount = 0;
    float *m_chipTemps = nullptr;
    bool validateVoltage(float core_voltage);
    bool validateFrequency(float frequency);
    float getMaxChipTemp();
    void loadClamp();
};

class NerdQaxePlus : public Board {
  public:
    StatusSource *m_tps;
    Board::Error getFault(uint32_t *status);
};

bool Board::validateVoltage(float core_voltage) {
    if (core_voltage == 0.0f) {
        return true;  // 0V = disable output
    }
    int millis = (int) (core_voltage * 1000.0f);
    // we allow m_absMaxAsicVoltageMillis = 0 for no limit to not break what was
    // working before on nerdaxe and nerdaxegamma
    if (m_absMinAsicVoltageMillis && millis < m_absMinAsicVoltageMillis) {
        ESP_LOGE(TAG, "Validation error. ASIC voltage %d is lower than absolute minimum value %d", millis, m_absMinAsicVoltageMillis);
        return false;
    }
    if (m_absMaxAsicVoltageMillis && millis > m_absMaxAsicVoltageMillis) {
        ESP_LOGE(TAG, "Validation error. ASIC voltage %d is higher than absolute maximum value %d", millis, m_absMaxAsicVoltageMillis);
        return false;
    }
    return true;
}

bool Board::validateFrequency(float frequency) {
    // we allow m_absMaxAsicFrequency = 0 for no limit to not break what was
    // working before on nerdaxe and nerdaxegamma
    if (m_absMaxAsicFrequency && frequency > (float) m_absMaxAsicFrequency) {
        ESP_LOGE(TAG, "Validation error. ASIC Frequency %.3f is higher than absolute maximum value %.3f", frequency, (float) m_absMaxAsicFrequency);
        return false;
    }
    return true;
}

float Board::getMaxChipTemp() {
    float maxTemp = 0.0f;
    for (int i=0;i<m_asicCount;i++) {
        maxTemp = std::max(maxTemp, m_chipTemps[i]);
    }
    return maxTemp;
}

void Board::loadClamp()
{
    // clamp frequency and voltage to absMax values
    if (m_absMaxAsicFrequency) {
        m_asicFrequency = std::min((int) Config::getAsicFrequency(m_asicFrequency), m_absMaxAsicFrequency);
    } else {
        m_asicFrequency = (int) Config::getAsicFrequency(m_asicFrequency);
    }

    if (m_absMaxAsicVoltageMillis) {
        m_asicVoltageMillis = std::min((int) Config::getAsicVoltage(m_asicVoltageMillis), m_absMaxAsicVoltageMillis);
    } else {
        m_asicVoltageMillis = (int) Config::getAsicVoltage(m_asicVoltageMillis);
    }
}

Board::Error NerdQaxePlus::getFault(uint32_t *status) {
    *status = 0x00000000;

    uint8_t status_byte = m_tps->get_status_byte();
    uint8_t status_iout = m_tps->get_status_iout();
    uint8_t status_vout = m_tps->get_status_vout();
    uint8_t status_input = m_tps->get_status_input();
    uint8_t status_temp = m_tps->get_status_temp();

    *status = (static_cast<uint32_t>(status_byte) << 24) |
              (static_cast<uint32_t>(status_iout) << 16) |
              (static_cast<uint32_t>(status_vout) << 8)  |
              (static_cast<uint32_t>(status_input));

    // If +12V is missing, the PMBus device does not respond to I2C reads,
    // resulting in all bytes being 0xFF due to no ACK.
    // The combined && check ensures we only flag a PSU fault when *all*
    // reads failed, avoiding false triggers from single read errors.
    if (status_byte == 0xff &&
        status_iout == 0xff &&
        status_vout == 0xff &&
        status_temp == 0xff &&
        status_input == 0xff) {
        return Board::Error::PSU_FAULT;
    }

    // Check for output overcurrent fault flag
    // Bit 7: IOUT_OCF
    if (status_iout != 0xff && (status_iout & 0x80)) {
        return Board::Error::IOUT_OC_FAULT;
    }

    // Check for output voltage fault flags
    // Bit 7: VOUT_OVF, Bit 4: VOUT_UVF
    if (status_vout != 0xff && (status_vout & 0x90)) {
        return Board::Error::VOUT_FAULT;
    }

    // Check for overtemperature fault flag
    // Bit 7: OTF
    if (status_temp != 0xff && (status_temp & 0x80)) {
        return Board::Error::VREG_TEMP_FAULT;
    }

    // Check for PSU-level input or state faults
    // status_input: Bit 7 = VIN_OVF, Bit 4 = VIN_UVF, Bit 2 = IIN_OCF
    if (status_input != 0xff && (status_input & 0x94)) {
        return Board::Error::PSU_FAULT;
    }

    // is buck off? Then something is wrong ...
    // return general error.
    // status_byte: Bit 6 = OFF
    // update: this has wrong behaviour because on eg over temp shutdown
    // it would trigger PSU error with #40000000 what actually only says the vreg is off
    // but without any TPS error flag set.
    //if (status_byte != 0xff && (status_byte & 0x40)) {
    //    return Board::Error::PSU_FAULT;
    //}

    return Board::Error::NONE;
}

extern "C" {
bool ref_board_validate_voltage(float v, int maxf, int minv, int maxv)
{
    Board b;
    b.m_absMaxAsicFrequency = maxf;
    b.m_absMinAsicVoltageMillis = minv;
    b.m_absMaxAsicVoltageMillis = maxv;
    return b.validateVoltage(v);
}
bool ref_board_validate_frequency(float f, int maxf, int minv, int maxv)
{
    Board b;
    b.m_absMaxAsicFrequency = maxf;
    b.m_absMinAsicVoltageMillis = minv;
    b.m_absMaxAsicVoltageMillis = maxv;
    return b.validateFrequency(f);
}
void ref_board_clamp(uint16_t freq, uint16_t volt, int maxf, int maxv, int *of, int *ov)
{
    Board b;
    b.m_absMaxAsicFrequency = maxf;
    b.m_absMaxAsicVoltageMillis = maxv;
    Config::s_freq = freq;
    Config::s_volt = volt;
    b.loadClamp();
    *of = b.m_asicFrequency;
    *ov = b.m_asicVoltageMillis;
}
float ref_board_max_chip_temp(float *temps, int n)
{
    Board b;
    b.m_chipTemps = temps;
    b.m_asicCount = n;
    return b.getMaxChipTemp();
}
int ref_board_fault(const uint8_t *regs, uint32_t *status)
{
    StatusSource src;
    for (int i = 0; i < 5; i++) src.regs[i] = regs[i];
    NerdQaxePlus b;
    b.m_tps = &src;
    return (int) b.getFault(status);
}
}
