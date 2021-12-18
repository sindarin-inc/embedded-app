Import("env")
from os.path import basename
from pathlib import Path
from shutil import copy

# Images uploaded to flash that aren't the main program
extra_images = env.get("FLASH_EXTRA_IMAGES")
# Upload flags minus the ones added for the extra images
upload_flags = env.get("UPLOADERFLAGS")[: -(len(extra_images) * 2)]

# Where to put the platform-specific things like bootloader_*.bin and boot_app0.bin
build_dir = Path(env.subst("$BUILD_DIR"))

images = []
for image in extra_images:
    images.append(image[0])
    # Strip out everything but the last path component
    images.append(basename(env.subst(image[1])))
    # If this file is a platform one (not something exported to BUILD_DIR) copy it
    if str(build_dir) not in image[1]:
        copy(env.subst(image[1]), str(build_dir))

command = f'esptool.py {" ".join(env.subst(upload_flags))} {" ".join(images)} {env.subst("$ESP32_APP_OFFSET")} {env.subst("${PROGNAME}.bin")}\n'
# Remove empty port flag (esptool can figure it out automatically)
command = command.replace('--port "" ', "")

with open(build_dir / "upload_command.sh", "w") as f:
    f.write(command)
    f.close()
