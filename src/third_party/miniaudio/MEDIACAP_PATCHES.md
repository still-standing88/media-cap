# Local miniaudio patches

## WASAPI non-extensible mix format copy (2026-11-06)

`ma_device_init_internal__wasapi` copied `sizeof(MA_WAVEFORMATEX)` when
`GetMixFormat()` returned a non-extensible format with `cbSize == 0`.
The internal struct is 20 bytes with padding, whereas Windows allocates the
18-byte WAVEFORMATEX header. AB13X USB Audio reproduces the resulting two-byte
heap overread under AddressSanitizer during device initialization.

Copy only the 18-byte base header for non-extensible formats. `cbSize` counts
extension bytes, not the total structure size. Extensible formats retain their
existing full-structure copy. The upstream mirror under `lib-repos/` is unchanged;
before the patch both headers had SHA256
`E9E742F8C4D7CDD0FC81BA7F1C47B719D98D45E0138849AA0EC1E574D554A1EE`.

Regression: run the opt-in `live_audio_probe` against the AB13X input and loopback
with AddressSanitizer. The null-backend suite cannot exercise `GetMixFormat()`.
See `reviews/audio-timing/hardware-asan-symbolized.txt` for the original failure
and `reviews/audio-timing/hardware-asan-after.txt` for the corrected run.
