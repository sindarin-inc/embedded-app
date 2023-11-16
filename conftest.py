import pytest


def pytest_addoption(parser):
    parser.addoption("--ssid", action="store", default="")
    parser.addoption("--password", action="store", default="")
    parser.addoption("--fullsync", action="store", default="")


@pytest.fixture(scope="session")
def ssid(request):
    ssid_value = request.config.option.ssid
    if ssid_value is None:
        pytest.skip()
    return ssid_value


@pytest.fixture(scope="session")
def password(request):
    password_value = request.config.option.password
    if password_value is None:
        pytest.skip()
    return password_value


@pytest.fixture(scope="session")
def fullsync(request):
    fullsync_value = request.config.option.fullsync
    if fullsync_value is None:
        pytest.skip()
    return fullsync_value
