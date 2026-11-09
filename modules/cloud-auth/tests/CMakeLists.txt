add_unit_test(
  CRITERION
  TARGET test_cloud_auth
  SOURCES test-cloud-auth.cpp
  INCLUDES ${PROJECT_SOURCE_DIR}/modules/cloud-auth
           ${PROJECT_SOURCE_DIR}/modules/cloud-auth/jwt-cpp/include
           ${Curl_INCLUDE_DIR}
  DEPENDS cloud-auth-cpp ${Curl_LIBRARIES}
)
