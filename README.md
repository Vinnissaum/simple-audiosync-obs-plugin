# Camera Sync

An OBS Studio plugin that aligns several cameras with one mic. Each camera reaches OBS with its own lag, and one audio offset can only match one of them. A phone plays a test pattern where every camera can see it and the mic can hear it. The plugin measures every selected camera against the mic at the same time, then writes the corrections into OBS's own settings in one undoable Apply: a Video Delay (Async) filter on each camera and the mic's Sync Offset.

The pattern and its detector come from [Audio Video Sync Dock](https://github.com/norihiro/obs-audio-video-sync-dock), so the plugin is GPL-2.0-or-later.

- [Design](.design/multi-camera-sync.md)
- [Technical design](docs/tdd/multi-camera-sync.md)
- [Architecture decision records](docs/adr/README.md)
