import os
import requests
import re

GITHUB_API_URL = "https://api.github.com"
GITHUB_REPO = os.environ["GITHUB_REPOSITORY"]
GITHUB_TOKEN = os.environ["GITHUB_TOKEN"]
GITHUB_REF = os.getenv("GITHUB_REF")
GITHUB_PULL_REQUEST = None

if GITHUB_REF:
    match = re.search(r"pull/(\d+)/merge", GITHUB_REF)
    if match:
        GITHUB_PULL_REQUEST = match.group(1)
else:
    raise ValueError("GITHUB_REF not found in environment variables")


def parse_warnings(file_path):
    warnings_and_errors = []
    with open(file_path, "r") as f:
        lines = f.readlines()
        for line in lines:
            match = re.match(r"(.*):(\d+):(\d+): (warning|error): (.*) \[.*\]", line)
            if match:
                file, line_number, column_number, issue_type, message = match.groups()
                warnings_and_errors.append(
                    {
                        "file": file,
                        "line_number": int(line_number),
                        "column_number": int(column_number),
                        "issue_type": issue_type,
                        "message": message,
                    }
                )
    return warnings_and_errors


def post_comments(warnings_and_errors):
    review_comments = []
    for issue in warnings_and_errors:
        review_comments.append(
            {
                "path": issue["file"],
                "position": issue["line_number"],
                "body": f"{issue['issue_type']}: {issue['message']}",
            }
        )
    data = {
        "commit_id": os.environ["GITHUB_SHA"],
        "event": "COMMENT",
        "comments": review_comments,
    }
    headers = {"Authorization": f"token {GITHUB_TOKEN}"}
    response = requests.post(
        f"{GITHUB_API_URL}/repos/{GITHUB_REPO}/pulls/{GITHUB_PULL_REQUEST}/reviews",
        headers=headers,
        json=data,
    )
    response.raise_for_status()


warnings_and_errors = parse_warnings("warnings.txt")
post_comments(warnings_and_errors)
