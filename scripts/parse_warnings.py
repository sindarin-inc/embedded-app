import curlify
import os
import requests
import re
from typing import List, Dict

GITHUB_TOKEN = os.getenv("GITHUB_TOKEN")
GITHUB_REPOSITORY = os.getenv("GITHUB_REPOSITORY")
GITHUB_SHA = os.getenv("PR_GITHUB_SHA")
PATH_PREFIX = "/__w/embedded-app/embedded-app/"

print(f"GITHUB_SHA: {GITHUB_SHA}")
print(f"GITHUB_REPOSITORY: {GITHUB_REPOSITORY}")


def parse_warnings(file_path):
    severity_map = {"warning": "warning", "error": "failure", "note": "notice"}

    warning_pattern = re.compile(r"^(.*?):(\d+):(\d+): (warning|error): (.*)$")
    annotations = []

    with open(file_path, "r") as f:
        lines = f.readlines()
        for line in lines:
            match = warning_pattern.match(line)
            if match:
                file_path, line, column, severity, message = match.groups()
                # Remove the absolute path prefix to make it a relative path
                file_path = file_path.replace(PATH_PREFIX, "")
                annotation = {
                    "path": file_path,
                    "start_line": int(line),
                    "end_line": int(line),
                    "start_column": int(column),
                    "annotation_level": severity_map.get(severity, "notice"),
                    "message": message,
                    "title": f"clang-tidy {severity}",
                }
                annotations.append(annotation)

    return annotations


def post_annotations(warnings_and_errors: List[Dict[str, str]]):
    url = f"https://api.github.com/repos/{GITHUB_REPOSITORY}/check-runs"
    headers = {
        "Accept": "application/vnd.github.v3+json",
        "Authorization": f"Bearer {GITHUB_TOKEN}",
    }
    data = {
        "name": "Clang-Tidy Analysis",
        "head_sha": GITHUB_SHA,
        "status": "completed",
        "conclusion": (
            "success"
            if all(issue["annotation_level"] != "failure" for issue in warnings_and_errors)
            else "failure"
        ),
        "output": {
            "title": "Clang-Tidy Results",
            "summary": f"{len(warnings_and_errors)} issues found",
            "annotations": warnings_and_errors,
        },
    }
    response = requests.post(url, headers=headers, json=data)
    print(f"Made request: {curlify.to_curl(response.request)}")
    print(f"Got response: {response.text}")
    response.raise_for_status()


def post_annotations_chunked(warnings_and_errors: List[Dict[str, str]]):
    chunk_size = 50
    for i in range(0, len(warnings_and_errors), chunk_size):
        chunk = warnings_and_errors[i : i + chunk_size]
        post_annotations(chunk)


if __name__ == "__main__":
    warnings_and_errors = parse_warnings("warnings.txt")
    post_annotations_chunked(warnings_and_errors)
