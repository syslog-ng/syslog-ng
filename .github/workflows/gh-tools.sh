gh_export()
{
    while [ "x$1" != "x" ]; do
        echo "$1<<__EOF__" >> $GITHUB_ENV
        eval echo \$$1     >> $GITHUB_ENV
        echo "__EOF__"     >> $GITHUB_ENV
        shift;
    done
}

gh_path()
{
    echo "$1" >> $GITHUB_PATH
}
