How to create a newsfile entry
==============================

 1. Create a file in the `news/` directory called `<type>-<pr-id>.md`, where `<type>` is either:
     * `feature`: New functionality.
     * `bugfix`: Fix to a reported bug.
     * `packaging`: Packaging related change.
     * `developer-note`: Changes, that are only interesting to developers. (internal API change, etc...)
     * `other`: Other important, but not categorized change.
    You can query the next PR ID with the `next_pr_id.py` script, but it could be a good practice to add the commit,
    containing the news entry, after you have opened your PR, as you will know your PR ID then.
    Multiple news entries can be created for a single PR, if necessary. The file name is numbered in that case:
    `<type>-<pr-id>-<n>.md`.
 2. Fill it with the summary of the PR.
     * You can use any markdown formatting.
     * Please make sure, that there are no lines longer than 120 characters.
     * Make it 2-3 sentences at most.
     * Start the entry with the affected modules then a colon. For example: "`network-dest`: Added..."
 3. Commit it with the message: `news: <type>-<pr-id>`, for example: "news: bugfix-1234"


How to create the `NEWS.md` file for the release
================================================

 1. Run `news/create-newsfile.py` (`python3` required)
 2. Fill the `Highlights` section in the newly created `NEWS.md` file.

Note, the script uses the `VERSION` file, so it is necessary bump the version there, before running the script.
