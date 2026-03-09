#!/usr/bin/env python3
"""Convert MOV video to MP4 for use in Go Ball application."""

import subprocess
import sys
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR)

INPUT_FILE = os.path.join(os.path.expanduser("~"), "Downloads", "IMG_0095.mov")
OUTPUT_DIR = os.path.join(PROJECT_DIR, "modules", "game_videos")
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "visualize_tip.mp4")


def convert():
    if not os.path.exists(INPUT_FILE):
        print(f"Error: Source file not found: {INPUT_FILE}")
        sys.exit(1)

    os.makedirs(OUTPUT_DIR, exist_ok=True)

    cmd = [
        "ffmpeg", "-y",
        "-i", INPUT_FILE,
        "-c:v", "libx264",
        "-preset", "medium",
        "-crf", "23",
        "-c:a", "aac",
        "-b:a", "128k",
        "-movflags", "+faststart",
        OUTPUT_FILE,
    ]

    print(f"Converting: {INPUT_FILE}")
    print(f"Output:     {OUTPUT_FILE}")
    result = subprocess.run(cmd, capture_output=True, text=True)

    if result.returncode != 0:
        print(f"FFmpeg error:\n{result.stderr}")
        sys.exit(1)

    size_mb = os.path.getsize(OUTPUT_FILE) / (1024 * 1024)
    print(f"Done! Output size: {size_mb:.1f} MB")


if __name__ == "__main__":
    convert()
