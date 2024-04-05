`s3()`: Added support for authentication from environment.

The `access-key()` and `secret-key()` options are now optional,
which makes it possible to use authentication methods originated
from the environment, e.g. `AWS_...` environment variables or
credentials files from the `~/.aws/` directory.

For more info, see:
https://boto3.amazonaws.com/v1/documentation/api/latest/guide/credentials.html
