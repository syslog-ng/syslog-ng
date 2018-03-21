import pytest


def pytest_addoption(parser):
    parser.addoption("--runslow", action="store_true", help="Also run @slow tests.")
    parser.addoption("--loglevel", action="store", default="info", help="Set loglevel for test runs. Available loglevels: ['info', 'error', 'debug']. Default loglevel: info")

    parser.addoption("--installmode", action="store", default="custom", help="Set installmode of syslog-ng. Available modes: ['path', 'osepkg', 'balabitpkg', 'custom']. Default installmode: custom")
    parser.addoption("--installpath", action="store", help="Set installpath for installed syslog-ng. Used when installmode is: custom. Example path: '/home/user/syslog-ng/installdir/'")
    parser.addoption("--syslogngversion", action="store", default="3.13", help="Used syslog-ng version in configurations. Default: 3.13")


@pytest.fixture
def installmode(request):
    return request.config.getoption("--installmode")


@pytest.fixture
def installpath(request):
    return request.config.getoption("--installpath")


@pytest.fixture
def syslogngversion(request):
    return request.config.getoption("--syslogngversion")


@pytest.fixture
def loglevel(request):
    return request.config.getoption("--loglevel")


@pytest.fixture
def runslow(request):
    return request.config.getoption("--runslow")
