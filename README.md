# 🎥 ProCapture

![Platform](https://img.shields.io/badge/Platform-Linux-lightgrey.svg)
![License](https://img.shields.io/badge/License-MIT-blue.svg)
![C](https://img.shields.io/badge/Language-C-orange.svg)

**ProCapture** is a blazing-fast, lightweight screen recorder for Linux. Built natively with C and GTK3, it acts as a highly efficient frontend for FFmpeg, focusing on **hardware acceleration** to ensure near-zero CPU overhead while recording.

Whether you are capturing gameplay, creating tutorials, or recording meetings, ProCapture leverages your GPU to do the heavy lifting.

> **Note:** Replace this line with a screenshot of your app! `![ProCapture Screenshot](link_to_your_image.png)`

## ✨ Features

* **Hardware Acceleration:** Native support for NVIDIA (`h264_nvenc`), Intel (`h264_qsv`), and AMD/Open-Source (`h264_vaapi`) encoders.
* **Minimalist Native UI:** Built with GTK3 to blend seamlessly into modern Linux desktop environments.
* **Dynamic Quality Control:** Uses CRF (Constant Rate Factor) and CQ (Constant Quality) instead of flat bitrates for mathematically perfect file sizes and visual fidelity.
* **Customizable Inputs:** Easily adjust framerates (up to 60fps), resolutions, and audio bitrates.
* **MP4 Faststart:** Automatically optimizes video files for immediate web streaming.

## 📦 Installation (Ubuntu / Debian)

The easiest way to install ProCapture is via the pre-compiled `.deb` package.

1. Go to the [Releases](../../releases) page.
2. Download the latest `procapture_1.0-1_amd64.deb` file.
3. Install it via terminal:
   ```bash
   sudo apt install ./procapture_1.0-1_amd64.deb