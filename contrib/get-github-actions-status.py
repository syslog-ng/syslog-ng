#!/bin/env python3

import json
import sys
import urllib.parse
import urllib.request
from time import sleep


def get_workflow_runs(workflow):
    url = "https://api.github.com/repos/syslog-ng/syslog-ng/actions/workflows/{}.yml/runs?branch=master".format(workflow)
    request = urllib.request.Request(url, None, {})
    try:
        with urllib.request.urlopen(request) as response:
            binary_response = response.read()
    except urllib.error.HTTPError:
        print('"' + workflow + '" is not a valid workflow. See .github/workflows/')
        sys.exit(1)

    runs = json.loads(binary_response.decode())["workflow_runs"]
    if len(runs) == 0:
        print('No runs have been executed of "' + workflow + '" yet.')
        sys.exit(1)

    return runs


def main():
    workflow = sys.argv[1]
    while True:
        runs = get_workflow_runs(workflow)
        not_pr_runs = filter(lambda run: run["event"] != "pull_request", runs)
        latest_run = max(not_pr_runs, key=lambda run: run["updated_at"])

        if latest_run["status"] != "completed":
            print("Job is still running. Trying again in 10 minutes.")
            sleep(10 * 60)
            continue

        result = latest_run["conclusion"]

        print('"' + latest_run["name"] + '" job finished.')
        print("Date: " + latest_run["updated_at"])
        print("URL: " + latest_run["html_url"])
        print("Result: " + result)

        if result == "success":
            sys.exit(0)
        else:
            sys.exit(1)


if __name__ == "__main__":
    main()
