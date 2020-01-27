How to create a newsfile entry
==============================

 1. Create a file in the `news/` directory called `<type>-<pr-num>.md`, where `<type>` is either:
     * `feature`: New functionality.
     * `bugfix`: Fix to a reported bug.
     * `developer-note`: Changes, that are only interesting to developers. (internal API change, etc...)
     * `other`: Other important, but not categorized change.
    You can calculate your PR number, by checking the most recent Issue or PR on github,
    but it could be a good practice to add the commit, containing the news entry, after you
    have opened your PR, as you know your PR number already.
 2. Fill it with the summary of the PR.
     * You can use any markdown formatting.
     * Please make sure, that there are no lines longer than 120 characters.
     * Make it 2-3 sentences at most.
     * Start the entry with the affected modules then a colon. For example: "`network-dest`: Added..."
 3. Commit it with the message: "news: <type>-<pr-num>", for example: "news: bugfix-1234"


How to create the `NEWS.md` file for the release
================================================

 1. Run `news/create-newsfile.py` (`python3` required)
 2. Fill the `Highlights` section in the newly created `NEWS.md` file.
 3. Fill the contributors part of the `Credits` section.
    The internal news file creating tool can generate it for you.

Note, the script uses the `VERSION` file, so it is advised to run the script, after the version is bumped,
or make sure, that you fill the version correctly by yourself.
