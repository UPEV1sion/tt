import re
import subprocess
from pathlib import Path
import pytest

BIN = Path(__file__).resolve().parent.parent / "tt"
ANSI_ESCAPE_RE = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")


def raw_output(text: str) -> str:
    return ANSI_ESCAPE_RE.sub("", text)


def assert_ok(res: subprocess.CompletedProcess) -> str:
    clean = raw_output(res.stdout)
    assert res.returncode == 0, (
        f"Command failed with exit code {res.returncode}\n"
        f"--- STDOUT ---\n{clean}\n"
        f"--- STDERR ---\n{res.stderr}"
    )
    return clean


def write_task(
    base_dir: Path,
    task_id: str,
    title: str,
    status: str = "OPEN",
    priority: int = 50,
    tags: list[str] | None = None,
    refs: list[str] | None = None,
    body: str = "",
) -> Path:
    task_dir = base_dir / "tasks" / task_id
    task_dir.mkdir(parents=True, exist_ok=True)
    tags_str = ", ".join(tags or [])
    refs_str = ", ".join(refs or [])

    lines = [
        f"# {title}",
        "",
        f"- STATUS: {status}",
        f"- PRIORITY: {priority}",
    ]
    if tags:
        lines.append(f"- TAGS:{', '.join(tags)}")
    if refs:
        lines.append(f"- REFERENCES: {refs_str}")
    if body:
        lines.extend(["", body])

    task_file = task_dir / "TASK.md"
    task_file.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return task_file


def run_tt(cwd: Path, *args: str, input_text: str | None = None) -> subprocess.CompletedProcess:
    return subprocess.run(
        [
            "valgrind",
            "--error-exitcode=1",
            "--leak-check=full",
            "--show-leak-kinds=all",
            "--errors-for-leak-kinds=all",
            str(BIN),
            *args,
        ],
        cwd=cwd,
        input=input_text,
        text=True,
        capture_output=True,
    )


def test_default_list_excludes_closed_and_sorts_by_priority_desc(tmp_path: Path):
    write_task(tmp_path, "19700101-000001", "Low prio", priority=10, tags=["cli"])
    write_task(tmp_path, "19700101-000002", "High prio", priority=90, tags=["bug"])
    write_task(tmp_path, "19700101-000003", "Closed task", status="CLOSED", priority=100)

    res = run_tt(tmp_path)
    out = assert_ok(res)
    lines = [ln for ln in out.splitlines() if ln.strip()]
    assert len(lines) == 2
    assert "High prio" in lines[0]
    assert "Low prio" in lines[1]


def test_all_flag_includes_closed_tasks(tmp_path: Path):
    write_task(tmp_path, "19700101-000001", "Open task", status="OPEN", priority=20)
    write_task(tmp_path, "19700101-000002", "Closed task", status="CLOSED", priority=50)

    res = run_tt(tmp_path, "list", "--all")
    out = assert_ok(res)
    assert "Closed task" in out
    assert "Open task" in out


def test_filter_query_language(tmp_path: Path):
    write_task(tmp_path, "19700101-000001", "Bug item", tags=["bug"])
    write_task(tmp_path, "19700101-000002", "Feature item", tags=["feature"])
    write_task(tmp_path, "19700101-000003", "No tags item", tags=[])

    res = run_tt(tmp_path, "list", "-f", ".bug or untagged")
    out = assert_ok(res)
    assert "Bug item" in out
    assert "No tags item" in out
    assert "Feature item" not in out


def test_verbose_preserves_body_after_preamble(tmp_path: Path):
    body = "Add support for verbose output (-v, --verbose)."
    write_task(
        tmp_path,
        "19700101-000001",
        "Verbose output",
        status="OPEN",
        priority=20,
        tags=["feature", "cli"],
        body=body,
    )

    res = run_tt(tmp_path, "list", "-v")
    out = assert_ok(res)
    assert (
        "./tasks/19700101-000001/TASK.md: "
        "[STATUS: OPEN, PRIORITY: 20, TAGS: feature, cli] Verbose output"
    ) in out
    assert body in out


def test_edit_preserves_body_below_preamble(tmp_path: Path):
    body = "Do not delete this commentary."
    task_file = write_task(
        tmp_path,
        "19700101-000001",
        "Editable task",
        status="OPEN",
        priority=20,
        tags=["cli"],
        body=body,
    )

    res = run_tt(tmp_path, "edit", "19700101-000001", "-s", "IN_PROGRESS", "-p", "75")
    assert_ok(res)

    updated = task_file.read_text(encoding="utf-8")
    assert "- STATUS: IN_PROGRESS" in updated
    assert "- PRIORITY: 75" in updated
    assert body in updated


@pytest.mark.parametrize("bad_args", [
    ["edit", "19700101-000001", "-p", "150"],
    ["edit", "19700101-000001", "-s", "INVALID_STATUS"],
    ["list", "-f", "(.bug and"],
])
def test_invalid_inputs_exit_nonzero(tmp_path: Path, bad_args: list[str]):
    write_task(tmp_path, "19700101-000001", "Sample")
    res = run_tt(tmp_path, *bad_args)
    assert res.returncode != 0, (
        f"Expected non-zero exit for {bad_args}, got 0.\n"
        f"STDOUT:\n{raw_output(res.stdout)}\nSTDERR:\n{res.stderr}"
    )
