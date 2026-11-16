add_unit_test(
  CRITERION
  TARGET test_cloud_auth
  SOURCES test-cloud-auth.cpp
  INCLUDES ${PROJECT_SOURCE_DIR}
           ${PROJECT_SOURCE_DIR}/modules/cloud-auth
  DEPENDS cloud-auth-cpp ${Curl_LIBRARIES}
)

# Keep dependency headers, including cURL headers provided by platform SDKs,
# classified as system headers under -Werror.
target_include_directories(test_cloud_auth SYSTEM PRIVATE
  ${PROJECT_SOURCE_DIR}/modules/cloud-auth/jwt-cpp/include
  ${Curl_INCLUDE_DIR}
)
