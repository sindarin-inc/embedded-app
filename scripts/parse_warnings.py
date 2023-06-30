import curlify
import os
import requests
import re
from typing import List, Dict

GITHUB_API_URL = "https://api.github.com"
GITHUB_REPO = os.getenv("GITHUB_REPOSITORY")
GITHUB_TOKEN = os.getenv("GITHUB_TOKEN")
GITHUB_REF = os.getenv("GITHUB_REF")
GITHUB_REPOSITORY = os.getenv("GITHUB_REPOSITORY")
GITHUB_SHA = os.getenv("PR_GITHUB_SHA")
# GITHUB_PULL_REQUEST = None

# if GITHUB_REF:
#     match = re.search(r"pull/(\d+)/merge", GITHUB_REF)
#     if match:
#         GITHUB_PULL_REQUEST = match.group(1)
# else:
#     raise ValueError("GITHUB_REF not found in environment variables")
print(f"GITHUB_REPO: {GITHUB_REPO}")
# print(f"GITHUB_PULL_REQUEST: {GITHUB_PULL_REQUEST}")
print(f"GITHUB_TOKEN: {GITHUB_TOKEN}")
print(f"GITHUB_REF: {GITHUB_REF}")
print(f"GITHUB_SHA: {GITHUB_SHA}")
print(f"GITHUB_REPOSITORY: {GITHUB_REPOSITORY}")

# def parse_warnings(file_path):
#     warnings_and_errors = []
#     with open(file_path, "r") as f:
#         lines = f.readlines()
#         for line in lines:
#             match = re.match(r"(.*):(\d+):(\d+): (warning|error): (.*) \[.*\]", line)
#             if match:
#                 file, line_number, column_number, issue_type, message = match.groups()
#                 warnings_and_errors.append(
#                     {
#                         "file": file,
#                         "line": int(line_number),
#                         "column_number": int(column_number),
#                         "severity": issue_type,
#                         "message": message,
#                     }
#                     {"file": "file1.cpp", "line": 10, "message": "warning: some warning", "severity": "warning"},

#                 )
#     return warnings_and_errors


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
                # Remove the '/Users/johnboiles/Developer/repos/sindarin-inc/embedded-app/src/' prefix
                file_path = file_path.replace(
                    "/Users/johnboiles/Developer/repos/sindarin-inc/embedded-app/src/", ""
                )
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


# def post_comments(warnings_and_errors):
#     review_comments = []
#     for issue in warnings_and_errors:
#         review_comments.append(
#             {
#                 "path": issue["file"].replace("/__w/embedded-app/embedded-app/", ""),
#                 "position": issue["line_number"],
#                 "body": f"{issue['issue_type']}: {issue['message']}",
#             }
#         )
#     data = {
#         # One does not simply get the commit for a PR
#         # See also: https://github.com/orgs/community/discussions/26325
#         # "commit_id": os.environ["GITHUB_SHA"],
#         "event": "COMMENT",
#         "comments": review_comments,
#     }
#     headers = {"Authorization": f"token {GITHUB_TOKEN}"}
#     response = requests.post(
#         f"{GITHUB_API_URL}/repos/{GITHUB_REPO}/pulls/{GITHUB_PULL_REQUEST}/reviews",
#         headers=headers,
#         json=data,
#     )
#     print(f"Made request: {curlify.to_curl(response.request)}")
#     print(f"Got response: {response.text}")
#     response.raise_for_status()


def post_annotations_chunked(warnings_and_errors: List[Dict[str, str]]):
    chunk_size = 50
    for i in range(0, len(warnings_and_errors), chunk_size):
        chunk = warnings_and_errors[i : i + chunk_size]
        post_annotations(chunk)


if __name__ == "__main__":
    warnings_and_errors = parse_warnings("warnings.txt")
    post_annotations_chunked(warnings_and_errors)
