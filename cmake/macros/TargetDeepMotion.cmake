macro(TARGET_DEEPMOTION)
    if (WIN32) # OR APPLE
        add_dependency_external_projects(deepMotion)
		
		SET(DEEPMOTION_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src)
		SET(DEEPMOTION_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/x64/deepMotion_highfidelity_integration.lib)
		
        target_include_directories(${TARGET_NAME} PRIVATE ${DEEPMOTION_INCLUDE_DIRS})
        target_link_libraries(${TARGET_NAME} ${DEEPMOTION_LIBRARIES})
    endif(WIN32) # OR APPLE
endmacro()
