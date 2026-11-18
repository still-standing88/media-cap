pkg = import('pkgconfig')
install_data('../../../src/third_party/screen_capture_lite/LICENSE',
  install_dir: get_option('datadir') / 'licenses/MediaCap/screen_capture_lite')
pkg.generate(mediacap_lib,
  description: 'Media capture and processing library',
  extra_cflags: get_option('default_library') == 'static' ? ['-DMEDIACAP_STATIC'] : []
)

if is_windows
  metadata_python = find_program('python3', 'python')
  metadata_args = ['--libdir', get_option('libdir'), '--bindir', get_option('bindir'),
    '--includedir', get_option('includedir'), '--datadir', get_option('datadir'),
    '--dependency-dir', ffmpeg_lib_dir, '--shared', get_option('default_library') == 'shared' ? 'true' : 'false',
    '--debug', get_option('buildtype') == 'debug' ? 'true' : 'false']
  if flexaudio_enabled and get_option('default_library') == 'static'
    metadata_args += ['--rust-archive', flex_archive_path]
  endif
  meson.add_install_script(metadata_python, files('../../../scripts/win/install_metadata.py'), metadata_args)
else
  notices_dir = get_option('datadir') / 'licenses/MediaCap'
  install_data('../../../LICENSE', '../../../COPYING', install_dir: notices_dir)
  if fs.is_file('../../../THIRD_PARTY_NOTICES.md')
    install_data('../../../THIRD_PARTY_NOTICES.md', install_dir: notices_dir)
  endif
  if fs.is_dir('../../../licenses')
    install_subdir('../../../licenses', install_dir: notices_dir / 'dependencies', strip_directory: true)
  endif
endif
