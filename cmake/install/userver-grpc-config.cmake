include_guard(GLOBAL)

if(userver_grpc_FOUND)
  return()
endif()

find_package(userver REQUIRED COMPONENTS
    core
)

set(USERVER_GRPC_SCRIPTS_PATH "${USERVER_CMAKE_DIR}/grpc")

include("${USERVER_CMAKE_DIR}/GrpcTargets.cmake")

include_directories(${USERVER_CMAKE_DIR}/proto_generated/proto)

set(userver_grpc_FOUND TRUE)
