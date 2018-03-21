import pytest
import time

from source.testdb.initializer.setup_classes import SetupClasses

slow = pytest.mark.skipif(
    not pytest.config.getoption("--runslow"),
    reason="need --runslow option to run"
)


def create_temporary_script(sc):
    script_path = sc.file_register.get_registered_file_path(prefix="unittest", extension="py")
    dummy_python_script = """import time
time.sleep(3)
"""
    sc.file_writer.write_content_to_regular_file(file_path=script_path, content=dummy_python_script)
    return script_path


def init_unittest(request):
    sc = SetupClasses(request)
    return sc


@slow
def test_start_stop_process(request):
    sc = init_unittest(request)
    script_path = create_temporary_script(sc)
    start_process_command = "python3 %s" % script_path
    background_process = sc.executor_interface.start_process(start_process_command)
    time.sleep(1)
    assert background_process.is_running() is True
    assert sc.executor_interface.is_pid_in_process_list(pid=background_process.pid) is True
    sc.executor_interface.stop_process(process=background_process)
    assert background_process.is_running() is False
    assert sc.executor_interface.is_pid_in_process_list(pid=background_process.pid) is False
