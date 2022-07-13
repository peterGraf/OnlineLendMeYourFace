# LEND ME YOUR FACE!

## Setup

1. Set up Python 3.9 environment with [Miniconda](https://docs.conda.io/en/latest/miniconda.html)

Download Miniconda and follow the instructions to install it.
```bash
mkdir -p ~/miniconda3
wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh -O ~/miniconda3/miniconda.sh
bash ~/miniconda3/miniconda.sh -b -u -p ~/miniconda3
rm -rf ~/miniconda3/miniconda.sh
~/miniconda3/bin/conda init
exec $SHELL
```

2. Install [FFmpeg](https://ffmpeg.org/), [gdown](https://github.com/wkentaro/gdown), and [youtube-dl](https://github.com/ytdl-org/youtube-dl) with conda

```bash
conda install -c conda-forge ffmpeg gdown youtube-dl
```

3. Download model checkpoint with gdown

```bash
mkdir checkpoints
cd checkpoints
gdown 'https://drive.google.com/file/d/1L8P-hpBhZi8Q_1vP2KlQ4N6dvlzpYBvZ/view?usp=sharing' --fuzzy
```

4. Install [Poetry](https://python-poetry.org)

```bash
curl -sSL https://install.python-poetry.org | python3 -
```

5. Install dependencies with Poetry

```bash
poetry install
```

6. Create data directories

```bash
mkdir data data/downloaded_videos data/driving_videos data/source_images
```

## Video preparation with FFmpeg

### Download video from YouTube with best quality audio and video with youtube-dl

```bash
youtube-dl -f bestvideo+bestaudio 'https://www.youtube.com/watch?v=EJQ9hknvaQM' -o data/downloaded_videos/trump_thailand.mp4
```

### Show video info

```bash
ffprobe -v quiet -print_format json -show_format -show_streams data/downloaded_videos/trump_thailand.mp4
```

### Cut and reencode video to 30 fps with FFmpeg

```bash
ffmpeg -i data/downloaded_videos/trump_thailand.mp4 -ss '00:00:00' -to '00:00:10' -r 30 data/downloaded_videos/trump_thailand_10s.mp4
```

### Crop videos to face

```bash
poetry run python crop-video.py --inp data/downloaded_videos/trump_thailand_10s.mp4 --image_shape 512,512
```

Run the printed ffmpeg command, e.g.:

```bash
ffmpeg -i data/downloaded_videos/trump_thailand_10s.mp4 -ss 0.16666666666666666 -t 9.8 -filter:v "crop=442:442:599:74, scale=512:512" data/driving_videos/trump_thailand_10s_cropped.m
p4
```

## Fake yourself

```bash
poetry run python fake-yourself.py --source_image data/source_images/PeterGBrille-512sq.png --driving_video data/driving_videos/trump_thailand_10s_cropped.mp4 --size 512
```

