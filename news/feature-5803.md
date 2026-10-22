`http`: `response-body-action()` classifies a 2xx response by its body

For APIs that report a failure with a 2xx status code and the error in the response body, for
example `200 {"Status": "Fail"}`: `"regexp" => success|retry|drop|disconnect` pairs, matched in
order against the response body when the status code means success.
