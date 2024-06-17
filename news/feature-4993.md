`s3()`: Introduced server side encryption related options

`server-side-encryption()` and `kms-key()` can be used to configure encryption.

Currently only `server-side-encryption("aws:kms")` is supported.
The `kms-key()` should be:
  * an ID of a key
  * an alias of a key, but in that case you have to add the alias/prefix
  * an ARN of a key

To be able to use the aws:kms encryption the AWS Role or User has to have the following
permissions on the given key:
  * `kms:Decrypt`
  * `kms:Encrypt`
  * `kms:GenerateDataKey`

Check [this](https://repost.aws/knowledge-center/s3-large-file-encryption-kms-key) page on why the `kms:Decrypt` is mandatory.

Example config:
```
destination d_s3 {
  s3(
    bucket("log-archive-bucket")
    object-key("logs/syslog")
    server-side-encryption("aws:kms")
    kms-key("alias/log-archive")
  );
};
```

See the [S3](https://docs.aws.amazon.com/AmazonS3/latest/userguide/UsingKMSEncryption.html) documentation for more details.
