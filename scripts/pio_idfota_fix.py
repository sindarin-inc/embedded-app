from os import environ, path

Import("env")
platform = env.PioPlatform()

# Fix for PlatformIO bug when using arduino from ./components where it points to
# the wrong espota.py path
if env["UPLOADER"] == "tools/espota.py":
    env.Replace(
        UPLOADER=path.join(
            platform.get_package_dir("framework-arduinoespressif32") or "", "components", "arduino", "tools", "espota.py"),
    )
