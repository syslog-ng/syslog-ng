import pytest
from functional_tests.common.testcase import TC

@pytest.fixture(scope="function")
def setup_testcase(request):
    global testcase
    print("setup")
    testcase = TC()

    request.addfinalizer(teardown_testcase)

def teardown_testcase():
    print("teardown")
