import pytest


def pytest_addoption(parser):
    parser.addoption("--ssid", action="store", default="")
    parser.addoption("--password", action="store", default="")
    parser.addoption("--fullsync", action="store_true", default=False, help="run full sync tests")


def pytest_collection_modifyitems(config, items):
    if config.getoption("--fullsync"):
        return
    skip_fullsync = pytest.mark.skip(reason="need --fullsync option to run")
    for item in items:
        if "fullsync" in item.keywords:
            item.add_marker(skip_fullsync)


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
