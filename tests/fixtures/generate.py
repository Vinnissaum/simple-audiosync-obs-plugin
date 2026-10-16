#! /usr/bin/env python3
'''
Writes the detector test fixtures with Audio Video Sync Dock's own generator, tool/videogen.py.

Usage: generate.py <Sync Dock checkout> <output directory>

The pattern is q=4,f=884,c=2 at 60 fps, 320x180 and 48 kHz, the settings of Sync Dock's shipped pattern.
Pillow and qrcode must be importable (Sync Dock's tool/requirements.txt). ffmpeg is not needed: this script calls the
generator's frame and sound classes directly and skips the muxing step.

It writes:
- qr-frame.pgm: the QR frame of the marker cycle with index 37.
- audio-48k-mono-s16le.pcm and audio-indices.txt: the sound of marker cycles 0 to 9, from time 0, and their indices.
- video-cycles/: the frames of marker cycles 0 to 3, from time 0, with the shipped pattern's index wrap I=256. A cycle is
  3q = 12 frames: q QR frames, then q frames of each sync image. Each distinct frame is written once as frame-NN.pgm,
  numbered in the order it first appears, and sequence.txt names the 48 frames in order, one per line. The sound of
  each cycle starts at frame 2q of the cycle, where the second sync image replaces the first.
- qr-frame-f9.pgm: qr-frame.pgm with f=9 in its QR code, below the detector's range of 10 to 32,000, so the detector
  rejects it. The generator refuses to build a pattern with that f, so the script sets f on a built pattern: only the
  QR code's text changes.
'''

import glob
import importlib.util
import os
import sys
import tempfile

QR_INDEX = 37
REJECTED_F = 9
AUDIO_MARKERS = 10
VIDEO_CYCLES = 4


def load_videogen(checkout):
    spec = importlib.util.spec_from_file_location('videogen', os.path.join(checkout, 'tool', 'videogen.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def pgm_bytes(image):
    luma = image.convert('L')
    return b'P5\n%d %d\n255\n' % luma.size + luma.tobytes()


def write_pgm(image, path):
    with open(path, 'wb') as out:
        out.write(pgm_bytes(image))


def write_video_cycles(videogen, pattern, outdir):
    os.makedirs(outdir, exist_ok=True)
    for stale in glob.glob(os.path.join(outdir, 'frame-*.pgm')):
        os.unlink(stale)

    names = {}
    sequence = []
    pattern.i = 0
    for _ in range(VIDEO_CYCLES):
        for frame in pattern.video_frames():
            with videogen.Image.open(frame) as image:
                data = pgm_bytes(image)
            if data not in names:
                names[data] = 'frame-%02d.pgm' % len(names)
                with open(os.path.join(outdir, names[data]), 'wb') as out:
                    out.write(data)
            sequence.append(names[data])

    with open(os.path.join(outdir, 'sequence.txt'), 'w') as out:
        out.write('\n'.join(sequence) + '\n')


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

        # The frames of the first four marker cycles, on the same time 0 as the sound.
        write_video_cycles(videogen, pattern, os.path.join(outdir, 'video-cycles'))

        # The QR frame of index 37 again, with an f the detector rejects.
        rejected = videogen.Pattern(ctx, 'q=4,f=884,c=2')
        rejected.f = REJECTED_F
        rejected.i = QR_INDEX
        with videogen.Image.open(rejected.video_frames()[0]) as image:
            write_pgm(image, os.path.join(outdir, 'qr-frame-f9.pgm'))

        ctx.remove_image_files()


if __name__ == '__main__':
    main()
