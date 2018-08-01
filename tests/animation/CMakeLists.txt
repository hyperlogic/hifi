# Declare dependencies
macro (setup_testcase_dependencies)
  # link in the shared libraries
  link_hifi_libraries(shared deepMotion animation gpu fbx graphics networking test-utils)

  package_libraries_for_deployment()
endmacro ()

setup_hifi_testcase()
