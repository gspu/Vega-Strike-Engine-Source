#SET_PROPERTY(DIRECTORY PROPERTY INCLUDE_DIRECTORIES "")
INCLUDE_DIRECTORIES("..")
SET(TRISORT_SOURCES trisort.cpp)
ADD_EXECUTABLE(vs-buildenv-trisort ${TRISORT_SOURCES})

SET(ASTEROID_SOURCES asteroidgen.cpp)
ADD_EXECUTABLE(vs-buildenv-asteroidgen ${ASTEROID_SOURCES})

SET(REPLACE_SOURCES replace.cpp)
ADD_EXECUTABLE(vs-buildenv-replace ${REPLACE_SOURCES})

#find Expat
FIND_PACKAGE(EXPAT REQUIRED)
IF (EXPAT_FOUND)
    SET(MSH_INCLUDES ${MSH_INCLUDES} ${EXPAT_INCLUDE_DIRS})
    SET(MSH_LIBS ${MSH_LIBS} ${EXPAT_LIBRARIES})
ELSE (EXPAT_FOUND)
    MESSAGE("!! We can't find expat for XML crap")
ENDIF (EXPAT_FOUND)

#ogre already looked for by parent
IF (OGRE_FOUND)
    SET(MSH_INCLUDES ${MSH_INCLUDES} ${OGRE_INCLUDE_DIRS})
    SET(MSH_LIBS ${MSH_LIBS} ${OGRE_LIBRARIES})
    MESSAGE("++ Compiling vega-meshtool with OgreMesh support")
ELSE (OGRE_FOUND)
    MESSAGE("-- Compiling vega-meshtool without OgreMesh support: Ogre not found")
ENDIF (OGRE_FOUND)

SET(MESHER_SOURCES
    mesher/Converter.cpp
    mesher/from_BFXM.cpp
    mesher/from_obj.cpp
    mesher/main.cpp
    mesher/to_obj.cpp
    mesher/to_OgreMesh.cpp
    mesher/to_BFXM.cpp
    mesher/Modules/Convert.cpp
    mesher/Modules/OldSyntax.cpp
    mesher/Modules/Dims.cpp
    mesher/Modules/BFXM_to_Wavefront.cpp
    mesher/Modules/BFXM_to_XMesh.cpp
    mesher/Modules/XMesh_to_BFXM.cpp
    mesher/Modules/XMesh_to_Ogre.cpp
    mesher/Modules/Wavefront_to_BFXM.cpp
    ${Vega_Strike_SOURCE_DIR}/src/hashtable.cpp
    ${Vega_Strike_SOURCE_DIR}/src/xml_support.cpp
)

INCLUDE_DIRECTORIES(${MSH_INCLUDES} mesher)
ADD_EXECUTABLE(vega-meshtool ${MESHER_SOURCES})

# OGRE already looked for by parent
# USE_SYSTEM_BOOST already set by main script
IF (OGRE_FOUND AND NOT USE_SYSTEM_BOOST)
    # Ogre usage in mesher requires boost::system
    ADD_DEPENDENCIES(vega-meshtool boost_system)
    SET(MSH_LIBS ${MSH_LIBS} boost_system)
ENDIF (OGRE_FOUND AND NOT USE_SYSTEM_BOOST)

TARGET_LINK_LIBRARIES(vega-meshtool ${MSH_LIBS})
INSTALL(TARGETS vega-meshtool DESTINATION bin)
