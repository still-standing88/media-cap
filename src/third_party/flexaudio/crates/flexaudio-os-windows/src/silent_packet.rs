// MediaCap patch: packet length is independent of the zero-fill allocation.
// Keep this helper platform independent so the bridge's normal suite tests it.
pub(super) fn push_silent_packet(
    silence: &[f32],
    mut samples: usize,
    mut push: impl FnMut(&[f32]),
) -> Result<(), &'static str> {
    if silence.is_empty() {
        return Err("Empty WASAPI silence scratch buffer");
    }
    while samples > 0 {
        let count = samples.min(silence.len());
        push(&silence[..count]);
        samples -= count;
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::push_silent_packet;

    #[test]
    fn process_loopback_silence_larger_than_buffer_is_preserved() {
        let silence = [0.0f32; 2048];
        let mut delivered = Vec::new();
        push_silent_packet(&silence, 5000, |chunk| {
            assert!(chunk.len() <= silence.len());
            assert_eq!(chunk.as_ptr(), silence.as_ptr());
            delivered.extend_from_slice(chunk);
        }).unwrap();
        assert_eq!(delivered, vec![0.0; 5000]);
    }

    #[test]
    fn empty_packet_does_not_emit_and_empty_storage_fails() {
        let mut calls = 0;
        push_silent_packet(&[0.0; 2], 0, |_| calls += 1).unwrap();
        assert_eq!(calls, 0);
        assert!(push_silent_packet(&[], 2, |_| calls += 1).is_err());
        assert_eq!(calls, 0);
    }
}
