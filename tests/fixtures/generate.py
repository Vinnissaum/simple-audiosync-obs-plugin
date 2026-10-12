#! /usr/bin/env python3
'''
Writes the detector test fixtures with Audio Video Sync Dock's own generator, tool/videogen.py.

Usage: generate.py <Sync Dock checkout> <output directory>

The pattern is q=4,f=884,c=2 at 60 fps, 320x180 and 48 kHz, the settings of Sync Dock's shipped pattern.
Pillow and qrcode must be importable (Sync Dock's tool/requirements.txt). ffmpeg is not needed: this script calls the
generator's frame and sound classes directly and skips the muxing step.
'''

import importlib.util
import os
import sys
import tempfile

QR_INDEX = 37
AUDIO_MARKERS = 10


def load_videogen(checkout):
    spec = importlib.util.spec_from_file_location('videogen', os.path.join(checkout, 'tool', 'videogen.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def write_pgm(image, path):
    luma = image.convert('L')
    with open(path, 'wb') as out:
        out.write(b'P5\n%d %d\n255\n' % luma.size)
        out.write(luma.tobytes())


def main():
    checkout, outdir = sys.argv[1], os.path.abspath(sys.argv[2])
    videogen = load_videogen(checkout)
    os.makedirs(outdir, exist_ok=True)

    with tempfile.TemporaryDirectory() as workdir:
        ctx = videogen.Context(workdir, 60, 48000)
        ctx.set_video_size('320x180')
        pattern = videogen.Pattern(ctx, 'q=4,f=884,c=2')

        # The first frame of a marker cycle is the QR code. It carries the cycle's index.
        pattern.i = QR_INDEX
        qr_frame = pattern.video_frames()[0]
        with videogen.Image.open(qr_frame) as image:
            write_pgm(image, os.path.join(outdir, 'qr-frame.pgm'))

        # Ten marker cycles of 0.2 s each: a 2 second mono 16-bit clip, indices 0 to 9.
        gen = videogen.VideoGen(ctx)
        gen.patterns.append(pattern)
        gen._generate_audio(os.path.join(outdir, 'audio-48k-mono-s16le.pcm'), AUDIO_MARKERS)
        with open(os.path.join(outdir, 'audio-indices.txt'), 'w') as out:
            out.write(' '.join(str(i) for i in range(AUDIO_MARKERS)) + '\n')

        ctx.remove_image_files()


if __name__ == '__main__':
    main()
