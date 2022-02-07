Import("env")
import json
import os
from pathlib import Path


max_id_length = 10


# Max ID length is 15 chars but we append the '-pr-p' for previous pages so we only have
# 10 chars for an id. This method shortens the IDs while attempting to keep them human
# readable.
def to_id(original_id):
    out_id = original_id
    if len(out_id) <= max_id_length:
        return out_id

    chars_to_strip = ("a", "e", "i", "o", "u", "-", "_")
    out_id = "".join([l for l in original_id if l not in chars_to_strip])

    if len(out_id) <= max_id_length:
        return out_id

    return out_id[:10]


def generate_manifest(path):
    """Generate manifest.json dynamically based on .txt files in `path`"""
    os.makedirs(path, exist_ok=True)

    manifest_file = path / Path("manifest.json")

    p = Path(path).glob("*.txt")
    files = [x for x in p if x.is_file()]

    manifest = []

    for file in files:
        path = f"/{file.name}"
        id = file.stem
        with open(file) as f:
            # First line of the file is the title
            name = f.readline().rstrip()
        manifest.append(
            {
                "name": name,
                "id": to_id(id),
                "path": path,
            }
        )

    with open(manifest_file, "w") as outfile:
        json.dump(manifest, outfile, indent=4, sort_keys=True)


# TODO: It will be cleaner to only generate the manifest when building the filesystem
# but I'll need to learn more about target dependencies in SCons to do this. For now
# we will just build it every time. It doesn't add much time.
env["SPIFFS_SOURCES"] = "data"
generate_manifest(env["SPIFFS_SOURCES"])
