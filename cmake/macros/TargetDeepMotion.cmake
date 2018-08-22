macro(TARGET_DEEP_MOTION)
    IF (WIN32) # OR APPLE
        add_dependency_external_projects(deep-motion)
		
        SET(DEEPMOTION_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src)        
        target_include_directories(${TARGET_NAME} PRIVATE ${DEEPMOTION_INCLUDE_DIRS})
        
        IF ((EXISTS ${DEEPMOTION_DLL_PATH}/dm_core_tech.lib) AND (EXISTS ${DEEPMOTION_DLL_PATH}/dm_core_tech.dll))
			target_link_libraries(${TARGET_NAME} ${DEEPMOTION_DLL_PATH}/dm_core_tech.lib)
			add_paths_to_fixup_libs(${DEEPMOTION_DLL_PATH})
		ENDIF()
        
    ENDIF(WIN32) # OR APPLE
endmacro()
