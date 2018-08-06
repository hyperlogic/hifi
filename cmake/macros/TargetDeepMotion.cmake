macro(TARGET_DEEP_MOTION)
    if (WIN32) # OR APPLE
        add_dependency_external_projects(deep-motion)
        
        SET(DEEPMOTION_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src)
        SET(DEEPMOTION_DLL_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/x64)
        
        target_include_directories(${TARGET_NAME} PRIVATE ${DEEPMOTION_INCLUDE_DIRS})
        target_link_libraries(${TARGET_NAME} 
            optimized ${DEEPMOTION_DLL_PATH}/deepMotion_highfidelity_integration.lib
            debug ${DEEPMOTION_DLL_PATH}/deepMotion_highfidelity_integration_d.lib)
        
        add_paths_to_fixup_libs(${DEEPMOTION_DLL_PATH})
    endif(WIN32) # OR APPLE
endmacro()
