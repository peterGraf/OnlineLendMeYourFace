from pathlib import Path
import subprocess

import fire
from loguru import logger


def main():
    fire.Fire(fake_yourself)


def fake_yourself(
    source_image: str = "data/source_images/PeterGBrille-512sq.png",
    driving_video: str = "data/driving_videos/Trump_8sMiracle249f-512.mp4",
    result_path: str = "data/results",
    size: int = 512,
):
    source_image = Path(source_image)
    driving_video = Path(driving_video)
    result_path = Path(result_path)

    config = Path("config") / f"vox-adv-{size}.yaml"
    checkpoint = Path("checkpoints") / "vox-adv-cpk.pth.tar"

    result_video = f"{result_path}/{source_image.stem}_{driving_video.stem}_withoutAudio.mp4"
    result_video_with_audio = f"{result_path}/{source_image.stem}_{driving_video.stem}.mp4"

    cmd_demo = (
        "python demo.py "
        f"--config {config} "
        f"--checkpoint {checkpoint} "
        f"--source_image {source_image} "
        f"--driving_video {driving_video} "
        f"--size {size} "
        f"--result_video {result_video} "
        "--relative "
        "--adapt_scale "
    )
    run_cmd(cmd_demo)

    # Add audio
    cmd_add_audio = f"ffmpeg -i {result_video} -i {driving_video} -c:v copy -map 0:v:0 -map 1:a:0 {result_video_with_audio}"  # noqa: E501
    run_cmd(cmd_add_audio)

    # Remove video without audio
    cmd_remove_video_without_audio = f"rm {result_video}"
    run_cmd(cmd_remove_video_without_audio)

    logger.info(f"created {result_video_with_audio}")


def run_cmd(cmd_deep_animate):
    logger.info(cmd_deep_animate)
    subprocess.run(cmd_deep_animate, shell=True)


if __name__ == "__main__":
    main()
