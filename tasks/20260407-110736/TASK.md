# Remove sleep in pytest

- STATUS: OPEN
- PRIORITY: 20
- TAGS: test

`time.sleep` in the tests is a bit hacky and is slowing down the tests e.g.

```python
create_task("Bug Task", "OPEN", 10, "bug")
time.sleep(1)
create_task("Feature Task", "OPEN", 10, "feature")
time.sleep(1)
create_task("Clean Task", "OPEN", 10, "")
```

Find a way to manipulate the timestamp without a fix...
