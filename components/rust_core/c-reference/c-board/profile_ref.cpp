/* The former board constructors, verbatim (extracted by a script from the
 * commit before board step B1; NerdQX's decode helpers included), on shim
 * classes with the members and default initializers of main/boards/board.h
 * and the subclass headers; drivers are markers, detections injected. */
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "board_profile.h"

#define ESP_LOGI(tag, ...) ((void) (tag))
#define ESP_LOGE(tag, ...) ((void) (tag))
#define CONFIG_AUTO_FAN_SPEED_VALUE 77
#define TPS546_THROTTLE_TEMP 105.0
#define pdMS_TO_TICKS(ms) (ms)
static const char *TAG = "ref";

enum gpio_num_t { GPIO_NUM_2 = 2, GPIO_NUM_3 = 3, GPIO_NUM_12 = 12 };
enum { GPIO_MODE_INPUT, GPIO_PULLDOWN_ONLY };
#define VR_DETECT_PIN GPIO_NUM_3
static bool s_vr_pin, s_q1373_tps67, s_q1373_6ph;
static void gpio_reset_pin(int) {}
static void gpio_set_direction(int, int) {}
static void gpio_set_pull_mode(int, int) {}
static int gpio_get_level(int) { return s_vr_pin; }
static void vTaskDelay(int) {}

/* drivers: the ASIC's default VR frequency as a marker (the chip number) */
struct Asic {
    uint32_t chip;
    uint32_t getDefaultVrFrequency() { return chip; }
};
struct BM1366 : Asic { BM1366() { chip = 1366; } };
struct BM1368 : Asic { BM1368() { chip = 1368; } };
struct BM1370 : Asic { BM1370() { chip = 1370; } };
struct BM1373 : Asic { BM1373() { chip = 1373; } };
struct BuckConverter { bool tps53667 = false; virtual ~BuckConverter() = default; };
struct TPS53647 : BuckConverter {};
struct TPS53667 : TPS53647 { TPS53667() { tps53667 = true; } };
struct Tmp451Mux {
    Tmp451Mux() {}
    Tmp451Mux(int, int, int, bool = false) {}
};
struct Theme {};

struct PidSettings {
    uint16_t targetTemp, p, i, d;
};

class Board {
  public:
    const char *m_deviceModel;
    int m_version;
    const char *m_asicModel;
    const char *m_miningAgent;
    int m_asicCount;
    int m_numTempSensors = 0;
    const char *m_swarmColorName = "blue";
    uint32_t m_vrFrequency;
    uint32_t m_defaultVrFrequency;
    bool m_hasHashCounter;
    const char *m_defaultTheme = "cosmic";
    PidSettings m_pidSettings[2] = {{}, {65, 600, 10, 1000}};
    int m_fanMode[2] = {CONFIG_AUTO_FAN_SPEED_VALUE, 3};
    const char *m_fanLabels[2] = {"Fan 1", "Fan 2"};
    int m_asicJobIntervalMs;
    int m_asicFrequency;
    int m_asicVoltageMillis;
    int m_absMaxAsicFrequency;
    int m_absMinAsicVoltageMillis;
    int m_absMaxAsicVoltageMillis;
    std::vector<uint32_t> m_asicFrequencies;
    std::vector<uint32_t> m_asicVoltages;
    int m_defaultAsicFrequency;
    int m_defaultAsicVoltageMillis;
    int m_ecoAsicFrequency;
    int m_ecoAsicVoltageMillis;
    uint32_t m_asicMinDifficulty;
    uint32_t m_asicMinDifficultyDualPool;
    uint32_t m_asicMaxDifficulty;
    float m_vr_maxTemp = 0.0;
    bool m_fanInvertPolarity;
    float m_fanPerc;
    bool m_flipScreen;
    float m_maxPin;
    float m_minPin;
    float m_maxVin;
    float m_minVin;
    float m_minCurrentA = 0.0f;
    float m_maxCurrentA = 8.0f;
    int m_numFans;
    Theme *m_theme = nullptr;
    Asic *m_asics = nullptr;
    /* NerdAxe family / NerdQAxe family members (0 where a class has none) */
    int m_numPhases = 0, m_imax = 0, m_initVoltageMillis = 0;
    float m_ifault = 0;
    BuckConverter *m_tps = nullptr;
    Board();
    virtual ~Board() = default;
    bool isEthConnected() { return false; }
};

class NerdQaxePlus : public Board {
  public:
    NerdQaxePlus();
};
class NerdQaxePlus2 : public NerdQaxePlus {
  public:
    NerdQaxePlus2();
};
class NerdOctaxePlus : public NerdQaxePlus {
  public:
    NerdOctaxePlus();
};
class NerdOctaxeGamma : public NerdQaxePlus2 {
  public:
    Tmp451Mux m_tmp451[2];
    bool m_isTPS53667 = false;
    NerdOctaxeGamma();
};
class NerdQX : public NerdQaxePlus2 {
  public:
    Tmp451Mux *m_tmp451 = nullptr;
    NerdQX();
};
class NerdHaxeGamma : public NerdQaxePlus2 {
  public:
    NerdHaxeGamma();
};
class NerdEko : public NerdQaxePlus2 {
  public:
    NerdEko();
};
class Q1370B : public NerdQaxePlus {
  public:
    void releaseIoExpanderReset() {}
    Q1370B();
};
class Q1373B : public Q1370B {
  public:
    bool m_hasEth = true;
    bool m_is6Phase = false;
    bool m_isTPS53667 = false;
    void detectBoardOptions()
    {
        m_is6Phase = s_q1373_6ph;
        m_isTPS53667 = s_q1373_tps67;
    }
    Q1373B();
};
class NerdAxe : public Board {
  public:
    NerdAxe();
};
class NerdaxeGamma : public NerdAxe {
  public:
    NerdaxeGamma();
};
class NerdaxeGaia : public NerdAxe {
  public:
    bool m_hasEth = false;
    NerdaxeGaia();
};

Board::Board() {
    m_absMaxAsicFrequency = 0;
    m_absMinAsicVoltageMillis = 0;
    m_absMaxAsicVoltageMillis = 0;
    m_vrFrequency = m_defaultVrFrequency = 0;
    m_hasHashCounter = false;
    m_ecoAsicFrequency = 0;
    m_ecoAsicVoltageMillis = 0;
    m_numFans = 1;
}

NerdQaxePlus::NerdQaxePlus() : Board() {
    m_deviceModel = "NerdQAxe+";
    m_miningAgent = m_deviceModel;
    m_version = 501;
    m_asicModel = "BM1368";
    m_asicCount = 4;
    m_asicJobIntervalMs = 1200;
    m_asicFrequencies = {400, 425, 450, 475, 490, 500, 525, 550, 575};
    m_asicVoltages = {1100, 1150, 1200, 1250, 1300, 1350};
    m_defaultAsicFrequency = m_asicFrequency = 490;
    m_defaultAsicVoltageMillis = m_asicVoltageMillis = 1250; // default voltage
    m_absMaxAsicFrequency = 800;
    m_absMinAsicVoltageMillis = 1050;
    m_absMaxAsicVoltageMillis = 1400;
    m_initVoltageMillis = 1250;
    m_fanInvertPolarity = false;
    m_fanPerc = 100;
    m_flipScreen = false;
    m_numPhases = 2;
    m_imax = m_numPhases * 30;
    m_ifault = (float) (m_imax - 5);

    m_numFans = 2;
    m_fanLabels[0] = "M2"; // ASIC/CPU fan connector
    m_fanLabels[1] = "M1"; // VReg fan connector

    m_maxPin = 70.0;
    m_minPin = 30.0;
    m_maxVin = 13.0;
    m_minVin = 11.0;
    m_minCurrentA = 0.0f;
    m_maxCurrentA = 6.0f;

    m_pidSettings[0].targetTemp = 55;
    m_pidSettings[0].p = 600; //   6.00
    m_pidSettings[0].i = 10;  //   0.10
    m_pidSettings[0].d = 1000; // 10.00

    m_pidSettings[1].targetTemp = 65;  // target temp for vreg
    m_pidSettings[1].p = 600;  //   6.00
    m_pidSettings[1].i = 10;   //   0.10
    m_pidSettings[1].d = 1000; // 10.00

    m_asicMaxDifficulty = 1024;
    m_asicMinDifficulty = 256;
    m_asicMinDifficultyDualPool = 128;

#ifdef NERDQAXEPLUS
    m_theme = new ThemeNerdqaxeplus();
#endif
    m_swarmColorName = "#e700d8"; // pink

    m_asics = new BM1368();
    m_hasHashCounter = true;
    m_vrFrequency = m_defaultVrFrequency = m_asics->getDefaultVrFrequency();

    m_tps = new TPS53647();
}

NerdQaxePlus2::NerdQaxePlus2() : NerdQaxePlus() {
    m_deviceModel = "NerdQAxe++";
    m_miningAgent = m_deviceModel;
    m_asicModel = "BM1370";
    m_asicCount = 4;
    m_numPhases = 3;
    m_imax = m_numPhases * 30;
    m_ifault = (float) (m_imax + 5);

    m_asicJobIntervalMs = 500;
    m_asicFrequencies = {500, 515, 525, 550, 575, 590, 600};
    m_asicVoltages = {1120, 1130, 1140, 1150, 1160, 1170, 1180, 1190, 1200};
    m_defaultAsicFrequency = m_asicFrequency = 600;
    m_defaultAsicVoltageMillis = m_asicVoltageMillis = 1150; // default voltage
    m_absMaxAsicFrequency = 800;
    m_absMaxAsicVoltageMillis = 1400;
    m_initVoltageMillis = 1200;

    m_pidSettings[0].targetTemp = 55;
    m_pidSettings[0].p = 600;  //  6.00
    m_pidSettings[0].i = 10;   //  0.10
    m_pidSettings[0].d = 1000; // 10.00

    m_maxPin = 100.0;
    m_minPin = 52.0;
    m_maxVin = 13.0;
    m_minVin = 11.0;
    m_minCurrentA = 0.0f;
    m_maxCurrentA = 8.0f;

    m_asicMaxDifficulty = 2048;
    m_asicMinDifficulty = 512;
    m_asicMinDifficultyDualPool = 256;

#ifdef NERDQAXEPLUS2
    m_theme = new ThemeNerdqaxeplus2();
#endif
    m_asics = new BM1370();
    m_hasHashCounter = true;
    m_vrFrequency = m_defaultVrFrequency = m_asics->getDefaultVrFrequency();
}

NerdOctaxePlus::NerdOctaxePlus() : NerdQaxePlus() {
    m_deviceModel = "NerdOCTAXE+";
    m_miningAgent = m_deviceModel;
    m_asicCount = 8;
    m_numPhases = 3;
    m_imax = m_numPhases * 30;
    m_ifault = (float) (m_imax - 5);

    m_asicMaxDifficulty = 2048;
    m_asicMinDifficulty = 512;
    m_asicMinDifficultyDualPool = 256;

    m_maxPin = 130.0;
    m_minPin = 70.0;
    m_maxVin = 13.0;
    m_minVin = 11.0;
    m_minCurrentA = 0.0f;
    m_maxCurrentA = 15.0f;

#ifdef NERDOCTAXEPLUS
    m_theme = new ThemeNerdoctaxeplus();
#endif

    m_fanLabels[0] = "M2 (Use a Y-splitter cable for multiple ASIC fans.)";
    m_fanLabels[1] = "M1";

    m_swarmColorName = "#11d51e"; // green
}

NerdOctaxeGamma::NerdOctaxeGamma()
    : NerdQaxePlus2(), m_tmp451{// ASICs 0–3: GPIO2=A0, GPIO12=A1, addr 0x4c
                                Tmp451Mux(GPIO_NUM_2, GPIO_NUM_12, 0x4c),
                                // ASICs 4–7: same MUX select lines, addr 0x4e
                                Tmp451Mux(GPIO_NUM_2, GPIO_NUM_12, 0x4e)}
{
    m_deviceModel = "NerdOCTAXE-γ";
    m_miningAgent = m_deviceModel;
    m_asicModel = "BM1370";
    m_asicCount = 8;

    m_asicMaxDifficulty = 4096;
    m_asicMinDifficulty = 1024;
    m_asicMinDifficultyDualPool = 256;

    m_initVoltageMillis = 0;

    m_maxVin = 13.0;
    m_minVin = 11.0;

#ifdef NERDOCTAXEGAMMA
    m_theme = new ThemeNerdoctaxegamma();
#endif

    m_fanLabels[0] = "M2 (Use a Y-splitter cable for multiple ASIC fans.)";
    m_fanLabels[1] = "M1";

    m_swarmColorName = "#11d51e"; // green

    // Hardware voltage regulator detection (available from rev 3.0+)
    // GPIO3 is located next to GPIO10 (TPS_EN) on the board for easy routing
    // The pin has internal pull-down, so older boards without the strapping
    // resistor will default to TPS53647 (backward compatibility)
    gpio_reset_pin(VR_DETECT_PIN);
    gpio_set_direction(VR_DETECT_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(VR_DETECT_PIN, GPIO_PULLDOWN_ONLY);

    // Allow pin to settle after configuration
    vTaskDelay(pdMS_TO_TICKS(1));

    bool isTPS53667 = gpio_get_level(VR_DETECT_PIN);
    m_isTPS53667 = isTPS53667;

    if (isTPS53667) {
        // TPS53667 configuration: 6 phases, 240A capability
        m_numPhases = 6;
        m_imax = 240; // Current hardware: 24.9kΩ → 240A max (40A per phase with 6 phases)
        m_ifault = 235.0;
        m_maxPin = 300.0; // ~300W output Typically
        m_minPin = 30.0;
        m_minCurrentA = 0.0f;
        m_maxCurrentA = 25.0f;
        m_tps = new TPS53667();

        // Extended frequency range for TPS53667 (6 phases, higher power capacity)
        m_asicFrequencies = {525, 550, 575, 600, 625, 650, 675, 700, 725, 750, 775, 800};
        m_absMaxAsicFrequency = 850; // Absolute max for manual input (danger zone)

        // Extended voltage range for TPS53667 (6 phases, higher current capacity)
        m_asicVoltages = {1120, 1130, 1140, 1150, 1160, 1170, 1180, 1190, 1200, 1210, 1220, 1230, 1240, 1250, 1260};

        // Set higher default values for 6-phase configuration
        m_defaultAsicFrequency = m_asicFrequency = 700;
        m_defaultAsicVoltageMillis = m_asicVoltageMillis = 1210; // 1.21V

        ESP_LOGI(TAG, "TPS53667 voltage regulator detected (GPIO3=HIGH, 6 phases, 240A max with 24.9kΩ resistor)");
    } else {
        // TPS53647 detected or pin not connected (older revisions)
        // Configure for 4-phase operation (uses inherited frequency limits)
        m_numPhases = 4;
        m_imax = 180; // 33.2kΩ → 180A max (45A per phase with 4 phases)
        m_ifault = 160.0;
        m_maxPin = 250.0;
        m_minPin = 50.0;
        m_minCurrentA = 0.0f;
        m_maxCurrentA = 20.0f;
        // m_asicFrequencies and m_absMaxAsicFrequency inherited from parent (500-600 MHz, max 800)

        ESP_LOGI(TAG, "TPS53647 voltage regulator detected (GPIO3=LOW, 4 phases, using inherited)");
    }
}

static int __attribute__((noinline))
decode_m_ifault(int phases) {
    int tIxTech = (500/10) - (6*7) + (81/9);
    int tPlebBase = (72/6) - (8/4);
    int tphil31 = ((56/2)/4) + (6/3) - (8/4);
    int tshufps = (36/6) - (8/4);
    int collaboration = tIxTech + tPlebBase + tphil31 + tshufps;
    return (phases * 60) - collaboration;
}

static int __attribute__((noinline))
decode_m_absMaxAsicFrequency(int imax) {
    int fourteen = (84/7) + (8/4);
    int ten      = ((27/9) + (8/4)) * 2;
    return (imax * fourteen) - ten;
}

static int __attribute__((noinline))
decode_m_absMaxAsicVoltageMillis(int imax) {
    int Bv = (84/7) + (36/18);
    int Cv = (49/7);
    int Av = (85/5) - (9/9) + (8/8);
    return (imax * Bv) + (Cv * Bv) + Av;
}


NerdQX::NerdQX() : NerdQaxePlus2() {
    m_deviceModel = "NerdQX";
    m_miningAgent = m_deviceModel;
    m_asicModel = "BM1370";
    m_numPhases = 3;
    m_imax = m_numPhases * 30;
    m_ifault = (float) decode_m_ifault(m_numPhases);
    m_asicFrequencies = {495, 500, 525, 550, 575, 600, 625, 650, 675, 700, 725, 750, 777, 800, 825, 850, 875, 900, 925, 950, 975, 1000};
    m_asicVoltages   = {1085, 1120, 1130, 1140, 1150, 1160, 1170, 1180, 1190, 1200, 1220, 1230, 1240, 1250, 1260, 1270, 1280, 1290, 1300, 1310, 1320, 1330, 1340, 1350};
    m_absMaxAsicFrequency = decode_m_absMaxAsicFrequency(m_imax);
    m_defaultAsicFrequency = m_asicFrequency = 777;
    m_defaultAsicVoltageMillis = m_asicVoltageMillis = 1200;
    m_ecoAsicFrequency = 495;
    m_ecoAsicVoltageMillis = 1085;
    m_absMaxAsicVoltageMillis = decode_m_absMaxAsicVoltageMillis(m_imax);
    m_initVoltageMillis = m_defaultAsicVoltageMillis;

    m_pidSettings[0].targetTemp = 58;
    m_pidSettings[0].p = 600;  //  6.00
    m_pidSettings[0].i = 10;   //  0.10
    m_pidSettings[0].d = 1000; // 10.00

    m_flipScreen = true;
    m_maxPin = 240.0;
    m_minPin = 50.0;
    m_maxVin = 13.0;
    m_minVin = 11.0;
    m_minCurrentA = 0.0f;
    m_maxCurrentA = 20.0f;

    m_asicMaxDifficulty = 4096;
    m_asicMinDifficulty = 1024;
    m_asicMinDifficultyDualPool = 256;

    m_tmp451 = new Tmp451Mux(GPIO_NUM_2, GPIO_NUM_3, 0x4c, true);
#ifdef NERDQX
    m_theme = new ThemeNerdqx();
#endif

    m_swarmColorName = "#7300e7";
    m_defaultTheme = "default"; // light theme
    m_vrFrequency = m_defaultVrFrequency = 35000;
}

NerdHaxeGamma::NerdHaxeGamma() : NerdQaxePlus2() {
    m_deviceModel = "NerdHaxe-γ";
    m_miningAgent = m_deviceModel;
    m_asicModel = "BM1370";
    m_asicCount = 6;
    m_numPhases = 4;
    m_imax = 120;
    m_ifault = 105.0;

    // use m_asicVoltage for init
    m_initVoltageMillis = 0;

    m_maxPin = 250.0;
    m_minPin = 75.0;
    m_maxVin = 13.0;
    m_minVin = 11.0;
    m_minCurrentA = 0.0f;
    m_maxCurrentA = 15.0f;

    m_asicMaxDifficulty = 4096;
    m_asicMinDifficulty = 1024;
    m_asicMinDifficultyDualPool = 256;

#ifdef NERDHAXEGAMMA
    m_theme = new ThemeNerdhaxegamma();
#endif

    m_swarmColorName = "#00e7e2";  // cyan

}

NerdEko::NerdEko() : NerdQaxePlus2()
{
    m_deviceModel = "NerdEKO";
    m_miningAgent = m_deviceModel;
    m_asicModel = "BM1370";
    m_asicCount = 12;
    m_numPhases = 6;
    m_imax = 240; // R = 6000 / (num_phases * max_current) = 24K9
    m_ifault = 235.0;

    // use m_asicVoltage for init
    m_initVoltageMillis = 0;

    m_maxPin = 350.0;
    m_minPin = 30.0;
    m_maxVin = 13.0;
    m_minVin = 8.0;
    m_minCurrentA = 0.0f;
    m_maxCurrentA = 25.0f;

    m_asicMaxDifficulty = 8192;
    m_asicMinDifficulty = 2048;
    m_asicMinDifficultyDualPool = 256;

#ifdef NERDEKO
    m_theme = new ThemeNerdeko();
#endif

    m_swarmColorName = "#e76500"; // orange

    m_tps = new TPS53667();
}

Q1370B::Q1370B() : NerdQaxePlus()
{
    m_deviceModel = "Q1370";
    m_miningAgent = m_deviceModel;
    m_asicModel = "BM1370";
    m_asicCount = 4;
    m_numPhases = 4;
    m_imax = 123;
    m_ifault = (float) 160.0f;

    m_asicJobIntervalMs = 500;
    m_asicFrequencies = {500, 515, 525, 550, 575, 590, 600};
    m_asicVoltages = {1120, 1130, 1140, 1150, 1160, 1170, 1180, 1190, 1200};
    m_defaultAsicFrequency = m_asicFrequency = 600;
    m_defaultAsicVoltageMillis = m_asicVoltageMillis = 1150; // default voltage
    m_absMaxAsicFrequency = 1200;
    m_absMaxAsicVoltageMillis = 1400;
    m_initVoltageMillis = 1200;

    m_pidSettings[0].targetTemp = 55;
    m_pidSettings[0].p = 600; //   6.00
    m_pidSettings[0].i = 10;  //   0.10
    m_pidSettings[0].d = 1000; // 10.00

    m_pidSettings[1].targetTemp = 65;  // target temp for vreg
    m_pidSettings[1].p = 600;  //   6.00
    m_pidSettings[1].i = 10;   //   0.10
    m_pidSettings[1].d = 1000; // 10.00

    m_maxPin = 150.0;
    m_minPin = 52.0;
    m_maxVin = 13.0;
    m_minVin = 11.0;
    m_minCurrentA = 0.0f;
    m_maxCurrentA = 20.0f;

    m_asicMaxDifficulty = 2048;
    m_asicMinDifficulty = 512;
    m_asicMinDifficultyDualPool = 256;

#ifdef Q1370
    m_theme = new ThemeGeneric();
#endif
    m_asics = new BM1370();
    m_hasHashCounter = true;

    // Release the FXL6408 IO expander from reset (GPIO3 low) as early as
    // possible. This base-class ctor completes before the Q1373 ctor body,
    // which reads the strap pins off the expander in detectBoardOptions() —
    // so a single call here covers both Q1370 and Q1373.
    releaseIoExpanderReset();
}

Q1373B::Q1373B() : Q1370B()
{
    m_deviceModel = "Q1373";
    m_miningAgent = m_deviceModel;
    m_asicModel = "BM1373";
    m_asicCount = 4;
    m_numPhases = 4;
    // formula from the tps53647 datasheet
    // Rmon=48.7k, Imax=122,17A
    m_imax = (int) round(0.85 / 48700.0 / 5.0e-3 * 35000.0);
    m_ifault = (float) 160.0f;

    m_maxPin = 180.0;
    m_minPin = 50.0;
    m_maxVin = 13.0;
    m_minVin = 11.0;
    m_minCurrentA = 0.0f;
    m_maxCurrentA = 15.0f;

    m_asicFrequencies = {250, 275, 300, 325, 350, 375, 400, 425, 475, 500, 550};
    m_asicVoltages = {980, 990, 1000, 1010, 1020, 1030, 1040, 1050, 1060, 1070, 1080};
    m_defaultAsicFrequency = m_asicFrequency = 425;
    m_defaultAsicVoltageMillis = m_asicVoltageMillis = 1010;
    m_absMaxAsicFrequency = 730;
    m_absMinAsicVoltageMillis = 900;
    m_absMaxAsicVoltageMillis = 1200;
    m_initVoltageMillis = 1050;

    m_pidSettings[0].targetTemp = 60;
    m_pidSettings[0].p = 600;  //   6.00
    m_pidSettings[0].i = 10;   //   0.10
    m_pidSettings[0].d = 1000; // 10.00

    m_pidSettings[1].targetTemp = 65; // target temp for vreg
    m_pidSettings[1].p = 600;         //   6.00
    m_pidSettings[1].i = 10;          //   0.10
    m_pidSettings[1].d = 1000;        // 10.00

    // ship with PID fan control by default (ch1 stays linked); with target 60°C
    // the fan holds max(ASIC, VReg) for good out-of-the-box efficiency
    m_fanMode[0] = 2; // PID

    m_asicMaxDifficulty = 4096;
    m_asicMinDifficulty = 1024;
    m_asicMinDifficultyDualPool = 512;

#ifdef Q1373
    m_theme = new ThemeGeneric();
#endif
    m_asics = new BM1373();

    // strap detection has to happen in the constructor because
    // hasEthernet() and loadSettings() are called before initBoard()
    detectBoardOptions();

    if (m_isTPS53667) {
        delete m_tps;
        m_tps = new TPS53667();

        // formula from tps53667 datasheet (33.2k 0.1% Rmon)
        m_imax = (int) round(0.85 / 33200.0 / 5.0e-3 * 35000.0);

        m_numPhases = m_is6Phase ? 6 : 4;

        m_ifault = (float) m_numPhases * 40.0f;
        m_maxPin = (float) m_numPhases * 45.0f;
        m_maxCurrentA = m_maxPin / 12.0f;
    } else if (m_is6Phase) {
        // 6 phases are only possible with the TPS53667
        ESP_LOGE(TAG, "6-phase strap set but no TPS53667 found; staying with 4-phase TPS53647");
    }
}

NerdAxe::NerdAxe() : Board() {
    m_deviceModel = "NerdAxe";
    m_miningAgent = m_deviceModel;
    m_version = 204;
    m_asicModel = "BM1366";
    m_asicCount = 1;
    m_asicJobIntervalMs = 1500;
    m_asicFrequencies = {400, 425, 450, 475, 485, 500, 525, 550, 575};
    m_asicVoltages = {1100, 1150, 1200, 1250, 1300};
    m_defaultAsicFrequency = m_asicFrequency = 485;
    m_defaultAsicVoltageMillis = m_asicVoltageMillis = 1200;
    // m_absMaxAsicFrequency = 650;
    // m_absMaxAsicVoltageMillis = 1400;
    m_fanInvertPolarity = false;
    m_fanPerc = 100;
    m_flipScreen = true;
    m_numTempSensors = 1;

    m_pidSettings[0].targetTemp = 55;
    m_pidSettings[0].p =  400; // 2.00
    m_pidSettings[0].i =   10; // 0.1
    m_pidSettings[0].d = 1000; // 10.00

    m_maxPin = 15.0;
    m_minPin = 5.0;
    m_maxVin = 5.5;
    m_minVin = 4.5;
    m_minCurrentA = 0.0f;
    m_maxCurrentA = 5.0f;

    m_asicMaxDifficulty = 256;
    m_asicMinDifficulty = 64;
    m_asicMinDifficultyDualPool = 32;

#ifdef NERDAXE
    m_theme = new ThemeNerdaxe();
#endif

    m_swarmColorName = "#e7cf00"; // yellow

    m_asics = new BM1366();
    m_hasHashCounter = true;
    m_vrFrequency = m_defaultVrFrequency = m_asics->getDefaultVrFrequency();
}

NerdaxeGamma::NerdaxeGamma() : NerdAxe() {
    m_deviceModel = "NerdAxeGamma";
    m_miningAgent = "NerdAxe";
    m_asicModel = "BM1370";
    m_version = 200;
    m_asicCount = 1;

    m_asicJobIntervalMs = 1500;
    m_asicFrequencies = {500, 515, 525, 550, 575};
    m_asicVoltages = {1120, 1130, 1140, 1150, 1160, 1170, 1180, 1190, 1200};
    m_defaultAsicFrequency = m_asicFrequency = 515;
    m_defaultAsicVoltageMillis = m_asicVoltageMillis = 1150;
    // m_absMaxAsicFrequency = 750;
    // m_absMaxAsicVoltageMillis = 1300;
    m_initVoltageMillis = 1150;
    m_fanInvertPolarity = false;
    m_fanPerc = 100;
    m_flipScreen = true;
    m_vr_maxTemp = TPS546_THROTTLE_TEMP; //Set max voltage regulator temp

    m_pidSettings[0].targetTemp = 60;
    m_pidSettings[0].p =  600; // 6.00
    m_pidSettings[0].i =   10; // 0.1
    m_pidSettings[0].d = 1000; // 10.00

    m_maxPin = 25.0;
    m_minPin = 5.0;
    m_maxVin = 5.5;
    m_minVin = 4.5;
    m_minCurrentA = 0.0f;
    m_maxCurrentA = 6.0f;

    m_asicMaxDifficulty = 2048;
    m_asicMinDifficulty = 512;
    m_asicMinDifficultyDualPool = 256;

#ifdef NERDAXEGAMMA
    m_theme = new ThemeNerdaxegamma();
#endif

    m_swarmColorName = "#e7cf00"; // yellow

    m_asics = new BM1370();
    m_hasHashCounter = true;
    m_vrFrequency = m_defaultVrFrequency = m_asics->getDefaultVrFrequency();
}

NerdaxeGaia::NerdaxeGaia() : NerdAxe() {
    m_deviceModel = "NerdAxeGaia";
    m_miningAgent = "NerdAxe";
    m_asicModel = "BM1373";
    m_version = 200;
    m_asicCount = 1;

    m_asicJobIntervalMs = 1500;
    m_asicFrequencies = {300, 320, 340, 350, 360, 380, 400, 420, 440};
    m_asicVoltages = {900, 920, 940, 960, 980, 1000, 1020, 1040};
    m_defaultAsicFrequency = m_asicFrequency = 350;
    m_defaultAsicVoltageMillis = m_asicVoltageMillis = 940;
    m_absMinAsicVoltageMillis = 800;    // hard floor for core voltage (mV)
    m_absMaxAsicVoltageMillis = 1300;   // hard ceiling for core voltage (mV)
    // m_absMaxAsicFrequency = 750;
    m_initVoltageMillis = 1000;
    m_fanInvertPolarity = false;
    m_fanPerc = 100;
    m_flipScreen = false;   // Gaia panel mounted rotated 180° vs NerdAxe → flip default
    m_vr_maxTemp = TPS546_THROTTLE_TEMP; //Set max voltage regulator temp

    m_pidSettings[0].targetTemp = 60;
    m_pidSettings[0].p =  600; // 6.00
    m_pidSettings[0].i =   10; // 0.1
    m_pidSettings[0].d = 1000; // 10.00

    m_maxPin = 40.0;   // web power-gauge reference ceiling (W) — display only, not a runtime cutoff
    m_minPin = 5.0;
    m_maxVin = 13.2;   // 12V input rail (+10%)
    m_minVin = 10.8;   // 12V input rail (-10%)
    m_minCurrentA = 0.0f;
    m_maxCurrentA = 6.0f;

    m_asicMaxDifficulty = 2048;
    m_asicMinDifficulty = 512;
    m_asicMinDifficultyDualPool = 256;

#ifdef NERDAXEGAIA
    m_theme = new ThemeNerdaxegaia();
#endif

    // Default web dashboard theme (Nebular) for this board = the Gaia theme.
    // Served to the browser via the API as `defaultTheme`; the web applies it
    // when there is no valid theme saved in localStorage. (This is the web
    // theme, separate from m_theme above, which is the on-device LCD theme.)
    m_defaultTheme = "gaia";

    m_swarmColorName = "#e7cf00"; // yellow

    m_asics = new BM1373();
    m_hasHashCounter = true;
    m_vrFrequency = m_defaultVrFrequency = m_asics->getDefaultVrFrequency();

    m_hasEth = isEthConnected();
}

static std::vector<uint32_t> s_freqs, s_volts;

/* the constructed board's fields; vr_frequency: the chip marker (1366..)
 * or the value set directly */
extern "C" bool ref_board_profile(int board, bool tps67, bool six_phase, board_profile_t *out)
{
    s_vr_pin = tps67;
    s_q1373_tps67 = tps67;
    s_q1373_6ph = six_phase;
    Board *b = nullptr;
    switch (board) {
    case BOARD_NERDAXE: b = new NerdAxe(); break;
    case BOARD_NERDAXEGAMMA: b = new NerdaxeGamma(); break;
    case BOARD_NERDAXEGAIA: b = new NerdaxeGaia(); break;
    case BOARD_NERDQAXEPLUS: b = new NerdQaxePlus(); break;
    case BOARD_NERDOCTAXEPLUS: b = new NerdOctaxePlus(); break;
    case BOARD_NERDQAXEPLUS2: b = new NerdQaxePlus2(); break;
    case BOARD_NERDQX: b = new NerdQX(); break;
    case BOARD_NERDHAXEGAMMA: b = new NerdHaxeGamma(); break;
    case BOARD_NERDEKO: b = new NerdEko(); break;
    case BOARD_NERDOCTAXEGAMMA: b = new NerdOctaxeGamma(); break;
    case BOARD_Q1370: b = new Q1370B(); break;
    case BOARD_Q1373: b = new Q1373B(); break;
    default: return false;
    }
    board_profile_t p = {};
    p.device_model = b->m_deviceModel;
    p.mining_agent = b->m_miningAgent;
    p.version = b->m_version;
    p.asic_model = b->m_asicModel;
    p.asic_count = b->m_asicCount;
    p.job_interval_ms = b->m_asicJobIntervalMs;
    s_freqs = b->m_asicFrequencies;
    s_volts = b->m_asicVoltages;
    p.frequencies = s_freqs.data();
    p.frequency_count = s_freqs.size();
    p.voltages = s_volts.data();
    p.voltage_count = s_volts.size();
    p.default_frequency = b->m_defaultAsicFrequency;
    p.default_voltage_mv = b->m_defaultAsicVoltageMillis;
    p.abs_max_frequency = b->m_absMaxAsicFrequency;
    p.abs_min_voltage_mv = b->m_absMinAsicVoltageMillis;
    p.abs_max_voltage_mv = b->m_absMaxAsicVoltageMillis;
    p.eco_frequency = b->m_ecoAsicFrequency;
    p.eco_voltage_mv = b->m_ecoAsicVoltageMillis;
    p.init_voltage_mv = b->m_initVoltageMillis;
    p.num_phases = b->m_numPhases;
    p.imax = b->m_imax;
    p.ifault = b->m_ifault;
    p.fan_invert_polarity = b->m_fanInvertPolarity;
    p.fan_perc = b->m_fanPerc;
    p.flip_screen = b->m_flipScreen;
    p.num_temp_sensors = b->m_numTempSensors;
    p.vr_max_temp = b->m_vr_maxTemp;
    for (int ch = 0; ch < 2; ch++) {
        p.pid[ch].target_temp = b->m_pidSettings[ch].targetTemp;
        p.pid[ch].p = b->m_pidSettings[ch].p;
        p.pid[ch].i = b->m_pidSettings[ch].i;
        p.pid[ch].d = b->m_pidSettings[ch].d;
        p.fan_labels[ch] = b->m_fanLabels[ch];
    }
    p.fan_mode0 = b->m_fanMode[0] == CONFIG_AUTO_FAN_SPEED_VALUE ? -1 : b->m_fanMode[0];
    p.num_fans = b->m_numFans;
    p.max_pin = b->m_maxPin;
    p.min_pin = b->m_minPin;
    p.max_vin = b->m_maxVin;
    p.min_vin = b->m_minVin;
    p.min_current_a = b->m_minCurrentA;
    p.max_current_a = b->m_maxCurrentA;
    p.max_difficulty = b->m_asicMaxDifficulty;
    p.min_difficulty = b->m_asicMinDifficulty;
    p.min_difficulty_dual_pool = b->m_asicMinDifficultyDualPool;
    p.swarm_color = b->m_swarmColorName;
    p.default_theme = b->m_defaultTheme;
    p.vr_frequency = b->m_vrFrequency;
    p.has_hash_counter = b->m_hasHashCounter;
    p.tps53667 = b->m_tps && b->m_tps->tps53667;
    *out = p;
    return b->m_defaultVrFrequency == b->m_vrFrequency && b->m_asicFrequency == b->m_defaultAsicFrequency &&
           b->m_asicVoltageMillis == b->m_defaultAsicVoltageMillis;
}
