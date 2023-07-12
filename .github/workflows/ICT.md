# ICT Self-Hosted Runner

To run tests from CI against real hardware, we set up a self-hosted GitHub actions runner running on Ubuntu with a Nordic PPK2 and our in-house ICT tester board connected.

## Provisioning the Self-Hosted Runner

### Install Docker

Follow the [official instructions](https://docs.docker.com/engine/install/ubuntu/).

It's probably something like:

```sh
sudo apt-get update
sudo apt-get install ca-certificates curl gnupg
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
sudo chmod a+r /etc/apt/keyrings/docker.gpg
echo \
  "deb [arch="$(dpkg --print-architecture)" signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu \
  "$(. /etc/os-release && echo "$VERSION_CODENAME")" stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
sudo apt-get update
sudo apt-get install docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
```

### Setup $USER permissions and dependencies

```sh
sudo apt install espeak
sudo usermod -aG dialout $USER
sudo groupadd docker
sudo usermod -aG docker $USER
sudo reboot
```

### Install ESP-IDF

You'll want to generally follow directions for [ESP-IDF installation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/linux-macos-setup.html). Get the dependencies:

```sh
sudo apt install git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
```

NOTE: You'll want to use our fork and make sure `pytest` is installed.

```sh
mkdir -p ~/esp
cd ~/esp
git clone -b v5.1 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3 --enable-pytest
```

TODO: Eventually it would probably be good to have the esp-idf version get set up and cached from the action yml file itself.

### Set up Runner

Add a new runner [here](https://github.com/organizations/sindarin-inc/settings/actions/runners/new?arch=x64&os=linux) in the ICT group and follow the setup instructions.

Install it as a service (from the root of the newly created `actions-runner` dir):

```sh
sudo ./svc.sh install
sudo ./svc.sh start
```

And check to make sure it's running

```sh
sudo ./svc.sh status
```
