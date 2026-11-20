//! The route table against the former server: the registrations of
//! start_rest_server() in order (URI, method, handler, context, websocket,
//! recovery/core dump condition) and the checks each handler made at its
//! top (Connection: close, network, CORS, OTP), as recorded from the former
//! sources in c-http-routes/routes_ref.txt.

use c_reference as _;
use rust_core::http_routes::{active, flag, needs_otp, Handler, Method, Route, ROUTES};

const REF: &str = include_str!("../c-http-routes/routes_ref.txt");

fn handler(name: &str) -> Handler {
    use Handler::*;
    match name {
        "rest_recovery_handler" => Recovery,
        "handle_options_request" => Options,
        "GET_system_info" => SystemInfo,
        "GET_system_asic" => SystemAsic,
        "GET_influx_info" => InfluxGet,
        "GET_can_nodes" => CanNodes,
        "POST_can_slave_action" => CanSlaveAction,
        "PATCH_can_slave" => CanSlavePatch,
        "DELETE_can_slave" => CanSlaveDelete,
        "GET_V2_dashboard" => V2Dashboard,
        "GET_V2_settings" => V2SettingsGet,
        "PATCH_V2_settings" => V2SettingsPatch,
        "GET_V2_identify" => V2Identify,
        "GET_V2_system" => V2System,
        "GET_system_coredump" => Coredump,
        "POST_restart" => Restart,
        "GET_wifi_scan" => WifiScan,
        "POST_reset_stats" => ResetStats,
        "POST_shutdown" => Shutdown,
        "PATCH_update_settings" => SettingsPatch,
        "PATCH_update_influx" => InfluxPatch,
        "PATCH_update_otp" => OtpPatch,
        "POST_create_otp" => OtpCreate,
        "POST_create_otp_session" => OtpSession,
        "GET_otp_status" => OtpStatus,
        "GET_alert_info" => AlertGet,
        "POST_update_alert" => AlertPatch,
        "POST_test_alert" => AlertTest,
        "POST_OTA_update" => Ota,
        "POST_WWW_update" => OtaWww,
        "echo_handler" => Websocket,
        "POST_OTA_update_from_url" => OtaGithubPost,
        "GET_OTA_status" => OtaGithubGet,
        "rest_common_get_handler" => File,
        _ => panic!("unknown handler {name}"),
    }
}

fn method(name: &str) -> Method {
    match name {
        "GET" => Method::Get,
        "POST" => Method::Post,
        "PATCH" => Method::Patch,
        "DELETE" => Method::Delete,
        "OPTIONS" => Method::Options,
        _ => panic!("unknown method {name}"),
    }
}

/// The flags the former code amounts to.
fn flags(ctx: &str, ws: &str, cond: &str, checks: &str) -> u16 {
    let mut f = 0;
    for c in checks.split(',').filter(|c| *c != "-") {
        f |= match c {
            "CLOSE" => flag::CLOSE,
            "NET" => flag::NET,
            "CORS" => flag::CORS,
            "OTP" => flag::OTP,
            "OTP_FORCE" => flag::OTP_FORCE,
            "OTP_UNLESS_RECOVERY" => flag::OTP_UNLESS_RECOVERY,
            _ => panic!("unknown check {c}"),
        };
    }
    if ctx == "ctx" {
        f |= flag::CTX;
    }
    if ws == "ws" {
        f |= flag::WEBSOCKET;
    }
    f | match cond {
        "always" => 0,
        "coredump" => flag::COREDUMP_ONLY,
        "recovery" => flag::RECOVERY_ONLY,
        "normal" => flag::NORMAL_ONLY,
        _ => panic!("unknown condition {cond}"),
    }
}

/// Routes added after the port (not in the former server).
fn added(r: &Route) -> bool {
    r.uri().starts_with("/api/v2/profiles")
}

#[test]
fn routes_match_the_former_server() {
    let rows: Vec<Vec<&str>> = REF
        .lines()
        .filter(|l| !l.starts_with('#') && !l.is_empty())
        .map(|l| l.split(' ').collect())
        .collect();
    let ported: Vec<&Route> = ROUTES.iter().filter(|r| !added(r)).collect();
    assert_eq!(rows.len(), ported.len());
    for (row, rt) in rows.iter().zip(ported) {
        let [uri, m, h, ctx, ws, cond, checks] = row[..] else {
            panic!("bad row {row:?}");
        };
        let want = Route {
            uri: rt.uri,
            method: method(m),
            handler: handler(h),
            flags: flags(ctx, ws, cond, checks),
        };
        assert_eq!(rt.uri(), uri);
        assert_eq!(
            (rt.method, rt.handler, rt.flags),
            (want.method, want.handler, want.flags),
            "{row:?}"
        );
    }
}

#[test]
fn registered_per_mode_as_before() {
    // normal mode without core dumps (the firmware's configuration): all
    // but the core dump and recovery catch-all routes
    let n = |rec, cd| ROUTES.iter().filter(|r| active(r, rec, cd)).count();
    assert_eq!(n(false, false), ROUTES.len() - 3);
    assert_eq!(n(true, false), ROUTES.len() - 3);
    assert_eq!(n(false, true), ROUTES.len() - 1);
    // OTP as the former handlers checked it
    let www = ROUTES
        .iter()
        .find(|r| r.handler == Handler::OtaWww)
        .unwrap();
    assert!(needs_otp(www, false));
    assert!(!needs_otp(www, true));
}
