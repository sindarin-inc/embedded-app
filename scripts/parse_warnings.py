import argparse
import curlify
import os
import requests
import re
from typing import List, Dict

GITHUB_TOKEN = os.getenv("GITHUB_TOKEN")
GITHUB_REPOSITORY = os.getenv("GITHUB_REPOSITORY")
GITHUB_SHA = os.getenv("PR_GITHUB_SHA")

print(f"GITHUB_SHA: {GITHUB_SHA}")
print(f"GITHUB_REPOSITORY: {GITHUB_REPOSITORY}")


def parse_warnings(file_path: str, title_prefix: str, path_prefix: str):
    severity_map = {"warning": "warning", "error": "failure", "note": "notice"}

    warning_pattern = re.compile(r"^(.*?):(\d+):(\d+): (warning|error): (.*)$")
    annotations = []
    existing_annotations = set()

    with open(file_path, "r") as f:
        lines = f.readlines()
        for line in lines:
            match = warning_pattern.match(line)
            if match:
                file_path, line, column, severity, message = match.groups()
                # Remove the absolute path prefix to make it a relative path
                file_path = file_path.replace(path_prefix, "")
                annotation = {
                    "path": file_path,
                    "start_line": int(line),
                    "end_line": int(line),
                    "start_column": int(column),
                    "annotation_level": severity_map.get(severity, "notice"),
                    "message": message,
                    "title": f"{title_prefix} {severity}",
                }
                # Check if the annotation already exists
                annotation_key = f"{file_path}:{line}:{column}:{severity}:{message}"
                if annotation_key in existing_annotations:
                    continue
                existing_annotations.add(annotation_key)
                annotations.append(annotation)

    return annotations


def post_annotations(warnings_and_errors: List[Dict[str, str]], title: str):
    url = f"https://api.github.com/repos/{GITHUB_REPOSITORY}/check-runs"
    headers = {
        "Accept": "application/vnd.github.v3+json",
        "Authorization": f"Bearer {GITHUB_TOKEN}",
    }
    data = {
        "name": title,
        "head_sha": GITHUB_SHA,
        "status": "completed",
        "conclusion": (
            "success"
            if all(issue["annotation_level"] != "failure" for issue in warnings_and_errors)
            else "failure"
        ),
        "output": {
            "title": title,
            "summary": f"{len(warnings_and_errors)} issues found",
            "annotations": warnings_and_errors,
        },
    }
    response = requests.post(url, headers=headers, json=data)
    print(f"Made request: {curlify.to_curl(response.request)}")
    print(f"Got response: {response.text}")
    response.raise_for_status()


def post_annotations_chunked(warnings_and_errors: List[Dict[str, str]], title: str):
    chunk_size = 50
    for i in range(0, len(warnings_and_errors), chunk_size):
        chunk = warnings_and_errors[i : i + chunk_size]
        post_annotations(chunk, title)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Process a filename and a title.")
    parser.add_argument("--filename", type=str, required=True, help="Filename to process")
    parser.add_argument("--title", type=str, required=True, help="Title to prepend to warnings")
    parser.add_argument("--path_prefix", type=str, required=True, help="Path to remove from warnings")
    args = parser.parse_args()

    warnings_and_errors = parse_warnings(args.filename, args.title, args.path_prefix)
    post_annotations_chunked(warnings_and_errors, args.title)
