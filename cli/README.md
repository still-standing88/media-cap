# MediaCap CLI

MediaCap records screens, windows, cameras, and audio from the command line. It also saves screenshots and raw frames or samples. Build with `--cli` using the [platform build instructions](../docs/building.rst).

```text
MediaCap list monitors
MediaCap info monitor 0
MediaCap record screen -o desktop.mp4 --duration 10
MediaCap screenshot screen -o desktop.bmp
MediaCap capabilities audio --json
```

Run `MediaCap COMMAND --help` for command options and examples, or `MediaCap --help-all` for the full terminal reference. The [CLI guide](../docs/cli.rst) covers device selection, recording, screenshots, raw streams, and JSON requests.

Run `MediaCap docs` for the plain-text reference or `MediaCap docs --output command-reference.rst` to export reStructuredText. Include it in the Sphinx site with `python scripts/build_api_docs.py --cli /path/to/MediaCap`.
