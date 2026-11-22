//! The free stack of every task (FreeRTOS high-water marks): a warning as
//! soon as a task gets below [`WARN_BYTES`], and every 30 minutes a table
//! of all tasks, so a stack that is too small shows up in the log before
//! it overflows. The system task checks every minute; tasks that live only
//! briefly (the ping session) check themselves ([`check_current`]).
//!
//! Needs `CONFIG_FREERTOS_USE_TRACE_FACILITY` (uxTaskGetSystemState()).

use core::ffi::CStr;
use std::collections::HashMap;
use std::sync::Mutex;

use crate::log::log;

const TAG: &CStr = c"stack";

/// A warning below this much free stack (bytes).
pub const WARN_BYTES: u32 = 512;
const CHECK_US: i64 = 60_000_000;
const TABLE_US: i64 = 30 * 60_000_000;
/// tasks per table line (the USB log drops characters in long lines)
const PER_LINE: usize = 6;

/// Held while a task that is created and deleted again and again (the
/// ping session) exists, and while the task list is read: the names are
/// read from the tasks' control blocks.
pub static TASK_LIFETIME: Mutex<()> = Mutex::new(());

#[derive(Default)]
struct Monitor {
    next_check_us: i64,
    next_table_us: i64,
    /// the lowest free stack a warning was logged for, per task
    warned: HashMap<String, u32>,
}

impl Monitor {
    /// A warning if `free` is below the limit and lower than the last
    /// warning for this task; the message, if any.
    fn note(&mut self, name: &str, free: u32) -> Option<String> {
        if free >= WARN_BYTES {
            return None;
        }
        if self.warned.get(name).is_some_and(|&w| free >= w) {
            return None;
        }
        self.warned.insert(name.to_string(), free);
        Some(format!(
            "task '{name}' has only {free} bytes of stack left (below {WARN_BYTES})"
        ))
    }
}

static MONITOR: Mutex<Option<Monitor>> = Mutex::new(None);

fn with<T>(f: impl FnOnce(&mut Monitor) -> T) -> T {
    let mut g = MONITOR.lock().unwrap_or_else(|e| e.into_inner());
    f(g.get_or_insert_with(Monitor::default))
}

/// The table lines: the tasks sorted by free stack, `name:free`.
fn table(mut tasks: Vec<(String, u32)>) -> Vec<String> {
    tasks.sort_by(|a, b| a.1.cmp(&b.1).then_with(|| a.0.cmp(&b.0)));
    tasks
        .chunks(PER_LINE)
        .map(|c| {
            c.iter()
                .map(|(n, f)| format!("{n}:{f}"))
                .collect::<Vec<_>>()
                .join(" ")
        })
        .collect()
}

/// Every tick of the system task: checks all tasks once a minute.
pub fn tick(now_us: i64) {
    let table_due = with(|m| {
        if now_us < m.next_check_us {
            return None;
        }
        m.next_check_us = now_us + CHECK_US;
        let due = now_us >= m.next_table_us;
        if due {
            m.next_table_us = now_us + TABLE_US;
        }
        Some(due)
    });
    let Some(table_due) = table_due else {
        return;
    };
    let Some(tasks) = tasks() else {
        // a ping session exists: again at the next tick
        with(|m| {
            m.next_check_us = 0;
            if table_due {
                m.next_table_us = 0;
            }
        });
        return;
    };
    let warnings: Vec<String> = with(|m| tasks.iter().filter_map(|(n, f)| m.note(n, *f)).collect());
    for w in warnings {
        log!(Warn, TAG, "{w}");
    }
    if table_due && !tasks.is_empty() {
        log!(Info, TAG, "free stack (bytes) of {} tasks:", tasks.len());
        for line in table(tasks) {
            log!(Info, TAG, "  {line}");
        }
    }
}

/// A task's check of its own stack (for tasks that do not live long
/// enough for the minute check).
pub fn check_current(name: &str) {
    #[cfg(target_os = "espidf")]
    {
        // SAFETY: NULL is the calling task
        let free = unsafe { crate::idf::raw::uxTaskGetStackHighWaterMark(core::ptr::null_mut()) };
        if let Some(w) = with(|m| m.note(name, free)) {
            log!(Warn, TAG, "{w}");
        }
    }
    #[cfg(not(target_os = "espidf"))]
    let _ = name;
}

/// The name and free stack (bytes) of every task; None while a ping
/// session exists.
#[cfg(target_os = "espidf")]
fn tasks() -> Option<Vec<(String, u32)>> {
    use crate::idf::raw;
    let _lifetime = match TASK_LIFETIME.try_lock() {
        Ok(g) => g,
        Err(std::sync::TryLockError::Poisoned(e)) => e.into_inner(),
        Err(std::sync::TryLockError::WouldBlock) => return None,
    };
    // a few spare entries for tasks created meanwhile
    // SAFETY: a plain query
    let n = unsafe { raw::uxTaskGetNumberOfTasks() } as usize + 4;
    // SAFETY: TaskStatus_t is plain data (numbers and pointers), valid zeroed
    let mut status: Vec<raw::TaskStatus_t> = vec![unsafe { core::mem::zeroed() }; n];
    // SAFETY: `status` holds `n` entries; the names are copied while the
    // scheduler of this core is suspended and no ping session can be
    // deleted (TASK_LIFETIME); the other tasks of this firmware live for
    // good or delete only themselves, which the idle task finishes later
    // (as FreeRTOS's own vTaskList() reads the names)
    unsafe {
        raw::vTaskSuspendAll();
        let got = raw::uxTaskGetSystemState(status.as_mut_ptr(), n as _, core::ptr::null_mut());
        let list = status[..got as usize]
            .iter()
            .map(|t| {
                let name = if t.pcTaskName.is_null() {
                    "?".into()
                } else {
                    CStr::from_ptr(t.pcTaskName).to_string_lossy().into_owned()
                };
                (name, t.usStackHighWaterMark)
            })
            .collect();
        raw::xTaskResumeAll();
        Some(list)
    }
}

#[cfg(not(target_os = "espidf"))]
fn tasks() -> Option<Vec<(String, u32)>> {
    Some(Vec::new())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn warns_once_per_new_low() {
        let mut m = Monitor::default();
        assert_eq!(m.note("ping", 1600), None);
        assert!(m.note("ping", 300).is_some());
        assert_eq!(m.note("ping", 300), None);
        assert_eq!(m.note("ping", 400), None);
        assert!(m.note("ping", 200).is_some());
        assert!(m.note("sys_evt", 300).is_some());
    }

    #[test]
    fn table_is_sorted_and_split() {
        let t: Vec<(String, u32)> = (0..8).map(|i| (format!("t{i}"), 1000 - i * 10)).collect();
        let lines = table(t);
        assert_eq!(lines.len(), 2);
        assert!(lines[0].starts_with("t7:930 t6:940"));
        assert_eq!(lines[1], "t1:990 t0:1000");
    }
}
