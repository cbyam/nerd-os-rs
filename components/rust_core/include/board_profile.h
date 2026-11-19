/*
 * The board profiles (implemented in Rust,
 * components/rust_core/src/board_profile.rs): the constants the board
 * constructors set (models, frequency/voltage options, defaults and limits,
 * buck phases and currents, fan/PID defaults, power gauge ranges, pool
 * difficulties, colours, themes, version rolling frequency), with the
 * hardware detections as inputs. The strings and lists are static.
 */

#ifndef BOARD_PROFILE_H
#define BOARD_PROFILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_NERDAXE 0
#define BOARD_NERDAXEGAMMA 1
#define BOARD_NERDAXEGAIA 2
#define BOARD_NERDQAXEPLUS 3
#define BOARD_NERDOCTAXEPLUS 4
#define BOARD_NERDQAXEPLUS2 5
#define BOARD_NERDQX 6
#define BOARD_NERDHAXEGAMMA 7
#define BOARD_NERDEKO 8
#define BOARD_NERDOCTAXEGAMMA 9
#define BOARD_Q1370 10
#define BOARD_Q1373 11

typedef struct
{
    uint16_t target_temp;
    uint16_t p;
    uint16_t i;
    uint16_t d;
} board_pid_t;

typedef struct
{
    const char *device_model;
    const char *mining_agent;
    int version;
    const char *asic_model;
    int asic_count;
    int job_interval_ms;
    const uint32_t *frequencies;
    size_t frequency_count;
    const uint32_t *voltages;
    size_t voltage_count;
    int default_frequency;
    int default_voltage_mv;
    int abs_max_frequency;
    int abs_min_voltage_mv;
    int abs_max_voltage_mv;
    int eco_frequency;
    int eco_voltage_mv;
    int init_voltage_mv;
    int num_phases;
    int imax;
    float ifault;
    bool fan_invert_polarity;
    float fan_perc;
    bool flip_screen;
    int num_temp_sensors;
    float vr_max_temp;
    board_pid_t pid[2];
    int fan_mode0; /* -1: the Kconfig default */
    int num_fans;
    const char *fan_labels[2];
    float max_pin;
    float min_pin;
    float max_vin;
    float min_vin;
    float min_current_a;
    float max_current_a;
    uint32_t max_difficulty;
    uint32_t min_difficulty;
    uint32_t min_difficulty_dual_pool;
    const char *swarm_color;
    const char *default_theme;
    uint32_t vr_frequency;
    bool has_hash_counter;
    bool tps53667; /* a TPS53667 instead of the TPS53647 */
} board_profile_t;

typedef struct
{
    bool tps53667;  /* Q1373: VR device code, NerdOCTAXE-γ: GPIO3 */
    bool six_phase; /* Q1373: the 6-phase strap */
} board_detect_t;

/* false for an unknown board */
bool board_profile(int board, const board_detect_t *detect, board_profile_t *out);
size_t board_profile_sizeof(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_PROFILE_H */
