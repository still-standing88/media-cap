use super::*;
use std::time::{Duration, Instant};

fn defaults() -> Config {
    Config {
        kind: 0,
        device_id: ptr::null(),
        process_id: 0,
        process_mode: 0,
        exclude_self: 0,
        exclude_pids: ptr::null(),
        exclude_pid_count: 0,
        mix_mic_device_id: ptr::null(),
        mix_system_device_id: ptr::null(),
        gain: 1.0,
        mix_mic_gain: 1.0,
        mix_system_gain: 1.0,
        sample_rate: 0,
        channels: 0,
        ring_capacity_chunks: 0,
    }
}

#[test]
fn configuration_rejects_ignored_routing_and_invalid_values() {
    unsafe {
        let mut c = defaults();
        assert!(config(&c).is_ok());
        c.kind = 2;
        assert!(config(&c).is_err()); // Never interpret a missing PID as system audio.
        c.process_id = std::process::id();
        assert!(config(&c).is_ok());
        let id = CString::new("selected-device").unwrap();
        c.device_id = id.as_ptr();
        assert!(config(&c).is_err()); // Process capture ignores device selection.
        c = defaults();
        c.gain = f32::NAN;
        assert!(config(&c).is_err());
        c = defaults();
        c.exclude_pid_count = 1;
        assert!(config(&c).is_err());
        c = defaults();
        c.channels = 3;
        assert!(config(&c).is_err());
        c = defaults();
        c.process_mode = 1;
        assert!(config(&c).is_err()); // Exclude has no meaning on microphone capture.
    }
}

#[test]
fn bridge_copies_configuration_and_preserves_utf8() {
    let mut c = defaults();
    let id = CString::new("Microphone \u{03b1} ".repeat(100)).unwrap();
    c.device_id = id.as_ptr();
    let owned = unsafe { config(&c).unwrap() };
    drop(id);
    assert_eq!(owned.device_id.unwrap(), "Microphone \u{03b1} ".repeat(100));
}

fn mock(rate: u32, channels: u16, capacity: usize) -> *mut c_void {
    let mut c = StreamConfig::default();
    c.output.sample_rate = rate;
    c.output.channels = channels;
    c.ring_capacity_chunks = capacity;
    let stream = Stream::open(c, Box::new(flexaudio::MockBackend::new(44_100, 1, 440.0))).unwrap();
    Box::into_raw(Box::new(Handle {
        stream,
        rate,
        channels,
        delivered: false,
    })) as *mut c_void
}

#[test]
fn sample_timeline_resampling_drain_and_retained_chunk_ownership() {
    for (rate, channels) in [(48_000, 2), (16_000, 1), (44_100, 2)] {
        unsafe {
            let h = mock(rate, channels, 50);
            let mut error = BridgeError::new(0, "");
            assert_eq!(mc_flex_start(h, &mut error), 0);
            let deadline = Instant::now() + Duration::from_secs(5);
            let mut chunks: Vec<Chunk> = Vec::new();
            while chunks.len() < 6 && Instant::now() < deadline {
                let mut chunk: Chunk = std::mem::zeroed();
                let result = mc_flex_poll(h, &mut chunk, &mut error);
                assert!(
                    result >= 0,
                    "{}",
                    CStr::from_ptr(error.message.as_ptr().cast()).to_string_lossy()
                );
                if result == 1 {
                    chunks.push(chunk);
                } else {
                    std::thread::sleep(Duration::from_millis(1));
                }
            }
            assert_eq!(chunks.len(), 6);
            assert_eq!(
                chunks[0].arrival_ns, 0,
                "arrival is stream-relative, not process-monotonic"
            );
            assert_eq!(mc_flex_stop(h, &mut error), 0);
            loop {
                let mut chunk: Chunk = std::mem::zeroed();
                let result = mc_flex_poll(h, &mut chunk, &mut error);
                assert!(result >= 0);
                if result == 0 {
                    break;
                }
                chunks.push(chunk);
            }
            mc_flex_free(h);
            let mut frames = 0u64;
            for chunk in chunks {
                assert_eq!(chunk.sample_rate, rate);
                assert_eq!(chunk.channels, channels);
                assert_eq!(chunk.length, chunk.frames as usize * channels as usize);
                let expected = (frames * 48_000 / rate as u64) * 1_000_000_000 / 48_000;
                assert_eq!(chunk.media_ns, expected as i64);
                let pcm = std::slice::from_raw_parts(chunk.data, chunk.length);
                assert!(pcm.iter().all(|s| s.is_finite()));
                assert!(pcm.iter().any(|s| s.abs() > 0.01));
                frames += chunk.frames as u64;
                mc_flex_chunk_free(chunk.owner); // Valid even after stream destruction.
            }
        }
    }
}

#[test]
fn queue_loss_is_an_error_instead_of_silent_success() {
    unsafe {
        let h = mock(48_000, 2, 1);
        let mut error = BridgeError::new(0, "");
        assert_eq!(mc_flex_start(h, &mut error), 0);
        std::thread::sleep(Duration::from_millis(250));
        let mut chunk: Chunk = std::mem::zeroed();
        assert_eq!(mc_flex_poll(h, &mut chunk, &mut error), -1);
        assert_eq!(error.code, 6);
        mc_flex_stop(h, &mut error);
        mc_flex_free(h);
    }
}

#[test]
fn invalid_handles_and_panics_are_contained() {
    let mut error = BridgeError::new(0, "");
    assert_eq!(unsafe { mc_flex_start(ptr::null_mut(), &mut error) }, -1);
    assert_eq!(error.code, 1);
    assert_eq!(boundary(&mut error, || panic!("test panic")), -1);
    assert_eq!(error.code, 8);
}
// Exercise the exact bounded-silence helper used by the WASAPI backend without
// resolving the unrelated upstream workspace CLI/binding dev dependencies.
#[path = "../../../third_party/flexaudio/crates/flexaudio-os-windows/src/silent_packet.rs"]
mod wasapi_silent_packet;

