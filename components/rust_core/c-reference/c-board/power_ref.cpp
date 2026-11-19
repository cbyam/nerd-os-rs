/* The former power sequencing of the NerdQAxe family, verbatim (extracted by
 * a script from the commit before board step B1): NerdQaxePlus::initAsics/
 * setVoltage/shutdown (main/boards/nerdqaxeplus.cpp), the rev7 variant of
 * NerdQaxePlus2 (nerdqaxeplus2.cpp) and Board::validateVoltage (board.cpp),
 * on classes whose hardware records every call into a trace. */
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <cmath>

#define ESP_LOGE(tag, ...) ((void) (tag))
#define ESP_LOGW(tag, ...) ((void) (tag))
#define ESP_LOGI(tag, ...) ((void) (tag))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define pdMS_TO_TICKS(ms) (ms)
static const char *TAG = "ref";

static std::string g_trace;
static void rec(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#include <cstdarg>
static void rec(const char *fmt, ...)
{
    char buf[96];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    g_trace += buf;
    g_trace += "\n";
}
static uint32_t bits(float f)
{
    uint32_t b;
    memcpy(&b, &f, 4);
    return b;
}

/* failure injection */
static bool s_buck_init_ok, s_set_vout_ok, s_disable_ok;
static int s_chips, s_baud;

static void vTaskDelay(uint32_t ms) { rec("delay %u", (unsigned) ms); }
static void SERIAL_clear_buffer() { rec("clear"); }
static void SERIAL_set_baud(int b) { rec("baud %d", b); }

struct RecBuck {
    bool init(int phases, int imax, float ifault)
    {
        rec("buck_init %d %d %08x", phases, imax, (unsigned) bits(ifault));
        return s_buck_init_ok;
    }
    bool set_vout(float v)
    {
        rec("vout %08x", (unsigned) bits(v));
        return s_set_vout_ok;
    }
    bool disable_vout()
    {
        rec("disable");
        return s_disable_ok;
    }
};

struct RecAsics {
    int init(int f, int c, uint32_t d, uint32_t vr)
    {
        rec("asic_init %d %d %u %u", f, c, (unsigned) d, (unsigned) vr);
        return s_chips;
    }
    int setMaxBaud()
    {
        rec("max_baud");
        return s_baud;
    }
};

class Board {
  public:
    int m_absMinAsicVoltageMillis = 0, m_absMaxAsicVoltageMillis = 0;
    int m_asicVoltageMillis = 0, m_asicFrequency = 0, m_asicCount = 0;
    uint32_t m_asicMaxDifficulty = 0, m_vrFrequency = 0;
    int m_chipsDetected = 0;
    bool m_isBuckInitialized = false, m_isInitialized = false, m_shutdown = false;
    bool validateVoltage(float core_voltage);
    virtual void shutdown() { m_shutdown = true; }
    virtual ~Board() = default;
};

class NerdQaxePlus : public Board {
  public:
    int m_numPhases = 0, m_imax = 0, m_initVoltageMillis = 0;
    float m_ifault = 0;
    RecBuck *m_tps;
    RecAsics *m_asics;
    virtual void LDO_enable() { rec("ldo 1"); }
    virtual void LDO_disable() { rec("ldo 0"); }
    virtual void VREG_enable() { rec("vreg 1"); }
    virtual void VREG_disable() { rec("vreg 0"); }
    virtual void setAsicReset(bool state) { rec("reset %d", (int) state); }
    virtual bool initAsics();
    virtual bool setVoltage(float core_voltage);
    void shutdown() override;
};

class NerdQaxePlus2 : public NerdQaxePlus {
  public:
    bool m_hasRev7TPS546 = false;
    bool initAsics() override;
    bool setVoltage(float core_voltage) override;
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

void NerdQaxePlus::shutdown() {
    setVoltage(0.0);

    vTaskDelay(pdMS_TO_TICKS(500));

    LDO_disable();

    vTaskDelay(pdMS_TO_TICKS(500));

    Board::shutdown();
}

bool NerdQaxePlus::initAsics()
{
    // disable buck (disables EN pin)
    setVoltage(0.0);

    // disable LDO
    LDO_disable();

    // set reset low
    setAsicReset(0);

    // wait 250ms
    vTaskDelay(pdMS_TO_TICKS(250));

    // enable LDOs
    LDO_enable();

    // wait 100ms
    vTaskDelay(pdMS_TO_TICKS(100));

    // init buck and enable output
    m_tps->init(m_numPhases, m_imax, m_ifault);

    // set the init voltage
    // use the higher voltage for initialization
    setVoltage((float) MAX(m_initVoltageMillis, m_asicVoltageMillis) / 1000.0f);

    // wait 500ms
    vTaskDelay(pdMS_TO_TICKS(500));

    m_isBuckInitialized = true;

    // release reset pin
    setAsicReset(1);

    // delay for 250ms
    vTaskDelay(pdMS_TO_TICKS(250));

    SERIAL_clear_buffer();
    m_chipsDetected = m_asics->init(m_asicFrequency, m_asicCount, m_asicMaxDifficulty, m_vrFrequency);
    if (!m_chipsDetected) {
        ESP_LOGE(TAG, "error initializing asics!");
        return false;
    }
    int maxBaud = m_asics->setMaxBaud();
    // no idea why a delay is needed here starting with esp-idf 5.4 🙈
    vTaskDelay(pdMS_TO_TICKS(500));
    SERIAL_set_baud(maxBaud);
    SERIAL_clear_buffer();

    vTaskDelay(pdMS_TO_TICKS(500));

    // set final output voltage
    setVoltage((float) m_asicVoltageMillis / 1000.0f);

    m_isInitialized = true;
    return true;
}

bool NerdQaxePlus::setVoltage(float core_voltage)
{
    if (!validateVoltage(core_voltage)) {
        return false;
    }

    if (core_voltage == 0.0) {
        VREG_disable();
        return true;
    }
    ESP_LOGI(TAG, "Set ASIC voltage = %.3fV", core_voltage);
    VREG_enable();
    return m_tps->set_vout(core_voltage);
}

bool NerdQaxePlus2::initAsics()
{
    if (!m_hasRev7TPS546) {
        return NerdQaxePlus::initAsics();
    }

    VREG_disable();
    LDO_disable();
    setAsicReset(false);
    vTaskDelay(pdMS_TO_TICKS(250));

    LDO_enable();
    vTaskDelay(pdMS_TO_TICKS(100));

    VREG_enable();
    vTaskDelay(pdMS_TO_TICKS(50));

    if (!m_tps->init(m_numPhases, m_imax, m_ifault)) {
        ESP_LOGE(TAG, "error initializing buck regulator");
        return false;
    }

    const float init_voltage = static_cast<float>(std::max(m_initVoltageMillis, m_asicVoltageMillis)) / 1000.0f;
    if (!setVoltage(init_voltage)) {
        ESP_LOGE(TAG, "Failed to set init voltage");
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(500));
    m_isBuckInitialized = true;

    setAsicReset(true);
    vTaskDelay(pdMS_TO_TICKS(250));

    SERIAL_clear_buffer();
    m_chipsDetected = m_asics->init(m_asicFrequency, m_asicCount, m_asicMaxDifficulty, m_vrFrequency);
    if (!m_chipsDetected) {
        ESP_LOGE(TAG, "error initializing asics!");
        return false;
    }

    int maxBaud = m_asics->setMaxBaud();
    vTaskDelay(pdMS_TO_TICKS(500));
    SERIAL_set_baud(maxBaud);
    SERIAL_clear_buffer();

    vTaskDelay(pdMS_TO_TICKS(500));

    if (!setVoltage(static_cast<float>(m_asicVoltageMillis) / 1000.0f)) {
        ESP_LOGE(TAG, "Failed to set final voltage");
        return false;
    }

    m_isInitialized = true;
    return true;
}

bool NerdQaxePlus2::setVoltage(float core_voltage)
{
    if (!m_hasRev7TPS546) {
        return NerdQaxePlus::setVoltage(core_voltage);
    }

    if (!validateVoltage(core_voltage)) {
        return false;
    }

    if (core_voltage == 0.0f) {
        ESP_LOGW(TAG, "Disable ASIC voltage");
        bool result = m_tps->disable_vout();
        VREG_disable();
        return result;
    }

    VREG_enable();
    vTaskDelay(pdMS_TO_TICKS(20));

    ESP_LOGI(TAG, "Set ASIC voltage = %.3fV", core_voltage);
    return m_tps->set_vout(core_voltage);
}

/* the board's parameters and failure injection */
struct RefParams {
    int init_mv, asic_mv, phases, imax;
    float ifault;
    int freq, count;
    uint32_t max_diff, vr_freq;
    int min_mv, max_mv;
    bool buck_init_ok, set_vout_ok, disable_ok;
    int chips, baud;
};

static NerdQaxePlus2 *make(const RefParams *p, bool rev7)
{
    static RecBuck buck;
    static RecAsics asics;
    auto *b = new NerdQaxePlus2();
    b->m_hasRev7TPS546 = rev7;
    b->m_initVoltageMillis = p->init_mv;
    b->m_asicVoltageMillis = p->asic_mv;
    b->m_numPhases = p->phases;
    b->m_imax = p->imax;
    b->m_ifault = p->ifault;
    b->m_asicFrequency = p->freq;
    b->m_asicCount = p->count;
    b->m_asicMaxDifficulty = p->max_diff;
    b->m_vrFrequency = p->vr_freq;
    b->m_absMinAsicVoltageMillis = p->min_mv;
    b->m_absMaxAsicVoltageMillis = p->max_mv;
    b->m_tps = &buck;
    b->m_asics = &asics;
    s_buck_init_ok = p->buck_init_ok;
    s_set_vout_ok = p->set_vout_ok;
    s_disable_ok = p->disable_ok;
    s_chips = p->chips;
    s_baud = p->baud;
    g_trace.clear();
    return b;
}

/* what: 0 initAsics, 1 setVoltage(volts), 2 shutdown; the result, the trace
 * and the state (buck initialized, chips, initialized, shut down) */
extern "C" int ref_power(const RefParams *p, bool rev7, int what, float volts, char *trace, size_t cap, int *state)
{
    NerdQaxePlus2 *b = make(p, rev7);
    int r = 0;
    switch (what) {
    case 0: r = b->initAsics(); break;
    case 1: r = b->setVoltage(volts); break;
    default: b->shutdown(); break;
    }
    state[0] = b->m_isBuckInitialized;
    state[1] = b->m_chipsDetected;
    state[2] = b->m_isInitialized;
    state[3] = b->m_shutdown;
    snprintf(trace, cap, "%s", g_trace.c_str());
    delete b;
    return r;
}

/* ---- the NerdAxe family (main/boards/nerdaxe*.cpp, verbatim) ---- */
#define PWR_EN_PIN 10
#define BM1366_RST_PIN 1
#define BM1370_RST_PIN 1
#define BM1373_RST_PIN 1
#define LDO_EN_PIN 12
#define DS4432_VRFS 0.997
#define TPS40305_VFB 0.6

// DS4432U Transfer function constants for Bitaxe board
// #define BITAXE_RFS 80000.0     // R16
// #define BITAXE_IFS ((DS4432_VRFS * 127.0) / (BITAXE_RFS * 16))
#define BITAXE_IFS 0.000098921 // (Vrfs / Rfs) x (127/16)  -> Vrfs = 0.997, Rfs = 80000
#define BITAXE_RA 4750.0       // R14
#define BITAXE_RB 3320.0       // R15
#define BITAXE_VNOM 1.451   // this is with the current DAC set to 0. Should be pretty close to (VFB*(RA+RB))/RB
#define BITAXE_VMAX 2.39
#define BITAXE_VMIN 0.046

static void gpio_set_level(int pin, int level)
{
    switch (pin) {
    case PWR_EN_PIN: rec("vreg %d", !level); break; // inverted enable
    case LDO_EN_PIN: rec("ldo %d", !!level); break;
    default: rec("reset %d", !!level); break;
    }
}
static int DS4432U_set_current_code(int output, uint8_t code)
{
    rec("dac %02x", code);
    return output;
}
static bool TPS546_set_vout(float v)
{
    rec("vout %08x", (unsigned) bits(v));
    return s_set_vout_ok;
}

class NerdAxe : public Board {
  public:
    RecAsics *m_asics;
    uint8_t ds4432_tps40305_bitaxe_voltage_to_reg(float vout);
    void shutdown() override;
    virtual bool initAsics();
    virtual bool setVoltage(float core_voltage);
};

class NerdaxeGamma : public NerdAxe {
  public:
    int m_initVoltageMillis = 0;
    void shutdown() override;
    bool initAsics() override;
    bool setVoltage(float core_voltage) override;
};

class NerdaxeGaia : public NerdAxe {
  public:
    int m_initVoltageMillis = 0;
    void LDO_enable();
    void LDO_disable();
    void shutdown() override;
    bool initAsics() override;
    bool setVoltage(float core_voltage) override;
};

uint8_t NerdAxe::ds4432_tps40305_bitaxe_voltage_to_reg(float vout)
{
    float change;
    uint8_t reg;

    // make sure the requested voltage is in within range of BITAXE_VMIN and BITAXE_VMAX
    if (vout > BITAXE_VMAX || vout < BITAXE_VMIN)
    {
        return 0;
    }

    // this is the transfer function. comes from the DS4432U+ datasheet
    change = fabs((((TPS40305_VFB / BITAXE_RB) - ((vout - TPS40305_VFB) / BITAXE_RA)) / BITAXE_IFS) * 127);
    reg = (uint8_t)ceil(change);

    // Set the MSB high if the requested voltage is BELOW nominal
    if (vout < BITAXE_VNOM)
    {
        reg |= 0x80;
    }

    return reg;
}

void NerdAxe::shutdown() {
    setVoltage(0.0);

    Board::shutdown();
}

bool NerdAxe::initAsics()
{
    // disable buck (disables EN pin)
    setVoltage(0.0);

    // set reset low
    gpio_set_level(BM1366_RST_PIN, 0);

    // wait 250ms
    vTaskDelay(pdMS_TO_TICKS(250));

     // set output voltage
    setVoltage((float) m_asicVoltageMillis / 1000.0f);

    // wait 500ms
    vTaskDelay(pdMS_TO_TICKS(500));

    m_isBuckInitialized = true;

    // release reset pin
    gpio_set_level(BM1366_RST_PIN, 1);

    // delay for 250ms
    vTaskDelay(pdMS_TO_TICKS(250));

    SERIAL_clear_buffer();
    m_chipsDetected = m_asics->init(m_asicFrequency, m_asicCount, m_asicMaxDifficulty, m_vrFrequency);
    if (!m_chipsDetected) {
        ESP_LOGE(TAG, "error initializing asics!");
        return false;
    }
    int maxBaud = m_asics->setMaxBaud();
    // no idea why a delay is needed here starting with esp-idf 5.4 🙈
    vTaskDelay(pdMS_TO_TICKS(500));
    SERIAL_set_baud(maxBaud);
    SERIAL_clear_buffer();

    vTaskDelay(pdMS_TO_TICKS(500));

    m_isInitialized = true;
    return true;
}

bool NerdAxe::setVoltage(float core_voltage)
{
    if (!validateVoltage(core_voltage)) {
        return false;
    }

    if (!core_voltage) {
        // inverted
        gpio_set_level(PWR_EN_PIN, 1);
        return true;
    }
    uint8_t reg_setting = ds4432_tps40305_bitaxe_voltage_to_reg(core_voltage);
    ESP_LOGI(TAG, "Set ASIC voltage = %.3fV [0x%02X]", core_voltage, reg_setting);
    DS4432U_set_current_code(0, reg_setting); /// eek!

    // inverted
    gpio_set_level(PWR_EN_PIN, 0);

    return true;
}

void NerdaxeGamma::shutdown() {
    setVoltage(0.0);

    Board::shutdown();
}

bool NerdaxeGamma::initAsics() {

    // set output voltage
    setVoltage(0.0);

    // wait 500ms
    vTaskDelay(pdMS_TO_TICKS(500));

    // set reset low
    gpio_set_level(BM1370_RST_PIN, 0);

    // wait 250ms
    vTaskDelay(pdMS_TO_TICKS(250));

    // set the init voltage
    // use the higher voltage for initialization
    setVoltage((float) MAX(m_initVoltageMillis, m_asicVoltageMillis) / 1000.0f);

    // wait 500ms
    vTaskDelay(pdMS_TO_TICKS(500));

    m_isBuckInitialized = true;

    // release reset pin
    gpio_set_level(BM1370_RST_PIN, 1);

    // delay for 250ms
    vTaskDelay(pdMS_TO_TICKS(250));

    SERIAL_clear_buffer();
    m_chipsDetected = m_asics->init(m_asicFrequency, m_asicCount, m_asicMaxDifficulty, m_vrFrequency);
    if (!m_chipsDetected) {
        ESP_LOGE(TAG, "error initializing asics!");
        return false;
    }
    int maxBaud = m_asics->setMaxBaud();
    // no idea why a delay is needed here starting with esp-idf 5.4 🙈
    vTaskDelay(pdMS_TO_TICKS(500));
    SERIAL_set_baud(maxBaud);
    SERIAL_clear_buffer();

    vTaskDelay(pdMS_TO_TICKS(500));

    m_isInitialized = true;
    return true;
}

bool NerdaxeGamma::setVoltage(float core_voltage)
{
    if (!validateVoltage(core_voltage)) {
        return false;
    }

    ESP_LOGI(TAG, "Set ASIC voltage = %.3fV", core_voltage);
    return TPS546_set_vout(core_voltage);
}

void NerdaxeGaia::shutdown() {
    setVoltage(0.0);

    // let the core rail collapse before cutting the LDO
    vTaskDelay(pdMS_TO_TICKS(500));

    LDO_disable();

    vTaskDelay(pdMS_TO_TICKS(500));

    Board::shutdown();
}

void NerdaxeGaia::LDO_enable() {
    ESP_LOGI(TAG, "Enable LDO");
    gpio_set_level(LDO_EN_PIN, 1);
}

void NerdaxeGaia::LDO_disable() {
    ESP_LOGI(TAG, "Disable LDO");
    gpio_set_level(LDO_EN_PIN, 0);
}

bool NerdaxeGaia::initAsics() {

    // Core buck off, but keep the LDO ON (already enabled in initBoard and
    // never cycled): the chip runs on the LDO alone during the self-test, so
    // cutting it here — as the old sequence did — left the chip unpowered and
    // it failed to enumerate. This matches the known-good jumpered-high case.
    setVoltage(0.0);
    LDO_enable();   // re-assert (no-op if already high); never disable

    // wait 500ms
    vTaskDelay(pdMS_TO_TICKS(500));

    // set reset low
    gpio_set_level(BM1373_RST_PIN, 0);

    // wait 250ms
    vTaskDelay(pdMS_TO_TICKS(250));

    // LDO already on and settled; give the reset a moment before the core ramp
    vTaskDelay(pdMS_TO_TICKS(100));

    // set the init voltage
    // use the higher voltage for initialization
    setVoltage((float) MAX(m_initVoltageMillis, m_asicVoltageMillis) / 1000.0f);

    // wait 500ms
    vTaskDelay(pdMS_TO_TICKS(500));

    m_isBuckInitialized = true;

    // release reset pin
    gpio_set_level(BM1373_RST_PIN, 1);

    // delay for 250ms
    vTaskDelay(pdMS_TO_TICKS(250));

    SERIAL_clear_buffer();
    m_chipsDetected = m_asics->init(m_asicFrequency, m_asicCount, m_asicMaxDifficulty, m_vrFrequency);
    if (!m_chipsDetected) {
        ESP_LOGE(TAG, "error initializing asics!");
        return false;
    }
    int maxBaud = m_asics->setMaxBaud();
    // no idea why a delay is needed here starting with esp-idf 5.4 🙈
    vTaskDelay(pdMS_TO_TICKS(500));
    SERIAL_set_baud(maxBaud);
    SERIAL_clear_buffer();

    vTaskDelay(pdMS_TO_TICKS(500));

    m_isInitialized = true;
    return true;
}

bool NerdaxeGaia::setVoltage(float core_voltage)
{
    if (!validateVoltage(core_voltage)) {
        return false;
    }

    ESP_LOGI(TAG, "Set ASIC voltage = %.3fV", core_voltage);
    return TPS546_set_vout(core_voltage);
}

/* which: 0 NerdAxe, 1 Gamma, 2 Gaia; what as ref_power() */
extern "C" int ref_power_axe(const RefParams *p, int which, int what, float volts, char *trace, size_t cap, int *state)
{
    static RecAsics asics;
    NerdAxe *b;
    if (which == 1) {
        auto *g = new NerdaxeGamma();
        g->m_initVoltageMillis = p->init_mv;
        b = g;
    } else if (which == 2) {
        auto *g = new NerdaxeGaia();
        g->m_initVoltageMillis = p->init_mv;
        b = g;
    } else {
        b = new NerdAxe();
    }
    b->m_asicVoltageMillis = p->asic_mv;
    b->m_asicFrequency = p->freq;
    b->m_asicCount = p->count;
    b->m_asicMaxDifficulty = p->max_diff;
    b->m_vrFrequency = p->vr_freq;
    b->m_absMinAsicVoltageMillis = p->min_mv;
    b->m_absMaxAsicVoltageMillis = p->max_mv;
    b->m_asics = &asics;
    s_set_vout_ok = p->set_vout_ok;
    s_chips = p->chips;
    s_baud = p->baud;
    g_trace.clear();
    int r = 0;
    switch (what) {
    case 0: r = b->initAsics(); break;
    case 1: r = b->setVoltage(volts); break;
    case 2: b->shutdown(); break;
    default: r = b->ds4432_tps40305_bitaxe_voltage_to_reg(volts); break;
    }
    state[0] = b->m_isBuckInitialized;
    state[1] = b->m_chipsDetected;
    state[2] = b->m_isInitialized;
    state[3] = b->m_shutdown;
    snprintf(trace, cap, "%s", g_trace.c_str());
    delete b;
    return r;
}
