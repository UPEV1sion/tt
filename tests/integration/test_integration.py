# TASK(20260407-110736)

import os
import subprocess
import shutil
import time
import tempfile
import re
import uuid

PROGRAM = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "tt_test")
)

def run_cmd(args, input_data=None):
    result = subprocess.run(
        args,
        input=input_data,
        text=True,
        capture_output=True,
    )
    return result

def setup_tmp_dir():
    tmp = tempfile.mkdtemp(prefix=f"tt_test_{int(time.time()*1000)}_")
    cwd = os.getcwd()
    os.chdir(tmp)
    return tmp, cwd

def teardown_tmp_dir(tmp, cwd):
    os.chdir(cwd)
    shutil.rmtree(tmp)

def create_task(title, status, prio, tags=""):
    input_data = f"{title}\n{status}\n{prio}\n{tags}\n"
    result = run_cmd([PROGRAM, "-n"], input_data=input_data)
    assert result.returncode == 0
    return result

def list_tasks(args=None):
    if args is None:
        args = []
    result = run_cmd([PROGRAM] + args)
    assert result.returncode == 0
    return result.stdout

def test_create_and_list_task():
    tmp, cwd = setup_tmp_dir()
    try:
        create_task("Test Task", "OPEN", 10, "bug")

        output = list_tasks()

        assert "Test Task" in output
        assert "PRIORITY: 10" in output
        assert "bug" in output
    finally:
        teardown_tmp_dir(tmp, cwd)

def test_sort_by_priority_desc():
    tmp, cwd = setup_tmp_dir()
    try:
        create_task("Low", "OPEN", 1)
        time.sleep(1)
        create_task("High", "OPEN", 99)

        output = list_tasks(["-p"])

        assert output.index("High") < output.index("Low")
    finally:
        teardown_tmp_dir(tmp, cwd)

def test_sort_by_priority_asc():
    tmp, cwd = setup_tmp_dir()
    try:
        create_task("Low", "OPEN", 1)
        time.sleep(1)
        create_task("High", "OPEN", 99)

        output = list_tasks(["-P"])

        assert output.index("Low") < output.index("High")
    finally:
        teardown_tmp_dir(tmp, cwd)


def test_filter_by_tag():
    tmp, cwd = setup_tmp_dir()
    try:
        create_task("Bug Task", "OPEN", 10, "bug")
        time.sleep(1)
        create_task("Feature Task", "OPEN", 10, "feature")

        output = list_tasks(["-f", ".bug"])

        assert "Bug Task" in output
        assert "Feature Task" not in output
    finally:
        teardown_tmp_dir(tmp, cwd)

def test_filter_tagged_vs_untagged():
    tmp, cwd = setup_tmp_dir()
    try:
        create_task("Tagged Task", "OPEN", 10, "bug")
        time.sleep(1)
        create_task("Untagged Task", "OPEN", 10, "")

        tagged_output = list_tasks(["-f", "tagged"])
        untagged_output = list_tasks(["-f", "untagged"])

        assert "Tagged Task" in tagged_output
        assert "Untagged Task" not in tagged_output

        assert "Untagged Task" in untagged_output
        assert "Tagged Task" not in untagged_output
    finally:
        teardown_tmp_dir(tmp, cwd)

def test_filter_and_or_not():
    tmp, cwd = setup_tmp_dir()
    try:
        create_task("Bug Task", "OPEN", 10, "bug")
        time.sleep(1)
        create_task("Feature Task", "OPEN", 10, "feature")
        time.sleep(1)
        create_task("Clean Task", "OPEN", 10, "")

        output = list_tasks(["-f", ".bug or untagged"])

        assert "Bug Task" in output
        assert "Clean Task" in output
        assert "Feature Task" not in output

        output = list_tasks(["-f", "not .feature"])

        assert "Bug Task" in output
        assert "Clean Task" in output
        assert "Feature Task" not in output
    finally:
        teardown_tmp_dir(tmp, cwd)


def test_closed_tasks_are_hidden():
    tmp, cwd = setup_tmp_dir()
    try:
        create_task("Open Task", "OPEN", 10)
        time.sleep(1)
        create_task("Closed Task", "CLOSED", 10)

        output = list_tasks()

        assert "Open Task" in output
        assert "Closed Task" not in output
    finally:
        teardown_tmp_dir(tmp, cwd)


def test_invalid_filter_fails():
    tmp, cwd = setup_tmp_dir()
    try:
        create_task("Task", "OPEN", 10)

        result = run_cmd([PROGRAM, "-f", "and"])

        assert result.returncode != 0
        assert "ERROR" in result.stderr
    finally:
        teardown_tmp_dir(tmp, cwd)