add_unit_test(
  CRITERION
  TARGET test_cloud_auth
  SOURCES test-cloud-auth.cpp
  INCLUDES ${PROJECT_SOURCE_DIR}
           ${PROJECT_SOURCE_DIR}/modules/cloud-auth
           ${Curl_INCLUDE_DIR}
  DEPENDS cloud-auth-cpp ${Curl_LIBRARIES}
)

# jwt-cpp is third-party code: treat it as a system include so its
# deprecation warnings are not promoted to errors by -Werror (e.g. clang on FreeBSD).
target_include_directories(test_cloud_auth SYSTEM PRIVATE
  ${PROJECT_SOURCE_DIR}/modules/cloud-auth/jwt-cpp/include
)
