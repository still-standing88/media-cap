//! Capture-only private C bridge. Upstream sources are kept unmodified.
#[cfg(test)]
mod tests;
use flexaudio::{
    AudioChunk, Availability, Error, ErrorKind, Event, NativeStatus, OpenOptions, PidScope,
    ProcessMode, SourceKind, Stream, StreamConfig,
};
use std::{
    ffi::{c_char, c_void, CStr, CString},
    panic::{catch_unwind, AssertUnwindSafe},
    ptr,
};

#[repr(C)]
pub struct Config {
    kind: i32,
    device_id: *const c_char,
    process_id: u32,
    process_mode: i32,
    exclude_self: i32,
    exclude_pids: *const u32,
    exclude_pid_count: usize,
    mix_mic_device_id: *const c_char,
    mix_system_device_id: *const c_char,
    gain: f32,
    mix_mic_gain: f32,
    mix_system_gain: f32,
    sample_rate: u32,
    channels: u16,
    ring_capacity_chunks: usize,
}
#[repr(C)]
#[derive(Debug)]
pub struct BridgeError {
    code: i32,
    native_code: i64,
    message: [u8; 512],
}
#[repr(C)]
pub struct Chunk {
    data: *const f32,
    length: usize,
    frames: u32,
    sample_rate: u32,
    channels: u16,
    media_ns: i64,
    arrival_ns: i64,
    owner: *mut c_void,
}
struct Handle {
    stream: Stream,
    rate: u32,
    channels: u16,
    delivered: bool,
}
impl BridgeError {
    fn new(code: i32, message: &str) -> Self {
        let mut out = Self {
            code,
            native_code: 0,
            message: [0; 512],
        };
        copy_text(&mut out.message, message);
        out
    }
}
fn copy_text<const N: usize>(out: &mut [u8; N], value: &str) {
    let mut end = value.len().min(N - 1);
    while !value.is_char_boundary(end) {
        end -= 1;
    }
    out[..end].copy_from_slice(&value.as_bytes()[..end]);
}
fn native(error: &Error) -> i64 {
    match error {
        Error::Context { source, context } => match context.native_status() {
            Some(NativeStatus::HResult { bits, .. }) => bits as i32 as i64,
            Some(NativeStatus::OsStatus { value, .. }) => value as i64,
            _ => native(source),
        },
        Error::Diagnostic { source, .. } => native(source),
        Error::Multiple(group) => native(group.primary()),
        _ => 0,
    }
}
impl From<Error> for BridgeError {
    fn from(error: Error) -> Self {
        let code = match error.kind() {
            ErrorKind::InvalidArg
            | ErrorKind::InvalidState
            | ErrorKind::IgnoredSettings
            | ErrorKind::AmbiguousDeviceName => 1,
            ErrorKind::DeviceNotFound => 2,
            ErrorKind::DeviceLost => 3,
            ErrorKind::Unsupported
            | ErrorKind::UnsupportedOsVersion
            | ErrorKind::UnsupportedFormat => 4,
            ErrorKind::PermissionDenied => 5,
            _ => 7,
        };
        let mut out = Self::new(code, &error.to_string());
        out.native_code = native(&error);
        out
    }
}
type Result<T> = std::result::Result<T, BridgeError>;
fn boundary(error: *mut BridgeError, f: impl FnOnce() -> Result<i32>) -> i32 {
    let result = catch_unwind(AssertUnwindSafe(f)).unwrap_or_else(|_| {
        Err(BridgeError::new(
            8,
            "Panic contained at flexaudio bridge boundary",
        ))
    });
    match result {
        Ok(value) => value,
        Err(value) => {
            if !error.is_null() {
                unsafe {
                    *error = value;
                }
            }
            -1
        }
    }
}
unsafe fn text(p: *const c_char) -> Result<Option<String>> {
    if p.is_null() {
        return Ok(None);
    }
    Ok(Some(
        CStr::from_ptr(p)
            .to_str()
            .map_err(|_| BridgeError::new(1, "Device ID is not UTF-8"))?
            .to_owned(),
    ))
}
unsafe fn config(p: *const Config) -> Result<StreamConfig> {
    let c = p
        .as_ref()
        .ok_or_else(|| BridgeError::new(1, "Missing audio configuration"))?;
    let mut out = StreamConfig::default();
    out.kind = match c.kind {
        0 => SourceKind::Mic,
        1 => SourceKind::SystemLoopback,
        2 => SourceKind::ProcessLoopback,
        3 => SourceKind::Mix,
        _ => return Err(BridgeError::new(1, "Unknown audio source kind")),
    };
    out.mode = match c.process_mode {
        0 => ProcessMode::Include,
        1 => ProcessMode::Exclude,
        _ => return Err(BridgeError::new(1, "Unknown process capture mode")),
    };
    if c.exclude_self != 0 && c.exclude_self != 1 {
        return Err(BridgeError::new(1, "exclude_self must be zero or one"));
    }
    if c.exclude_pid_count > 0 && c.exclude_pids.is_null() {
        return Err(BridgeError::new(1, "Missing excluded PID array"));
    }
    if c.exclude_pid_count > isize::MAX as usize / std::mem::size_of::<u32>() {
        return Err(BridgeError::new(1, "Excluded PID array is too large"));
    }
    out.device_id = text(c.device_id)?;
    out.mix_mic_device_id = text(c.mix_mic_device_id)?;
    out.mix_system_device_id = text(c.mix_system_device_id)?;
    out.target_pid = (c.process_id != 0).then_some(c.process_id);
    out.exclude_self = c.exclude_self != 0;
    if c.exclude_pid_count > 0 {
        out.exclude_pids = std::slice::from_raw_parts(c.exclude_pids, c.exclude_pid_count).to_vec();
    }
    if out.exclude_pids.contains(&0) {
        return Err(BridgeError::new(1, "Excluded process IDs must be nonzero"));
    }
    out.gain = c.gain;
    out.mix_mic_gain = c.mix_mic_gain;
    out.mix_system_gain = c.mix_system_gain;
    for gain in [out.gain, out.mix_mic_gain, out.mix_system_gain] {
        if !gain.is_finite() || gain < 0. {
            return Err(BridgeError::new(
                1,
                "Audio gains must be finite and nonnegative",
            ));
        }
    }
    if c.sample_rate != 0 {
        out.output.sample_rate = c.sample_rate;
    }
    if c.channels != 0 {
        out.output.channels = c.channels;
    }
    if c.ring_capacity_chunks != 0 {
        out.ring_capacity_chunks = c.ring_capacity_chunks;
    }
    out.output.validate()?;
    if out.kind == SourceKind::ProcessLoopback && out.target_pid.is_none() {
        return Err(BridgeError::new(1, "Process audio requires a target PID"));
    }
    #[cfg(target_os = "macos")]
    if out.kind != SourceKind::Mic {
        out.silence_probe = false;
    }
    let report = flexaudio::resolve_settings(&flexaudio::SettingsRequest::new(out.clone()))?;
    if report
        .entries
        .iter()
        .any(|e| matches!(e.outcome, flexaudio::SettingOutcome::Ignored { .. }))
    {
        return Err(BridgeError::new(
            1,
            "Requested audio settings would be ignored on this source/platform",
        ));
    }
    Ok(out)
}
#[no_mangle]
pub unsafe extern "C" fn mc_flex_validate(c: *const Config, e: *mut BridgeError) -> i32 {
    boundary(e, || {
        config(c)?;
        Ok(0)
    })
}
#[no_mangle]
pub unsafe extern "C" fn mc_flex_open(
    c: *const Config,
    out: *mut *mut c_void,
    e: *mut BridgeError,
) -> i32 {
    boundary(e, || {
        if out.is_null() {
            return Err(BridgeError::new(1, "Missing stream output"));
        }
        *out = ptr::null_mut();
        let c = config(c)?;
        let rate = c.output.sample_rate;
        let channels = c.output.channels;
        let stream = flexaudio::open_with_options(c, OpenOptions::new().strict(true))?;
        *out = Box::into_raw(Box::new(Handle {
            stream,
            rate,
            channels,
            delivered: false,
        })) as *mut c_void;
        Ok(0)
    })
}
unsafe fn handle<'a>(p: *mut c_void) -> Result<&'a mut Handle> {
    (p as *mut Handle)
        .as_mut()
        .ok_or_else(|| BridgeError::new(1, "Missing stream"))
}
#[no_mangle]
pub unsafe extern "C" fn mc_flex_start(p: *mut c_void, e: *mut BridgeError) -> i32 {
    boundary(e, || {
        handle(p)?.stream.start()?;
        Ok(0)
    })
}
#[no_mangle]
pub unsafe extern "C" fn mc_flex_stop(p: *mut c_void, e: *mut BridgeError) -> i32 {
    boundary(e, || {
        handle(p)?.stream.stop_checked()?;
        Ok(0)
    })
}
#[no_mangle]
pub unsafe extern "C" fn mc_flex_free(p: *mut c_void) {
    let _ = catch_unwind(AssertUnwindSafe(|| {
        if !p.is_null() {
            drop(Box::from_raw(p as *mut Handle));
        }
    }));
}
#[no_mangle]
pub unsafe extern "C" fn mc_flex_chunk_free(p: *mut c_void) {
    if !p.is_null() {
        drop(Box::from_raw(p as *mut AudioChunk));
    }
}
#[no_mangle]
pub unsafe extern "C" fn mc_flex_poll(p: *mut c_void, out: *mut Chunk, e: *mut BridgeError) -> i32 {
    boundary(e, || {
        if out.is_null() {
            return Err(BridgeError::new(1, "Missing chunk output"));
        }
        let h = handle(p)?;
        while let Some(event) = h.stream.poll_event() {
            match event {
                Event::TerminalError { error }
                | Event::ShutdownError { error }
                | Event::RecoverableError { error } => return Err(error.into()),
                Event::DeviceLost => return Err(Error::DeviceLost.into()),
                Event::StreamStalled => return Err(BridgeError::new(3, "Audio source stalled")),
                Event::AudioLoss { .. } | Event::ChunkDropped { .. } => {
                    return Err(BridgeError::new(6, "flexaudio reported lost audio samples"))
                }
                Event::PermissionDenied { permission, detail } => {
                    return Err(Error::PermissionDenied { permission, detail }.into())
                }
                Event::Error(message) => return Err(BridgeError::new(7, &message)),
                _ => {}
            }
        }
        if let Some(error) = h.stream.terminal_error() {
            return Err(error.into());
        }
        let Some(chunk) = h.stream.poll_chunk() else {
            return Ok(0);
        };
        if chunk.dropped_before != 0 {
            return Err(BridgeError::new(6, "flexaudio chunk queue dropped audio"));
        }
        if chunk.flags.contains(flexaudio::ChunkFlags::RECOVERED)
            || (h.delivered && chunk.flags.contains(flexaudio::ChunkFlags::DISCONTINUITY))
        {
            return Err(BridgeError::new(
                3,
                "Audio timeline was interrupted; restart the recording",
            ));
        }
        let frames = u32::try_from(chunk.frames)
            .map_err(|_| BridgeError::new(7, "Audio chunk size overflow"))?;
        let media_ns = (u128::from(chunk.frame_index) * 1_000_000_000
            / u128::from(flexaudio::core::types::SAMPLE_RATE))
        .try_into()
        .map_err(|_| BridgeError::new(7, "Audio timeline overflow"))?;
        let owner = Box::new(chunk);
        h.delivered = true;
        *out = Chunk {
            data: owner.data.as_ptr(),
            length: owner.data.len(),
            frames,
            sample_rate: h.rate,
            channels: h.channels,
            media_ns,
            arrival_ns: owner.pts_ns,
            owner: Box::into_raw(owner) as *mut c_void,
        };
        Ok(1)
    })
}
struct Device {
    id: CString,
    name: CString,
    kind: i32,
    rate: u32,
    channels: u16,
    default: i32,
}
#[repr(C)]
pub struct DeviceView {
    id: *const c_char,
    name: *const c_char,
    kind: i32,
    rate: u32,
    channels: u16,
    default: i32,
}
fn cstring(value: String) -> Result<CString> {
    CString::new(value).map_err(|_| BridgeError::new(7, "Invalid NUL in device metadata"))
}
#[no_mangle]
pub unsafe extern "C" fn mc_flex_devices(
    out: *mut *mut c_void,
    count: *mut usize,
    e: *mut BridgeError,
) -> i32 {
    boundary(e, || {
        if out.is_null() || count.is_null() {
            return Err(BridgeError::new(1, "Missing inventory output"));
        }
        *out = ptr::null_mut();
        *count = 0;
        let mut list = Vec::new();
        for d in flexaudio::devices()? {
            list.push(Device {
                id: cstring(d.id)?,
                name: cstring(d.name)?,
                kind: if d.is_loopback { 1 } else { 0 },
                rate: d.sample_rate,
                channels: d.channels,
                default: i32::from(d.is_default),
            });
        }
        *count = list.len();
        *out = Box::into_raw(Box::new(list)) as *mut c_void;
        Ok(0)
    })
}
#[no_mangle]
pub unsafe extern "C" fn mc_flex_device_at(p: *mut c_void, i: usize, out: *mut DeviceView) -> i32 {
    if p.is_null() || out.is_null() {
        return -1;
    }
    let list = &*(p as *const Vec<Device>);
    let Some(d) = list.get(i) else { return -1 };
    *out = DeviceView {
        id: d.id.as_ptr(),
        name: d.name.as_ptr(),
        kind: d.kind,
        rate: d.rate,
        channels: d.channels,
        default: d.default,
    };
    0
}
#[no_mangle]
pub unsafe extern "C" fn mc_flex_devices_free(p: *mut c_void) {
    if !p.is_null() {
        drop(Box::from_raw(p as *mut Vec<Device>));
    }
}
struct Process {
    pid: u32,
    name: CString,
    executable: Option<CString>,
    active: i32,
}
#[repr(C)]
pub struct ProcessView {
    pid: u32,
    name: *const c_char,
    executable: *const c_char,
    active: i32,
}
#[no_mangle]
pub unsafe extern "C" fn mc_flex_processes(
    out: *mut *mut c_void,
    count: *mut usize,
    e: *mut BridgeError,
) -> i32 {
    boundary(e, || {
        if out.is_null() || count.is_null() {
            return Err(BridgeError::new(1, "Missing process output"));
        }
        *out = ptr::null_mut();
        *count = 0;
        let mut list = Vec::new();
        for p in flexaudio::processes()? {
            list.push(Process {
                pid: p.pid,
                name: cstring(p.name)?,
                executable: p.executable.map(cstring).transpose()?,
                active: p.is_output_active.map(i32::from).unwrap_or(-1),
            });
        }
        *count = list.len();
        *out = Box::into_raw(Box::new(list)) as *mut c_void;
        Ok(0)
    })
}
#[no_mangle]
pub unsafe extern "C" fn mc_flex_process_at(
    p: *mut c_void,
    i: usize,
    out: *mut ProcessView,
) -> i32 {
    if p.is_null() || out.is_null() {
        return -1;
    }
    let list = &*(p as *const Vec<Process>);
    let Some(v) = list.get(i) else { return -1 };
    *out = ProcessView {
        pid: v.pid,
        name: v.name.as_ptr(),
        executable: v.executable.as_ref().map_or(ptr::null(), |s| s.as_ptr()),
        active: v.active,
    };
    0
}
#[no_mangle]
pub unsafe extern "C" fn mc_flex_processes_free(p: *mut c_void) {
    if !p.is_null() {
        drop(Box::from_raw(p as *mut Vec<Process>));
    }
}
#[repr(C)]
pub struct Capabilities {
    enabled: i32,
    mic: i32,
    system: i32,
    process: i32,
    mix: i32,
    exclusions: i32,
    process_scope: i32,
    exclusion_scope: i32,
    honors_device: i32,
    max_pids: u32,
    process_detail: [u8; 256],
    system_detail: [u8; 256],
}
fn availability(a: &Availability) -> i32 {
    match a {
        Availability::Supported => 1,
        Availability::Unavailable { .. } => 0,
        _ => 2,
    }
}

#[no_mangle]
pub unsafe extern "C" fn mc_flex_minimum_gated_os(major: *mut u32, minor: *mut u32, build: *mut u32) {
    #[cfg(any(target_os = "macos", target_os = "windows"))]
    {
        let version = flexaudio::minimum_gated_os_version();
        *major = version.major;
        *minor = version.minor;
        *build = version.build.unwrap_or(0);
    }
    #[cfg(not(any(target_os = "macos", target_os = "windows")))]
    {
        *major = 0;
        *minor = 0;
        *build = 0;
    }
}
fn scope(s: &PidScope) -> i32 {
    match s {
        PidScope::ExactPid => 1,
        PidScope::ProcessTree => 2,
        _ => 0,
    }
}
#[no_mangle]
pub unsafe extern "C" fn mc_flex_capabilities(out: *mut Capabilities, e: *mut BridgeError) -> i32 {
    boundary(e, || {
        if out.is_null() {
            return Err(BridgeError::new(1, "Missing capability output"));
        }
        let c = flexaudio::capabilities();
        let mut v = Capabilities {
            enabled: 1,
            mic: availability(&c.sources.mic.availability),
            system: availability(&c.sources.system.availability),
            process: availability(&c.sources.process.availability),
            mix: availability(&c.sources.mix.availability),
            exclusions: availability(&c.system_exclusions.availability),
            process_scope: scope(&c.process.include_scope),
            exclusion_scope: scope(&c.system_exclusions.scope),
            honors_device: i32::from(matches!(
                c.system_exclusions.device_selection,
                flexaudio::DeviceSelectionBehavior::Honored
            )),
            max_pids: c.system_exclusions.max_distinct_pids.unwrap_or(0),
            process_detail: [0; 256],
            system_detail: [0; 256],
        };
        copy_text(
            &mut v.process_detail,
            &format!("{:?}", c.sources.process.availability),
        );
        copy_text(
            &mut v.system_detail,
            &format!("{:?}", c.sources.system.availability),
        );
        *out = v;
        Ok(0)
    })
}
