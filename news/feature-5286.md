`s3`: Added two new options

* `content-type()`: users now can change the content type of the objects uploaded by syslog-ng.
* `use_compression()`: This option allows the users to change the default checksum settings for 
S3 compatible solutions that don't support checksums. Requires botocore 1.36 or above. Acceptable values are
`when_supported` (default) and `when_required`.

Example:
```
s3(
	url("http://localhost:9000")
	bucket("testbucket")
	object_key("testobject")
	access_key("<ACCESS_KEY_ID>")
	secret_key("<SECRET_ACCESS_KEY>")
	content_type("text/plain")
	use_checksum("when_required")
);
```

